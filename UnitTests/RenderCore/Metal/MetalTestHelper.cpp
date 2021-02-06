// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MetalTestHelper.h"
#include "../../../RenderCore/Metal/FrameBuffer.h"
#include "../../../RenderCore/Metal/DeviceContext.h"
#include "../../../RenderCore/Metal/ObjectFactory.h"
#include "../../../RenderCore/IDevice.h"
#include "../../../RenderCore/IThreadContext.h"
#include "../../../RenderCore/OpenGLES/IDeviceOpenGLES.h"
#include "../../../RenderCore/Vulkan/IDeviceVulkan.h"
#include "../../../RenderCore/MinimalShaderSource.h"
#include "../../../RenderCore/ResourceUtils.h"
#include "../../../Assets/AssetUtils.h"
#include "../../../Assets/DepVal.h"

#if GFXAPI_TARGET == GFXAPI_DX11
	#include "../../../RenderCore/Metal/State.h"
#endif

namespace UnitTests
{
	
	RenderCore::CompiledShaderByteCode MetalTestHelper::MakeShader(StringSection<> shader, StringSection<> shaderModel, StringSection<> defines)
	{
		auto codeBlob = _shaderSource->CompileFromMemory(shader, "main", shaderModel, defines);
		return RenderCore::CompiledShaderByteCode {
			codeBlob._payload,
			::Assets::AsDepVal(MakeIteratorRange(codeBlob._deps)),
			{}
		};
	}

	MetalTestHelper::MetalTestHelper(RenderCore::UnderlyingAPI api)
	{
		_device = RenderCore::CreateDevice(api);

		// For GLES, we must initialize the root context to something. Since we're not going to be
		// rendering to window for unit tests, we will never create a PresentationChain (during which the
		// device implicitly initializes the root context in the normal flow)
		auto* glesDevice = (RenderCore::IDeviceOpenGLES*)_device->QueryInterface(typeid(RenderCore::IDeviceOpenGLES).hash_code());
		if (glesDevice)
			glesDevice->InitializeRootContextHeadless();

		std::shared_ptr<RenderCore::ILowLevelCompiler> shaderCompiler;
		auto* vulkanDevice  = (RenderCore::IDeviceVulkan*)_device->QueryInterface(typeid(RenderCore::IDeviceVulkan*).hash_code());
		if (vulkanDevice) {
			// Vulkan allows for multiple ways for compiling shaders. The tests currently use a HLSL to GLSL to SPIRV 
			// cross compilation approach
		 	shaderCompiler = vulkanDevice->CreateShaderCompiler(RenderCore::VulkanShaderMode::HLSLCrossCompiled);
		} else {
			shaderCompiler = _device->CreateShaderCompiler();
		}

		_shaderService = std::make_unique<RenderCore::ShaderService>();
		_shaderSource = std::make_shared<RenderCore::MinimalShaderSource>(shaderCompiler);
		_shaderService->SetShaderSource(_shaderSource);
	}

	MetalTestHelper::MetalTestHelper(const std::shared_ptr<RenderCore::IDevice>& device)
	{
		_device = device;

		_shaderService = std::make_unique<RenderCore::ShaderService>();
		_shaderSource = std::make_shared<RenderCore::MinimalShaderSource>(_device->CreateShaderCompiler());
		_shaderService->SetShaderSource(_shaderSource);
	}

	MetalTestHelper::~MetalTestHelper()
	{
		_shaderSource.reset();
		_shaderService.reset();
		_device.reset();
	}

	std::unique_ptr<MetalTestHelper> MakeTestHelper()
	{
		#if GFXAPI_TARGET == GFXAPI_APPLEMETAL
			return std::make_unique<MetalTestHelper>(RenderCore::UnderlyingAPI::AppleMetal);
		#elif GFXAPI_TARGET == GFXAPI_OPENGLES
			return std::make_unique<MetalTestHelper>(RenderCore::UnderlyingAPI::OpenGLES);
		#elif GFXAPI_TARGET == GFXAPI_VULKAN
			return std::make_unique<MetalTestHelper>(RenderCore::UnderlyingAPI::Vulkan);
		#elif GFXAPI_TARGET == GFXAPI_DX11
			auto res = std::make_unique<MetalTestHelper>(RenderCore::UnderlyingAPI::DX11);
			// hack -- required for D3D11 currently
			auto metalContext = RenderCore::Metal::DeviceContext::Get(*res->_device->GetImmediateContext());
			metalContext->Bind(RenderCore::Metal::RasterizerState{RenderCore::CullMode::None});
			return res;
		#else
			#error GFX-API not handled in MakeTestHelper()
		#endif
	}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////


	class UnitTestFBHelper::Pimpl : public RenderCore::INamedAttachments
	{
	public:
		std::shared_ptr<RenderCore::IResource> _mainTarget;
		RenderCore::ResourceDesc _originalMainTargetDesc;
		std::shared_ptr<RenderCore::Metal::FrameBuffer> _fb;
		RenderCore::FrameBufferDesc _fbDesc;

		RenderCore::IResourcePtr GetResource(
			RenderCore::AttachmentName resName, 
			const RenderCore::AttachmentDesc& requestDesc) const override
		{
			assert(resName == 0);
			// the "requestDesc" is passed in here so that we can validate it. We're expecting
			// it to match up to the desc that was provided in the FrameBufferDesc
			auto expectedDesc = RenderCore::AsAttachmentDesc(_originalMainTargetDesc);
			assert(requestDesc._format == expectedDesc._format);
			assert(requestDesc._width == expectedDesc._width);
			assert(requestDesc._height == expectedDesc._height);
			assert(requestDesc._arrayLayerCount == expectedDesc._arrayLayerCount);
			assert(requestDesc._dimsMode == expectedDesc._dimsMode);
			assert(requestDesc._flags == expectedDesc._flags);
			assert(requestDesc.CalculateHash() == expectedDesc.CalculateHash());
			return _mainTarget;
		}
	};

	class RenderPassToken : public UnitTestFBHelper::IRenderPassToken
	{
	public:
		std::shared_ptr<RenderCore::Metal::DeviceContext> _devContext;
		std::shared_ptr<RenderCore::Metal::FrameBuffer> _fb;

		RenderPassToken(
			const std::shared_ptr<RenderCore::Metal::DeviceContext>& devContext, 
			const std::shared_ptr<RenderCore::Metal::FrameBuffer>& fb)
		: _devContext(devContext), _fb(fb) {}

		~RenderPassToken()
		{
			RenderCore::Metal::EndRenderPass(*_devContext);
		}
	};

	auto UnitTestFBHelper::BeginRenderPass(RenderCore::IThreadContext& threadContext) -> std::shared_ptr<IRenderPassToken>
	{
		auto devContext = RenderCore::Metal::DeviceContext::Get(threadContext);
		RenderCore::Metal::BeginRenderPass(*devContext, *_pimpl->_fb);
		return std::make_shared<RenderPassToken>(devContext, _pimpl->_fb);
	}

	std::map<unsigned, unsigned> UnitTestFBHelper::GetFullColorBreakdown(RenderCore::IThreadContext& threadContext)
	{
		std::map<unsigned, unsigned> result;

		auto data = _pimpl->_mainTarget->ReadBack(threadContext);

		assert(data.size() == (size_t)RenderCore::ByteCount(_pimpl->_mainTarget->GetDesc()));
		auto pixels = MakeIteratorRange((unsigned*)AsPointer(data.begin()), (unsigned*)AsPointer(data.end()));
		for (auto p:pixels) ++result[p];

		return result;
	}

	const std::shared_ptr<RenderCore::IResource> UnitTestFBHelper::GetMainTarget() const
	{
		return _pimpl->_mainTarget;
	}

	UnitTestFBHelper::UnitTestFBHelper(
		RenderCore::IDevice& device, 
		const RenderCore::ResourceDesc& mainFBDesc)
	{
		using namespace RenderCore;
		_pimpl = std::make_unique<UnitTestFBHelper::Pimpl>();

		// Create a resource that matches the given desc, and then also create
		// a framebuffer with a single subpass rendering into that resource;
		std::vector<uint8_t> initBuffer(ByteCount(mainFBDesc), 0xdd);
		SubResourceInitData initData { MakeIteratorRange(initBuffer), MakeTexturePitches(mainFBDesc._textureDesc) };
		_pimpl->_mainTarget = device.CreateResource(mainFBDesc, initData);
		_pimpl->_originalMainTargetDesc = mainFBDesc;

		FrameBufferDesc::Attachment mainAttachment { 0, AsAttachmentDesc(mainFBDesc) };
		SubpassDesc mainSubpass;
		mainSubpass.AppendOutput(0, LoadStore::Clear, LoadStore::Retain);
		mainSubpass.SetName("unit-test-subpass");
		_pimpl->_fbDesc = FrameBufferDesc { 
			std::vector<FrameBufferDesc::Attachment>{ mainAttachment },
			std::vector<SubpassDesc>{ mainSubpass } };

		_pimpl->_fb = std::make_shared<RenderCore::Metal::FrameBuffer>(
			Metal::GetObjectFactory(device),
			_pimpl->_fbDesc,
			*_pimpl);
	}

	UnitTestFBHelper::~UnitTestFBHelper()
	{
	}

////////////////////////////////////////////////////////////////////////////////////////////////////
            //    U T I L I T Y    F N S

	std::shared_ptr<RenderCore::IResource> CreateVB(RenderCore::IDevice& device, IteratorRange<const void*> data)
    {
        using namespace RenderCore;
        return device.CreateResource(
            CreateDesc(
                BindFlag::VertexBuffer, 0, GPUAccess::Read,
                LinearBufferDesc::Create((unsigned)data.size()),
                "vertexBuffer"),
            SubResourceInitData { data });
    }

    RenderCore::Metal::ShaderProgram MakeShaderProgram(MetalTestHelper& testHelper, StringSection<> vs, StringSection<> ps)
    {
        return RenderCore::Metal::ShaderProgram(RenderCore::Metal::GetObjectFactory(), testHelper.MakeShader(vs, "vs_*"), testHelper.MakeShader(ps, "ps_*"));
    }

}

