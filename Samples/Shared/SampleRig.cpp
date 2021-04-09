// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SampleRig.h"

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
#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/IThreadContext.h"
#include "../../RenderCore/ResourceDesc.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../RenderCore/Techniques/RenderPass.h"
#include "../../RenderCore/Techniques/RenderPassUtils.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/Services.h"
#include "../../RenderCore/Techniques/ImmediateDrawables.h"

#include "../../RenderOverlays/Font.h"
#include "../../PlatformRig/DebugHotKeys.h"
#include "../../BufferUploads/IBufferUploads.h"

#include "../../Assets/AssetServices.h"
#include "../../Assets/AssetSetManager.h"
#include "../../Assets/IFileSystem.h"
#include "../../Assets/MountingTree.h"
#include "../../Assets/OSFileSystem.h"
#include "../../Assets/IntermediateCompilers.h"
#include "../../Assets/CompileAndAsyncManager.h"

#include "../../ConsoleRig/Console.h"
#include "../../OSServices/Log.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../ConsoleRig/AttachablePtr.h"
#include "../../ConsoleRig/ResourceBox.h"
#include "../../Utility/Profiling/CPUProfiler.h"
#include "../../OSServices/FileSystemMonitor.h"
#include "../../Utility/StringFormat.h"

#include <functional>

#include "../../RenderCore/Metal/DeviceContext.h"		// (for PrepareForDestruction)

#include "../../RenderCore/Assets/PredefinedPipelineLayout.h"
#include "../../RenderCore/Assets/PipelineConfigurationUtils.h"
#include "../../RenderCore/Techniques/CompiledShaderPatchCollection.h"
#include "../../RenderCore/Vulkan/IDeviceVulkan.h"
#include "../../RenderOverlays/FontRendering.h"
#include "../../Assets/Assets.h"

namespace Sample
{
	static void InitProfilerDisplays(
		RenderOverlays::DebuggingDisplay::DebugScreensSystem& debugSys, 
		RenderCore::IAnnotator* annotator,
		Utility::HierarchicalCPUProfiler& cpuProfiler);

    static std::shared_ptr<RenderCore::Techniques::IImmediateDrawables> CreateImmediateDrawables(
        const std::shared_ptr<RenderCore::IDevice>&,
        RenderCore::Assets::PredefinedPipelineLayoutFile& pipelineLayout,
        const std::string& pipelineLayoutName);

    static std::shared_ptr<RenderCore::ILowLevelCompiler> CreateDefaultShaderCompiler(RenderCore::IDevice& device);

	void ExecuteSample(std::shared_ptr<ISampleOverlay>&& sampleOverlay)
    {
		SampleGlobals sampleGlobals;

            // We need to startup some basic objects:
            //      * OverlappedWindow (corresponds to a single basic window on Windows)
            //      * RenderDevice & presentation chain
            //      * BufferUploads
            //
            // Note that the render device should be created first, so that the window
            // object is destroyed before the device is destroyed.
        Log(Verbose) << "Building primary managers" << std::endl;
        sampleGlobals._renderDevice = RenderCore::CreateDevice(RenderCore::Techniques::GetTargetAPI());

        PlatformRig::OverlappedWindow window;
        auto clientRect = window.GetRect();
        sampleGlobals._presentationChain = 
            sampleGlobals._renderDevice->CreatePresentationChain(
                window.GetUnderlyingHandle(), 
				RenderCore::PresentationChainDesc{unsigned(clientRect.second[0] - clientRect.first[0]), unsigned(clientRect.second[1] - clientRect.first[1])});
		RenderCore::Techniques::SetThreadContext(sampleGlobals._renderDevice->GetImmediateContext());

        auto assetServices = ConsoleRig::MakeAttachablePtr<::Assets::Services>();
        auto renderAssetServices = ConsoleRig::MakeAttachablePtr<RenderCore::Assets::Services>(sampleGlobals._renderDevice);
		auto techniquesServices = ConsoleRig::MakeAttachablePtr<RenderCore::Techniques::Services>(sampleGlobals._renderDevice);

        auto rawosmnt = ::Assets::MainFileSystem::GetMountingTree()->Mount("rawos", ::Assets::CreateFileSystem_OS());

		::ConsoleRig::GlobalServices::GetInstance().LoadDefaultPlugins();

        auto shaderCompiler = CreateDefaultShaderCompiler(*sampleGlobals._renderDevice);
		auto shaderService = std::make_unique<RenderCore::ShaderService>();
		auto shaderSource = std::make_shared<RenderCore::MinimalShaderSource>(shaderCompiler);
		shaderService->SetShaderSource(shaderSource);
        
        auto& compilers = ::Assets::Services::GetAsyncMan().GetIntermediateCompilers();
		auto filteringRegistration = ShaderSourceParser::RegisterShaderSelectorFilteringCompiler(compilers);
		auto shaderCompilerRegistration = RenderCore::RegisterShaderCompiler(shaderSource, compilers);
		auto shaderCompiler2Registration = RenderCore::Techniques::RegisterInstantiateShaderGraphCompiler(shaderSource, compilers);

        // Verbose.SetConfiguration({});

        {
			auto attachmentPool = std::make_shared<RenderCore::Techniques::AttachmentPool>(sampleGlobals._renderDevice);
			auto frameBufferPool = std::make_shared<RenderCore::Techniques::FrameBufferPool>();

				//  Tie in the window handler so we get presentation chain resizes, and give our
				//  window a title
			window.AddWindowHandler(std::make_shared<PlatformRig::ResizePresentationChain>(sampleGlobals._presentationChain, frameBufferPool));
			auto v = sampleGlobals._renderDevice->GetDesc();
			window.SetTitle(StringMeld<128>() << "XLE sample [RenderCore: " << v._buildVersion << ", " << v._buildDate << "]");

				// Some secondary initalisation:
				//  * attach compilers
				//  * the global technique context contains some global rendering settings
			renderAssetServices->InitModelCompilers();
			sampleGlobals._techniqueContext = std::make_shared<PlatformRig::GlobalTechniqueContext>();

                // currently we need to maintain a reference on these two fonts -- 
            // auto defaultFont0 = RenderOverlays::GetX2Font("Raleway", 16);
            // auto defaultFont1 = RenderOverlays::GetX2Font("Vera", 16);
			Utility::HierarchicalCPUProfiler cpuProfiler;

                //  Create the debugging system, and add any "displays"
                //  If we have any custom displays to add, we can add them here. Often it's 
                //  useful to create a debugging display to go along with any new feature. 
                //  It just provides a convenient architecture for visualizing important information.
            Log(Verbose) << "Setup tools and debugging" << std::endl;
            auto pipelineLayout = ::Assets::MakeAsset<RenderCore::Assets::PredefinedPipelineLayoutFile>("xleres/TechniqueLibrary/Config/Immediate.pipeline");
            pipelineLayout->StallWhilePending();
            auto immediateDrawables = CreateImmediateDrawables(sampleGlobals._renderDevice, *pipelineLayout->Actualize(), "ImmediateDrawables");
            auto fontRenderer = std::make_shared<RenderOverlays::FontRenderingManager>(*sampleGlobals._renderDevice);
            PlatformRig::FrameRig frameRig(immediateDrawables, fontRenderer);
			auto threadContext = sampleGlobals._renderDevice->GetImmediateContext();

            // PlatformRig::InitDebugDisplays(*frameRig.GetDebugSystem());
            // InitProfilerDisplays(*frameRig.GetDebugSystem(), &threadContext->GetAnnotator(), cpuProfiler);

            auto overlaySwitch = std::make_shared<PlatformRig::OverlaySystemSwitch>();
            overlaySwitch->AddSystem(PlatformRig::KeyId_Make("~"), PlatformRig::CreateConsoleOverlaySystem(immediateDrawables, fontRenderer));
            frameRig.GetDebugScreensOverlaySystem()->AddSystem(overlaySwitch);

			sampleGlobals._debugScreens = frameRig.GetDebugSystem();

                //  Setup input:
                //      * We create a main input handler, and tie that to the window to receive inputs
                //      * We can add secondary input handles to the main input handler as required
                //      * The order in which we add handlers determines their priority in intercepting messages
            Log(Verbose) << "Setup input" << std::endl;
            sampleGlobals._mainInputHander = std::make_shared<PlatformRig::MainInputHandler>();
            sampleGlobals._mainInputHander->AddListener(PlatformRig::MakeHotKeysHandler("xleres/hotkey.txt"));
            sampleGlobals._mainInputHander->AddListener(frameRig.GetMainOverlaySystem()->GetInputListener());
			sampleGlobals._mainInputHander->AddListener(frameRig.GetDebugScreensOverlaySystem()->GetInputListener());
            window.GetInputTranslator().AddListener(sampleGlobals._mainInputHander);

            Log(Verbose) << "Setup frame rig and rendering context" << std::endl;

            char currentDir[256];
            OSServices::GetCurrentDirectory(dimof(currentDir), currentDir);
            
			frameRig.GetMainOverlaySystem()->AddSystem(sampleOverlay);
			sampleOverlay->OnStartup(sampleGlobals);

                //  Finally, we execute the frame loop
            while (PlatformRig::OverlappedWindow::DoMsgPump() != PlatformRig::OverlappedWindow::PumpResult::Terminate) {
                    // ------- Render ----------------------------------------
                RenderCore::Techniques::ParsingContext parserContext(*sampleGlobals._techniqueContext, attachmentPool.get(), frameBufferPool.get());
                auto frameResult = frameRig.ExecuteFrame(
                    *threadContext.get(), sampleGlobals._presentationChain.get(), 
					parserContext, &cpuProfiler);

                    // ------- Update ----------------------------------------
                RenderCore::Techniques::Services::GetBufferUploads().Update(*threadContext);
				sampleOverlay->OnUpdate(frameResult._elapsedTime * Tweakable("TimeScale", 1.0f));
                cpuProfiler.EndFrame();
            }
        }

		sampleOverlay.reset();		// (ensure this gets destroyed before the engine is shutdown)
		sampleGlobals._mainInputHander.reset();
		sampleGlobals._debugScreens.reset();

            //  There are some manual destruction operations we need to perform...
            //  (note that currently some shutdown steps might get skipped if we get 
            //  an unhandled exception)
            //  Before we go too far, though, let's log a list of active assets.
        Log(Verbose) << "Starting shutdown" << std::endl;
        // assetServices->GetAssetSets().LogReport();
        RenderCore::Metal::DeviceContext::PrepareForDestruction(sampleGlobals._renderDevice.get(), sampleGlobals._presentationChain.get());
        assetServices->GetAssetSets().Clear();

        compilers.DeregisterCompiler(shaderCompiler2Registration._registrationId);
		compilers.DeregisterCompiler(shaderCompilerRegistration._registrationId);
		compilers.DeregisterCompiler(filteringRegistration._registrationId);
        ::Assets::MainFileSystem::GetMountingTree()->Unmount(rawosmnt);

		ConsoleRig::ResourceBoxes_Shutdown();
		::ConsoleRig::GlobalServices::GetInstance().UnloadDefaultPlugins();
		assetServices->GetAssetSets().Clear();
		ConsoleRig::ResourceBoxes_Shutdown();

		techniquesServices = nullptr;
		renderAssetServices = nullptr;
        assetServices = nullptr;
    }

	void InitProfilerDisplays(
		RenderOverlays::DebuggingDisplay::DebugScreensSystem& debugSys, 
		RenderCore::IAnnotator* annotator,
		Utility::HierarchicalCPUProfiler& cpuProfiler)
    {
        if (annotator) {
            auto gpuProfilerDisplay = std::make_shared<PlatformRig::Overlays::GPUProfileDisplay>(*annotator);
            debugSys.Register(gpuProfilerDisplay, "[Profiler] GPU Profiler");
        }
        debugSys.Register(
            std::make_shared<PlatformRig::Overlays::HierarchicalProfilerDisplay>(&cpuProfiler),
            "[Profiler] CPU Profiler");
    }

    std::shared_ptr<RenderCore::Techniques::IImmediateDrawables> CreateImmediateDrawables(
        const std::shared_ptr<RenderCore::IDevice>& device,
        RenderCore::Assets::PredefinedPipelineLayoutFile& pipelineLayoutFile,
        const std::string& pipelineLayoutName)
    {
        auto i = pipelineLayoutFile._pipelineLayouts.find(pipelineLayoutName);
        if (i == pipelineLayoutFile._pipelineLayouts.end())
            Throw(std::runtime_error("Did not find pipeline layout with the name " + pipelineLayoutName + " in the given pipeline layout file"));
        auto pipelineInit = i->second->MakePipelineLayoutInitializer(RenderCore::ShaderLanguage::HLSL);
        auto pipelineLayout = device->CreatePipelineLayout(pipelineInit);
        return RenderCore::Techniques::CreateImmediateDrawables(
            device, 
            pipelineLayout,
            RenderCore::Techniques::FindLayout(pipelineLayoutFile, pipelineLayoutName, "Material"),
            RenderCore::Techniques::FindLayout(pipelineLayoutFile, pipelineLayoutName, "Sequencer"));
    }

    std::shared_ptr<RenderCore::ILowLevelCompiler> CreateDefaultShaderCompiler(RenderCore::IDevice& device)
	{
		auto* vulkanDevice  = (RenderCore::IDeviceVulkan*)device.QueryInterface(typeid(RenderCore::IDeviceVulkan).hash_code());
		if (vulkanDevice) {
			// Vulkan allows for multiple ways for compiling shaders. The tests currently use a HLSL to GLSL to SPIRV 
			// cross compilation approach
			RenderCore::VulkanCompilerConfiguration cfg;
			cfg._shaderMode = RenderCore::VulkanShaderMode::HLSLCrossCompiled;
			cfg._legacyBindings = RenderCore::Assets::CreateDefaultLegacyRegisterBindingDesc();
		 	return vulkanDevice->CreateShaderCompiler(cfg);
		} else {
			return device.CreateShaderCompiler();
		}
	}

	void ISampleOverlay::OnStartup(const SampleGlobals& globals) {}
	void ISampleOverlay::OnUpdate(float deltaTime) {}
}

