// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RenderStep.h"
#include "LightingParserContext.h"
#include "SceneEngineUtils.h"
#include "RenderStepUtil.h"
#include "../RenderCore/Techniques/RenderPass.h"
#include "../RenderCore/Techniques/CommonBindings.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Format.h"
#include "../RenderCore/FrameBufferDesc.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../ConsoleRig/Console.h"
#include "../Utility/PtrUtils.h"

namespace SceneEngine
{
	using namespace RenderCore;

	static const utf8* StringShadowCascadeMode = u("SHADOW_CASCADE_MODE");
    static const utf8* StringShadowEnableNearCascade = u("SHADOW_ENABLE_NEAR_CASCADE");

    PreparedDMShadowFrustum LightingParser_PrepareDMShadow(
        IThreadContext& threadContext,
        Techniques::ParsingContext& parserContext,
		LightingParserContext& lightingParserContext, 
		ViewDrawables_Shadow& executedScene,
        PreparedScene& preparedScene,
        const ShadowProjectionDesc& frustum,
        unsigned shadowFrustumIndex)
    {
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
            singleSidedBias, doubleSidedBias, unsigned(frustum._windingCull));

            /////////////////////////////////////////////

        metalContext.Bind(Metal::ViewportDesc(0.f, 0.f, float(frustum._width), float(frustum._height)));

        parserContext.GetNamedResources().DefineAttachment(
			IMainTargets::ShadowDepthMap + shadowFrustumIndex, 
            {   // 
                AsTypelessFormat(frustum._format),
				float(frustum._width), float(frustum._height),
                frustum._projections.Count(),
                TextureViewDesc::DepthStencil,
				AttachmentDesc::DimensionsMode::Absolute, 
                AttachmentDesc::Flags::ShaderResource | AttachmentDesc::Flags::DepthStencil });

		SubpassDesc subpasses[] = {
			SubpassDesc {
				{},
				{IMainTargets::ShadowDepthMap + shadowFrustumIndex, LoadStore::Clear, LoadStore::Retain}
			}
		};
        FrameBufferDesc resolveLighting = MakeIteratorRange(subpasses);

		auto fb = parserContext.GetFrameBufferPool().BuildFrameBuffer(resolveLighting, parserContext.GetNamedResources());
		ClearValue clearValues[] = {MakeClearValue(1.f, 0x0)};
        Techniques::RenderPassInstance rpi(
            threadContext, fb, resolveLighting,
            parserContext.GetNamedResources(),
            (Techniques::RenderPassBeginDesc)MakeIteratorRange(clearValues));

        preparedResult._shadowTextureName = IMainTargets::ShadowDepthMap + shadowFrustumIndex;

            /////////////////////////////////////////////

        Float4x4 savedWorldToProjection = parserContext.GetProjectionDesc()._worldToProjection;
        parserContext.GetProjectionDesc()._worldToProjection = frustum._worldToClip;
        auto cleanup2 = MakeAutoCleanup(
            [&parserContext, &savedWorldToProjection]() {
                parserContext.GetProjectionDesc()._worldToProjection = savedWorldToProjection;
            });

            /////////////////////////////////////////////

		ExecuteDrawablesContext executeDrawablesContext;
        RenderStateDelegateChangeMarker stateMarker(parserContext, resources._stateResolver);
        metalContext.Bind(resources._rasterizerState);
        ExecuteDrawables(
            threadContext, parserContext, executeDrawablesContext,
            executedScene._general,
            TechniqueIndex_ShadowGen, "ShadowGen-Prepare");

        for (auto p=lightingParserContext._plugins.cbegin(); p!=lightingParserContext._plugins.cend(); ++p)
            (*p)->OnPostSceneRender(threadContext, parserContext, lightingParserContext, BatchFilter::DMShadows, TechniqueIndex_ShadowGen);
        
        return std::move(preparedResult);
    }

    PreparedRTShadowFrustum LightingParser_PrepareRTShadow(
        IThreadContext& context,
		Techniques::ParsingContext& parserContext,
        LightingParserContext& lightingParserContext,
		ViewDrawables_Shadow& executedScene,
        PreparedScene& preparedScene, const ShadowProjectionDesc& frustum,
        unsigned shadowFrustumIndex)
    {
        return PrepareRTShadows(context, parserContext, lightingParserContext, executedScene, preparedScene, frustum, shadowFrustumIndex);
    }

    void LightingParser_PrepareShadows(
        IThreadContext& context,
        Techniques::ParsingContext& parserContext, 
		LightingParserContext& lightingParserContext,
		SceneExecuteContext_Main& executedScene,
		PreparedScene& preparedScene,
        IMainTargets& mainTargets)
    {
        if (!executedScene._shadowViewCount) {
            lightingParserContext._preparedDMShadows.clear();
            lightingParserContext._preparedRTShadows.clear();
            return;
        }

        GPUAnnotation anno(context, "Prepare-Shadows");

            // todo --  we should be using a temporary frame heap for this vector
        auto shadowFrustumCount = scene->GetShadowProjectionCount();
        lightingParserContext._preparedDMShadows.reserve(shadowFrustumCount);

        for (unsigned c=0; c<shadowFrustumCount; ++c) {
            auto frustum = scene->GetShadowProjectionDesc(c, parserContext.GetProjectionDesc());
            CATCH_ASSETS_BEGIN
                if (frustum._resolveType == ShadowProjectionDesc::ResolveType::DepthTexture) {

                    auto shadow = LightingParser_PrepareDMShadow(context, parserContext, lightingParserContext, *scene, preparedScene, frustum, c);
                    if (shadow.IsReady())
                        lightingParserContext._preparedDMShadows.push_back(std::make_pair(frustum._lightId, std::move(shadow)));

                } else if (frustum._resolveType == ShadowProjectionDesc::ResolveType::RayTraced) {

                    auto shadow = LightingParser_PrepareRTShadow(context, parserContext, lightingParserContext, *scene, preparedScene, frustum, c);
                    if (shadow.IsReady())
                        lightingParserContext._preparedRTShadows.push_back(std::make_pair(frustum._lightId, std::move(shadow)));

                }
            CATCH_ASSETS_END(parserContext)
        }
    }

}

