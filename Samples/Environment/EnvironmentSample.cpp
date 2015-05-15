// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "EnvironmentScene.h"

#include "../Shared/SampleInputHandler.h"
#include "../Shared/SampleGlobals.h"
#include "../Shared/Character.h"
#include "../Shared/PlacementsOverlaySystem.h"
#include "../Shared/TerrainOverlaySystem.h"

#include "../../PlatformRig/OverlappedWindow.h"
#include "../../PlatformRig/MainInputHandler.h"
#include "../../PlatformRig/InputTranslator.h"
#include "../../PlatformRig/DebuggingDisplays/GPUProfileDisplay.h"
#include "../../PlatformRig/DebuggingDisplays/CPUProfileDisplay.h"
#include "../../PlatformRig/FrameRig.h"
#include "../../PlatformRig/PlatformRigUtil.h"
#include "../../PlatformRig/OverlaySystem.h"

#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../SceneEngine/LightingParser.h"
#include "../../SceneEngine/LightingParserStandardPlugin.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/ResourceBox.h"
#include "../../SceneEngine/PlacementsQuadTreeDebugger.h"
#include "../../SceneEngine/IntersectionTest.h"

#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/Assets/ColladaCompilerInterface.h"
#include "../../RenderCore/Assets/MaterialScaffold.h"
#include "../../RenderCore/Metal/GPUProfiler.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderOverlays/Font.h"
#include "../../RenderOverlays/DebugHotKeys.h"
#include "../../RenderOverlays/Overlays/ShadowFrustumDebugger.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../Assets/CompileAndAsyncManager.h"

#include "../../ConsoleRig/Log.h"
#include "../../ConsoleRig/Console.h"
#include "../../ConsoleRig/GlobalServices.h"
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

    class PrimaryManagers
    {
    public:
        std::shared_ptr<RenderCore::IDevice> _rDevice;
        std::shared_ptr<RenderCore::IPresentationChain> _presChain;
        std::unique_ptr<BufferUploads::IManager> _bufferUploads;
        PlatformRig::OverlappedWindow _window;

        std::unique_ptr<Assets::Services> _assetServices;

        std::shared_ptr<PlatformRig::GlobalTechniqueContext> _globalTechContext;

        PrimaryManagers()
        {
            auto clientRect = _window.GetRect();

            auto renderDevice = RenderCore::CreateDevice();
            std::shared_ptr<RenderCore::IPresentationChain> presentationChain = 
                renderDevice->CreatePresentationChain(_window.GetUnderlyingHandle(), 
                    clientRect.second[0] - clientRect.first[0], clientRect.second[1] - clientRect.first[1]);
            BufferUploads::Attach(ConsoleRig::GlobalServices::GetInstance());
            auto bufferUploads = BufferUploads::CreateManager(renderDevice.get());
            auto assetServices = std::make_unique<::Assets::Services>(0);
            RenderCore::Metal::InitCompileAndAsyncManager();

            _window.AddWindowHandler(std::make_shared<PlatformRig::ResizePresentationChain>(presentationChain));
            auto v = renderDevice->GetVersionInformation();
            _window.SetTitle(StringMeld<128>() << "XLE sample [RenderCore: " << v.first << ", " << v.second << "]");

            auto globalTechniqueContext = std::make_shared<PlatformRig::GlobalTechniqueContext>();

                // commit ptrs
            _rDevice = std::move(renderDevice);
            _presChain = std::move(presentationChain);
            _bufferUploads = std::move(bufferUploads);
            _assetServices = std::move(assetServices);
            _globalTechContext = std::move(globalTechniqueContext);
        }
    };

    class UsefulFonts
    {
    public:
        class Desc {};

        intrusive_ptr<RenderOverlays::Font> _defaultFont0;
        intrusive_ptr<RenderOverlays::Font> _defaultFont1;

        UsefulFonts(const Desc&)
        {
            _defaultFont0 = RenderOverlays::GetX2Font("Raleway", 16);
            _defaultFont1 = RenderOverlays::GetX2Font("Vera", 16);
        }
    };

    

    static std::shared_ptr<PlatformRig::MainInputHandler> CreateInputHandler(
        std::shared_ptr<EnvironmentSceneParser> mainScene, 
        std::shared_ptr<SceneEngine::IntersectionTestContext> intersectionTestContext,
        std::shared_ptr<RenderOverlays::DebuggingDisplay::IInputListener> cameraInputListener,
        std::shared_ptr<RenderOverlays::DebuggingDisplay::IInputListener> overlaySystemInputListener)
    {
        auto mainInputHandler = std::make_shared<PlatformRig::MainInputHandler>();
        mainInputHandler->AddListener(RenderOverlays::MakeHotKeysHandler("game/xleres/hotkey.txt"));
        if (overlaySystemInputListener) {
            mainInputHandler->AddListener(overlaySystemInputListener);
        }

            // tie in input for player character & camera
        mainInputHandler->AddListener(std::move(cameraInputListener));
        mainInputHandler->AddListener(mainScene->GetPlayerCharacter());

            // some special input options for samples
        mainInputHandler->AddListener(
            std::make_shared<SampleInputHandler>(
                mainScene->GetPlayerCharacter(), mainScene->GetTerrainManager(), std::move(intersectionTestContext)));

        return std::move(mainInputHandler);
    }

    static void SetupCompilers(::Assets::CompileAndAsyncManager& asyncMan);
    static PlatformRig::FrameRig::RenderResult RenderFrame(
        RenderCore::IThreadContext& context,
        SceneEngine::LightingParserContext& lightingParserContext, EnvironmentSceneParser* scene,
        RenderCore::IPresentationChain* presentationChain,
        PlatformRig::IOverlaySystem* overlaySys);

    void ExecuteSample()
    {
        using namespace PlatformRig;
        using namespace Sample;

            // We need to startup some basic objects:
            //      * OverlappedWindow (corresponds to a single basic window on Windows)
            //      * RenderDevice & presentation chain
            //      * BufferUploads
            //      * CompileAndAsyncManager
        LogInfo << "Building primary managers";
        PrimaryManagers primMan;

            // Some secondary initalisation:
        SetupCompilers(primMan._assetServices->GetAsyncMan());
        g_gpuProfiler = RenderCore::Metal::GPUProfiler::CreateProfiler();
        RenderOverlays::InitFontSystem(primMan._rDevice.get(), primMan._bufferUploads.get());

            // main scene
        LogInfo << "Creating main scene";
        auto mainScene = std::make_shared<EnvironmentSceneParser>();
        
        {
                //  Create the debugging system, and add any "displays"
                //  These are optional. They are for debugging and development tasks.
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

            frameRig.GetDebugSystem()->Register(
                std::make_shared<::Overlays::ShadowFrustumDebugger>(mainScene), 
                "[Test] Shadow frustum debugger");

            frameRig.GetDebugSystem()->Register(
                std::make_shared<SceneEngine::PlacementsQuadTreeDebugger>(mainScene->GetPlacementManager()),
                "[Placements] Culling");

            auto intersectionContext = std::make_shared<SceneEngine::IntersectionTestContext>(
                primMan._rDevice->GetImmediateContext(), mainScene, 
                primMan._presChain->GetViewportContext(), primMan._globalTechContext);

                //  We also create some "overlay systems" in this sample. 
                //  Again, it's optional. An overlay system will redirect all input
                //  to some alternative task. 
                //
                //  For example, when the "overlay system" for the console is opened 
                //  it will consume all input and prevent other systems from
                //  getting input events. This is convenient, because we don't want
                //  key presses to cause other things to happen while we enter text
                //  into the console.
                //
                //  But the console is just a widget. So we can also add it as a 
                //  debugging display. In this case, it won't consume all input, just
                //  the specific input directed at it.
            auto overlaySwitch = std::make_shared<PlatformRig::OverlaySystemSwitch>();
            {
                using RenderOverlays::DebuggingDisplay::KeyId_Make;
                overlaySwitch->AddSystem(KeyId_Make("~"), PlatformRig::CreateConsoleOverlaySystem());
                if (mainScene->GetPlacementManager()) {
                    overlaySwitch->AddSystem(
                        KeyId_Make("1"),
                        Sample::CreatePlacementsEditorOverlaySystem(
                            mainScene->GetPlacementManager(), mainScene->GetTerrainManager(), 
                            intersectionContext));
                }
                if (mainScene->GetTerrainManager()) {
                    overlaySwitch->AddSystem(
                        KeyId_Make("2"),
                        Sample::CreateTerrainEditorOverlaySystem(
                            mainScene->GetTerrainManager(), intersectionContext));
                }
            }

            frameRig.GetMainOverlaySystem()->AddSystem(overlaySwitch);

                //  We need to create input handlers, and then direct input from the 
                //  OS to that input handler.
            auto cameraInputHandler = std::make_shared<PlatformRig::Camera::CameraInputHandler>(
                mainScene->GetCameraPtr(), mainScene->GetPlayerCharacter(), CharactersScale);
            auto mainInputHandler = CreateInputHandler(
                mainScene, intersectionContext, cameraInputHandler, frameRig.GetMainOverlaySystem()->GetInputListener());

            primMan._window.GetInputTranslator().AddListener(mainInputHandler);
            auto stdPlugin = std::make_shared<SceneEngine::LightingParserStandardPlugin>();
            primMan._assetServices->GetAssetSets().LogReport();

                //  One last object required for rendering:
                //      *   the DeviceContext provides the methods for directly 
                //          interacting with the GPU
            LogInfo << "Setup frame rig and rendering context";
            auto context = primMan._rDevice->GetImmediateContext();

                //  Finally, we execute the frame loop
            for (;;) {
                if (OverlappedWindow::DoMsgPump() == OverlappedWindow::Terminate) {
                    break;
                }

                    // ------- Render ----------------------------------------
                SceneEngine::LightingParserContext lightingParserContext(
                    *primMan._globalTechContext);
                lightingParserContext._plugins.push_back(stdPlugin);

                auto frameResult = frameRig.ExecuteFrame(
                    *context.get(), primMan._presChain.get(), 
                    g_gpuProfiler.get(), &g_cpuProfiler,
                    std::bind(
                        RenderFrame, std::placeholders::_1,
                        std::ref(lightingParserContext), mainScene.get(), 
                        primMan._presChain.get(), 
                        frameRig.GetMainOverlaySystem().get()));

                    // ------- Update ----------------------------------------
                primMan._bufferUploads->Update(*context);
                mainScene->Update(frameResult._elapsedTime);
                cameraInputHandler->Commit(frameResult._elapsedTime);   // we need to be careful to update the camera at the right time (relative to character update)
                g_cpuProfiler.EndFrame();
                ++FrameRenderCount;
            }
        }

        LogInfo << "Starting shutdown";
        primMan._assetServices->GetAssetSets().LogReport();
        RenderCore::Metal::DeviceContext::PrepareForDestruction(primMan._rDevice.get(), primMan._presChain.get());

        mainScene.reset();
        g_gpuProfiler.reset();
        RenderCore::Techniques::ResourceBoxes_Shutdown();
        RenderOverlays::CleanupFontSystem();
        primMan._assetServices->GetAssetSets().Clear();
        primMan._assetServices.reset();
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
        compilers.AddCompiler(
            RenderCore::Assets::MaterialScaffold::CompileProcessType,
            std::make_shared<RenderCore::Assets::MaterialScaffoldCompiler>());
    }

    PlatformRig::FrameRig::RenderResult RenderFrame(
        RenderCore::IThreadContext& context,
        SceneEngine::LightingParserContext& lightingParserContext,
        EnvironmentSceneParser* scene,
        RenderCore::IPresentationChain* presentationChain,
        PlatformRig::IOverlaySystem* overlaySys)
    {
        CPUProfileEvent pEvnt("RenderFrame", g_cpuProfiler);

            //  some scene might need a "prepare" step to 
            //  build some resources before the main render occurs.
        scene->PrepareFrame(context);

        using namespace SceneEngine;
        if (scene) {
            auto presChainDims = presentationChain->GetViewportContext()->_dimensions;
            LightingParser_ExecuteScene(
                context, lightingParserContext, *scene, 
                RenderingQualitySettings(
                    presChainDims, 
                    (Tweakable("LightingModel", 0) == 0) ? RenderingQualitySettings::LightingModel::Deferred : RenderingQualitySettings::LightingModel::Forward,
                    Tweakable("SamplingCount", 1), Tweakable("SamplingQuality", 0)));
        }

        if (overlaySys) {
            overlaySys->RenderToScene(&context, lightingParserContext);
        }

        auto& usefulFonts = RenderCore::Techniques::FindCachedBox<UsefulFonts>(UsefulFonts::Desc());
        auto metalContext = RenderCore::Metal::DeviceContext::Get(context);
        DrawPendingResources(metalContext.get(), lightingParserContext, usefulFonts._defaultFont0.get());

        if (overlaySys) {
            overlaySys->RenderWidgets(&context, lightingParserContext.GetProjectionDesc());
        }

        return PlatformRig::FrameRig::RenderResult(!lightingParserContext._pendingResources.empty());
    }
}

