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
#include "../../../Utility/Streams/StreamFormatter.h"
#include "../../../Utility/Streams/StreamDOM.h"
#include "../../../Utility/Streams/SerializationUtils.h"

#if GFXAPI_TARGET == GFXAPI_DX11
	#include "../../../RenderCore/Metal/State.h"
#endif

namespace RenderCore
{
	static LegacyRegisterBindingDesc::RegisterQualifier AsQualifier(StringSection<char> str)
	{
		// look for "(image)" or "(buffer)" qualifiers
		if (str.IsEmpty() || str[0] != '(') return LegacyRegisterBindingDesc::RegisterQualifier::None;

		if (XlEqStringI(StringSection<char>(str.begin()+1, str.end()), "buffer)"))
			return LegacyRegisterBindingDesc::RegisterQualifier::Buffer;

		if (XlEqStringI(StringSection<char>(str.begin()+1, str.end()), "texture)"))
			return LegacyRegisterBindingDesc::RegisterQualifier::Texture;

		return LegacyRegisterBindingDesc::RegisterQualifier::None;
	}
	
	struct RegisterRange
	{
		unsigned long _begin = 0, _end = 0;
		LegacyRegisterBindingDesc::RegisterQualifier _qualifier;
	};

	static RegisterRange AsRegisterRange(StringSection<> input)
	{
		if (input.IsEmpty()) return {};

		char* endPt = nullptr;
		auto start = std::strtoul(input.begin(), &endPt, 10);
		auto end = start+1;
		if (endPt && endPt[0] == '.' && endPt[1] == '.')
			end = std::strtoul(endPt+2, &endPt, 10);

		auto qualifier = AsQualifier(StringSection<char>(endPt, input.end()));
		return {start, end, qualifier};
	}

	static LegacyRegisterBindingDesc::RegisterType AsLegacyRegisterType(char type)
	{
		// convert between HLSL style register binding indices to a type enum
		switch (type) {
		case 'b': return LegacyRegisterBindingDesc::RegisterType::ConstantBuffer;
		case 's': return LegacyRegisterBindingDesc::RegisterType::Sampler;
		case 't': return LegacyRegisterBindingDesc::RegisterType::ShaderResource;
		case 'u': return LegacyRegisterBindingDesc::RegisterType::UnorderedAccess;
		default:  return LegacyRegisterBindingDesc::RegisterType::Unknown;
		}
	}

	void DeserializationOperator(
		InputStreamFormatter<>& formatter,
		LegacyRegisterBindingDesc& result)
	{
		StreamDOM<InputStreamFormatter<>> dom(formatter);
		auto element = dom.RootElement();
		for (auto e:element.children()) {
			auto name = e.Name();
			if (name.IsEmpty())
				Throw(std::runtime_error("Legacy register binding with empty name"));

			auto regType = AsLegacyRegisterType(name[0]);
			if (regType == LegacyRegisterBindingDesc::RegisterType::Unknown)
				Throw(::Exceptions::BasicLabel("Could not parse legacy register binding (%s)", name.AsString().c_str()));

			auto legacyRegisters = AsRegisterRange({name.begin()+1, name.end()});
			if (legacyRegisters._end <= legacyRegisters._begin)
				Throw(::Exceptions::BasicLabel("Could not parse legacy register binding (%s)", name.AsString().c_str()));

			auto mappedRegisters = AsRegisterRange(e.Attribute("mapping").Value());
			if (mappedRegisters._begin == mappedRegisters._end)
				Throw(::Exceptions::BasicLabel("Could not parse target register mapping in ReadLegacyRegisterBinding (%s)", e.Attribute("mapping").Value().AsString().c_str()));
			
			if ((mappedRegisters._end - mappedRegisters._begin) != (legacyRegisters._end - legacyRegisters._begin))
				Throw(::Exceptions::BasicLabel("Number of legacy register and number of mapped registers don't match up in ReadLegacyRegisterBinding"));

			result.AppendEntry(
				regType, legacyRegisters._qualifier,
				LegacyRegisterBindingDesc::Entry {
					(unsigned)legacyRegisters._begin, (unsigned)legacyRegisters._end,
					Hash64(e.Attribute("set").Value()),
					e.Attribute("setIndex").As<unsigned>().value(),
					(unsigned)mappedRegisters._begin, (unsigned)mappedRegisters._end });
		}
	}
}

namespace UnitTests
{
	std::shared_ptr<RenderCore::ICompiledPipelineLayout> CreateDefaultPipelineLayout(RenderCore::IDevice& device);
	RenderCore::LegacyRegisterBindingDesc CreateDefaultLegacyRegisterBindingDesc();
	
	RenderCore::CompiledShaderByteCode MetalTestHelper::MakeShader(StringSection<> shader, StringSection<> shaderModel, StringSection<> defines)
	{
		return UnitTests::MakeShader(_shaderSource, shader, shaderModel, defines);
	}

	RenderCore::Metal::ShaderProgram MetalTestHelper::MakeShaderProgram(StringSection<> vs, StringSection<> ps)
	{
		return UnitTests::MakeShaderProgram(_shaderSource, _pipelineLayout, vs, ps);
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
		auto* vulkanDevice  = (RenderCore::IDeviceVulkan*)_device->QueryInterface(typeid(RenderCore::IDeviceVulkan).hash_code());
		if (vulkanDevice) {
			// Vulkan allows for multiple ways for compiling shaders. The tests currently use a HLSL to GLSL to SPIRV 
			// cross compilation approach
			RenderCore::VulkanCompilerConfiguration cfg;
			cfg._shaderMode = RenderCore::VulkanShaderMode::HLSLCrossCompiled;
			cfg._legacyBindings = CreateDefaultLegacyRegisterBindingDesc();
		 	shaderCompiler = vulkanDevice->CreateShaderCompiler(cfg);
		} else {
			shaderCompiler = _device->CreateShaderCompiler();
		}

		_pipelineLayout = CreateDefaultPipelineLayout(*_device);

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
		_pipelineLayout.reset();
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
			assert(requestDesc._flags == expectedDesc._flags);
			assert(requestDesc._bindFlagsForFinalLayout == expectedDesc._bindFlagsForFinalLayout);
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
			RenderCore::Metal::EndSubpass(*_devContext, *_fb);
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
		// std::vector<uint8_t> initBuffer(RenderCore::ByteCount(mainFBDesc), 0xdd);
		// SubResourceInitData initData { MakeIteratorRange(initBuffer), MakeTexturePitches(mainFBDesc._textureDesc) };
		SubResourceInitData initData{};
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

	RenderCore::CompiledShaderByteCode MakeShader(const std::shared_ptr<RenderCore::ShaderService::IShaderSource>& shaderSource, StringSection<> shader, StringSection<> shaderModel, StringSection<> defines)
	{
		auto codeBlob = shaderSource->CompileFromMemory(shader, "main", shaderModel, defines);
		if (!codeBlob._payload || codeBlob._payload->empty()) {
			std::cout << "Shader compile failed with errors: " << ::Assets::AsString(codeBlob._errors) << std::endl;
			assert(0);
		}
		return RenderCore::CompiledShaderByteCode {
			codeBlob._payload,
			::Assets::AsDepVal(MakeIteratorRange(codeBlob._deps)),
			{}
		};
	}

	RenderCore::Metal::ShaderProgram MakeShaderProgram(
        const std::shared_ptr<RenderCore::ShaderService::IShaderSource>& shaderSource,
        const std::shared_ptr<RenderCore::ICompiledPipelineLayout>& pipelineLayout,
        StringSection<> vs, StringSection<> ps)
	{
		return RenderCore::Metal::ShaderProgram(RenderCore::Metal::GetObjectFactory(), pipelineLayout, MakeShader(shaderSource, vs, "vs_*"), MakeShader(shaderSource, ps, "ps_*"));
	}
	
	std::shared_ptr<RenderCore::IResource> MetalTestHelper::CreateVB(IteratorRange<const void*> data)
	{
		using namespace RenderCore;
		return _device->CreateResource(
			CreateDesc(
				BindFlag::VertexBuffer, 0, GPUAccess::Read,
				LinearBufferDesc::Create((unsigned)data.size()),
				"vertexBuffer"),
			SubResourceInitData { data });
	}

	std::shared_ptr<RenderCore::IResource> MetalTestHelper::CreateIB(IteratorRange<const void*> data)
	{
		using namespace RenderCore;
		return _device->CreateResource(
			CreateDesc(
				BindFlag::IndexBuffer, 0, GPUAccess::Read,
				LinearBufferDesc::Create((unsigned)data.size()),
				"indexBuffer"),
			SubResourceInitData { data });
	}

	std::shared_ptr<RenderCore::ICompiledPipelineLayout> CreateDefaultPipelineLayout(RenderCore::IDevice& device)
	{
		using namespace RenderCore;
		RenderCore::DescriptorSetSignature sequencerSet {
			{DescriptorType::ConstantBuffer},
			{DescriptorType::ConstantBuffer},
			{DescriptorType::ConstantBuffer},
			{DescriptorType::ConstantBuffer},
			{DescriptorType::ConstantBuffer},
			{DescriptorType::ConstantBuffer},

			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture}
		};

		RenderCore::DescriptorSetSignature materialSet {
			{DescriptorType::ConstantBuffer},
			{DescriptorType::ConstantBuffer},
			{DescriptorType::ConstantBuffer},

			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},

			{DescriptorType::UnorderedAccessBuffer}
		};

		RenderCore::DescriptorSetSignature drawSet {
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture}
		};

		RenderCore::DescriptorSetSignature numericSet {
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},
			{DescriptorType::Texture},

			{DescriptorType::Sampler},
			{DescriptorType::Sampler},
			{DescriptorType::Sampler},
			{DescriptorType::Sampler},
			{DescriptorType::Sampler},
			{DescriptorType::Sampler},
			{DescriptorType::Sampler},

			{DescriptorType::ConstantBuffer},
			{DescriptorType::ConstantBuffer},
			{DescriptorType::ConstantBuffer},
			{DescriptorType::ConstantBuffer},

			{DescriptorType::Sampler}
		};

		RenderCore::PipelineLayoutDesc desc;
		desc.AppendDescriptorSet("Sequencer", sequencerSet);
		desc.AppendDescriptorSet("Material", materialSet);
		desc.AppendDescriptorSet("Draw", drawSet);
		desc.AppendDescriptorSet("Numeric", numericSet);
		return device.CreatePipelineLayout(desc);
	}

	RenderCore::LegacyRegisterBindingDesc CreateDefaultLegacyRegisterBindingDesc()
	{
		const char* defaultCfg = R"--(
			t0..16=~
				set = Numeric
				setIndex = 3
				mapping = 0..16
			t16..23=~
				set = Sequencer
				setIndex = 0
				mapping = 6..13
			t23..31=~
				set = Material
				setIndex = 1
				mapping = 3..11
			t27(buffer)=~
				set = Material
				setIndex = 1
				mapping = 11

			s0..7=~
				set = Numeric
				setIndex = 3
				mapping = 16..23
			s16=~		~~ (this the DummySampler generated by the HLSLCrossCompiler)
				set = Numeric
				setIndex = 3
				mapping = 27

			b0..4=~
				set = Numeric
				setIndex = 3
				mapping = 23..27
			b4..7=~
				set = Material
				setIndex = 1
				mapping = 0..3
			b7..13=~
				set = Sequencer
				setIndex = 0
				mapping = 0..6
		)--";

		RenderCore::LegacyRegisterBindingDesc result;
		InputStreamFormatter<> formatter(MakeStringSection(defaultCfg));
		formatter >> result;
		return result;
	}

}
