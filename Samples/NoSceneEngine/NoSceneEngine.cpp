// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _CRT_SECURE_NO_WARNINGS
#define SELECT_VULKAN

#include "../../PlatformRig/OverlappedWindow.h"
#include "../../PlatformRig/MainInputHandler.h"
#include "../../PlatformRig/InputTranslator.h"
#include "../../PlatformRig/DebuggingDisplays/GPUProfileDisplay.h"
#include "../../PlatformRig/DebuggingDisplays/CPUProfileDisplay.h"
#include "../../PlatformRig/FrameRig.h"
#include "../../PlatformRig/PlatformRigUtil.h"
#include "../../PlatformRig/OverlaySystem.h"

#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/GPUProfiler.h"
#include "../../RenderCore/Format.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../RenderCore/Techniques/ResourceBox.h"
#include "../../RenderOverlays/Font.h"
#include "../../RenderOverlays/DebugHotKeys.h"
#include "../../BufferUploads/IBufferUploads.h"

#include "../../Assets/AssetServices.h"
#include "../../Assets/AssetSetManager.h"

#include "../../ConsoleRig/Console.h"
#include "../../ConsoleRig/Log.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/Profiling/CPUProfiler.h"
#include "../../Utility/Streams/FileSystemMonitor.h"

#include <functional>

#include "../../RenderCore/ShaderService.h"

#include "../../RenderCore/DX11/Metal/Shader.h"
#include "../../RenderCore/DX11/Metal/InputLayout.h"
#include "../../RenderCore/DX11/Metal/DeviceContext.h"
#include "../../RenderCore/DX11/Metal/Buffer.h"
#include "../../RenderCore/DX11/Metal/Resource.h"
#include "../../RenderCore/DX11/Metal/TextureView.h"
#include "../../RenderCore/DX11/Metal/State.h"

#include "../../RenderCore/Vulkan/Metal/Shader.h"
#include "../../RenderCore/Vulkan/Metal/InputLayout.h"
#include "../../RenderCore/Vulkan/Metal/DeviceContext.h"
#include "../../RenderCore/Vulkan/Metal/DeviceContextImpl.h"
#include "../../RenderCore/Vulkan/Metal/Buffer.h"
#include "../../RenderCore/Vulkan/Metal/Resource.h"
#include "../../RenderCore/Vulkan/Metal/TextureView.h"
#include "../../RenderCore/Vulkan/Metal/State.h"

#include "../../RenderCore/Assets/DeferredShaderResource.h"
#include "../../Tools/ToolsRig/VisualisationGeo.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../Math/Transformations.h"

unsigned FrameRenderCount = 0;

namespace VulkanTest
{
    struct texture_object 
    {
        RenderCore::ResourcePtr _resource;
		RenderCore::Metal_Vulkan::ImageLayout _imageLayout;
    };

    bool read_ppm(char const *const filename, int &width, int &height,
                  uint64_t rowPitch, unsigned char *dataPtr) {
        // PPM format expected from http://netpbm.sourceforge.net/doc/ppm.html
        //  1. magic number
        //  2. whitespace
        //  3. width
        //  4. whitespace
        //  5. height
        //  6. whitespace
        //  7. max color value
        //  8. whitespace
        //  7. data

        // Comments are not supported, but are detected and we kick out
        // Only 8 bits per channel is supported
        // If dataPtr is nullptr, only width and height are returned

        // Read in values from the PPM file as characters to check for comments
        char magicStr[3] = {}, heightStr[6] = {}, widthStr[6] = {},
             formatStr[6] = {};

        FILE *fPtr = fopen(filename, "rb");
        if (!fPtr) {
            printf("Bad filename in read_ppm: %s\n", filename);
            return false;
        }

        // Read the four values from file, accounting with any and all whitepace
        fscanf(fPtr, "%s %s %s %s ", magicStr, widthStr, heightStr, formatStr);

        // Kick out if comments present
        if (magicStr[0] == '#' || widthStr[0] == '#' || heightStr[0] == '#' ||
            formatStr[0] == '#') {
            printf("Unhandled comment in PPM file\n");
            return false;
        }

        // Only one magic value is valid
        if (strncmp(magicStr, "P6", sizeof(magicStr))) {
            printf("Unhandled PPM magic number: %s\n", magicStr);
            return false;
        }

        width = atoi(widthStr);
        height = atoi(heightStr);

        // Ensure we got something sane for width/height
        static const int saneDimension = 32768; //??
        if (width <= 0 || width > saneDimension) {
            printf("Width seems wrong.  Update read_ppm if not: %u\n", width);
            return false;
        }
        if (height <= 0 || height > saneDimension) {
            printf("Height seems wrong.  Update read_ppm if not: %u\n", height);
            return false;
        }

        if (dataPtr == nullptr) {
            // If no destination pointer, caller only wanted dimensions
            return true;
        }

        // Now read the data
        for (int y = 0; y < height; y++) {
            unsigned char *rowPtr = dataPtr;
            for (int x = 0; x < width; x++) {
                fread(rowPtr, 3, 1, fPtr);
                rowPtr[3] = 255; /* Alpha of 1 */
                rowPtr += 4;
            }
            dataPtr += rowPitch;
        }
        fclose(fPtr);

        return true;
    }

	namespace Internal
	{
		static RenderCore::SubResourceInitData SingleSubResHelper(const RenderCore::SubResourceInitData& data, unsigned m, unsigned a)
		{
			return (m == 0 && a == 0) ? data : RenderCore::SubResourceInitData{};
		}
	}

	static RenderCore::IDevice::ResourceInitializer SingleSubRes(
		const RenderCore::SubResourceInitData& initData)
	{
		return std::bind(&Internal::SingleSubResHelper, std::ref(initData), std::placeholders::_1, std::placeholders::_2);
	}

	void init_image2(
		RenderCore::IDevice& dev,
		RenderCore::IThreadContext& threadContext,
		texture_object &texObj)
	{
		using namespace RenderCore;
		std::string filename = "lunarg.ppm";

		int width = 0, height = 0;
		if (!read_ppm(filename.c_str(), width, height, 0, nullptr)) {
			std::cout << "Could not read texture file lunarg.ppm\n";
			exit(-1);
		}

		auto fmt = Format::R8G8B8A8_UNORM;
		auto bufferSize = width * height * BitsPerPixel(fmt) / 8;
		auto buffer = std::make_unique<uint8[]>(bufferSize);

		if (!read_ppm(filename.c_str(), width, height, bufferSize / height, buffer.get())) {
			std::cout << "Could not read texture file lunarg.ppm\n";
			exit(-1);
		}

		auto stagingResource = dev.CreateResource(
			CreateDesc(
				BindFlag::TransferSrc,
				0, 0,
				TextureDesc::Plain2D(width, height, fmt),
				"texture"),
                SingleSubRes(SubResourceInitData{ buffer.get(), bufferSize, TexturePitches { bufferSize / height, bufferSize }}));

		auto gpuResource = dev.CreateResource(
			CreateDesc(
                BindFlag::ShaderResource | BindFlag::TransferDst,
				0, GPUAccess::Read,
				TextureDesc::Plain2D(width, height, fmt),
				"texture"));

		texObj._resource = gpuResource;

		auto dxContext = RenderCore::Metal_DX11::DeviceContext::Get(threadContext);
		if (dxContext) {
			Metal_DX11::Copy(
				*dxContext, gpuResource, stagingResource,
				Metal_DX11::ImageLayout::TransferDstOptimal, Metal_DX11::ImageLayout::TransferSrcOptimal);
		} else {
			auto vkContext = RenderCore::Metal_Vulkan::DeviceContext::Get(threadContext);
			// is it a good idea to change the layout of the staging resource before we use it?
			Metal_Vulkan::SetImageLayout(*vkContext, stagingResource, Metal_Vulkan::ImageLayout::Preinitialized, Metal_Vulkan::ImageLayout::TransferSrcOptimal);
			Metal_Vulkan::SetImageLayout(*vkContext, gpuResource, Metal_Vulkan::ImageLayout::Undefined, Metal_Vulkan::ImageLayout::TransferDstOptimal);
			Metal_Vulkan::Copy(
				*vkContext, gpuResource, stagingResource,
				Metal_Vulkan::ImageLayout::TransferDstOptimal, Metal_Vulkan::ImageLayout::TransferSrcOptimal);
			Metal_Vulkan::SetImageLayout(*vkContext, gpuResource, Metal_Vulkan::ImageLayout::TransferDstOptimal, Metal_Vulkan::ImageLayout::ShaderReadOnlyOptimal);
			texObj._imageLayout = Metal_Vulkan::ImageLayout::ShaderReadOnlyOptimal;
		}
	}
}

namespace Sample
{
///////////////////////////////////////////////////////////////////////////////////////////////////

        // "GPU profiler" doesn't have a place to live yet. We just manage it here, at 
        //  the top level
    RenderCore::GPUProfiler::Ptr g_gpuProfiler;
    Utility::HierarchicalCPUProfiler g_cpuProfiler;

///////////////////////////////////////////////////////////////////////////////////////////////////

    static void InitProfilerDisplays(RenderOverlays::DebuggingDisplay::DebugScreensSystem& debugSys);

    static VulkanTest::texture_object texObj = {};

    static void RunShaderTest(RenderCore::IThreadContext& genericThreadContext)
    {
        TRY
        {
			genericThreadContext.InvalidateCachedState();
			using namespace RenderCore;

			auto dxContext = Metal_DX11::DeviceContext::Get(genericThreadContext);
			if (dxContext) {
				auto& shader = ::Assets::GetAssetDep<Metal_DX11::ShaderProgram>(
					"game/xleres/deferred/basic.vsh:main:vs_*",
					"game/xleres/basic.psh:copy_bilinear:ps_*",
					"GEO_HAS_TEXCOORD=1;RES_HAS_DiffuseTexture=1");

				Metal_DX11::BoundInputLayout inputLayout(
					ToolsRig::Vertex3D_InputLayout,
					shader.GetCompiledVertexShader());

				Metal_DX11::BoundUniforms boundUniforms(shader);
				boundUniforms.BindConstantBuffers(0, {"GlobalTransform", "LocalTransform"});
				boundUniforms.BindShaderResources(0, {"DiffuseTexture"});

            
				dxContext->Bind(inputLayout);
				dxContext->Bind(shader);

				dxContext->Bind(Metal_DX11::Topology::TriangleList);
				dxContext->Bind(Metal_DX11::DepthStencilState(false, false));
				dxContext->Bind(Metal_DX11::RasterizerState(Metal_DX11::CullMode::None));
				dxContext->BindPS(MakeResourceList(Metal_DX11::SamplerState(), Metal_DX11::SamplerState()));

				auto& factory = dxContext->GetFactory();

				// ------ uniforms -----------
				auto projDesc = Techniques::ProjectionDesc();
				auto globalTrans = Techniques::BuildGlobalTransformConstants(projDesc);
				auto localTrans = Techniques::MakeLocalTransform(
					Identity<Float4x4>(), 
					ExtractTranslation(projDesc._cameraToWorld));

				Metal_DX11::ConstantBuffer globalTransBuffer(&globalTrans, sizeof(globalTrans));
				Metal_DX11::ConstantBuffer localTransBuffer(&localTrans, sizeof(localTrans));
				Metal_DX11::ShaderResourceView srv(factory, texObj._resource);

				const Metal_DX11::ConstantBuffer* cbs[] = {&globalTransBuffer, &localTransBuffer};
				const Metal_DX11::ShaderResourceView* srvs[] = {&srv};
				boundUniforms.Apply(
					*dxContext,
					Metal_DX11::UniformsStream(nullptr, cbs, dimof(cbs), srvs, dimof(srvs)),
					Metal_DX11::UniformsStream());

				// ------ final states -----------
				auto cubeGeo = ToolsRig::BuildCube();
				auto vertexStride = sizeof(decltype(cubeGeo)::value_type);
				Metal_DX11::VertexBuffer vb(AsPointer(cubeGeo.begin()), cubeGeo.size() * vertexStride);
				dxContext->Bind(MakeResourceList(vb), (unsigned)vertexStride);
				dxContext->Bind(Metal_DX11::ViewportDesc(0, 0, 512, 512));

				// ------ draw! -----------
				dxContext->BindPipeline();
				dxContext->Draw((unsigned)cubeGeo.size());
			} else {
				auto vkContext = Metal_Vulkan::DeviceContext::Get(genericThreadContext);

				auto& shader = ::Assets::GetAssetDep<Metal_Vulkan::ShaderProgram>(
					"game/xleres/deferred/basic.vsh:main:vs_*",
					"game/xleres/basic.psh:copy_bilinear:ps_*",
					"GEO_HAS_TEXCOORD=1;RES_HAS_DiffuseTexture=1");

				Metal_Vulkan::BoundInputLayout inputLayout(
					ToolsRig::Vertex3D_InputLayout,
					shader.GetCompiledVertexShader());

				Metal_Vulkan::BoundUniforms boundUniforms(shader);
				boundUniforms.BindConstantBuffers(0, { "GlobalTransform", "LocalTransform" });
				boundUniforms.BindShaderResources(0, { "DiffuseTexture" });

				vkContext->Bind(inputLayout);
				vkContext->Bind(shader);

				vkContext->Bind(Metal_Vulkan::Topology::TriangleList);
				vkContext->Bind(Metal_Vulkan::DepthStencilState(false, false));
				vkContext->Bind(Metal_Vulkan::RasterizerState(Metal_Vulkan::CullMode::None));
				vkContext->BindPS(MakeResourceList(Metal_Vulkan::SamplerState(), Metal_Vulkan::SamplerState()));

				// ------ uniforms -----------
				auto projDesc = Techniques::ProjectionDesc();
				auto globalTrans = Techniques::BuildGlobalTransformConstants(projDesc);
				auto localTrans = Techniques::MakeLocalTransform(
					Identity<Float4x4>(),
					ExtractTranslation(projDesc._cameraToWorld));

				Metal_Vulkan::ConstantBuffer globalTransBuffer(&globalTrans, sizeof(globalTrans));
				Metal_Vulkan::ConstantBuffer localTransBuffer(&localTrans, sizeof(localTrans));
				const Metal_Vulkan::ConstantBuffer* cbs[] = { &globalTransBuffer, &localTransBuffer };
                
                #if GFXAPI_ACTIVE == GFXAPI_VULKAN
                    auto& tex = ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("game/xleres/DefaultResources/DiffuseTexture.dds:L");
                    const Metal_Vulkan::ShaderResourceView* srvs[] = { &tex.GetShaderResource() };
                #else
                    auto& factory = vkContext->GetFactory();
                    Metal_Vulkan::ShaderResourceView srv(factory, texObj._resource);
				    const Metal_Vulkan::ShaderResourceView* srvs[] = { &srv };
                #endif

				boundUniforms.Apply(
					*vkContext,
					Metal_Vulkan::UniformsStream(nullptr, cbs, dimof(cbs), srvs, dimof(srvs)),
					Metal_Vulkan::UniformsStream());

				// ------ final states -----------
				auto cubeGeo = ToolsRig::BuildCube();
				auto vertexStride = sizeof(decltype(cubeGeo)::value_type);
				Metal_Vulkan::VertexBuffer vb(AsPointer(cubeGeo.begin()), cubeGeo.size() * vertexStride);
				vkContext->Bind(MakeResourceList(vb), (unsigned)vertexStride);
				vkContext->Bind(Metal_Vulkan::ViewportDesc(0, 0, 512, 512));

				// ------ draw! -----------
				vkContext->BindPipeline();
				vkContext->Draw((unsigned)cubeGeo.size());
			}
        }
        CATCH(const ::Assets::Exceptions::AssetException&) {}
        CATCH_END
    }

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
        auto renderDevice = RenderCore::CreateDevice();

        PlatformRig::OverlappedWindow window;
        auto clientRect = window.GetRect();
        std::shared_ptr<RenderCore::IPresentationChain> presentationChain = 
            renderDevice->CreatePresentationChain(
                window.GetUnderlyingHandle(), 
                clientRect.second[0] - clientRect.first[0], clientRect.second[1] - clientRect.first[1]);

        auto assetServices = std::make_unique<::Assets::Services>(0);
        auto renderAssetServices = std::make_unique<RenderCore::Assets::Services>(renderDevice.get());

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
        renderAssetServices->InitColladaCompilers();
        g_gpuProfiler = RenderCore::GPUProfiler::CreateProfiler();
        RenderOverlays::InitFontSystem(renderDevice.get(), &renderAssetServices->GetBufferUploads());
        auto globalTechniqueContext = std::make_shared<PlatformRig::GlobalTechniqueContext>();
        
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
            // InitDebugDisplays(*frameRig.GetDebugSystem());
            InitProfilerDisplays(*frameRig.GetDebugSystem());

            auto overlaySwitch = std::make_shared<PlatformRig::OverlaySystemSwitch>();
            overlaySwitch->AddSystem(RenderOverlays::DebuggingDisplay::KeyId_Make("~"), PlatformRig::CreateConsoleOverlaySystem());
            frameRig.GetMainOverlaySystem()->AddSystem(overlaySwitch);

                //  Setup input:
                //      * We create a main input handler, and tie that to the window to receive inputs
                //      * We can add secondary input handles to the main input handler as required
                //      * The order in which we add handlers determines their priority in intercepting messages
            LogInfo << "Setup input";
            auto mainInputHandler = std::make_shared<PlatformRig::MainInputHandler>();
            mainInputHandler->AddListener(RenderOverlays::MakeHotKeysHandler("game/xleres/hotkey.txt"));
            mainInputHandler->AddListener(frameRig.GetMainOverlaySystem()->GetInputListener());
            window.GetInputTranslator().AddListener(mainInputHandler);

                //  We can log the active assets at any time using this method.
                //  At this point during startup, we should only have a few assets loaded.
            assetServices->GetAssetSets().LogReport();

                //  We need 2 final objects for rendering:
                //      * the FrameRig schedules continuous rendering. It will take care
                //          of timing and some thread management taskes
                //      * the DeviceContext provides the methods we need for rendering.
            LogInfo << "Setup frame rig and rendering context";
            auto context = renderDevice->GetImmediateContext();

			bool initTex = false;
			if (!initTex) {
				// using namespace RenderCore;
				// auto initContext = renderDevice->CreateDeferredContext();
				// auto metalContext = Metal::DeviceContext::Get(*initContext);
				// metalContext->BeginCommandList();
				// VulkanTest::init_image2(*renderDevice, *initContext.get(), texObj);
				// auto cmdList = metalContext->ResolveCommandList();
				// Metal::DeviceContext::Get(*context)->CommitCommandList(*cmdList, false);
				VulkanTest::init_image2(*renderDevice, *context, texObj);
				initTex = true;
			}

                //  Finally, we execute the frame loop
            for (;;) {
                if (OverlappedWindow::DoMsgPump() == OverlappedWindow::PumpResult::Terminate) {
                    break;
                }

				context->BeginFrame(*presentationChain);
                RunShaderTest(*context);
                context->Present(*presentationChain);

                    // ------- Update ----------------------------------------
                RenderCore::Assets::Services::GetBufferUploads().Update(*context, false);
                g_cpuProfiler.EndFrame();
                ++FrameRenderCount;
            }

			texObj._resource.reset();
        }

            //  There are some manual destruction operations we need to perform...
            //  (note that currently some shutdown steps might get skipped if we get 
            //  an unhandled exception)
            //  Before we go too far, though, let's log a list of active assets.
        LogInfo << "Starting shutdown";
        assetServices->GetAssetSets().LogReport();
        // RenderCore::Metal::DeviceContext::PrepareForDestruction(renderDevice.get(), presentationChain.get());

        g_gpuProfiler.reset();

        assetServices->GetAssetSets().Clear();
        RenderCore::Techniques::ResourceBoxes_Shutdown();
        RenderOverlays::CleanupFontSystem();

        renderAssetServices.reset();
        assetServices.reset();
        TerminateFileSystemMonitoring();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
    static void InitProfilerDisplays(RenderOverlays::DebuggingDisplay::DebugScreensSystem& debugSys)
    {
        // if (g_gpuProfiler) {
        //     auto gpuProfilerDisplay = std::make_shared<PlatformRig::Overlays::GPUProfileDisplay>(g_gpuProfiler.get());
        //     debugSys.Register(gpuProfilerDisplay, "[Profiler] GPU Profiler");
        // }
        debugSys.Register(
            std::make_shared<PlatformRig::Overlays::CPUProfileDisplay>(&g_cpuProfiler), 
            "[Profiler] CPU Profiler");
    }
}

