// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RenderStep.h"
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
#include "../RenderCore/Format.h"
#include "../RenderCore/FrameBufferDesc.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/ResourceBox.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/FunctionUtils.h"

namespace SceneEngine
{
	using namespace RenderCore;

	static const utf8* StringShadowCascadeMode = u("SHADOW_CASCADE_MODE");
    static const utf8* StringShadowEnableNearCascade = u("SHADOW_ENABLE_NEAR_CASCADE");

    static PreparedDMShadowFrustum LightingParser_PrepareDMShadow(
        IThreadContext& threadContext,
        Techniques::ParsingContext& parserContext,
		LightingParserContext& lightingParserContext, 
		ViewDelegate_Shadow& executedScene)
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
        parserContext.GetSubframeShaderSelectors().SetParameter(
            StringShadowCascadeMode, 
            preparedResult._mode == ShadowProjectionDesc::Projections::Mode::Ortho?2:1);
        parserContext.GetSubframeShaderSelectors().SetParameter(
            StringShadowEnableNearCascade,  preparedResult._enableNearCascade?1:0);

        auto cleanup = MakeAutoCleanup(
            [&parserContext]() {
                parserContext.GetSubframeShaderSelectors().SetParameter(StringShadowCascadeMode, 0);
                parserContext.GetSubframeShaderSelectors().SetParameter(StringShadowEnableNearCascade, 0);
            });

            /////////////////////////////////////////////

        RenderCore::Techniques::RSDepthBias singleSidedBias(
            frustum._rasterDepthBias, frustum._depthBiasClamp, frustum._slopeScaledBias);
        RenderCore::Techniques::RSDepthBias doubleSidedBias(
            frustum._dsRasterDepthBias, frustum._dsDepthBiasClamp, frustum._dsSlopeScaledBias);
        auto& resources = ConsoleRig::FindCachedBox2<ShadowWriteResources>(
            singleSidedBias, doubleSidedBias, frustum._cullMode);

            /////////////////////////////////////////////

        /* Float4x4 savedWorldToProjection = parserContext.GetProjectionDesc()._worldToProjection;
        parserContext.GetProjectionDesc()._worldToProjection = frustum._worldToClip;
        auto cleanup2 = MakeAutoCleanup(
            [&parserContext, &savedWorldToProjection]() {
                parserContext.GetProjectionDesc()._worldToProjection = savedWorldToProjection;
            }); */

            /////////////////////////////////////////////

		RenderStateDelegateChangeMarker stateMarker(parserContext, resources._stateResolver);
		ExecuteDrawablesContext executeDrawablesContext(parserContext);
        metalContext.Bind(resources._rasterizerState);
        ExecuteDrawables(
            threadContext, parserContext, executeDrawablesContext,
            executedScene._general,
            TechniqueIndex_ShadowGen, "ShadowGen-Prepare");

        for (auto p=lightingParserContext._plugins.cbegin(); p!=lightingParserContext._plugins.cend(); ++p)
            (*p)->OnPostSceneRender(threadContext, parserContext, lightingParserContext, Techniques::BatchFilter::General, TechniqueIndex_ShadowGen);
        
        return preparedResult;
    }

	void RenderStep_PrepareDMShadows::Execute(
		IThreadContext& threadContext,
		Techniques::ParsingContext& parsingContext,
		LightingParserContext& lightingParserContext,
		Techniques::RenderPassFragment& rpi,
		IViewDelegate* viewDelegate)
	{
		auto& shadowDelegate = *checked_cast<ViewDelegate_Shadow*>(viewDelegate);
		assert(shadowDelegate._shadowProj._resolveType == ShadowProjectionDesc::ResolveType::DepthTexture);

		auto shadow = LightingParser_PrepareDMShadow(
			threadContext, parsingContext,
			lightingParserContext,
			shadowDelegate);
		shadow._srv = Metal::ShaderResourceView(_resource, TextureViewDesc{TextureViewDesc::Aspect::Depth});
		if (shadow.IsReady())
			lightingParserContext._preparedDMShadows.push_back(std::make_pair(shadowDelegate._shadowProj._lightId, std::move(shadow)));
	}

	RenderStep_PrepareDMShadows::RenderStep_PrepareDMShadows(Format format, UInt2 dims, unsigned projectionCount)
	{
		auto output = _fragment.DefineAttachment(
			Techniques::AttachmentSemantics::ShadowDepthMap, 
            {
                AsTypelessFormat(format),
				float(dims[0]), float(dims[1]),
                projectionCount,
				AttachmentDesc::DimensionsMode::Absolute, 
                AttachmentDesc::Flags::ShaderResource | AttachmentDesc::Flags::DepthStencil });

		_fragment.AddSubpass(
			SubpassDesc {
				{},
				{output, LoadStore::Clear, LoadStore::Retain}
			});
	}

	RenderStep_PrepareDMShadows::~RenderStep_PrepareDMShadows() {}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void RenderStep_PrepareRTShadows::Execute(
		IThreadContext& threadContext,
		Techniques::ParsingContext& parsingContext,
		LightingParserContext& lightingParserContext,
		Techniques::RenderPassFragment& rpi,
		IViewDelegate* viewDelegate)
	{
		auto& shadowDelegate = *checked_cast<ViewDelegate_Shadow*>(viewDelegate);
		assert(shadowDelegate._shadowProj._resolveType == ShadowProjectionDesc::ResolveType::RayTraced);

		auto shadow = PrepareRTShadows(threadContext, parsingContext, lightingParserContext, shadowDelegate);
        if (shadow.IsReady())
            lightingParserContext._preparedRTShadows.push_back(std::make_pair(shadowDelegate._shadowProj._lightId, std::move(shadow)));
	}

	RenderStep_PrepareRTShadows::RenderStep_PrepareRTShadows()
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

	ViewDelegate_Shadow::ViewDelegate_Shadow(ShadowProjectionDesc shadowProjection)
	: _shadowProj(shadowProjection)
	{
	}

	ViewDelegate_Shadow::~ViewDelegate_Shadow()
	{
	}

}

