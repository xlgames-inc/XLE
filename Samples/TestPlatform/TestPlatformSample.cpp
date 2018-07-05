// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TestPlatformScene.h"
#include "../Shared/SampleGlobals.h"

#include "../../PlatformRig/OverlappedWindow.h"
#include "../../PlatformRig/MainInputHandler.h"
#include "../../PlatformRig/InputTranslator.h"
#include "../../PlatformRig/DebuggingDisplays/GPUProfileDisplay.h"
#include "../../PlatformRig/DebuggingDisplays/CPUProfileDisplay.h"
#include "../../PlatformRig/FrameRig.h"
#include "../../PlatformRig/PlatformRigUtil.h"
#include "../../PlatformRig/OverlaySystem.h"

#include "../../SceneEngine/LightingParser.h"
#include "../../SceneEngine/LightingParserStandardPlugin.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../SceneEngine/PreparedScene.h"

#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/IAnnotator.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../RenderOverlays/Font.h"
#include "../../RenderOverlays/DebugHotKeys.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../Tools/ToolsRig/BasicManipulators.h"
#include "../../Assets/AssetServices.h"

#include "../../ConsoleRig/Log.h"
#include "../../ConsoleRig/Console.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../ConsoleRig/ResourceBox.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/Profiling/CPUProfiler.h"

#include "../../SceneEngine/StraightSkeleton.h"

#include <functional>

unsigned FrameRenderCount = 0;

namespace Sample
{
///////////////////////////////////////////////////////////////////////////////////////////////////

    Utility::HierarchicalCPUProfiler g_cpuProfiler;

///////////////////////////////////////////////////////////////////////////////////////////////////

    class PrimaryManagers
    {
    public:
        std::shared_ptr<RenderCore::IDevice> _rDevice;
        std::shared_ptr<RenderCore::IPresentationChain> _presChain;
        PlatformRig::OverlappedWindow _window;

        std::unique_ptr<Assets::Services> _assetServices;
        std::unique_ptr<RenderCore::Assets::Services> _renderAssetServices;

        std::shared_ptr<PlatformRig::GlobalTechniqueContext> _globalTechContext;

        PrimaryManagers()
        {
            auto clientRect = _window.GetRect();

            _rDevice = RenderCore::CreateDevice(RenderCore::Assets::Services::GetTargetAPI());
            _presChain = _rDevice->CreatePresentationChain(_window.GetUnderlyingHandle(), 
                    clientRect.second[0] - clientRect.first[0], clientRect.second[1] - clientRect.first[1]);

            _assetServices = std::make_unique<::Assets::Services>(0);
            _renderAssetServices = std::make_unique<RenderCore::Assets::Services>(_rDevice);

            _window.AddWindowHandler(std::make_shared<PlatformRig::ResizePresentationChain>(_presChain));
            // auto v = _rDevice->GetVersionInformation();
            // _window.SetTitle(StringMeld<128>() << "XLE sample [RenderCore: " << v.first << ", " << v.second << "]");

            _globalTechContext = std::make_shared<PlatformRig::GlobalTechniqueContext>();
        }
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    static std::shared_ptr<PlatformRig::MainInputHandler> CreateInputHandler(
        std::shared_ptr<TestPlatformSceneParser> mainScene, 
        std::shared_ptr<SceneEngine::IntersectionTestContext> intersectionTestContext,
        std::shared_ptr<RenderOverlays::DebuggingDisplay::IInputListener> cameraInputListener,
        std::shared_ptr<RenderOverlays::DebuggingDisplay::IInputListener> overlaySystemInputListener);
    static void InitProfilerDisplays(RenderOverlays::DebuggingDisplay::DebugScreensSystem& debugSys);
    static std::shared_ptr<RenderOverlays::DebuggingDisplay::IInputListener> CreateCameraListener(TestPlatformSceneParser& scene);

    static PlatformRig::FrameRig::RenderResult RenderFrame(
        RenderCore::IThreadContext& context,
        SceneEngine::LightingParserContext& lightingParserContext, 
        TestPlatformSceneParser* scene,
        RenderCore::IPresentationChain* presentationChain,
        PlatformRig::IOverlaySystem* overlaySys);

///////////////////////////////////////////////////////////////////////////////////////////////////

	static void TestStraightSkeleton()
	{
		using namespace SceneEngine::StraightSkeleton;
		#if 0
			Float2 pts[] = {
				Float2(-2,-1),
				Float2( 2,-1),
				Float2( 2, 1),
				Float2(-2, 1)
			};
		#elif 1
			Float2 pts[] = {
				Float2(-2,-1),
				Float2( 3,-1),
				Float2( 3, 3),
				Float2( 1, 3),
				Float2( 1, 1),
				Float2(-2, 1)
			};
		#endif
		auto graph = BuildGraphFromVertexLoop(MakeIteratorRange(pts));
		auto skel = graph.GenerateSkeleton(std::numeric_limits<float>::max());
		(void)skel;
	}

    void ExecuteSample()
    {
        using namespace PlatformRig;
        using namespace Sample;

		TestStraightSkeleton();

        LogInfo << "Building primary managers";
        PrimaryManagers primMan;

            // Some secondary initalisation:
        primMan._renderAssetServices->InitModelCompilers();
        RenderOverlays::InitFontSystem(
            primMan._rDevice.get(), 
            &RenderCore::Assets::Services::GetBufferUploads());

            // main scene
        LogInfo << "Creating main scene";
        auto mainScene = std::make_shared<TestPlatformSceneParser>();
        
        {
            LogInfo << "Setup tools and debugging";
            FrameRig frameRig;
            
            InitDebugDisplays(*frameRig.GetDebugSystem());
            InitProfilerDisplays(*frameRig.GetDebugSystem());

            auto overlaySwitch = std::make_shared<PlatformRig::OverlaySystemSwitch>();
            overlaySwitch->AddSystem(RenderOverlays::DebuggingDisplay::KeyId_Make("~"), PlatformRig::CreateConsoleOverlaySystem());
            frameRig.GetMainOverlaySystem()->AddSystem(overlaySwitch);
            auto mainInputHandler = CreateInputHandler(
                    mainScene, nullptr, CreateCameraListener(*mainScene), 
                    frameRig.GetMainOverlaySystem()->GetInputListener());
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
                if (OverlappedWindow::DoMsgPump() == OverlappedWindow::PumpResult::Terminate)
                    break;

                    // ------- Render ----------------------------------------
                SceneEngine::LightingParserContext lightingParserContext(
                    *primMan._globalTechContext);
                lightingParserContext._plugins.push_back(stdPlugin);

                auto frameResult = frameRig.ExecuteFrame(
                    *context.get(), primMan._presChain.get(), 
                    &g_cpuProfiler,
                    std::bind(
                        RenderFrame, std::placeholders::_1,
                        std::ref(lightingParserContext), mainScene.get(), 
                        primMan._presChain.get(), 
                        frameRig.GetMainOverlaySystem().get()));

                    // ------- Update ----------------------------------------
                RenderCore::Assets::Services::GetBufferUploads().Update(*context, false);
                mainScene->Update(frameResult._elapsedTime);
                g_cpuProfiler.EndFrame();
                ++FrameRenderCount;
            }
        }

        LogInfo << "Starting shutdown";
        primMan._assetServices->GetAssetSets().LogReport();
        RenderCore::Metal::DeviceContext::PrepareForDestruction(primMan._rDevice.get(), primMan._presChain.get());

        mainScene.reset();

        primMan._assetServices->GetAssetSets().Clear();
        ConsoleRig::ResourceBoxes_Shutdown();
        RenderOverlays::CleanupFontSystem();
        
        primMan._renderAssetServices.reset();
        primMan._assetServices.reset();
        TerminateFileSystemMonitoring();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class UsefulFonts
    {
    public:
        class Desc {};

		std::shared_ptr<RenderOverlays::Font> _defaultFont0;
		std::shared_ptr<RenderOverlays::Font> _defaultFont1;

        UsefulFonts(const Desc&)
        {
            _defaultFont0 = RenderOverlays::GetX2Font("Raleway", 16);
            _defaultFont1 = RenderOverlays::GetX2Font("Vera", 16);
        }
    };

    PlatformRig::FrameRig::RenderResult RenderFrame(
        RenderCore::IThreadContext& context,
        SceneEngine::LightingParserContext& lightingParserContext,
        TestPlatformSceneParser* scene,
        RenderCore::IPresentationChain* presentationChain,
        PlatformRig::IOverlaySystem* overlaySys)
    {
        CPUProfileEvent pEvnt("RenderFrame", g_cpuProfiler);

            //  some scene might need a "prepare" step to 
            //  build some resources before the main render occurs.
        scene->PrepareFrame(context);

        using namespace SceneEngine;
        if (scene) {
            UInt2 presChainDims(presentationChain->GetDesc()->_width, presentationChain->GetDesc()->_height);
            LightingParser_ExecuteScene(
                context, lightingParserContext, *scene, scene->GetCameraDesc(),
                RenderingQualitySettings(
                    presChainDims, 
                    (Tweakable("LightingModel", 0) == 0) ? RenderingQualitySettings::LightingModel::Deferred : RenderingQualitySettings::LightingModel::Forward,
                    Tweakable("SamplingCount", 1), Tweakable("SamplingQuality", 0)));
        }

        if (overlaySys) {
            overlaySys->RenderToScene(context, lightingParserContext);
        }

        auto& usefulFonts = ConsoleRig::FindCachedBox2<UsefulFonts>();
        DrawPendingResources(context, lightingParserContext, usefulFonts._defaultFont0.get());

        if (overlaySys) {
            overlaySys->RenderWidgets(context, lightingParserContext);
        }

        return PlatformRig::FrameRig::RenderResult(lightingParserContext.HasPendingAssets());
    }


    static std::shared_ptr<PlatformRig::MainInputHandler> CreateInputHandler(
        std::shared_ptr<TestPlatformSceneParser> mainScene, 
        std::shared_ptr<SceneEngine::IntersectionTestContext> intersectionTestContext,
        std::shared_ptr<RenderOverlays::DebuggingDisplay::IInputListener> cameraInputListener,
        std::shared_ptr<RenderOverlays::DebuggingDisplay::IInputListener> overlaySystemInputListener)
    {
        auto mainInputHandler = std::make_shared<PlatformRig::MainInputHandler>();
        mainInputHandler->AddListener(RenderOverlays::MakeHotKeysHandler("xleres/hotkey.txt"));
        if (overlaySystemInputListener) {
            mainInputHandler->AddListener(overlaySystemInputListener);
        }

            // tie in input for player character & camera
        mainInputHandler->AddListener(std::move(cameraInputListener));

        return std::move(mainInputHandler);
    }

    static void InitProfilerDisplays(RenderOverlays::DebuggingDisplay::DebugScreensSystem& debugSys)
    {
        // if (g_gpuProfiler) {
        //     auto gpuProfilerDisplay = std::make_shared<PlatformRig::Overlays::GPUProfileDisplay>(*g_gpuProfiler.get());
        //     debugSys.Register(gpuProfilerDisplay, "[Profiler] GPU Profiler");
        // }
        debugSys.Register(
            std::make_shared<PlatformRig::Overlays::HierarchicalProfilerDisplay>(&g_cpuProfiler), 
            "[Profiler] CPU Profiler");
    }

    static std::shared_ptr<RenderOverlays::DebuggingDisplay::IInputListener> CreateCameraListener(TestPlatformSceneParser& scene)
    {
        auto manipulators = std::make_shared<ToolsRig::ManipulatorStack>();
        manipulators->Register(
            ToolsRig::ManipulatorStack::CameraManipulator,
            ToolsRig::CreateCameraManipulator(scene.GetCameraPtr()));
        return std::move(manipulators);
    }
}

