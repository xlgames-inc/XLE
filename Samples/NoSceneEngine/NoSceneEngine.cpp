// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _CRT_SECURE_NO_WARNINGS

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
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/Buffer.h"
#include "../../RenderCore/Metal/Resource.h"
#include "../../RenderCore/Metal/ShaderResource.h"
#include "../../RenderCore/Metal/State.h"
#include "../../Tools/ToolsRig/VisualisationGeo.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../Math/Transformations.h"

unsigned FrameRenderCount = 0;

namespace VulkanTest
{
    struct texture_object 
    {
        RenderCore::ResourcePtr _resource;
		RenderCore::Metal::ImageLayout _imageLayout;
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
		RenderCore::Metal::DeviceContext& context,
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
				BindFlag::ShaderResource,
				0, GPUAccess::Read | GPUAccess::Write,
				TextureDesc::Plain2D(width, height, fmt),
				"texture"),
			SingleSubRes(SubResourceInitData{ buffer.get(), bufferSize, bufferSize / height, bufferSize }));

		auto gpuResource = dev.CreateResource(
			CreateDesc(
				BindFlag::ShaderResource,
				0, GPUAccess::Read | GPUAccess::Write,
				TextureDesc::Plain2D(width, height, fmt),
				"texture"));

		texObj._resource = gpuResource;
		
		// is it a good idea to change the layout of the staging resource before we use it?
		Metal::SetImageLayout(context, stagingResource, Metal::ImageLayout::Preinitialized, Metal::ImageLayout::TransferSrcOptimal);
		Metal::SetImageLayout(context, gpuResource, Metal::ImageLayout::Undefined, Metal::ImageLayout::TransferDstOptimal);
		Metal::Copy(
			context, gpuResource, stagingResource,
			Metal::ImageLayout::TransferDstOptimal, Metal::ImageLayout::TransferSrcOptimal);
		Metal::SetImageLayout(context, gpuResource, Metal::ImageLayout::TransferDstOptimal, Metal::ImageLayout::ShaderReadOnlyOptimal);
		texObj._imageLayout = Metal::ImageLayout::ShaderReadOnlyOptimal;
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
            using namespace RenderCore;
            auto& shader = ::Assets::GetAsset<Metal::ShaderProgram>(
                    "game/xleres/deferred/basic.vsh:main:vs_*",
                    "game/xleres/basic.psh:copy_bilinear:ps_*",
                    "GEO_HAS_TEXCOORD=1;RES_HAS_DiffuseTexture=1");

            Metal::BoundInputLayout inputLayout(
                ToolsRig::Vertex3D_InputLayout,
                shader.GetCompiledVertexShader());

            Metal::BoundUniforms boundUniforms(shader);
            boundUniforms.BindConstantBuffers(0, {"GlobalTransform", "LocalTransform"});
            boundUniforms.BindShaderResources(1, {"DiffuseTexture"});

            auto metalContext = Metal::DeviceContext::Get(genericThreadContext);
            metalContext->Bind(inputLayout);
            metalContext->Bind(shader);

            auto& factory = metalContext->GetFactory();

            // ------ uniforms -----------
            Metal::SamplerState sampler;
            auto projDesc = Techniques::ProjectionDesc();
            auto globalTrans = Techniques::BuildGlobalTransformConstants(projDesc);
            auto localTrans = Techniques::MakeLocalTransform(
                Identity<Float4x4>(), 
                ExtractTranslation(projDesc._cameraToWorld));

            Metal::ConstantBuffer globalTransBuffer(&globalTrans, sizeof(globalTrans));
            Metal::ConstantBuffer localTransBuffer(&localTrans, sizeof(localTrans));
            Metal::ShaderResourceView srv(factory, texObj._resource);

            const Metal::ConstantBuffer* cbs[] = {&globalTransBuffer, &localTransBuffer};
            const Metal::ShaderResourceView* srvs[] = {&srv};
            boundUniforms.Apply(
                *metalContext,
                Metal::UniformsStream(nullptr, cbs, dimof(cbs), srvs, dimof(srvs)),
                Metal::UniformsStream());

			// ------ final states -----------
            auto cubeGeo = ToolsRig::BuildCube();
            auto vertexStride = sizeof(decltype(cubeGeo)::value_type);
            Metal::VertexBuffer vb(AsPointer(cubeGeo.begin()), cubeGeo.size() * vertexStride);
            metalContext->Bind(MakeResourceList(vb), (unsigned)vertexStride);
            metalContext->Bind(Metal::ViewportDesc(0, 0, 512, 512));

			// ------ draw! -----------
			metalContext->BindPipeline();
            metalContext->Draw((unsigned)cubeGeo.size());
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

                //  Finally, we execute the frame loop
            for (;;) {
                if (OverlappedWindow::DoMsgPump() == OverlappedWindow::PumpResult::Terminate) {
                    break;
                }

                renderDevice->BeginFrame(presentationChain.get());

				if (!initTex) {
					using namespace RenderCore;
					auto initContext = renderDevice->CreateDeferredContext();
					auto metalContext = Metal::DeviceContext::Get(*initContext);
					metalContext->BeginCommandList();
					VulkanTest::init_image2(*renderDevice, *metalContext.get(), texObj);
					auto cmdList = metalContext->ResolveCommandList();
					Metal::DeviceContext::Get(*context)->CommitCommandList(*cmdList, false);
					// VulkanTest::init_image2(*Metal::DeviceContext::Get(*context), texObj);
					initTex = true;
				}

                RunShaderTest(*context);

                presentationChain->Present();

                    // ------- Update ----------------------------------------
                RenderCore::Assets::Services::GetBufferUploads().Update(*context, false);
                g_cpuProfiler.EndFrame();
                ++FrameRenderCount;
            }
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

