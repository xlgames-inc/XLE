// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RayVsModel.h"
#include "SceneEngineUtils.h"
#include "LightingParser.h"
#include "LightingParserContext.h"
#include "MetalStubs.h"
#include "RenderStepUtils.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/ObjectFactory.h"
#include "../RenderCore/Metal/QueryPool.h"
#include "../RenderCore/Format.h"
#include "../RenderCore/Types.h"
#include "../RenderCore/IThreadContext.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/TechniqueUtils.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Techniques/DrawableDelegates.h"
#include "../RenderCore/Techniques/BasicDelegates.h"
#include "../RenderCore/Techniques/RenderPass.h"
#include "../RenderCore/Assets/Services.h"
#include "../FixedFunctionModel/PreboundShaders.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/DataPacket.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../Assets/Assets.h"
#include "../Assets/DepVal.h"
#include "../Math/Transformations.h"
#include "../ConsoleRig/ResourceBox.h"

namespace SceneEngine
{
    using namespace RenderCore;

	class RayDefinitionUniformDelegate : public Techniques::IUniformBufferDelegate
	{
	public:
		struct Buffer
        {
            Float3 _rayStart;
            float _rayLength;
            Float3 _rayDirection;
            unsigned _dummy;
        };
		Buffer _data = {};

		virtual ConstantBufferView WriteBuffer(Techniques::ParsingContext& context, const void*) { return MakeSubFramePkt(_data); }
		static const uint64_t s_binding;
	};
	const uint64_t RayDefinitionUniformDelegate::s_binding = Hash64("RayDefinition");

	class FrustumDefinitionUniformDelegate : public Techniques::IUniformBufferDelegate
	{
	public:
		struct Buffer
        {
            Float4x4 _frustum;
        };
		Buffer _data = {};

		virtual ConstantBufferView WriteBuffer(Techniques::ParsingContext& context, const void*) { return MakeSubFramePkt(_data); }
		static const uint64_t s_binding;
	};
	const uint64_t FrustumDefinitionUniformDelegate::s_binding = Hash64("IntersectionFrustumDefinition");

    class ModelIntersectionStateContext::Pimpl
    {
    public:
        IThreadContext* _threadContext;
        ModelIntersectionResources* _res;
		bool _pendingUnbind = false;

		Techniques::RenderPassInstance _rpi;
		unsigned _queryId = ~0u;
    };

    class ModelIntersectionResources
    {
    public:
        class Desc 
        {
        public:
            unsigned _elementSize;
            unsigned _elementCount;
            Desc(unsigned elementSize, unsigned elementCount) : _elementSize(elementSize), _elementCount(elementCount) {}
        };

        RenderCore::IResourcePtr _streamOutputBuffer;
        RenderCore::IResourcePtr _cpuAccessBuffer;

		std::unique_ptr<RenderCore::Metal::QueryPool> _streamOutputQueryPool;

		Metal::DepthStencilState _dds;
		Metal::RasterizerState _rs;

		std::shared_ptr<RayDefinitionUniformDelegate> _rayDefinition;
		std::shared_ptr<FrustumDefinitionUniformDelegate> _frustumDefinition;
		Techniques::AttachmentPool _dummyAttachmentPool;
		Techniques::FrameBufferPool _frameBufferPool;

        ModelIntersectionResources(const Desc&);
    };

    ModelIntersectionResources::ModelIntersectionResources(const Desc& desc)
	: _dds{false, false}
	, _rs{Metal::RasterizerState::Null()}
    {
        auto& device = RenderCore::Assets::Services::GetDevice();

        LinearBufferDesc lbDesc;
        lbDesc._structureByteSize = desc._elementSize;
        lbDesc._sizeInBytes = desc._elementSize * desc._elementCount;

        auto bufferDesc = CreateDesc(
            BindFlag::StreamOutput | BindFlag::TransferSrc, 0, GPUAccess::Read | GPUAccess::Write,
            lbDesc, "ModelIntersectionBuffer");
        
        _streamOutputBuffer = device.CreateResource(bufferDesc);

        _cpuAccessBuffer = device.CreateResource(
            CreateDesc(BindFlag::TransferDst, CPUAccess::Read, 0, lbDesc, "ModelIntersectionCopyBuffer"));

		_streamOutputQueryPool = std::make_unique<RenderCore::Metal::QueryPool>(
			Metal::GetObjectFactory(device), 
			Metal::QueryPool::QueryType::StreamOutput_Stream0, 4);

		_rayDefinition = std::make_shared<RayDefinitionUniformDelegate>();
		_frustumDefinition = std::make_shared<FrustumDefinitionUniformDelegate>();
    }
	
#if GFXAPI_TARGET == GFXAPI_VULKAN
	void BufferBarrier0(Metal::DeviceContext& context, Metal::Resource& buffer)
	{
		VkBufferMemoryBarrier globalBarrier =
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
				nullptr,
				VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT,
				VK_ACCESS_TRANSFER_READ_BIT,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				buffer.GetBuffer(),
				0, VK_WHOLE_SIZE
			};
		context.GetActiveCommandList().PipelineBarrier(
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			0, nullptr,
			1, &globalBarrier,
			0, nullptr);
	}

	void BufferBarrier1(Metal::DeviceContext& context, Metal::Resource& buffer)
	{
		VkBufferMemoryBarrier globalBarrier =
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
				nullptr,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				buffer.GetBuffer(),
				0, VK_WHOLE_SIZE
			};
		context.GetActiveCommandList().PipelineBarrier(
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0,
			0, nullptr,
			1, &globalBarrier,
			0, nullptr);
	}
#endif

    auto ModelIntersectionStateContext::GetResults() -> std::vector<ResultEntry>
    {
        std::vector<ResultEntry> result;

        auto& metalContext = *Metal::DeviceContext::Get(*_pimpl->_threadContext);

            // We must lock the stream output buffer, and look for results within it
            // It seems that this kind of thing wasn't part of the original intentions
            // for stream output. So the results can appear anywhere within the buffer.
            // We have to search for non-zero entries. Results that haven't been written
            // to will appear zeroed out.
		if (_pimpl->_queryId != ~0u)
			_pimpl->_res->_streamOutputQueryPool->End(metalContext, _pimpl->_queryId);
		MetalStubs::UnbindSO(metalContext);

		_pimpl->_rpi = Techniques::RenderPassInstance {};
		#if GFXAPI_TARGET == GFXAPI_VULKAN
			metalContext.QueueCommandList(*_pimpl->_threadContext->GetDevice());
		#endif
		_pimpl->_pendingUnbind = false;

		unsigned hitEventsWritten = 0;
		if (_pimpl->_queryId != ~0u) {
			Metal::QueryPool::QueryResult_StreamOutput out;
			_pimpl->_res->_streamOutputQueryPool->GetResults_Stall(metalContext, _pimpl->_queryId, AsOpaqueIteratorRange(out));
			_pimpl->_queryId = ~0u;
			hitEventsWritten = out._primitivesWritten;
		}

		if (hitEventsWritten!=0) {
			auto& cpuAccessRes = Metal::AsResource(*_pimpl->_res->_cpuAccessBuffer);
			auto& soRes = Metal::AsResource(*_pimpl->_res->_streamOutputBuffer);

			#if GFXAPI_TARGET == GFXAPI_VULKAN
				metalContext.BeginCommandList();
				BufferBarrier0(metalContext, soRes);
				Metal::Copy(metalContext, cpuAccessRes, soRes);
				BufferBarrier1(metalContext, soRes);
				metalContext.QueueCommandList(*_pimpl->_threadContext->GetDevice(), Metal::DeviceContext::QueueCommandListFlags::Stall);
			#else
				Metal::Copy(metalContext, cpuAccessRes, soRes);
			#endif

			{
				using namespace BufferUploads;
				auto& uploads = SceneEngine::GetBufferUploads();
				auto readback = uploads.Resource_ReadBack(IResourcePtr(_pimpl->_res->_cpuAccessBuffer));
				// auto readback = uploads.Resource_ReadBack(IResourcePtr(_pimpl->_res->_streamOutputBuffer));
				if (readback && readback->GetData()) {
					const auto* mappedData = (const ResultEntry*)readback->GetData();
					result.reserve(std::min(hitEventsWritten, s_maxResultCount));
					for (unsigned c=0; c<std::min(hitEventsWritten, s_maxResultCount); ++c)
						result.push_back(mappedData[c]);
				}
			}

			std::sort(result.begin(), result.end(), &ResultEntry::CompareDepth);
		}

        return std::move(result);
    }

    void ModelIntersectionStateContext::SetRay(const std::pair<Float3, Float3> worldSpaceRay)
    {
        float rayLength = Magnitude(worldSpaceRay.second - worldSpaceRay.first);
		_pimpl->_res->_rayDefinition->_data =
			RayDefinitionUniformDelegate::Buffer {
				worldSpaceRay.first, rayLength,
				(worldSpaceRay.second - worldSpaceRay.first) / rayLength, 0
			};
    }

    void ModelIntersectionStateContext::SetFrustum(const Float4x4& frustum)
    {
		_pimpl->_res->_frustumDefinition->_data = 
			FrustumDefinitionUniformDelegate::Buffer { frustum };
    }

	Techniques::SequencerTechnique ModelIntersectionStateContext::MakeRayTestSequencerTechnique()
	{
		Techniques::SequencerTechnique sequencer;
		sequencer._techniqueDelegate = SceneEngine::CreateRayTestTechniqueDelegate();
		sequencer._materialDelegate = std::make_shared<Techniques::MaterialDelegate_Basic>();

		auto& techUSI = Techniques::TechniqueContext::GetGlobalUniformsStreamInterface();
		for (unsigned c=0; c<techUSI._cbBindings.size(); ++c)
			sequencer._sequencerUniforms.emplace_back(std::make_pair(techUSI._cbBindings[c]._hashName, std::make_shared<Techniques::GlobalCBDelegate>(c)));

		sequencer._sequencerUniforms.push_back({FrustumDefinitionUniformDelegate::s_binding, _pimpl->_res->_frustumDefinition});
		sequencer._sequencerUniforms.push_back({RayDefinitionUniformDelegate::s_binding, _pimpl->_res->_rayDefinition});
		return sequencer;
	}

    ModelIntersectionStateContext::ModelIntersectionStateContext(
        TestType testType,
        RenderCore::IThreadContext& threadContext,
        Techniques::ParsingContext& parsingContext,
        const Techniques::CameraDesc* cameraForLOD)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_threadContext = &threadContext;

        parsingContext.GetSubframeShaderSelectors().SetParameter(
            (const utf8*)"INTERSECTION_TEST", unsigned(testType));

        auto& metalContext = *Metal::DeviceContext::Get(threadContext);

		#if GFXAPI_TARGET == GFXAPI_VULKAN
			metalContext.BeginCommandList();
		#endif
		_pimpl->_pendingUnbind = true;

            // The camera settings can affect the LOD that objects a rendered with.
            // So, in some cases we need to initialise the camera to the same state
            // used in rendering. This will ensure that we get the right LOD behaviour.
        Techniques::CameraDesc camera;
        if (cameraForLOD) { camera = *cameraForLOD; }

		auto projDesc = BuildProjectionDesc(camera, UInt2(256, 256));
		projDesc._cameraToProjection = MakeFloat4x4(
            0.f, 0.f, 0.f, 0.5f,
            0.f, 0.f, 0.f, 0.5f,
            0.f, 0.f, 0.f, 0.5f,
            0.f, 0.f, 0.f, 1.f);
		projDesc._worldToProjection = Combine(InvertOrthonormalTransform(projDesc._cameraToWorld), projDesc._cameraToProjection);
        LightingParser_SetGlobalTransform(threadContext, parsingContext, projDesc);

        _pimpl->_res = &ConsoleRig::FindCachedBox<ModelIntersectionResources>(
            ModelIntersectionResources::Desc(sizeof(ResultEntry), s_maxResultCount));

		    // We're doing the intersection test in the geometry shader. This means
            // we have to setup a projection transform to avoid removing any potential
            // intersection results during screen-edge clipping.
            // Also, if we want to know the triangle pts and barycentric coordinates,
            // we need to make sure that no clipping occurs.
            // The easiest way to prevent clipping would be use a projection matrix that
            // would transform all points into a single point in the center of the view
            // frustum.
        Metal::ViewportDesc newViewport(0.f, 0.f, float(255.f), float(255.f), 0.f, 1.f);
        metalContext.Bind(newViewport);

		_pimpl->_rpi = Techniques::RenderPassInstance {
			threadContext,
			FrameBufferDesc::s_empty,
			_pimpl->_res->_frameBufferPool, _pimpl->_res->_dummyAttachmentPool };

        MetalStubs::BindSO(metalContext, *_pimpl->_res->_streamOutputBuffer);
		_pimpl->_queryId = _pimpl->_res->_streamOutputQueryPool->Begin(metalContext);

		#if GFXAPI_TARGET != GFXAPI_VULKAN
			auto& commonRes = Techniques::CommonResources();
			metalContext.GetNumericUniforms(ShaderStage::Geometry).Bind(MakeResourceList(commonRes._defaultSampler));
		#endif

		metalContext.Bind(_pimpl->_res->_dds);
		metalContext.Bind(_pimpl->_res->_rs);
    }

    ModelIntersectionStateContext::~ModelIntersectionStateContext()
    {
		auto& metalContext = *Metal::DeviceContext::Get(*_pimpl->_threadContext);

		_pimpl->_rpi = Techniques::RenderPassInstance {};
		if (_pimpl->_pendingUnbind) {
			MetalStubs::UnbindSO(metalContext);
			if (_pimpl->_queryId != ~0u)
				_pimpl->_res->_streamOutputQueryPool->End(metalContext, _pimpl->_queryId);
			#if GFXAPI_TARGET == GFXAPI_VULKAN
				metalContext.QueueCommandList(*_pimpl->_threadContext->GetDevice());
			#endif
		}

		if (_pimpl->_queryId != ~0u) {
			Metal::QueryPool::QueryResult_StreamOutput out;
			_pimpl->_res->_streamOutputQueryPool->GetResults_Stall(metalContext, _pimpl->_queryId, AsOpaqueIteratorRange(out));
			_pimpl->_queryId = ~0u;
		}
    }

	static const InputElementDesc s_soEles[] = {
        InputElementDesc("POINT",               0, Format::R32G32B32A32_FLOAT),
        InputElementDesc("POINT",               1, Format::R32G32B32A32_FLOAT),
        InputElementDesc("POINT",               2, Format::R32G32B32A32_FLOAT),
		InputElementDesc("PROPERTIES",			0, Format::R32G32B32A32_UINT)
    };

    static const unsigned s_soStrides[] = { sizeof(ModelIntersectionStateContext::ResultEntry) };

	class TechniqueDelegate_RayTest : public Techniques::ITechniqueDelegate
	{
	public:
		Metal::ShaderProgram* GetShader(
			Techniques::ParsingContext& context,
			StringSection<::Assets::ResChar> techniqueCfgFile,
			const ParameterBox* shaderSelectors[],
			unsigned techniqueIndex) override;

		TechniqueDelegate_RayTest();
		~TechniqueDelegate_RayTest();
	private:
		Techniques::UniqueShaderVariationSet _resolvedShaders;
	};

	static const std::string s_pixelShaderName = "null";
	static const std::string s_geometryShaderName = "xleres/forward/raytest.gsh:triangles:gs_*";

	static void TryRegisterDependency(
		::Assets::DepValPtr& dst,
		const std::shared_ptr<::Assets::AssetFuture<CompiledShaderByteCode>>& future)
	{
		auto futureDepVal = future->GetDependencyValidation();
		if (futureDepVal)
			::Assets::RegisterAssetDependency(dst, futureDepVal);
	}

	class ShaderVariationFactory_RayTest : public Techniques::IShaderVariationFactory
	{
	public:
		::Assets::FuturePtr<Metal::ShaderProgram> MakeShaderVariation(StringSection<> defines) 
		{
			std::string definesTable = defines.AsString() + ";OUTPUT_WORLD_POSITION=1";
			auto vsCode = ::Assets::MakeAsset<CompiledShaderByteCode>(_entry->_vertexShaderName, definesTable);
			auto psCode = ::Assets::MakeAsset<CompiledShaderByteCode>(s_pixelShaderName, definesTable);

			std::stringstream str;
			str << ";SO_OFFSETS=";
			unsigned rollingOffset = 0;
			for (const auto&e:s_soEles) {
				assert(e._alignedByteOffset == ~0x0u);		// expecting to use packed sequential ordering
				if (rollingOffset!=0) str << ",";
				str << Hash64(e._semanticName) + e._semanticIndex << "," << rollingOffset;
				rollingOffset += BitsPerPixel(e._nativeFormat) / 8;
			}
			definesTable += str.str();
			auto gsCode = ::Assets::MakeAsset<CompiledShaderByteCode>(s_geometryShaderName, definesTable);

			auto future = std::make_shared<::Assets::AssetFuture<Metal::ShaderProgram>>("RayTestShader");
			future->SetPollingFunction(
				[vsCode, gsCode, psCode](::Assets::AssetFuture<Metal::ShaderProgram>& thatFuture) -> bool {

				auto vsActual = vsCode->TryActualize();
				auto gsActual = gsCode->TryActualize();
				auto psActual = psCode->TryActualize();

				if (!vsActual || !gsActual || !psActual) {
					auto vsState = vsCode->GetAssetState();
					auto gsState = gsCode->GetAssetState();
					auto psState = psCode->GetAssetState();
					if (vsState == ::Assets::AssetState::Invalid || gsState == ::Assets::AssetState::Invalid || psState == ::Assets::AssetState::Invalid) {
						auto depVal = std::make_shared<::Assets::DependencyValidation>();
						TryRegisterDependency(depVal, vsCode);
						TryRegisterDependency(depVal, gsCode);
						TryRegisterDependency(depVal, psCode);
						thatFuture.SetInvalidAsset(depVal, nullptr);
						return false;
					}
					return true;
				}

				StreamOutputInitializers so { MakeIteratorRange(s_soEles), MakeIteratorRange(s_soStrides) };
				auto newShaderProgram = std::make_shared<Metal::ShaderProgram>(Metal::GetObjectFactory(), *vsActual, *gsActual, *psActual, so);
				thatFuture.SetAsset(std::move(newShaderProgram), {});
				return false;
			});

			return future;
		}

		ShaderVariationFactory_RayTest(const Techniques::TechniqueEntry& entry) : _entry(&entry) {}
	private:
		const Techniques::TechniqueEntry* _entry;
	};

	Metal::ShaderProgram* TechniqueDelegate_RayTest::GetShader(
		Techniques::ParsingContext& context,
		StringSection<::Assets::ResChar> techniqueCfgFile,
		const ParameterBox* shaderSelectors[],
		unsigned techniqueIndex)
	{
		auto techFuture = ::Assets::MakeAsset<Techniques::Technique>(techniqueCfgFile);
		auto tech = techFuture->TryActualize();
		if (!tech) return nullptr;

		auto& entry = tech->GetEntry(techniqueIndex);
		ShaderVariationFactory_RayTest factory(entry);
		const auto& variation = _resolvedShaders.FindVariation(entry._baseSelectors, shaderSelectors, factory);
		if (!variation._shaderFuture) return nullptr;
		return variation._shaderFuture->TryActualize().get();
	}

	TechniqueDelegate_RayTest::TechniqueDelegate_RayTest()
	{}

	TechniqueDelegate_RayTest::~TechniqueDelegate_RayTest()
	{}

	std::shared_ptr<Techniques::ITechniqueDelegate> CreateRayTestTechniqueDelegate()
	{
		return std::make_shared<TechniqueDelegate_RayTest>();
	}
}

