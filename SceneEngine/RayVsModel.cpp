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
#include "../RenderCore/Format.h"
#include "../RenderCore/Types.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/TechniqueUtils.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Techniques/DrawableDelegates.h"
#include "../RenderCore/Techniques/ResolvedTechniqueShaders.h"
#include "../RenderCore/Assets/Services.h"
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

    class ModelIntersectionStateContext::Pimpl
    {
    public:
        IThreadContext* _threadContext;
        ModelIntersectionResources* _res;
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
        RenderCore::IResourcePtr _clearedBuffer;
        RenderCore::IResourcePtr _cpuAccessBuffer;

		Metal::DepthStencilState _dds;
		Metal::RasterizerState _rs;
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
            BindFlag::StreamOutput | BindFlag::TransferDst, 0, GPUAccess::Read | GPUAccess::Write,
            lbDesc, "ModelIntersectionBuffer");
        
        _streamOutputBuffer = device.CreateResource(bufferDesc);

        _cpuAccessBuffer = device.CreateResource(
            CreateDesc(0, CPUAccess::Read, 0, lbDesc, "ModelIntersectionCopyBuffer"));

        std::vector<uint8_t> emptyPkt(lbDesc._sizeInBytes, 0);
        _clearedBuffer = device.CreateResource(
            CreateDesc(
                BindFlag::StreamOutput | BindFlag::TransferSrc, 0, GPUAccess::Read | GPUAccess::Write,
                lbDesc, "ModelIntersectionClearingBuffer"), 
			[&emptyPkt](SubResourceId) { return SubResourceInitData { MakeIteratorRange(emptyPkt) }; });
    }

    auto ModelIntersectionStateContext::GetResults() -> std::vector<ResultEntry>
    {
        std::vector<ResultEntry> result;

        auto& metalContext = *Metal::DeviceContext::Get(*_pimpl->_threadContext);

            // We must lock the stream output buffer, and look for results within it
            // It seems that this kind of thing wasn't part of the original intentions
            // for stream output. So the results can appear anywhere within the buffer.
            // We have to search for non-zero entries. Results that haven't been written
            // to will appear zeroed out.
        Metal::Copy(
            metalContext,
            Metal::AsResource(*_pimpl->_res->_cpuAccessBuffer), 
            Metal::AsResource(*_pimpl->_res->_streamOutputBuffer));

        using namespace BufferUploads;
        auto& uploads = SceneEngine::GetBufferUploads();
		BufferUploads::ResourceLocator locator { IResourcePtr(_pimpl->_res->_cpuAccessBuffer) };
        auto readback = uploads.Resource_ReadBack(locator);
        if (readback && readback->GetData()) {
            const auto* mappedData = (const ResultEntry*)readback->GetData();
            unsigned resultCount = 0;
            for (unsigned c=0; c<s_maxResultCount; ++c) {
                if (mappedData[c]._depthAsInt) { ++resultCount; }
            }
            result.reserve(resultCount);
            for (unsigned c=0; c<s_maxResultCount; ++c) {
                if (mappedData[c]._depthAsInt) { 
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
        struct RayDefinitionCBuffer
        {
            Float3 _rayStart;
            float _rayLength;
            Float3 _rayDirection;
            unsigned _dummy;
        } rayDefinitionCBuffer = 
        {
            worldSpaceRay.first, rayLength,
            (worldSpaceRay.second - worldSpaceRay.first) / rayLength, 0
        };

        auto metalContext = Metal::DeviceContext::Get(*_pimpl->_threadContext);
        metalContext->GetNumericUniforms(ShaderStage::Geometry).Bind(
            MakeResourceList(
                8, MakeMetalCB(&rayDefinitionCBuffer, sizeof(rayDefinitionCBuffer))));
    }

    void ModelIntersectionStateContext::SetFrustum(const Float4x4& frustum)
    {
        struct FrustumDefinitionCBuffer
        {
            Float4x4 _frustum;
        } frustumDefinitionCBuffer = { frustum };

        auto metalContext = Metal::DeviceContext::Get(*_pimpl->_threadContext);
        metalContext->GetNumericUniforms(ShaderStage::Geometry).Bind(
            MakeResourceList(
                9, MakeMetalCB(&frustumDefinitionCBuffer, sizeof(frustumDefinitionCBuffer))));
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

            // the only way to clear these things is copy from another buffer...
        Metal::Copy(
            metalContext,
            Metal::AsResource(*_pimpl->_res->_streamOutputBuffer),
            Metal::AsResource(*_pimpl->_res->_clearedBuffer));

        MetalStubs::BindSO(metalContext, *_pimpl->_res->_streamOutputBuffer);

        auto& commonRes = Techniques::CommonResources();
        metalContext.GetNumericUniforms(ShaderStage::Geometry).Bind(MakeResourceList(commonRes._defaultSampler));
		metalContext.Bind(_pimpl->_res->_dds);
		metalContext.Bind(_pimpl->_res->_rs);
    }

    ModelIntersectionStateContext::~ModelIntersectionStateContext()
    {
        auto& metalContext = *Metal::DeviceContext::Get(*_pimpl->_threadContext);
        MetalStubs::UnbindSO(metalContext);
    }

	static const InputElementDesc s_soEles[] = {
        InputElementDesc("INTERSECTIONDEPTH",   0, Format::R32_FLOAT),
        InputElementDesc("POINT",               0, Format::R32G32B32A32_FLOAT),
        InputElementDesc("POINT",               1, Format::R32G32B32A32_FLOAT),
        InputElementDesc("POINT",               2, Format::R32G32B32A32_FLOAT),
        InputElementDesc("DRAWCALLINDEX",       0, Format::R32_UINT),
        InputElementDesc("MATERIALGUID",        0, Format::R32G32_UINT)
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
		Techniques::ResolvedShaderVariationSet _resolvedShaders;
	};

	static const std::string s_pixelShaderName = "null";
	static const std::string s_geometryShaderName = "xleres/forward/raytest.gsh:triangles:gs_*";

	Metal::ShaderProgram* TechniqueDelegate_RayTest::GetShader(
		Techniques::ParsingContext& context,
		StringSection<::Assets::ResChar> techniqueCfgFile,
		const ParameterBox* shaderSelectors[],
		unsigned techniqueIndex)
	{
		auto techFuture = ::Assets::MakeAsset<Techniques::Technique>(techniqueCfgFile);
		auto tech = techFuture->TryActualize();
		if (!tech) return nullptr;

		const auto& shaderFuture = _resolvedShaders.FindVariation(tech->GetEntry(techniqueIndex), shaderSelectors);
		if (!shaderFuture) return nullptr;
		return shaderFuture->TryActualize().get();
	}

	static void TryRegisterDependency(
		::Assets::DepValPtr& dst,
		const std::shared_ptr<::Assets::AssetFuture<CompiledShaderByteCode>>& future)
	{
		auto futureDepVal = future->GetDependencyValidation();
		if (futureDepVal)
			::Assets::RegisterAssetDependency(dst, futureDepVal);
	}

	TechniqueDelegate_RayTest::TechniqueDelegate_RayTest()
	{
		_resolvedShaders._creationFn = 
			[](	StringSection<> vsName,
				StringSection<> gsName,
				StringSection<> psName,
				StringSection<> defines)
			{
				std::string definesTable = defines.AsString() + ";OUTPUT_WORLD_POSITION=1";
				auto vsCode = ::Assets::MakeAsset<CompiledShaderByteCode>(vsName, definesTable);
				auto gsCode = ::Assets::MakeAsset<CompiledShaderByteCode>(s_geometryShaderName, definesTable);
				auto psCode = ::Assets::MakeAsset<CompiledShaderByteCode>(s_pixelShaderName, definesTable);

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
			};
	}

	TechniqueDelegate_RayTest::~TechniqueDelegate_RayTest()
	{}

	std::shared_ptr<Techniques::ITechniqueDelegate> CreateRayTestTechniqueDelegate()
	{
		return std::make_shared<TechniqueDelegate_RayTest>();
	}
}

