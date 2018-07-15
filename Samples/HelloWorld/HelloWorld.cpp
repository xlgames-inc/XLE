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
#include "../../ConsoleRig/AttachableInternal.h"
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

    static PlatformRig::FrameRig::RenderResult RenderFrame(
        RenderCore::IThreadContext& context,
        const RenderCore::ResourcePtr& resPtr,
        RenderCore::Techniques::ParsingContext& parserContext, BasicSceneParser* scene,
        RenderCore::IPresentationChain* presentationChain,
        PlatformRig::IOverlaySystem* debugSystem);
    static void InitProfilerDisplays(RenderOverlays::DebuggingDisplay::DebugScreensSystem& debugSys, RenderCore::IAnnotator* annotator);
    void RenderPostScene(RenderCore::IThreadContext& context);

	void TestMaterialSerialization()
	{
		using namespace RenderCore;
		using namespace RenderCore::Assets;
		std::unique_ptr<uint8[]> blob;

		struct TestStruct
		{
			SerializableVector<char> _testVector; 
			Techniques::Material _mat;			
			MaterialImmutableData _data;
		};
		
		{
			SerializableVector<std::pair<MaterialGuid, Techniques::Material>> resolved;
			SerializableVector<std::pair<MaterialGuid, SerializableVector<char>>> resolvedNames;

			resolved.push_back({ 0, Techniques::Material{} });
			resolved.push_back({ 1, Techniques::Material{} });
			resolved.push_back({ 2, Techniques::Material{} });

			char name[] = "SomeName";
			resolvedNames.push_back({ 0, SerializableVector<char>{name, &name[dimof(name) - 1]} });

			std::sort(resolved.begin(), resolved.end(), CompareFirst<MaterialGuid, Techniques::Material>());
			std::sort(resolvedNames.begin(), resolvedNames.end(), CompareFirst<MaterialGuid, SerializableVector<char>>());

			Serialization::NascentBlockSerializer blockSerializer;
			::Serialize(blockSerializer, SerializableVector<char>{name, &name[dimof(name) - 1]}); 
			Techniques::Material testMat;
			testMat._bindings.SetParameter(u("binding"), 1);
			testMat._matParams.SetParameter(u("matParams"), 2);
			testMat._constants.SetParameter(u("constants"), 3);
			XlCopyString(testMat._techniqueConfig, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
			::Serialize(blockSerializer, testMat);
			::Serialize(blockSerializer, resolved);
			::Serialize(blockSerializer, resolvedNames);
			blob = blockSerializer.AsMemoryBlock();
		}

		Serialization::Block_Initialize(blob.get());
		const auto& matScaffold = *(const TestStruct*)Serialization::Block_GetFirstObject(blob.get());

		for (const auto& res : matScaffold._data._materials) {
			std::cout << res.first << " = " << res.second._stateSet.GetHash() << std::endl;
		}

	}

    void ExecuteSample()
    {
        using namespace PlatformRig;
        using namespace Sample;

		TestMaterialSerialization();

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
        auto renderDevice = RenderCore::CreateDevice(RenderCore::Assets::Services::GetTargetAPI());

        PlatformRig::OverlappedWindow window;
        auto clientRect = window.GetRect();
        std::shared_ptr<RenderCore::IPresentationChain> presentationChain = 
            renderDevice->CreatePresentationChain(
                window.GetUnderlyingHandle(), 
				RenderCore::PresentationChainDesc{unsigned(clientRect.second[0] - clientRect.first[0]), unsigned(clientRect.second[1] - clientRect.first[1])});

        auto assetServices = std::make_unique<::Assets::Services>(0);
		assetServices->AttachCurrentModule();
		ConsoleRig::GlobalServices::GetCrossModule().Publish(*assetServices);
        auto renderAssetServices = std::make_unique<RenderCore::Assets::Services>(renderDevice);
		renderAssetServices->AttachCurrentModule();

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
        RenderOverlays::InitFontSystem(renderDevice.get(), &renderAssetServices->GetBufferUploads());
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
            frameRig.GetMainOverlaySystem()->AddSystem(overlaySwitch);

            frameRig.GetDebugSystem()->Register(
                std::make_shared<::Overlays::ShadowFrustumDebugger>(mainScene), 
                "[Test] Shadow frustum debugger");

                //  Setup input:
                //      * We create a main input handler, and tie that to the window to receive inputs
                //      * We can add secondary input handles to the main input handler as required
                //      * The order in which we add handlers determines their priority in intercepting messages
            Log(Verbose) << "Setup input" << std::endl;
            auto mainInputHandler = std::make_shared<PlatformRig::MainInputHandler>();
            mainInputHandler->AddListener(RenderOverlays::MakeHotKeysHandler("xleres/hotkey.txt"));
            mainInputHandler->AddListener(frameRig.GetMainOverlaySystem()->GetInputListener());
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
                    &g_cpuProfiler,
                    std::bind(
                        RenderFrame, std::placeholders::_1, std::placeholders::_2,
                        std::ref(parserContext), mainScene.get(), 
                        presentationChain.get(), 
                        frameRig.GetMainOverlaySystem().get()));

                    // ------- Update ----------------------------------------
                RenderCore::Assets::Services::GetBufferUploads().Update(*context, false);
                mainScene->Update(frameResult._elapsedTime);
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

        assetServices->GetAssetSets().Clear();
		ConsoleRig::ResourceBoxes_Shutdown();
        RenderOverlays::CleanupFontSystem();

		renderAssetServices.reset();
		ConsoleRig::GlobalServices::GetCrossModule().Withhold(*assetServices);
        assetServices.reset();
        TerminateFileSystemMonitoring();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
    PlatformRig::FrameRig::RenderResult RenderFrame(
        RenderCore::IThreadContext& context,
        const RenderCore::ResourcePtr& presentationResource,
		RenderCore::Techniques::ParsingContext& parsingContext,
        BasicSceneParser* scene,
        RenderCore::IPresentationChain* presentationChain,
        PlatformRig::IOverlaySystem* overlaySys)
    {
		SceneEngine::LightingParserContext lightingParserContext;
        auto& namedRes = parsingContext.GetNamedResources();
        auto viewContext = presentationChain->GetDesc();
        auto samples = RenderCore::TextureSamples::Create((uint8)Tweakable("SamplingCount", 1), (uint8)Tweakable("SamplingQuality", 0));
        namedRes.Bind(RenderCore::FrameBufferProperties{viewContext->_width, viewContext->_height, samples});
        namedRes.Bind(0u, presentationResource);

            //  Some scene might need a "prepare" step to 
            //  build some resources before the main render occurs.
        scene->PrepareFrame(context);

        using namespace SceneEngine;

            //  Execute the lighting parser!
            //      This is where most rendering actually happens.
        if (scene) {
            LightingParser_ExecuteScene(
                context, parsingContext, lightingParserContext, *scene, scene->GetCameraDesc(),
                RenderingQualitySettings(
                    UInt2(viewContext->_width, viewContext->_height),
                    (Tweakable("LightingModel", 0) == 0) ? RenderingQualitySettings::LightingModel::Deferred : RenderingQualitySettings::LightingModel::Forward,
                    samples._sampleCount, samples._samplingQuality));
        }

        if (overlaySys) {
            overlaySys->RenderToScene(context, parsingContext);
        }

            // Begin a default render pass just rendering to the 
            // presentation buffer (which is always target "0")
        bool hasPendingResources = false;
        {
			auto rpi = SceneEngine::RenderPassToPresentationTarget(context, parsingContext);

                //  If we need to, we can render outside of the lighting parser.
                //  We just need to to use the device context to perform any rendering
                //  operations here.
            RenderPostScene(context);
            LightingParser_Overlays(context, parsingContext, lightingParserContext);

                //  The lighting parser will tell us if there where any pending resources
                //  during the render. Here, we can render them as a short list...
            hasPendingResources = parsingContext.HasPendingAssets();
            auto defaultFont0 = RenderOverlays::GetX2Font("Raleway", 16);
            DrawPendingResources(context, parsingContext, defaultFont0);

            if (overlaySys) {
                overlaySys->RenderWidgets(context, parsingContext);
            }
        }

        namedRes.Unbind(0u);
        return PlatformRig::FrameRig::RenderResult(hasPendingResources);
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

