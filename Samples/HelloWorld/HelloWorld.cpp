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

#include "../../RenderCore/Init.h"
#include "../../RenderCore/IAnnotator.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../RenderCore/Techniques/RenderPass.h"
#include "../../RenderCore/Techniques/RenderPassUtils.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderOverlays/Font.h"
#include "../../RenderOverlays/DebugHotKeys.h"
#include "../../RenderOverlays/Overlays/ShadowFrustumDebugger.h"
#include "../../BufferUploads/IBufferUploads.h"

#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../SceneEngine/LightingParser.h"
#include "../../SceneEngine/LightingParserStandardPlugin.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/PreparedScene.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/AssetSetManager.h"
#include "../../Assets/IFileSystem.h"
#include "../../Assets/OSFileSystem.h"
#include "../../Assets/MountingTree.h"

#include "../../ConsoleRig/Console.h"
#include "../../ConsoleRig/Log.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../ConsoleRig/AttachablePtr.h"
#include "../../ConsoleRig/ResourceBox.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/Profiling/CPUProfiler.h"
#include "../../Utility/Streams/FileSystemMonitor.h"

#include <functional>

unsigned FrameRenderCount = 0;


#include "../../RenderCore/Techniques/TechniqueMaterial.h"
#include "../../RenderCore/Assets/MaterialScaffold.h"
#include "../../RenderCore/Assets/ModelImmutableData.h"
#include "../../Assets/BlockSerializer.h"

namespace Sample
{
///////////////////////////////////////////////////////////////////////////////////////////////////

        // "GPU profiler" doesn't have a place to live yet. We just manage it here, at 
        //  the top level
    Utility::HierarchicalCPUProfiler g_cpuProfiler;

///////////////////////////////////////////////////////////////////////////////////////////////////

    static void InitProfilerDisplays(RenderOverlays::DebuggingDisplay::DebugScreensSystem& debugSys, RenderCore::IAnnotator* annotator);

	class SampleOverlay : public PlatformRig::IOverlaySystem
	{
	public:
		virtual void Render(
            RenderCore::IThreadContext& device,
			const RenderCore::IResourcePtr& renderTarget,
			RenderCore::Techniques::ParsingContext& parserContext) override; 

		std::shared_ptr<SceneEngine::IScene> _scene;
		std::shared_ptr<SampleLightingDelegate> _lightingDelegate;

		SampleOverlay(
			const std::shared_ptr<SceneEngine::IScene>& scene,
			const std::shared_ptr<SampleLightingDelegate>& lightingDelegate)
		: _scene(scene), _lightingDelegate(lightingDelegate) {}
	};

    void ExecuteSample()
    {
        using namespace PlatformRig;
        using namespace Sample;

		::Assets::MainFileSystem::GetMountingTree()->Mount(u("xleres"), ::Assets::CreateFileSystem_OS(u("Game/xleres")));

            // We need to startup some basic objects:
            //      * OverlappedWindow (corresponds to a single basic window on Windows)
            //      * RenderDevice & presentation chain
            //      * BufferUploads
            //      * CompileAndAsyncManager
            //
            // Note that the render device should be created first, so that the window
            // object is destroyed before the device is destroyed.
        Log(Verbose) << "Building primary managers" << std::endl;
        auto renderDevice = RenderCore::CreateDevice(RenderCore::Techniques::GetTargetAPI());

        PlatformRig::OverlappedWindow window;
        auto clientRect = window.GetRect();
        std::shared_ptr<RenderCore::IPresentationChain> presentationChain = 
            renderDevice->CreatePresentationChain(
                window.GetUnderlyingHandle(), 
				RenderCore::PresentationChainDesc{unsigned(clientRect.second[0] - clientRect.first[0]), unsigned(clientRect.second[1] - clientRect.first[1])});

        auto assetServices = ConsoleRig::MakeAttachablePtr<::Assets::Services>(0);
        auto renderAssetServices = ConsoleRig::MakeAttachablePtr<RenderCore::Assets::Services>(renderDevice);

            //  Tie in the window handler so we get presentation chain resizes, and give our
            //  window a title
            //  Here, I show 2 different ways to do dynamic string formatting.
            //      (note that XlDynFormatString will always allocate at least once!)
        window.AddWindowHandler(std::make_shared<PlatformRig::ResizePresentationChain>(presentationChain));
        auto v = renderDevice->GetDesc();
        window.SetTitle(XlDynFormatString("XLE sample [RenderCore: %s : %s]", v._buildVersion, v._buildDate).c_str());
        window.SetTitle(StringMeld<128>() << "XLE sample [RenderCore: " << v._buildVersion << ", " << v._buildDate << "]");

            // Some secondary initalisation:
            //  * attach compilers
            //  * pass buffer uploads pointer to the scene engine
            //  * init the gpu profiler (this init step will probably change someday)
            //  * the font system needs an explicit init (and shutdown)
            //  * the global technique context contains some global rendering settings
        renderAssetServices->InitModelCompilers();
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
        Log(Verbose) << "Creating main scene" << std::endl;
        auto mainScene = std::make_shared<BasicSceneParser>();
		auto lightingDelegate = std::make_shared<SampleLightingDelegate>();
        
        {
                // currently we need to maintain a reference on these two fonts -- 
            auto defaultFont0 = RenderOverlays::GetX2Font("Raleway", 16);
            auto defaultFont1 = RenderOverlays::GetX2Font("Vera", 16);

                //  Create the debugging system, and add any "displays"
                //  If we have any custom displays to add, we can add them here. Often it's 
                //  useful to create a debugging display to go along with any new feature. 
                //  It just provides a convenient architecture for visualizing important information.
            Log(Verbose) << "Setup tools and debugging" << std::endl;
            FrameRig frameRig;
			auto context = renderDevice->GetImmediateContext();

            InitDebugDisplays(*frameRig.GetDebugSystem());
            InitProfilerDisplays(*frameRig.GetDebugSystem(), &context->GetAnnotator());

            auto overlaySwitch = std::make_shared<PlatformRig::OverlaySystemSwitch>();
            overlaySwitch->AddSystem(RenderOverlays::DebuggingDisplay::KeyId_Make("~"), PlatformRig::CreateConsoleOverlaySystem());
            frameRig.GetDebugScreensOverlaySystem()->AddSystem(overlaySwitch);

            frameRig.GetDebugSystem()->Register(
                std::make_shared<::Overlays::ShadowFrustumDebugger>(lightingDelegate), 
                "[Test] Shadow frustum debugger");

                //  Setup input:
                //      * We create a main input handler, and tie that to the window to receive inputs
                //      * We can add secondary input handles to the main input handler as required
                //      * The order in which we add handlers determines their priority in intercepting messages
            Log(Verbose) << "Setup input" << std::endl;
            auto mainInputHandler = std::make_shared<PlatformRig::MainInputHandler>();
            mainInputHandler->AddListener(RenderOverlays::MakeHotKeysHandler("xleres/hotkey.txt"));
            mainInputHandler->AddListener(frameRig.GetDebugScreensOverlaySystem()->GetInputListener());
            window.GetInputTranslator().AddListener(mainInputHandler);

                //  The lighting parser allows plug ins for customizing the lighting process
                //  To make large-scale changes to the lighting pipeline, you should modify
                //  the lighting parser code directly. But sometimes you might want optional
                //  settings (maybe for quality settings or different rendering modes). In
                //  these cases, plugins can provide a way to customize the pipeline at run-time.
            // auto stdPlugin = std::make_shared<SceneEngine::LightingParserStandardPlugin>();

                //  We can log the active assets at any time using this method.
                //  At this point during startup, we should only have a few assets loaded.
            //assetServices->GetAssetSets().LogReport();

                //  We need 2 final objects for rendering:
                //      * the FrameRig schedules continuous rendering. It will take care
                //          of timing and some thread management taskes
                //      * the DeviceContext provides the methods we need for rendering.
            Log(Verbose) << "Setup frame rig and rendering context" << std::endl;
            
			frameRig.GetMainOverlaySystem()->AddSystem(std::make_shared<SampleOverlay>(mainScene, lightingDelegate));

            RenderCore::Techniques::AttachmentPool namedResources;
			RenderCore::Techniques::FrameBufferPool frameBufferPool;

                //  Finally, we execute the frame loop
            for (;;) {
                if (OverlappedWindow::DoMsgPump() == OverlappedWindow::PumpResult::Terminate) {
                    break;
                }

                    // ------- Render ----------------------------------------
                RenderCore::Techniques::ParsingContext parserContext(*globalTechniqueContext, &namedResources, &frameBufferPool);
                //lightingParserContext._plugins.push_back(stdPlugin);

                auto frameResult = frameRig.ExecuteFrame(
                    *context.get(), presentationChain.get(), 
					parserContext, &g_cpuProfiler);

                    // ------- Update ----------------------------------------
                RenderCore::Assets::Services::GetBufferUploads().Update(*context, false);
                lightingDelegate->Update(frameResult._elapsedTime * Tweakable("TimeScale", 1.0f));
                g_cpuProfiler.EndFrame();
                ++FrameRenderCount;
            }
        }

            //  There are some manual destruction operations we need to perform...
            //  (note that currently some shutdown steps might get skipped if we get 
            //  an unhandled exception)
            //  Before we go too far, though, let's log a list of active assets.
        Log(Verbose) << "Starting shutdown" << std::endl;
        // assetServices->GetAssetSets().LogReport();
        RenderCore::Metal::DeviceContext::PrepareForDestruction(renderDevice.get(), presentationChain.get());

        mainScene.reset();
		lightingDelegate.reset();

        assetServices->GetAssetSets().Clear();
		ConsoleRig::ResourceBoxes_Shutdown();

		renderAssetServices.reset();
        assetServices.reset();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
	void RenderPostScene(RenderCore::IThreadContext& context);
	
	void SampleOverlay::Render(
        RenderCore::IThreadContext& threadContext,
		const RenderCore::IResourcePtr& renderTarget,
		RenderCore::Techniques::ParsingContext& parsingContext)
	{
            //  Execute the lighting parser!
            //      This is where most rendering actually happens.
        SceneEngine::LightingParserContext lightingParserContext;
		if (_scene) {
			auto samples = RenderCore::TextureSamples::Create((uint8)Tweakable("SamplingCount", 1), (uint8)Tweakable("SamplingQuality", 0));
            lightingParserContext = LightingParser_ExecuteScene(
                threadContext, renderTarget, parsingContext, *_scene, _lightingDelegate->GetCameraDesc(),
                SceneEngine::RenderSceneSettings {
                    (Tweakable("LightingModel", 0) == 0) ? SceneEngine::RenderSceneSettings::LightingModel::Deferred : SceneEngine::RenderSceneSettings::LightingModel::Forward,
					_lightingDelegate.get(),
					{},
					samples._sampleCount, samples._samplingQuality } );
        }

		{
			auto rpi = RenderCore::Techniques::RenderPassToPresentationTarget(threadContext, renderTarget, parsingContext);
			RenderPostScene(threadContext);
			SceneEngine::LightingParser_Overlays(threadContext, parsingContext, lightingParserContext);
		}
	}

///////////////////////////////////////////////////////////////////////////////////////////////////
    static void InitProfilerDisplays(RenderOverlays::DebuggingDisplay::DebugScreensSystem& debugSys, RenderCore::IAnnotator* annotator)
    {
        if (annotator) {
            auto gpuProfilerDisplay = std::make_shared<PlatformRig::Overlays::GPUProfileDisplay>(*annotator);
            debugSys.Register(gpuProfilerDisplay, "[Profiler] GPU Profiler");
        }
        debugSys.Register(
            std::make_shared<PlatformRig::Overlays::HierarchicalProfilerDisplay>(&g_cpuProfiler),
            "[Profiler] CPU Profiler");
    }
}

