// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RenderStep_PrepareShadows.h"
#include "LightingParserContext.h"
#include "LightingParser.h"		// (for ILightingParserPlugin);
#include "SceneEngineUtils.h"
#include "RenderStepUtils.h"
#include "LightDesc.h"
#include "LightInternal.h"
#include "ShadowResources.h"
#include "RayTracedShadows.h"
#include "LightingTargets.h"		// for ShadowGen_DrawDebugging
#include "../RenderCore/Techniques/RenderPass.h"
#include "../RenderCore/Techniques/CommonBindings.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Techniques/RenderStateResolver.h"
#include "../RenderCore/Techniques/TechniqueDelegates.h"
#include "../RenderCore/Techniques/PipelineAccelerator.h"
#include "../RenderCore/Format.h"
#include "../RenderCore/FrameBufferDesc.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/Shader.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/ResourceBox.h"
#include "../Assets/Assets.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/FunctionUtils.h"
#include "../xleres/FileList.h"

namespace SceneEngine
{
	using namespace RenderCore;

	static const utf8* StringShadowCascadeMode = u("SHADOW_CASCADE_MODE");
    static const utf8* StringShadowEnableNearCascade = u("SHADOW_ENABLE_NEAR_CASCADE");

	class ShadowGenTechniqueDelegateBox
	{
	public:
		std::shared_ptr<RenderCore::Techniques::TechniqueSetFile> _techniqueSetFile;
		std::shared_ptr<RenderCore::Techniques::TechniqueSharedResources> _techniqueSharedResources;
		std::shared_ptr<RenderCore::Techniques::ITechniqueDelegate> _shadowGenDelegate;

		const ::Assets::DepValPtr& GetDependencyValidation() const { return _techniqueSetFile->GetDependencyValidation(); }

		class Desc
        {
        public:
            using RSDepthBias = RenderCore::Techniques::RSDepthBias;
            RSDepthBias     _singleSidedBias;
            RSDepthBias     _doubleSidedBias;
            RenderCore::CullMode	_windingCullMode;

            Desc(   const RSDepthBias& singleSidedBias,
                    const RSDepthBias& doubleSidedBias,
                    RenderCore::CullMode windingCullMode) 
            : _singleSidedBias(singleSidedBias)
            , _doubleSidedBias(doubleSidedBias)
            , _windingCullMode(windingCullMode) {}
        };

		ShadowGenTechniqueDelegateBox(const Desc& desc)
		{
			_techniqueSetFile = ::Assets::AutoConstructAsset<RenderCore::Techniques::TechniqueSetFile>(ILLUM_TECH);
			_techniqueSharedResources = std::make_shared<RenderCore::Techniques::TechniqueSharedResources>();
			_shadowGenDelegate = RenderCore::Techniques::CreateTechniqueDelegate_ShadowGen(
				_techniqueSetFile, _techniqueSharedResources,
				desc._singleSidedBias, desc._doubleSidedBias, desc._windingCullMode);
		}
	};

    static PreparedDMShadowFrustum LightingParser_PrepareDMShadow(
        IThreadContext& threadContext,
        Techniques::ParsingContext& parserContext,
		LightingParserContext& lightingParserContext, 
		ViewDelegate_Shadow& executedScene,
		RenderStepFragmentInstance& rpi)
    {
		const ShadowProjectionDesc& frustum = executedScene._shadowProj;
        auto projectionCount = std::min(frustum._projections.Count(), MaxShadowTexturesPerLight);
        if (!projectionCount)
            return PreparedDMShadowFrustum();

        if (!BatchHasContent(executedScene._general))
            return PreparedDMShadowFrustum();

		auto& metalContext = *RenderCore::Metal::DeviceContext::Get(threadContext);

        PreparedDMShadowFrustum preparedResult;
        preparedResult.InitialiseConstants(&metalContext, frustum._projections);
        using TC = Techniques::TechniqueContext;
        parserContext.SetGlobalCB(metalContext, TC::CB_ShadowProjection, &preparedResult._arbitraryCBSource, sizeof(preparedResult._arbitraryCBSource));
        parserContext.SetGlobalCB(metalContext, TC::CB_OrthoShadowProjection, &preparedResult._orthoCBSource, sizeof(preparedResult._orthoCBSource));
        preparedResult._resolveParameters._worldSpaceBias = frustum._worldSpaceResolveBias;
        preparedResult._resolveParameters._tanBlurAngle = frustum._tanBlurAngle;
        preparedResult._resolveParameters._minBlurSearch = frustum._minBlurSearch;
        preparedResult._resolveParameters._maxBlurSearch = frustum._maxBlurSearch;
        preparedResult._resolveParameters._shadowTextureSize = (float)std::min(frustum._shadowGeneratorDesc._width, frustum._shadowGeneratorDesc._height);
        XlZeroMemory(preparedResult._resolveParameters._dummy);
        preparedResult._resolveParametersCB = MakeMetalCB(
            &preparedResult._resolveParameters, sizeof(preparedResult._resolveParameters));

            /////////////////////////////////////////////

        Float4x4 savedWorldToProjection = parserContext.GetProjectionDesc()._worldToProjection;
        parserContext.GetProjectionDesc()._worldToProjection = frustum._worldToClip;
        auto cleanup2 = MakeAutoCleanup(
            [&parserContext, &savedWorldToProjection]() {
                parserContext.GetProjectionDesc()._worldToProjection = savedWorldToProjection;
            });

            /////////////////////////////////////////////

        CATCH_ASSETS_BEGIN
			ExecuteDrawables(
				threadContext, parserContext,
				MakeSequencerContext(parserContext, *rpi.GetSequencerConfig(), TechniqueIndex_ShadowGen),
				executedScene._general,
				"ShadowGen-Prepare");
		CATCH_ASSETS_END(parserContext)

        for (auto p=lightingParserContext._plugins.cbegin(); p!=lightingParserContext._plugins.cend(); ++p)
            (*p)->OnPostSceneRender(threadContext, parserContext, lightingParserContext, Techniques::BatchFilter::General, TechniqueIndex_ShadowGen);
        
        return preparedResult;
    }

	

	void RenderStep_PrepareDMShadows::Execute(
		IThreadContext& threadContext,
		Techniques::ParsingContext& parsingContext,
		LightingParserContext& lightingParserContext,
		RenderStepFragmentInstance& rpi,
		IViewDelegate* viewDelegate)
	{
		auto& shadowDelegate = *checked_cast<ViewDelegate_Shadow*>(viewDelegate);
		assert(shadowDelegate._shadowProj._shadowGeneratorDesc._resolveType == ShadowResolveType::DepthTexture);

		auto shadow = LightingParser_PrepareDMShadow(
			threadContext, parsingContext,
			lightingParserContext,
			shadowDelegate,
			rpi);
		shadow._srv = *rpi.GetDepthStencilAttachmentSRV(TextureViewDesc{TextureViewDesc::Aspect::Depth});
		if (shadow.IsReady()) {
			lightingParserContext._preparedDMShadows.push_back(std::make_pair(shadowDelegate._shadowProj._lightId, std::move(shadow)));

			if (lightingParserContext._preparedDMShadows.size() == Tweakable("ShadowGenDebugging", 0)) {
				auto srvForDebugging = *rpi.GetDepthStencilAttachmentSRV(TextureViewDesc{TextureViewDesc::Aspect::ColorLinear});
				parsingContext._pendingOverlays.push_back(
					std::bind(
						&ShadowGen_DrawDebugging, 
						std::placeholders::_1, std::placeholders::_2,
						srvForDebugging));
			}

			if (lightingParserContext._preparedDMShadows.size() == Tweakable("ShadowGenFrustumDebugging", 0)) {
				parsingContext._pendingOverlays.push_back(
					std::bind(
						&ShadowGen_DrawShadowFrustums, 
						std::placeholders::_1, std::placeholders::_2,
						lightingParserContext.GetMainTargets(),
						shadowDelegate._shadowProj));
			}
		}
	}

	RenderStep_PrepareDMShadows::RenderStep_PrepareDMShadows(const ShadowGeneratorDesc& desc)
	: _fragment(RenderCore::PipelineType::Graphics)
	{
		RenderCore::Techniques::RSDepthBias singleSidedBias {
			desc._rasterDepthBias, desc._depthBiasClamp, desc._slopeScaledBias };
		RenderCore::Techniques::RSDepthBias doubleSidedBias {
			desc._dsRasterDepthBias, desc._dsDepthBiasClamp, desc._dsSlopeScaledBias };

		auto output = _fragment.DefineAttachment(
			Techniques::AttachmentSemantics::ShadowDepthMap, 
            {
                AsTypelessFormat(desc._format),
				float(desc._width), float(desc._height),
                desc._arrayCount,
				AttachmentDesc::DimensionsMode::Absolute, 
                AttachmentDesc::Flags::ShaderResource | AttachmentDesc::Flags::DepthStencil });
		
        auto& resources = ConsoleRig::FindCachedBox2<ShadowGenTechniqueDelegateBox>(
            singleSidedBias, doubleSidedBias, desc._cullMode);

		ParameterBox box;
		box.SetParameter(StringShadowCascadeMode, desc._projectionMode == ShadowProjectionMode::Ortho?2:1);
        box.SetParameter(StringShadowEnableNearCascade, desc._enableNearCascade?1:0);

		_fragment.AddSubpass(
			SubpassDesc {
				{},
				{output, LoadStore::Clear, LoadStore::Retain}
			},
			resources._shadowGenDelegate,
			std::move(box));
	}

	RenderStep_PrepareDMShadows::~RenderStep_PrepareDMShadows() {}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void RenderStep_PrepareRTShadows::Execute(
		IThreadContext& threadContext,
		Techniques::ParsingContext& parsingContext,
		LightingParserContext& lightingParserContext,
		RenderStepFragmentInstance& rpi,
		IViewDelegate* viewDelegate)
	{
		auto& shadowDelegate = *checked_cast<ViewDelegate_Shadow*>(viewDelegate);
		assert(shadowDelegate._shadowProj._shadowGeneratorDesc._resolveType == ShadowResolveType::RayTraced);

		auto shadow = PrepareRTShadows(threadContext, parsingContext, lightingParserContext, shadowDelegate);
        if (shadow.IsReady())
            lightingParserContext._preparedRTShadows.push_back(std::make_pair(shadowDelegate._shadowProj._lightId, std::move(shadow)));
	}

	RenderStep_PrepareRTShadows::RenderStep_PrepareRTShadows()
	: _fragment(RenderCore::PipelineType::Graphics)
	{
	}

	RenderStep_PrepareRTShadows::~RenderStep_PrepareRTShadows()
	{
	}

	RenderCore::Techniques::DrawablesPacket* ViewDelegate_Shadow::GetDrawablesPacket(RenderCore::Techniques::BatchFilter batch)
	{
		if (batch == RenderCore::Techniques::BatchFilter::General)
			return &_general;
		return nullptr;
	}

	void ViewDelegate_Shadow::Reset()
	{
		_general.Reset();
	}

	ViewDelegate_Shadow::ViewDelegate_Shadow(ShadowProjectionDesc shadowProjection)
	: _shadowProj(shadowProjection)
	{
	}

	ViewDelegate_Shadow::~ViewDelegate_Shadow()
	{
	}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	ICompiledShadowGenerator::~ICompiledShadowGenerator() {}

	class CompiledShadowGenerator : public ICompiledShadowGenerator
	{
	public:
		void ExecutePrepare(
			RenderCore::IThreadContext& threadContext, 
			RenderCore::Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			ViewDelegate_Shadow& shadowDelegate,
			RenderCore::Techniques::FrameBufferPool& shadowGenFrameBufferPool,
			RenderCore::Techniques::AttachmentPool& shadowGenAttachmentPool) override;

		CompiledShadowGenerator(
			const ShadowGeneratorDesc& desc,
			const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAccelerators);
		~CompiledShadowGenerator();

	private:
		std::shared_ptr<IRenderStep> _renderStep;
		std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool> _pipelineAccelerators;

		RenderCore::FrameBufferDesc _fbDesc;
		RenderCore::Techniques::FrameBufferFragmentMapping _fbRemapping;
		std::vector<std::shared_ptr<RenderCore::Techniques::SequencerConfig>> _sequencerConfigs;
	};

	void CompiledShadowGenerator::ExecutePrepare(
		RenderCore::IThreadContext& threadContext, 
		RenderCore::Techniques::ParsingContext& parsingContext,
		LightingParserContext& lightingParserContext,
		ViewDelegate_Shadow& shadowDelegate,
		RenderCore::Techniques::FrameBufferPool& shadowGenFrameBufferPool,
		RenderCore::Techniques::AttachmentPool& shadowGenAttachmentPool)
	{
		if (!_fbDesc.GetSubpasses().empty()) {
			Techniques::RenderPassInstance rpi(threadContext, _fbDesc, shadowGenFrameBufferPool, shadowGenAttachmentPool);
			RenderStepFragmentInstance rpf(rpi, _fbRemapping, MakeIteratorRange(_sequencerConfigs));
			_renderStep->Execute(threadContext, parsingContext, lightingParserContext, rpf, &shadowDelegate);
		} else {
			RenderStepFragmentInstance rpf;
			_renderStep->Execute(threadContext, parsingContext, lightingParserContext, rpf, &shadowDelegate);
		}
	}

	CompiledShadowGenerator::CompiledShadowGenerator(
		const ShadowGeneratorDesc& desc,
		const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAccelerators)
	: _pipelineAccelerators(pipelineAccelerators)
	{
		if (desc._resolveType == ShadowResolveType::DepthTexture) {

			_renderStep = std::make_shared<RenderStep_PrepareDMShadows>(desc);

			auto interf = _renderStep->GetInterface();
			auto merged = Techniques::MergeFragments(
				{}, MakeIteratorRange(&interf.GetFrameBufferDescFragment(), &interf.GetFrameBufferDescFragment()+1));
			_fbDesc = Techniques::BuildFrameBufferDesc(std::move(merged._mergedFragment));

			auto sequencerConfig = pipelineAccelerators->CreateSequencerConfig(
				interf.GetSubpassAddendums()[0]._techniqueDelegate,
				interf.GetSubpassAddendums()[0]._sequencerSelectors,
				_fbDesc,
				0);

			_fbRemapping = std::move(merged._remapping[0]);
			_sequencerConfigs.push_back(std::move(sequencerConfig));

		} else {
			_renderStep = std::make_shared<RenderStep_PrepareRTShadows>();
		}
	}

	CompiledShadowGenerator::~CompiledShadowGenerator() {}

	std::shared_ptr<ICompiledShadowGenerator> CreateCompiledShadowGenerator(
		const ShadowGeneratorDesc& desc, 
		const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAccelerator)
	{
		return std::make_shared<CompiledShadowGenerator>(desc, pipelineAccelerator);
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			//		draw frustum debugging

	class SFDResources
    {
    public:
        class Desc 
        {
        public:
            unsigned    _cascadeMode;
            bool        _enableNearCascade;

            Desc(unsigned cascadeMode, bool enableNearCascade) 
            : _cascadeMode(cascadeMode), _enableNearCascade(enableNearCascade) {}
        };

        const Metal::ShaderProgram*    _shader;
        Metal::BoundUniforms           _uniforms;
        
        const ::Assets::DepValPtr& GetDependencyValidation() const   { return _depVal; }
        SFDResources(const Desc&);
        ~SFDResources();
    protected:
        ::Assets::DepValPtr _depVal;
    };

    SFDResources::SFDResources(const Desc& desc)
    {
        _shader = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            BASIC2D_VERTEX_HLSL ":fullscreen_viewfrustumvector:vs_*",
            CASCADE_VIS_HLSL ":main:ps_*",
            (const ::Assets::ResChar*)(StringMeld<128, ::Assets::ResChar>() 
                << "SHADOW_CASCADE_MODE=" << desc._cascadeMode 
                << ";SHADOW_ENABLE_NEAR_CASCADE=" << (desc._enableNearCascade?1:0)));

		UniformsStreamInterface uniformsInterf;
		uniformsInterf.BindConstantBuffer(0, { Hash64("ArbitraryShadowProjection") });
		uniformsInterf.BindConstantBuffer(1, { Hash64("OrthogonalShadowProjection") });
		uniformsInterf.BindConstantBuffer(2, { Hash64("ScreenToShadowProjection") });
		uniformsInterf.BindShaderResource(0, { Hash64("DepthTexture") });
		_uniforms = Metal::BoundUniforms(
			*_shader,
			Metal::PipelineLayoutConfig{},
			Techniques::TechniqueContext::GetGlobalUniformsStreamInterface(),
			uniformsInterf);
        
        _depVal = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(_depVal, _shader->GetDependencyValidation());
    }

    SFDResources::~SFDResources() {}

    void ShadowGen_DrawShadowFrustums(
        Metal::DeviceContext& devContext, 
		RenderCore::Techniques::ParsingContext& parserContext,
        MainTargets mainTargets,
		const ShadowProjectionDesc& projectionDesc)
    {
        devContext.Bind(Techniques::CommonResources()._dssDisable);
        devContext.Bind(Techniques::CommonResources()._blendAlphaPremultiplied);

        Metal::ShaderResourceView depthSrv = mainTargets.GetSRV(
			parserContext, 
			Techniques::AttachmentSemantics::MultisampleDepth,
			RenderCore::TextureViewDesc{RenderCore::TextureViewDesc::Aspect::ColorLinear});

        auto& res = ConsoleRig::FindCachedBoxDep2<SFDResources>(
            (projectionDesc._projections._mode == ShadowProjectionMode::Ortho)?2:1,
            projectionDesc._projections._useNearProj);
        devContext.Bind(*res._shader);

        CB_ArbitraryShadowProjection arbitraryCB;
        CB_OrthoShadowProjection orthoCB;
        BuildShadowConstantBuffers(arbitraryCB, orthoCB, projectionDesc._projections);

		auto mainCameraProjDesc = parserContext.GetProjectionDesc();

        ConstantBufferView constantBufferPackets[3];
        constantBufferPackets[0] = RenderCore::MakeSharedPkt(arbitraryCB);
        constantBufferPackets[1] = RenderCore::MakeSharedPkt(orthoCB);
        constantBufferPackets[2] = BuildScreenToShadowConstants(
            projectionDesc._projections._normalProjCount,
            arbitraryCB, orthoCB, 
            mainCameraProjDesc._cameraToWorld,
            mainCameraProjDesc._cameraToProjection);
        const Metal::ShaderResourceView* srv[] = { &depthSrv };

        res._uniforms.Apply(devContext, 0, parserContext.GetGlobalUniformsStream());
		res._uniforms.Apply(devContext, 1,
            UniformsStream{
                MakeIteratorRange(constantBufferPackets),
				UniformsStream::MakeResources(MakeIteratorRange(srv))});

        devContext.Bind(Topology::TriangleStrip);
        devContext.Draw(4);

        // devContext.UnbindPS<Metal::ShaderResourceView>(4, 1);
    }
}

