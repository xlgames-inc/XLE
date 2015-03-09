// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "BasicScene.h"

#include "../../PlatformRig/OverlappedWindow.h"
#include "../../PlatformRig/MainInputHandler.h"
#include "../../PlatformRig/InputTranslator.h"
#include "../../PlatformRig/DebuggingDisplays/GPUProfileDisplay.h"
#include "../../PlatformRig/DebuggingDisplays/CPUProfileDisplay.h"
#include "../../PlatformRig/FrameRig.h"
#include "../../PlatformRig/PlatformRigUtil.h"
#include "../../PlatformRig/OverlaySystem.h"

#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/Assets/ColladaCompilerInterface.h"
#include "../../RenderCore/Metal/GPUProfiler.h"
#include "../../RenderCore/Metal/State.h"
#include "../../RenderOverlays/Font.h"
#include "../../RenderOverlays/DebugHotKeys.h"
#include "../../BufferUploads/IBufferUploads.h"

#include "../../SceneEngine/SceneEngineUtility.h"
#include "../../SceneEngine/LightingParser.h"
#include "../../SceneEngine/LightingParserStandardPlugin.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/ResourceBox.h"
#include "../../SceneEngine/SceneParser.h"
#include "../../SceneEngine/LightDesc.h"

#include "../../ConsoleRig/Console.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/Profiling/CPUProfiler.h"

#include <functional>

unsigned FrameRenderCount = 0;

namespace Sample
{
///////////////////////////////////////////////////////////////////////////////////////////////////

        // "GPU profiler" doesn't have a place to live yet. We just manage it here, at 
        //  the top level
    RenderCore::Metal::GPUProfiler::Ptr g_gpuProfiler;
    Utility::HierarchicalCPUProfiler g_cpuProfiler;

///////////////////////////////////////////////////////////////////////////////////////////////////

    static void SetupCompilers(::Assets::CompileAndAsyncManager& asyncMan);
    static PlatformRig::FrameRig::RenderResult RenderFrame(
        RenderCore::IThreadContext* context,
        SceneEngine::LightingParserContext& lightingParserContext, BasicSceneParser* scene,
        RenderCore::IDevice* renderDevice, RenderCore::IPresentationChain* presentationChain,
        PlatformRig::IOverlaySystem* debugSystem);
    void RenderPostScene(RenderCore::IThreadContext* context);

    void ExecuteSample()
    {
        using namespace PlatformRig;
        using namespace Sample;

            // We need to startup some basic objects:
            //      * OverlappedWindow (corresponds to a single basic window on Windows)
            //      * RenderDevice & presentation chain
            //      * BufferUploads
            //      * CompileAndAsyncManager
            //
            // Note that the render device should be created first, so that the window
            // object is destroyed before the device is destroyed.
        LogInfo << "Building primary managers";
        auto console = std::make_unique<ConsoleRig::Console>();
        auto renderDevice = RenderCore::CreateDevice();

        PlatformRig::OverlappedWindow window;
        auto clientRect = window.GetRect();
        std::shared_ptr<RenderCore::IPresentationChain> presentationChain = 
            renderDevice->CreatePresentationChain(window.GetUnderlyingHandle(), 
                clientRect.second[0] - clientRect.first[0], clientRect.second[1] - clientRect.first[1]);
        auto bufferUploads = BufferUploads::CreateManager(renderDevice.get());
        auto asyncMan = RenderCore::Metal::CreateCompileAndAsyncManager();

            //  Tie in the window handler so we get presentation chain resizes, and give our
            //  window a title
            //  Here, I show 2 different ways to do dynamic string formatting.
            //      (note that XlDynFormatString will always allocate at least once!)
        window.AddWindowHandler(std::make_shared<PlatformRig::ResizePresentationChain>(presentationChain));
        auto v = renderDevice->GetVersionInformation();
        window.SetTitle(XlDynFormatString("XLE sample [RenderCore: %s : %s]", v.first, v.second).c_str());
        window.SetTitle(StringMeld<128>() << "XLE sample [RenderCore: " << v.first << ", " << v.second << "]");

            // Some secondary initalisation:
            //  * attach compilers
            //  * pass buffer uploads pointer to the scene engine
            //  * init the gpu profiler (this init step will probably change someday)
            //  * the font system needs an explicit init (and shutdown)
            //  * the global technique context contains some global rendering settings
        SetupCompilers(*asyncMan);
        SceneEngine::SetBufferUploads(bufferUploads.get());
        g_gpuProfiler = RenderCore::Metal::GPUProfiler::CreateProfiler();
        RenderOverlays::InitFontSystem(renderDevice.get(), bufferUploads.get());
        auto globalTechniqueContext = std::make_shared<PlatformRig::GlobalTechniqueContext>();

            //  We need a ISceneParser object to define the scene we want to 
            //  render. 
            //  The SceneEngine uses 2 basic "parser" objects:
            //      * the "lighting parser" defines what lighting steps are taken, and in which order
            //          generally this means managing shaders and render targets
            //      * the "scene parser" defines the contents of the scene
            //          generally this means managing models, animation and also environment settings
            //
            //  The scene parser is naturally more extensible. In this example, we'll use a verys 
            //  simple scene parser.
        LogInfo << "Creating main scene";
        auto mainScene = std::make_shared<BasicSceneParser>();
        
        {
                // currently we need to maintain a reference on these two fonts -- 
            auto defaultFont0 = RenderOverlays::GetX2Font("Raleway", 16);
            auto defaultFont1 = RenderOverlays::GetX2Font("Vera", 16);

                //  Create the debugging system, and add any "displays"
                //  If we have any custom displays to add, we can add them here. Often it's 
                //  useful to create a debugging display to go along with any new feature. 
                //  It just provides a convenient architecture for visualizing important information.
            LogInfo << "Setup tools and debugging";
            FrameRig frameRig;

            InitDebugDisplays(*frameRig.GetDebugSystem());

            if (g_gpuProfiler) {
                auto gpuProfilerDisplay = std::make_shared<PlatformRig::Overlays::GPUProfileDisplay>(g_gpuProfiler.get());
                frameRig.GetDebugSystem()->Register(gpuProfilerDisplay, "[Profiler] GPU Profiler");
            }
            frameRig.GetDebugSystem()->Register(
                std::make_shared<PlatformRig::Overlays::CPUProfileDisplay>(&g_cpuProfiler), 
                "[Profiler] CPU Profiler");


                //  Setup input:
                //      * We create a main input handler, and tie that to the window to receive inputs
                //      * We can add secondary input handles to the main input handler as required
                //      * The order in which we add handlers determines their priority in intercepting messages
            LogInfo << "Setup input";
            auto mainInputHandler = std::make_shared<PlatformRig::MainInputHandler>();
            mainInputHandler->AddListener(RenderOverlays::MakeHotKeysHandler("game/xleres/hotkey.txt"));
            mainInputHandler->AddListener(frameRig.GetMainOverlaySystem()->GetInputListener());
            window.GetInputTranslator().AddListener(mainInputHandler);

                //  The lighting parser allows plug ins for customizing the lighting process
                //  To make large-scale changes to the lighting pipeline, you should modify
                //  the lighting parser code directly. But sometimes you might want optional
                //  settings (maybe for quality settings or different rendering modes). In
                //  these cases, plugins can provide a way to customize the pipeline at run-time.
            auto stdPlugin = std::make_shared<SceneEngine::LightingParserStandardPlugin>();

                //  We can log the active assets at any time using this method.
                //  At this point during startup, we should only have a few assets loaded.
            asyncMan->GetAssetSets().LogReport();

                //  We need 2 final objects for rendering:
                //      * the FrameRig schedules continuous rendering. It will take care
                //          of timing and some thread management taskes
                //      * the DeviceContext provides the methods we need for rendering.
            LogInfo << "Setup frame rig and rendering context";
            auto context = renderDevice->GetImmediateContext();

                //  Finally, we execute the frame loop
            for (;;) {
                if (OverlappedWindow::DoMsgPump() == OverlappedWindow::Terminate) {
                    break;
                }

                    // ------- Render ----------------------------------------
                SceneEngine::LightingParserContext lightingParserContext(mainScene.get(), *globalTechniqueContext);
                lightingParserContext._plugins.push_back(stdPlugin);

                auto frameResult = frameRig.ExecuteFrame(
                    context.get(), renderDevice.get(), presentationChain.get(), 
                    g_gpuProfiler.get(), &g_cpuProfiler,
                    std::bind(
                        RenderFrame, std::placeholders::_1,
                        std::ref(lightingParserContext), mainScene.get(), 
                        renderDevice.get(), presentationChain.get(), 
                        frameRig.GetMainOverlaySystem().get()));

                    // ------- Update ----------------------------------------
                bufferUploads->Update(*context);
                mainScene->Update(frameResult._elapsedTime);
                g_cpuProfiler.EndFrame();
                ++FrameRenderCount;
            }
        }

            //  There are some manual destruction operations we need to perform...
            //  (note that currently some shutdown steps might get skipped if we get 
            //  an unhandled exception)
            //  Before we go too far, though, let's log a list of active assets.
        LogInfo << "Starting shutdown";
        asyncMan->GetAssetSets().LogReport();
        RenderCore::Metal::DeviceContext::PrepareForDestruction(renderDevice.get(), presentationChain.get());

        mainScene.reset();
        g_gpuProfiler.reset();
        RenderCore::Techniques::ResourceBoxes_Shutdown();
        RenderOverlays::CleanupFontSystem();
        asyncMan->GetAssetSets().Clear();
        asyncMan.reset();
        TerminateFileSystemMonitoring();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
    void SetupCompilers(::Assets::CompileAndAsyncManager& asyncMan)
    {
            //  Here, we can attach whatever asset compilers we might need
            //  A common compiler is used for converting Collada data into
            //  our native run-time format.
        auto& compilers = asyncMan.GetIntermediateCompilers();

        typedef RenderCore::Assets::ColladaCompiler ColladaCompiler;
        auto colladaProcessor = std::make_shared<ColladaCompiler>();
        compilers.AddCompiler(ColladaCompiler::Type_Model, colladaProcessor);
        compilers.AddCompiler(ColladaCompiler::Type_AnimationSet, colladaProcessor);
        compilers.AddCompiler(ColladaCompiler::Type_Skeleton, colladaProcessor);
    }

    PlatformRig::FrameRig::RenderResult RenderFrame(
        RenderCore::IThreadContext* context,
        SceneEngine::LightingParserContext& lightingParserContext,
        BasicSceneParser* scene, RenderCore::IDevice* renderDevice,
        RenderCore::IPresentationChain* presentationChain,
        PlatformRig::IOverlaySystem* overlaySys)
    {
            //  some scene might need a "prepare" step to 
            //  build some resources before the main render occurs.
        auto metalContext = RenderCore::Metal::DeviceContext::Get(*context);
        scene->PrepareFrame(metalContext.get());

        using namespace SceneEngine;
        auto presChainDesc = presentationChain->GetDesc();

            //  Execute the lighting parser!
            //      This is where most rendering actually happens.
        LightingParser_Execute(metalContext.get(), lightingParserContext, 
            RenderingQualitySettings(presChainDesc._dimensions, Tweakable("SamplingCount", 1), Tweakable("SamplingQuality", 0)));

            //  If we need to, we can render outside of the lighting parser.
            //  We just need to to use the device context to perform any rendering
            //  operations here.
        RenderPostScene(context);

        if (overlaySys) {
            overlaySys->RenderToScene(context, lightingParserContext);
        }

            //  The lighting parser will tell us if there where any pending resources
            //  during the render. Here, we can render them as a short list...
        bool hasPendingResources = !lightingParserContext._pendingResources.empty();
        auto defaultFont0 = RenderOverlays::GetX2Font("Raleway", 16);
        DrawPendingResources(metalContext.get(), lightingParserContext, defaultFont0.get());

        if (overlaySys) {
            overlaySys->RenderWidgets(context, lightingParserContext.GetProjectionDesc());
        }

        return PlatformRig::FrameRig::RenderResult(hasPendingResources);
    }
}

