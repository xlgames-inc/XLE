// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _CRT_SECURE_NO_WARNINGS
// #define SELECT_VULKAN

#include "../../PlatformRig/OverlappedWindow.h"
#include "../../PlatformRig/MainInputHandler.h"
#include "../../PlatformRig/InputTranslator.h"
#include "../../PlatformRig/DebuggingDisplays/GPUProfileDisplay.h"
#include "../../PlatformRig/DebuggingDisplays/CPUProfileDisplay.h"
#include "../../PlatformRig/FrameRig.h"
#include "../../PlatformRig/PlatformRigUtil.h"
#include "../../PlatformRig/OverlaySystem.h"

#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/IAnnotator.h"
#include "../../RenderCore/Format.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../RenderOverlays/Font.h"
#include "../../RenderOverlays/DebugHotKeys.h"
#include "../../BufferUploads/IBufferUploads.h"

#include "../../Assets/AssetServices.h"
#include "../../Assets/AssetSetManager.h"
#include "../../Assets/Assets.h"
#include "../../Assets/IFileSystem.h"
#include "../../Assets/MountingTree.h"
#include "../../Assets/OSFileSystem.h"

#include "../../ConsoleRig/Console.h"
#include "../../ConsoleRig/Log.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../ConsoleRig/ResourceBox.h"
#include "../../ConsoleRig/AttachableInternal.h"
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
#include "../../RenderCore/Vulkan/Metal/FrameBuffer.h"
#include "../../RenderCore/Vulkan/Metal/ObjectFactory.h"

#include "../../RenderCore/Assets/DeferredShaderResource.h"
#include "../../RenderCore/Assets/MaterialScaffold.h"
#include "../../Tools/ToolsRig/VisualisationGeo.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../RenderCore/Techniques/RenderPass.h"
#include "../../Math/Transformations.h"

#include "../../RenderCore/Assets/ModelRunTime.h"
#include "../../RenderCore/Assets/SharedStateSet.h"
#include "../../RenderCore/Assets/RawMaterial.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../SceneEngine/ShaderLightDesc.h"      // todo -- this should really be in RenderCore::Techniques

#pragma warning(disable:4505)

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
		static RenderCore::SubResourceInitData SingleSubResHelper(const RenderCore::SubResourceInitData& data, RenderCore::SubResourceId sr)
		{
			return (sr._mip == 0 && sr._arrayLayer == 0) ? data : RenderCore::SubResourceInitData{};
		}
	}

	static RenderCore::IDevice::ResourceInitializer SingleSubRes(
		const RenderCore::SubResourceInitData& initData)
	{
		return std::bind(&Internal::SingleSubResHelper, std::ref(initData), std::placeholders::_1);
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
		auto buffer = std::vector<uint8>(bufferSize);

		if (!read_ppm(filename.c_str(), width, height, bufferSize / height, buffer.data())) {
			std::cout << "Could not read texture file lunarg.ppm\n";
			exit(-1);
		}

		auto stagingResource = dev.CreateResource(
			CreateDesc(
				BindFlag::TransferSrc,
				0, 0,
				TextureDesc::Plain2D(width, height, fmt),
				"texture"),
                SingleSubRes(SubResourceInitData{ MakeIteratorRange(buffer), TexturePitches { bufferSize / height, bufferSize }}));

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
				*dxContext, Metal_DX11::AsID3DResource(*gpuResource), Metal_DX11::AsID3DResource(*stagingResource),
				Metal_DX11::ImageLayout::TransferDstOptimal, Metal_DX11::ImageLayout::TransferSrcOptimal);
		} else {
#if 0
			auto vkContext = RenderCore::Metal_Vulkan::DeviceContext::Get(threadContext);
			// is it a good idea to change the layout of the staging resource before we use it?
			Metal_Vulkan::SetImageLayouts(*vkContext, {
                {stagingResource, Metal_Vulkan::ImageLayout::Preinitialized, Metal_Vulkan::ImageLayout::TransferSrcOptimal},
                {gpuResource, Metal_Vulkan::ImageLayout::Undefined, Metal_Vulkan::ImageLayout::TransferDstOptimal}});
			Metal_Vulkan::Copy(
				*vkContext, gpuResource, stagingResource,
				Metal_Vulkan::ImageLayout::TransferDstOptimal, Metal_Vulkan::ImageLayout::TransferSrcOptimal);
            Metal_Vulkan::SetImageLayouts(*vkContext, {{gpuResource, Metal_Vulkan::ImageLayout::TransferDstOptimal, Metal_Vulkan::ImageLayout::ShaderReadOnlyOptimal}});
			texObj._imageLayout = Metal_Vulkan::ImageLayout::ShaderReadOnlyOptimal;
#endif
		}
	}
}

namespace Sample
{
///////////////////////////////////////////////////////////////////////////////////////////////////

        // "GPU profiler" doesn't have a place to live yet. We just manage it here, at 
        //  the top level
    std::unique_ptr<RenderCore::IAnnotator> g_gpuProfiler;
    Utility::HierarchicalCPUProfiler g_cpuProfiler;

///////////////////////////////////////////////////////////////////////////////////////////////////

    static void InitProfilerDisplays(RenderOverlays::DebuggingDisplay::DebugScreensSystem& debugSys);

	static VulkanTest::texture_object texObj = {};

#if 0
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

				dxContext->Bind(Topology::TriangleList);
				dxContext->Bind(Metal_DX11::DepthStencilState(false, false));
				dxContext->Bind(Metal_DX11::RasterizerState(CullMode::None));
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

				vkContext->Bind(Topology::TriangleList);
				vkContext->Bind(Metal_Vulkan::DepthStencilState(false, false));
				vkContext->Bind(Metal_Vulkan::RasterizerState(CullMode::None));
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
				vkContext->Draw((unsigned)cubeGeo.size());
			}
        }
        CATCH(const ::Assets::Exceptions::RetrievalError&) {}
        CATCH_END
    }
#endif

    static RenderCore::Techniques::CameraDesc GetDefaultCamera(float time)
    { 
        RenderCore::Techniques::CameraDesc result;
        static const auto camDist = 50.f;
        const auto camHeight = 7.5f;
        const auto secondsPerRotation = 40.f;
        const auto rotationSpeed = -gPI * 2.f / secondsPerRotation;
        Float3 cameraForward(XlCos(time * rotationSpeed), XlSin(time * rotationSpeed), 0.f);
        Float3 cameraPosition = -camDist * cameraForward + Float3(0.f, 0.f, camHeight);
        result._cameraToWorld = MakeCameraToWorld(cameraForward, Float3(0.f, 0.f, 1.f), cameraPosition);
        result._farClip = 1000.f;
        return result;
    }

    static RenderCore::Techniques::ProjectionDesc BuildProjectionDesc(
        const RenderCore::Techniques::CameraDesc& sceneCamera,
        UInt2 viewportDims)
    {
        const float aspectRatio = viewportDims[0] / float(viewportDims[1]);
        auto cameraToProjection = RenderCore::Techniques::Projection(sceneCamera, aspectRatio);

        RenderCore::Techniques::ProjectionDesc projDesc;
        projDesc._verticalFov = sceneCamera._verticalFieldOfView;
        projDesc._aspectRatio = aspectRatio;
        projDesc._nearClip = sceneCamera._nearClip;
        projDesc._farClip = sceneCamera._farClip;
        projDesc._worldToProjection = Combine(InvertOrthonormalTransform(sceneCamera._cameraToWorld), cameraToProjection);
        projDesc._cameraToProjection = cameraToProjection;
        projDesc._cameraToWorld = sceneCamera._cameraToWorld;
        return projDesc;
    }

    SceneEngine::ShaderLightDesc::BasicEnvironment MakeBasicLightingEnvironment()
    {
        SceneEngine::ShaderLightDesc::BasicEnvironment env;
        env._ambient =  { Float3(1.f, 1.f, 1.f), 1.f, 0.f, {0, 0, 0} };
        env._rangeFog = { Float3(0.f, 0.f, 0.f), 1.f };
        env._volumeFog = { 1.f, 0.f, 100.f, 0, Float3(0.f, 0.f, 0.f), 0, Float3(0.f, 0.f, 0.f), 0 };
        env._dominant[0] = {
            Normalize(Float3(-5.f, 1.f, 1.f)), 1000.f, 
            Float3(5.f, 5.f, 5.f),  1000.f,
            Float3(5.f, 5.f, 5.f),  1000.f,
            Float3(1.f, 0.f, 0.f),  1.f,
            Float3(0.f, 1.f, 0.f),  1.f,
            Float3(0.f, 0.f, 1.f),  0
        };
        return env;
    }

    class ModelTestBox
    {
    public:
        class Desc {};

        std::unique_ptr<RenderCore::Assets::SharedStateSet> _sharedStateSet;
        std::unique_ptr<RenderCore::Assets::ModelRenderer> _modelRenderer;

        ModelTestBox(const Desc&);
        ~ModelTestBox();

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const
            { return _modelRenderer->GetDependencyValidation(); }
    };

    ModelTestBox::ModelTestBox(const Desc&)
    {
        using namespace RenderCore::Assets;
        // const char sampleAsset[] = "game/model/simple/box.dae";
        // const char sampleMaterial[] = "game/model/simple/box.dae";
        const char sampleAsset[] = "game/model/galleon/galleon.dae";
        const char sampleMaterial[] = "game/model/galleon/galleon.material";
        auto& scaffold = ::Assets::GetAssetComp<ModelScaffold>(sampleAsset);
        auto& matScaffold = ::Assets::GetAssetComp<MaterialScaffold>(sampleMaterial, sampleAsset);

        _sharedStateSet = std::make_unique<RenderCore::Assets::SharedStateSet>(
            RenderCore::Assets::Services::GetTechniqueConfigDirs());

        auto searchRules = ::Assets::DefaultDirectorySearchRules(sampleAsset);
        const unsigned levelOfDetail = 0;
        _modelRenderer = std::unique_ptr<ModelRenderer>(
            new ModelRenderer(
                scaffold, matScaffold, ModelRenderer::Supplements(),
                *_sharedStateSet, &searchRules, levelOfDetail));
    }

    ModelTestBox::~ModelTestBox() {}

    static void SetupLightingParser(
        RenderCore::IThreadContext& genericThreadContext,
        RenderCore::Techniques::ParsingContext& parserContext)
    {
        using namespace RenderCore;
        auto metalContext = Metal::DeviceContext::Get(genericThreadContext);
		if (!metalContext) return;

        static float time = 0.f;
        time += 1.0f/60.f;
        auto camera = GetDefaultCamera(time);

        auto projDesc = BuildProjectionDesc(camera, UInt2(512, 512));
        parserContext.GetProjectionDesc() = projDesc;
        auto globalTransform = RenderCore::Techniques::BuildGlobalTransformConstants(projDesc);
        parserContext.SetGlobalCB(
            *metalContext, Techniques::TechniqueContext::CB_GlobalTransform,
            &globalTransform, sizeof(globalTransform));

        struct GlobalCBuffer
        {
            float _time; unsigned _samplingPassIndex; 
            unsigned _samplingPassCount; unsigned _dummy;
        } globalStateCB { time, 0, 1, 0 };
        parserContext.SetGlobalCB(
            *metalContext, Techniques::TechniqueContext::CB_GlobalState,
            &globalStateCB, sizeof(globalStateCB));

        auto env = MakeBasicLightingEnvironment();
        parserContext.SetGlobalCB(
            *metalContext, Techniques::TechniqueContext::CB_BasicLightingEnvironment,
            &env, sizeof(env));
    }

    static void RunModelTest(
        RenderCore::IThreadContext& genericThreadContext,
        RenderCore::Techniques::ParsingContext& parserContext)
    {
        TRY
        {
			genericThreadContext.InvalidateCachedState();
			using namespace RenderCore;

			auto metalContext = Metal::DeviceContext::Get(genericThreadContext);
			if (metalContext) {

                /*auto& horizBlur = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                    "game/xleres/basic2D.vsh:fullscreen:vs_*", 
                    "game/xleres/deferred/resolvelight.psh:main:ps_5_0",
                    "GBUFFER_TYPE=1;MSAA_SAMPLES=0;SHADOW_CASCADE_MODE=1;SHADOW_ENABLE_NEAR_CASCADE=0;SHADOW_RESOLVE_MODEL=1;SHADOW_RT_HYBRID=0;LIGHT_SHAPE=0;DIFFUSE_METHOD=0;HAS_SCREENSPACE_AO=0");*/

                auto& box = ConsoleRig::FindCachedBoxDep2<ModelTestBox>();

                auto captureMarker = box._sharedStateSet->CaptureState(
                    *metalContext, 
                    parserContext.GetStateSetResolver(), parserContext.GetStateSetEnvironment());

                metalContext->Bind(Metal::DepthStencilState(true, true));
				metalContext->Bind(Metal::RasterizerState());
                metalContext->Bind(Metal::BlendState());
				metalContext->BindPS(MakeResourceList(Metal::SamplerState(), Metal::SamplerState()));

                    //  Finally, we can render the object!
                box._modelRenderer->Render(
                    RenderCore::Assets::ModelRendererContext(*metalContext, parserContext, RenderCore::Techniques::TechniqueIndex::Forward),
                    *box._sharedStateSet, Identity<Float4x4>());

            }
        }
        CATCH(const ::Assets::Exceptions::RetrievalError& e) { parserContext.Process(e); }
        CATCH_END
    }

    static void RunRenderPassTest(
        RenderCore::IThreadContext& genericThreadContext,
        RenderCore::Techniques::ParsingContext& parserContext,
        RenderCore::Techniques::AttachmentPool& namedResources,
        const RenderCore::TextureSamples& samples)
    {
        TRY
        {
            using namespace RenderCore;
            auto metalContext = Metal::DeviceContext::Get(genericThreadContext);
			if (!metalContext) return;

			// Hack -- cached state isn't getting cleared out automatically, we must automatically flush it
			metalContext->InvalidateCachedState();

            using namespace RenderCore::Techniques::Attachments;

				// Main depth stencil
			AttachmentDesc d_mainDepthStencil =		// MainDepthStencil, 
                {	RenderCore::Format::R24G8_TYPELESS,
					1.f, 1.f, 0u,
                    TextureViewDesc::Aspect::Depth,
					AttachmentDesc::DimensionsMode::OutputRelative,
                    AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::DepthStencil };

                // gbuffer diffuse
			AttachmentDesc d_diffuse =		// GBufferDiffuse,
				{	RenderCore::Format::R8G8B8A8_UNORM_SRGB,
					1.f, 1.f, 0u,
                    TextureViewDesc::ColorSRGB,
					AttachmentDesc::DimensionsMode::OutputRelative,
                    AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::RenderTarget | AttachmentDesc::Flags::ShaderResource };

                // gbuffer normals
			AttachmentDesc d_normals =		// GBufferNormals
                {	RenderCore::Format::R8G8B8A8_SNORM,
                    1.f, 1.f, 0u,
                    TextureViewDesc::ColorLinear,
					AttachmentDesc::DimensionsMode::OutputRelative,
                    AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::RenderTarget | AttachmentDesc::Flags::ShaderResource };

                // gbuffer params
			AttachmentDesc d_params =		// GBufferParams
                {   RenderCore::Format::R8G8B8A8_UNORM,
                    1.f, 1.f, 0u,
                    TextureViewDesc::ColorLinear,
					AttachmentDesc::DimensionsMode::OutputRelative,
                    AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::RenderTarget | AttachmentDesc::Flags::ShaderResource };

                // lighting resolve
			AttachmentDesc d_lightingResolve	// LightingResolve
                {   RenderCore::Format::R16G16B16A16_FLOAT,
                    1.f, 1.f, 0u,
                    TextureViewDesc::ColorLinear,
					AttachmentDesc::DimensionsMode::OutputRelative,
                    AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::RenderTarget | AttachmentDesc::Flags::ShaderResource };

			namedResources.DefineAttachment(MainDepthStencil, d_mainDepthStencil);
			namedResources.DefineAttachment(GBufferDiffuse, d_diffuse);
			namedResources.DefineAttachment(GBufferNormals, d_normals);
			namedResources.DefineAttachment(GBufferParams, d_params);
			namedResources.DefineAttachment(LightingResolve, d_lightingResolve);

			AttachmentViewDesc v_presTarget = { PresentationTarget, LoadStore::DontCare, LoadStore::Retain };
			AttachmentViewDesc v_mainDepthStencil = { MainDepthStencil, LoadStore::Clear, LoadStore::DontCare, TextureViewDesc::Aspect::DepthStencil };
			AttachmentViewDesc v_diffuse = { GBufferDiffuse, LoadStore::DontCare, LoadStore::DontCare };
			AttachmentViewDesc v_normals = { GBufferNormals, LoadStore::DontCare, LoadStore::DontCare };
			AttachmentViewDesc v_params = { GBufferParams, LoadStore::DontCare, LoadStore::DontCare };
			AttachmentViewDesc v_lightingResolve = { LightingResolve, LoadStore::DontCare, LoadStore::DontCare };

            SubpassDesc subpasses[] = 
            {
				// SubpassDesc{{ v_presTarget }, v_mainDepthStencil },

                // render to fbuffer
				SubpassDesc{{ v_diffuse, v_normals, v_params }, v_mainDepthStencil },
                // resolve lighting & resolve
				SubpassDesc{{ v_presTarget }, SubpassDesc::Unused, { v_diffuse, v_normals, v_params }}
            };

			FrameBufferDesc fbLayout(MakeIteratorRange(subpasses));

			auto clearValues = {MakeClearValue(1.f, 0)};
            {
                Techniques::RenderPassInstance rpi(
                    *metalContext, fbLayout,
                    0u, namedResources, 
					MakeIteratorRange(clearValues));

				// First, render gbuffer subpass
                {
                    auto& box = ConsoleRig::FindCachedBoxDep2<ModelTestBox>();
                    auto captureMarker = box._sharedStateSet->CaptureState(
                        *metalContext, 
                        parserContext.GetStateSetResolver(), parserContext.GetStateSetEnvironment());
                    box._modelRenderer->Render(
                        RenderCore::Assets::ModelRendererContext(*metalContext, parserContext, RenderCore::Techniques::TechniqueIndex::Deferred),
                        *box._sharedStateSet, Identity<Float4x4>());
                }
                rpi.NextSubpass();

                // This is the lighting resolve. 
                {
                    metalContext->BindPS(MakeResourceList(*namedResources.GetSRV(GBufferDiffuse), *namedResources.GetSRV(GBufferNormals)));

                    auto& resolveShdr = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                        "xleres/basic2D.vsh:fullscreen:vs_*", 
                        "xleres/basic.psh:copy:ps_*",
                        "");
                    metalContext->Unbind<Metal::BoundInputLayout>();
                    metalContext->Bind(Topology::TriangleStrip);
                    metalContext->Bind(resolveShdr);
                    metalContext->Draw(4);

					metalContext->UnbindPS<Metal::ShaderResourceView>(0, 2);
                }
            }

        }
        CATCH(const ::Assets::Exceptions::RetrievalError& e) { parserContext.Process(e); }
        CATCH_END
    }

    void ExecuteSample()
    {
        using namespace PlatformRig;
        using namespace Sample;

		std::cout << std::endl;
		Log(Warning) << "TestLog" << std::endl;
		std::cout << "TestCOUT" << std::endl;
		std::cout << "TestCOUT2" << std::endl;
		std::cout.flush();

		::Assets::MainFileSystem::GetMountingTree()->Mount(u("xleres"), ::Assets::CreateFileSystem_OS(u("Game/xleres")));

            // We need to startup some basic objects:
            //      * OverlappedWindow (corresponds to a single basic window on Windows)
            //      * RenderDevice & presentation chain
            //      * BufferUploads
            //      * CompileAndAsyncManager
            //
            // Note that the render device should be created first, so that the window
            // object is destroyed before the device is destroyed.
        LogInfo << "Building primary managers" << std::endl;
        auto renderDevice = RenderCore::CreateDevice(RenderCore::Assets::Services::GetTargetAPI());

        PlatformRig::OverlappedWindow window;
        auto clientRect = window.GetRect();
        std::shared_ptr<RenderCore::IPresentationChain> presentationChain = 
            renderDevice->CreatePresentationChain(
                window.GetUnderlyingHandle(), 
                clientRect.second[0] - clientRect.first[0], clientRect.second[1] - clientRect.first[1]);

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
        g_gpuProfiler = RenderCore::CreateAnnotator(*renderDevice);
        RenderOverlays::InitFontSystem(renderDevice.get(), &renderAssetServices->GetBufferUploads());
        auto globalTechniqueContext = std::make_shared<PlatformRig::GlobalTechniqueContext>();
        
        {
            auto cleanup = MakeAutoCleanup(
                [&assetServices]()
                {
                    texObj._resource.reset();
                    assetServices->GetAssetSets().Clear();
                    RenderOverlays::CleanupFontSystem();
                    TerminateFileSystemMonitoring();
                });

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
            // assetServices->GetAssetSets().LogReport();

                //  We need 2 final objects for rendering:
                //      * the FrameRig schedules continuous rendering. It will take care
                //          of timing and some thread management taskes
                //      * the DeviceContext provides the methods we need for rendering.
            LogInfo << "Setup frame rig and rendering context";
            auto context = renderDevice->GetImmediateContext();

#if 0
			bool initTex = false;
			if (!initTex) {
				using namespace RenderCore;
				auto initContext = renderDevice->CreateDeferredContext();
				auto metalContext = Metal::DeviceContext::Get(*initContext);
				metalContext->BeginCommandList();
				VulkanTest::init_image2(*renderDevice, *initContext.get(), texObj);
				auto cmdList = metalContext->ResolveCommandList();
				Metal::DeviceContext::Get(*context)->CommitCommandList(*cmdList, false);
				// VulkanTest::init_image2(*renderDevice, *context, texObj);
				initTex = true;
			}
#endif

                //  Frame buffer layout
            using AttachmentDesc = RenderCore::AttachmentDesc;
            using SubpassDesc = RenderCore::SubpassDesc;
            const unsigned PresentationTarget = 0;
            const unsigned MainDepthStencil = 1;

				// Presentation chain target
                // {   PresentationTarget, 
                //     Attachment::DimensionsMode::OutputRelative, 1.f, 1.f, 
                //     presentationChain->GetDesc()->_format },

                // Main depth stencil
			AttachmentDesc d_mainDepthStencil
                {   RenderCore::Format::D24_UNORM_S8_UINT, 1.f, 1.f, 0u,
                    RenderCore::TextureViewDesc::DepthStencil,
					AttachmentDesc::DimensionsMode::OutputRelative,
                    AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::DepthStencil };

            RenderCore::AttachmentViewDesc v_mainColor = 
                // Presentation chain target
                {   RenderCore::Techniques::Attachments::PresentationTarget,
                    RenderCore::LoadStore::Clear, RenderCore::LoadStore::Retain };

			RenderCore::AttachmentViewDesc v_mainDepthStencil = 
                // Main depth stencil
                {   RenderCore::Techniques::Attachments::MainDepthStencil,
                    RenderCore::LoadStore::Clear, RenderCore::LoadStore::DontCare };

            SubpassDesc subpasses[] = 
            {
				{{v_mainColor}, v_mainDepthStencil}
            };

            RenderCore::FrameBufferDesc fbLayout(MakeIteratorRange(subpasses));

            RenderCore::Techniques::AttachmentPool namedResources;
            // namedResources.Bind(presentationChain->GetDesc()->_samples);
            namedResources.DefineAttachment(RenderCore::Techniques::Attachments::MainDepthStencil, d_mainDepthStencil);

                //  Finally, we execute the frame loop
            for (;;) {
                if (OverlappedWindow::DoMsgPump() == OverlappedWindow::PumpResult::Terminate) {
                    break;
                }

				auto res = context->BeginFrame(*presentationChain);
				RenderCore::Assets::Services::GetBufferUploads().Update(*context, false);
                auto presDims = context->GetStateDesc()._viewportDimensions;
				auto samples = presentationChain->GetDesc()->_samples;
                namedResources.Bind(RenderCore::FrameBufferProperties{presDims[0], presDims[1], samples});
                namedResources.Bind(0, res);
                // context->BeginRenderPass(
                //     fbLayout, 
                //     namedResources,
                //     {RenderCore::MakeClearValue(.5f, .3f, .1f, 1.f), RenderCore::MakeClearValue(1.f, 0)});

                // RunShaderTest(*context);
                RenderCore::Techniques::ParsingContext parserContext(*globalTechniqueContext, &namedResources);
                SetupLightingParser(*context, parserContext);
				RenderCore::Metal::DeviceContext::Get(*context)->Bind(RenderCore::Metal::ViewportDesc(0.f, 0.f, (float)presDims[0], (float)presDims[1]));
                // RunModelTest(*context, parserContext);
                // context->EndRenderPass();
				RunRenderPassTest(
                    *context, parserContext, 
                    namedResources,
                    RenderCore::TextureSamples::Create()); // 2));
                context->Present(*presentationChain);

				// ------- Update ----------------------------------------
                g_cpuProfiler.EndFrame();
                ++FrameRenderCount;
            }

			texObj._resource.reset();
        }

        g_gpuProfiler.reset();

        renderAssetServices.reset();
		ConsoleRig::GlobalServices::GetCrossModule().Withhold(*assetServices);
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
            std::make_shared<PlatformRig::Overlays::HierarchicalProfilerDisplay>(&g_cpuProfiler), 
            "[Profiler] CPU Profiler");
    }
}

