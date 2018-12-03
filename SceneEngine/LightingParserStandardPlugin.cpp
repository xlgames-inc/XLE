// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LightingParserStandardPlugin.h"
#include "SceneParser.h"

#include "LightingTargets.h"
#include "AmbientOcclusion.h"
#include "TiledLighting.h"
#include "ScreenspaceReflections.h"
#include "LightingParserContext.h"
#include "LightDesc.h"
#include "MetricsBox.h"

#include "../RenderCore/IAnnotator.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Techniques/CommonBindings.h"
#include "../ConsoleRig/ResourceBox.h"
#include "../ConsoleRig/Console.h"
#include "../Core/Exceptions.h"

namespace SceneEngine
{
    using namespace RenderCore;
    using namespace RenderCore::Metal;

    void LightingParserStandardPlugin::OnPreScenePrepare(
		RenderCore::IThreadContext&, RenderCore::Techniques::ParsingContext&, LightingParserContext&) const
    {}

    void LightingParserStandardPlugin::InitBasicLightEnvironment(
        RenderCore::IThreadContext&, RenderCore::Techniques::ParsingContext&, LightingParserContext&, ShaderLightDesc::BasicEnvironment& env) const 
	{}

            ////////////////////////////////////////////////////////////////////////

    static void TiledLighting_Prepare(  DeviceContext& metalContext,
                                        RenderCore::Techniques::ParsingContext& parserContext,
										LightingParserContext& lightingParserContext,
                                        LightingResolveContext& resolveContext)
    {
        resolveContext._tiledLightingResult = 
            TiledLighting_CalculateLighting(
                &metalContext, parserContext,
                lightingParserContext.GetMainTargets().GetSRV(Techniques::AttachmentSemantics::MultisampleDepth), lightingParserContext.GetMainTargets().GetSRV(Techniques::AttachmentSemantics::GBufferNormal),
				lightingParserContext.GetMetricsBox()->_metricsBufferUAV);
    }

    static void AmbientOcclusion_Prepare(   DeviceContext& metalContext,
                                            RenderCore::Techniques::ParsingContext& parserContext,
											LightingParserContext& lightingParserContext,
                                            LightingResolveContext& resolveContext)
    {
        if (Tweakable("DoAO", true)) {
            const bool useNormals = Tweakable("AO_UseNormals", true);
            auto& mainTargets = lightingParserContext.GetMainTargets();
            auto& aoRes = ConsoleRig::FindCachedBox2<AmbientOcclusionResources>(
                mainTargets.GetDimensions()[0], mainTargets.GetDimensions()[1], Format::R8_UNORM,
                useNormals, (useNormals && mainTargets.GetSamplingCount() > 1)?Format::R8G8B8A8_SNORM:Format::Unknown);
            ViewportDesc mainViewportDesc(metalContext);
			auto normalSRV = mainTargets.GetSRV(Techniques::AttachmentSemantics::GBufferNormal);
            AmbientOcclusion_Render(
				&metalContext, parserContext, aoRes, 
				mainTargets.GetSRV(Techniques::AttachmentSemantics::MultisampleDepth), 
				&normalSRV, 
				mainViewportDesc);
            resolveContext._ambientOcclusionResult = aoRes._aoSRV;
        }
    }

    static void ScreenSpaceReflections_Prepare(     DeviceContext& metalContext,
                                                    RenderCore::Techniques::ParsingContext& parserContext,
													LightingParserContext& lightingParserContext,
                                                    LightingResolveContext& resolveContext)
   {
        if (Tweakable("DoScreenSpaceReflections", false)) {
            auto& mainTargets = lightingParserContext.GetMainTargets();
            resolveContext._screenSpaceReflectionsResult = ScreenSpaceReflections_BuildTextures(
                metalContext, parserContext,
                unsigned(mainTargets.GetDimensions()[0]), unsigned(mainTargets.GetDimensions()[1]), resolveContext.UseMsaaSamplers(),
                mainTargets.GetSRV(Techniques::AttachmentSemantics::GBufferDiffuse), mainTargets.GetSRV(Techniques::AttachmentSemantics::GBufferNormal), mainTargets.GetSRV(Techniques::AttachmentSemantics::GBufferParameter),
                mainTargets.GetSRV(Techniques::AttachmentSemantics::MultisampleDepth),
				lightingParserContext._delegate->GetGlobalLightingDesc());
        }
    }

    void LightingParserStandardPlugin::OnLightingResolvePrepare(
        RenderCore::IThreadContext& context, 
		RenderCore::Techniques::ParsingContext& parserContext, 
        LightingParserContext& lightingParserContext,
        LightingResolveContext& resolveContext) const
    {
		auto& metalContext = *RenderCore::Metal::DeviceContext::Get(context);
        TiledLighting_Prepare(metalContext, parserContext, lightingParserContext, resolveContext);
        AmbientOcclusion_Prepare(metalContext, parserContext, lightingParserContext, resolveContext);
        ScreenSpaceReflections_Prepare(metalContext, parserContext, lightingParserContext, resolveContext);
    }


            ////////////////////////////////////////////////////////////////////////

    void LightingParserStandardPlugin::OnPostSceneRender(
        RenderCore::IThreadContext& context, RenderCore::Techniques::ParsingContext& parserContext, LightingParserContext& lightingParserContext, 
        BatchFilter batch, unsigned techniqueIndex) const
    {
        const bool doTiledBeams             = Tweakable("TiledBeams", false);
        const bool doTiledRenderingTest     = Tweakable("DoTileRenderingTest", false);
        const bool tiledBeamsTransparent    = Tweakable("TiledBeamsTransparent", false);

        const bool isTransparentPass = batch == BatchFilter::Transparent;
        if (doTiledRenderingTest && tiledBeamsTransparent == isTransparentPass) {
			auto& metalContext = *RenderCore::Metal::DeviceContext::Get(context);
            ViewportDesc viewport(metalContext);

            TiledLighting_RenderBeamsDebugging(
                &metalContext, parserContext, doTiledBeams, 
                unsigned(viewport.Width), unsigned(viewport.Height),
                techniqueIndex);
        }
    }
}

