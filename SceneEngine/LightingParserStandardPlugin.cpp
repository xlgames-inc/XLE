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

#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/GPUProfiler.h"
#include "../ConsoleRig/Console.h"
#include "../Core/Exceptions.h"

namespace SceneEngine
{
    using namespace RenderCore;
    using namespace RenderCore::Metal;

    void LightingParserStandardPlugin::OnPreScenePrepare(
            RenderCore::IThreadContext&, LightingParserContext&, PreparedScene&) const
    {
    }

    void LightingParserStandardPlugin::InitBasicLightEnvironment(
        RenderCore::Metal::DeviceContext&, LightingParserContext&, ShaderLightDesc::BasicEnvironment& env) const {}

            ////////////////////////////////////////////////////////////////////////

    static void TiledLighting_Prepare(  DeviceContext& context,
                                        LightingParserContext& parserContext,
                                        LightingResolveContext& resolveContext)
    {
        resolveContext._tiledLightingResult = 
            TiledLighting_CalculateLighting(
                &context, parserContext,
                resolveContext.GetMainTargets()._msaaDepthBufferSRV, resolveContext.GetMainTargets()._gbufferRTVsSRV[1]);
    }

    static void AmbientOcclusion_Prepare(   DeviceContext& context,
                                            LightingParserContext& parserContext,
                                            LightingResolveContext& resolveContext)
    {
        if (Tweakable("DoAO", true)) {
            const bool useNormals = Tweakable("AO_UseNormals", true);
            auto& mainTargets = resolveContext.GetMainTargets();
            auto& aoRes = Techniques::FindCachedBox2<AmbientOcclusionResources>(
                mainTargets._desc._width, mainTargets._desc._height, Format::R8_UNORM,
                useNormals, (useNormals && resolveContext.GetSamplingCount() > 1)?Format::R8G8B8A8_SNORM:Format::Unknown);
            ViewportDesc mainViewportDesc(context);
            AmbientOcclusion_Render(&context, parserContext, aoRes, mainTargets._msaaDepthBufferSRV, &mainTargets._gbufferRTVsSRV[1], mainViewportDesc);
            resolveContext._ambientOcclusionResult = aoRes._aoSRV;
        }
    }

    static void ScreenSpaceReflections_Prepare(     DeviceContext* context,
                                                    LightingParserContext& parserContext,
                                                    LightingResolveContext& resolveContext)
   {
        if (Tweakable("DoScreenSpaceReflections", false)) {
            auto& mainTargets = resolveContext.GetMainTargets();
            resolveContext._screenSpaceReflectionsResult = ScreenSpaceReflections_BuildTextures(
                context, parserContext,
                unsigned(mainTargets._desc._width), unsigned(mainTargets._desc._height), resolveContext.UseMsaaSamplers(),
                mainTargets._gbufferRTVsSRV[0], mainTargets._gbufferRTVsSRV[1], mainTargets._gbufferRTVsSRV[2],
                mainTargets._msaaDepthBufferSRV);
        }
    }

    void LightingParserStandardPlugin::OnLightingResolvePrepare(
            RenderCore::Metal::DeviceContext& context, 
            LightingParserContext& parserContext,
            LightingResolveContext& resolveContext) const
    {
        TiledLighting_Prepare(context, parserContext, resolveContext);
        AmbientOcclusion_Prepare(context, parserContext, resolveContext);
        ScreenSpaceReflections_Prepare(&context, parserContext, resolveContext);
    }


            ////////////////////////////////////////////////////////////////////////

    void LightingParserStandardPlugin::OnPostSceneRender(
            RenderCore::Metal::DeviceContext& context, LightingParserContext& parserContext, 
            const SceneParseSettings& parseSettings, unsigned techniqueIndex) const
    {
        const bool doTiledBeams             = Tweakable("TiledBeams", false);
        const bool doTiledRenderingTest     = Tweakable("DoTileRenderingTest", false);
        const bool tiledBeamsTransparent    = Tweakable("TiledBeamsTransparent", false);

        const bool isTransparentPass = parseSettings._batchFilter == SceneParseSettings::BatchFilter::Transparent;
        if (doTiledRenderingTest && tiledBeamsTransparent == isTransparentPass) {
            ViewportDesc viewport(context);

            TiledLighting_RenderBeamsDebugging(
                &context, parserContext, doTiledBeams, 
                unsigned(viewport.Width), unsigned(viewport.Height),
                techniqueIndex);
        }
    }
}

