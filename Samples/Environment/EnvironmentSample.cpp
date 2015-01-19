// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "EnvironmentScene.h"

#include "../Shared/SampleInputHandler.h"
#include "../Shared/SampleGlobals.h"
#include "../Shared/Character.h"

#include "../../PlatformRig/OverlappedWindow.h"
#include "../../PlatformRig/MainInputHandler.h"
#include "../../PlatformRig/InputTranslator.h"
#include "../../PlatformRig/DebuggingDisplays/GPUProfileDisplay.h"
#include "../../PlatformRig/FrameRig.h"
#include "../../PlatformRig/PlatformRigUtil.h"
#include "../../PlatformRig/ManipulatorsUtil.h"
#include "../../PlatformRig/CameraManager.h"

#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/Assets/ColladaCompilerInterface.h"
#include "../../RenderCore/Metal/GPUProfiler.h"
#include "../../RenderCore/Metal/State.h"
#include "../../RenderOverlays/Font.h"
#include "../../RenderOverlays/DebugHotKeys.h"
#include "../../BufferUploads/IBufferUploads.h"

#include "../../SceneEngine/Techniques.h"
#include "../../SceneEngine/SceneEngineUtility.h"
#include "../../SceneEngine/LightingParser.h"
#include "../../SceneEngine/LightingParserStandardPlugin.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/ResourceBox.h"
#include "../../SceneEngine/SceneParser.h"
#include "../../SceneEngine/LightDesc.h"

#include "../../ConsoleRig/Console.h"
#include "../../ConsoleRig/Log.h"
#include "../../ConsoleRig/IncludeLUA.h"
#include "../../Utility/StringFormat.h"

#include <functional>

unsigned FrameRenderCount = 0;

namespace Sample
{
///////////////////////////////////////////////////////////////////////////////////////////////////

        // "GPU profiler" doesn't have a place to live yet. We just manage it here, at 
        //  the top level
    RenderCore::Metal::GPUProfiler::Ptr gpuProfiler;

///////////////////////////////////////////////////////////////////////////////////////////////////

    class PrimaryManagers
    {
    public:
        std::unique_ptr<ConsoleRig::Console> _console;

        PlatformRig::OverlappedWindow _window;
        std::unique_ptr<RenderCore::IDevice> _rDevice;
        std::shared_ptr<RenderCore::IPresentationChain> _presChain;
        std::unique_ptr<BufferUploads::IManager> _bufferUploads;

        std::unique_ptr<Assets::CompileAndAsyncManager> _asyncMan;

        std::shared_ptr<PlatformRig::GlobalTechniqueContext> _globalTechContext;

        PrimaryManagers()
        {
            auto clientRect = _window.GetRect();
            auto console = std::make_unique<ConsoleRig::Console>();

            auto renderDevice = RenderCore::CreateDevice();
            std::shared_ptr<RenderCore::IPresentationChain> presentationChain = 
                renderDevice->CreatePresentationChain(_window.GetUnderlyingHandle(), 
                    clientRect.second[0] - clientRect.first[0], clientRect.second[1] - clientRect.first[1]);
            auto bufferUploads = BufferUploads::CreateManager(renderDevice.get());
            auto asyncMan = RenderCore::Metal::CreateCompileAndAsyncManager();

            _window.AddWindowHandler(std::make_shared<PlatformRig::ResizePresentationChain>(presentationChain));
            auto v = renderDevice->GetVersionInformation();
            _window.SetTitle(StringMeld<128>() << "XLE sample [RenderCore: " << v.first << ", " << v.second << "]");

            auto globalTechniqueContext = std::make_shared<PlatformRig::GlobalTechniqueContext>();

                // commit ptrs
            _console = std::move(console);
            _rDevice = std::move(renderDevice);
            _presChain = std::move(presentationChain);
            _bufferUploads = std::move(bufferUploads);
            _asyncMan = std::move(asyncMan);
            _globalTechContext = std::move(globalTechniqueContext);
            SceneEngine::SetBufferUploads(_bufferUploads.get());
        }
    };

    static void SetupCompilers(::Assets::CompileAndAsyncManager& asyncMan);
    static PlatformRig::FrameRig::RenderResult RenderFrame(
        RenderCore::Metal::DeviceContext* context,
        SceneEngine::LightingParserContext& lightingParserContext, EnvironmentSceneParser* scene,
        RenderCore::IDevice* renderDevice, RenderCore::IPresentationChain* presentationChain,
        RenderOverlays::DebuggingDisplay::DebugScreensSystem* debugSystem);

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
        SetupCompilers(*primMan._asyncMan);
        gpuProfiler = RenderCore::Metal::GPUProfiler::CreateProfiler();
        RenderOverlays::InitFontSystem(primMan._rDevice.get(), primMan._bufferUploads.get());

            // main scene
        LogInfo << "Creating main scene";
        auto mainScene = std::make_shared<EnvironmentSceneParser>();
        
        {
            auto& usefulFonts = SceneEngine::FindCachedBox<UsefulFonts>(UsefulFonts::Desc());
            (void)usefulFonts;

                //  Create the debugging system, and add any "displays"
            LogInfo << "Setup tools and debugging";
            auto debugSystem = std::make_shared<RenderOverlays::DebuggingDisplay::DebugScreensSystem>();
            InitDebugDisplays(*debugSystem);

            if (gpuProfiler) {
                auto gpuProfilerDisplay = std::make_shared<PlatformRig::Overlays::GPUProfileDisplay>(gpuProfiler.get());
                debugSystem->Register(gpuProfilerDisplay, "[Profiler] GPU Profiler");
            }

            Tools::HitTestResolver hitTest(mainScene->GetTerrainManager(), mainScene, primMan._globalTechContext);

                //  Setup input:
            LogInfo << "Setup input";
            auto mainInputHandler = std::make_unique<PlatformRig::MainInputHandler>();
            mainInputHandler->AddListener(RenderOverlays::MakeHotKeysHandler("game/xleres/hotkey.txt"));
            mainInputHandler->AddListener(std::make_shared<PlatformRig::DebugScreensInputHandler>(debugSystem));
            mainInputHandler->AddListener(std::make_shared<SampleInputHandler>(
                mainScene->GetPlayerCharacter(), hitTest));
            primMan._window.GetInputTranslator().AddListener(mainInputHandler.get());

                // tie in input for player character & the camera
            auto cameraInputHandler = std::make_shared<PlatformRig::Camera::CameraInputHandler>(
                mainScene->GetCameraPtr(), mainScene->GetPlayerCharacter(), CharactersScale);
            mainInputHandler->AddListener(cameraInputHandler);
            mainInputHandler->AddListener(mainScene->GetPlayerCharacter());

            auto stdPlugin = std::make_shared<SceneEngine::LightingParserStandardPlugin>();

            primMan._asyncMan->GetAssetSets().LogReport();

                //  We need 2 final objects for rendering:
                //      * the FrameRig schedules continuous rendering. It will take care
                //          of timing and some thread management taskes
                //      * the DeviceContext provides the methods we need for rendering.
            LogInfo << "Setup frame rig and rendering context";
            FrameRig frameRig(debugSystem);
            auto context = RenderCore::Metal::DeviceContext::GetImmediateContext(primMan._rDevice.get());

                //  Finally, we execute the frame loop
            for (;;) {
                if (OverlappedWindow::DoMsgPump() == OverlappedWindow::Terminate) {
                    break;
                }

                    // ------- Render ----------------------------------------
                SceneEngine::LightingParserContext lightingParserContext(mainScene.get(), *primMan._globalTechContext);
                lightingParserContext._plugins.push_back(stdPlugin);

                auto frameResult = frameRig.ExecuteFrame(
                    context.get(), primMan._rDevice.get(), primMan._presChain.get(), gpuProfiler.get(),
                    std::bind(
                        RenderFrame, std::placeholders::_1,
                        std::ref(lightingParserContext), mainScene.get(), 
                        primMan._rDevice.get(), primMan._presChain.get(), debugSystem.get()));

                    // ------- Update ----------------------------------------
                primMan._bufferUploads->Update();
                mainScene->Update(frameResult._elapsedTime);
                cameraInputHandler->Commit(frameResult._elapsedTime);
                ++FrameRenderCount;
            }
        }

        LogInfo << "Starting shutdown";
        primMan._asyncMan->GetAssetSets().LogReport();
        RenderCore::Metal::DeviceContext::PrepareForDestruction(primMan._rDevice.get(), primMan._presChain.get());

        mainScene.reset();
        gpuProfiler.reset();
        SceneEngine::ResourceBoxes_Shutdown();
        RenderOverlays::CleanupFontSystem();
        primMan._asyncMan->GetAssetSets().Clear();
        primMan._asyncMan.reset();
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
        RenderCore::Metal::DeviceContext* context,
        SceneEngine::LightingParserContext& lightingParserContext,
        EnvironmentSceneParser* scene, RenderCore::IDevice* renderDevice,
        RenderCore::IPresentationChain* presentationChain,
        RenderOverlays::DebuggingDisplay::DebugScreensSystem* debugSystem)
    {
            //  some scene might need a "prepare" step to 
            //  build some resources before the main render occurs.
        scene->PrepareFrame(context);

        auto presChainDesc = presentationChain->GetDesc();
        SceneEngine::RenderingQualitySettings qualitySettings;
        qualitySettings._width = presChainDesc._width;
        qualitySettings._height = presChainDesc._height;
        qualitySettings._samplingCount = Tweakable("SamplingCount", 1); 
        qualitySettings._samplingQuality = Tweakable("SamplingQuality", 0);

        SceneEngine::LightingParser_Execute(context, lightingParserContext, qualitySettings);

        auto& usefulFonts = SceneEngine::FindCachedBox<UsefulFonts>(UsefulFonts::Desc());
        SceneEngine::DrawPendingResources(context, lightingParserContext, usefulFonts._defaultFont0.get());
        debugSystem->Render(renderDevice, lightingParserContext.GetProjectionDesc()._worldToProjection);

        return PlatformRig::FrameRig::RenderResult(!lightingParserContext._pendingResources.empty());
    }
}

