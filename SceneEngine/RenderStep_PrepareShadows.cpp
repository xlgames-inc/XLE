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
#include "../RenderCore/Techniques/RenderPass.h"
#include "../RenderCore/Techniques/CommonBindings.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Techniques/RenderStateResolver.h"
#include "../RenderCore/Techniques/TechniqueDelegates.h"
#include "../RenderCore/Format.h"
#include "../RenderCore/FrameBufferDesc.h"
#include "../RenderCore/Metal/DeviceContext.h"
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
        preparedResult._resolveParameters._shadowTextureSize = (float)std::min(frustum._width, frustum._height);
        XlZeroMemory(preparedResult._resolveParameters._dummy);
        preparedResult._resolveParametersCB = MakeMetalCB(
            &preparedResult._resolveParameters, sizeof(preparedResult._resolveParameters));

            //  we need to set the "shadow cascade mode" settings to the right
            //  mode for this prepare step;
        /*parserContext.GetSubframeShaderSelectors().SetParameter(
            StringShadowCascadeMode, 
            preparedResult._mode == ShadowProjectionDesc::Projections::Mode::Ortho?2:1);
        parserContext.GetSubframeShaderSelectors().SetParameter(
            StringShadowEnableNearCascade,  preparedResult._enableNearCascade?1:0);

        auto cleanup = MakeAutoCleanup(
            [&parserContext]() {
                parserContext.GetSubframeShaderSelectors().SetParameter(StringShadowCascadeMode, 0);
                parserContext.GetSubframeShaderSelectors().SetParameter(StringShadowEnableNearCascade, 0);
            });*/

            /////////////////////////////////////////////

        Float4x4 savedWorldToProjection = parserContext.GetProjectionDesc()._worldToProjection;
        parserContext.GetProjectionDesc()._worldToProjection = frustum._worldToClip;
        auto cleanup2 = MakeAutoCleanup(
            [&parserContext, &savedWorldToProjection]() {
                parserContext.GetProjectionDesc()._worldToProjection = savedWorldToProjection;
            });

            /////////////////////////////////////////////

        CATCH_ASSETS_BEGIN
			// RenderStateDelegateChangeMarker stateMarker(parserContext, resources._stateResolver);
			// ExecuteDrawablesContext executeDrawablesContext(parserContext);
			// metalContext.Bind(resources._rasterizerState);
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
		assert(shadowDelegate._shadowProj._resolveType == ShadowProjectionDesc::ResolveType::DepthTexture);

		auto shadow = LightingParser_PrepareDMShadow(
			threadContext, parsingContext,
			lightingParserContext,
			shadowDelegate,
			rpi);
		shadow._srv = Metal::ShaderResourceView(_resource, TextureViewDesc{TextureViewDesc::Aspect::Depth});
		if (shadow.IsReady())
			lightingParserContext._preparedDMShadows.push_back(std::make_pair(shadowDelegate._shadowProj._lightId, std::move(shadow)));
	}

	RenderStep_PrepareDMShadows::RenderStep_PrepareDMShadows(
		Format format, UInt2 dims, unsigned projectionCount,
		const RenderCore::Techniques::RSDepthBias& singleSidedBias,
        const RenderCore::Techniques::RSDepthBias& doubleSidedBias,
        CullMode cullMode)
	: _fragment(RenderCore::PipelineType::Graphics)
	{
		auto output = _fragment.DefineAttachment(
			Techniques::AttachmentSemantics::ShadowDepthMap, 
            {
                AsTypelessFormat(format),
				float(dims[0]), float(dims[1]),
                projectionCount,
				AttachmentDesc::DimensionsMode::Absolute, 
                AttachmentDesc::Flags::ShaderResource | AttachmentDesc::Flags::DepthStencil });
		
        auto& resources = ConsoleRig::FindCachedBox2<ShadowGenTechniqueDelegateBox>(
            singleSidedBias, doubleSidedBias, cullMode);

		ParameterBox box;
		bool enableNearCascade = true;		// preparedResult._enableNearCascade
		auto projectionMode = ShadowProjectionDesc::Projections::Mode::Ortho;  // preparedResult._mode
		box.SetParameter(StringShadowCascadeMode, projectionMode == ShadowProjectionDesc::Projections::Mode::Ortho?2:1);
        box.SetParameter(StringShadowEnableNearCascade, enableNearCascade?1:0);

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
		assert(shadowDelegate._shadowProj._resolveType == ShadowProjectionDesc::ResolveType::RayTraced);

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

}

