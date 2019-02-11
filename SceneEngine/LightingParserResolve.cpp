// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LightingParser.h"
#include "LightingParserContext.h"
#include "LightingTargets.h"
#include "SceneParser.h"
#include "SceneEngineUtils.h"
#include "ShadowResources.h"
#include "ShaderLightDesc.h"
#include "LightInternal.h"
#include "RenderStep.h"

#include "Sky.h"
#include "VolumetricFog.h"
#include "RayTracedShadows.h"
#include "MetalStubs.h"

#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/RenderPass.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/RenderUtils.h"
#include "../RenderCore/Format.h"
#include "../RenderCore/Techniques/DeferredShaderResource.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/Resource.h"
#include "../RenderCore/Metal/ObjectFactory.h"
#include "../RenderCore/Metal/QueryPool.h"
#include "../RenderCore/IAnnotator.h"
#include "../BufferUploads/ResourceLocator.h"

#include "../Assets/Assets.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/ResourceBox.h"
#include "../Math/Transformations.h"
#include "../Utility/StringFormat.h"

#include "../RenderCore/DX11/Metal/IncludeDX11.h"   // for unbind depth below

namespace SceneEngine
{
    using namespace RenderCore;

///////////////////////////////////////////////////////////////////////////////////////////////////

    static const uint32 StencilSky = 1<<7;
    static const uint32 StencilSampleCount = 1<<6;

    MaterialOverride GlobalMaterialOverride = { 0.f, 0.6f, 0.05f, 0.f, 1.f, 1.f, 1.f, 0.f, 0.f, 1.f, 0.f, 0.f };

    class LightingResolveResources
    {
    public:
        class Desc 
        {
        public:
            unsigned _samplingCount;
            Desc(unsigned samplingCount) : _samplingCount(samplingCount) {}
        };

        Metal::DepthStencilState       _dssPrepareSampleCount;
        Metal::DepthStencilState       _dssPrepareSky;
        Metal::DepthStencilState       _writePixelFrequencyPixels;
        Metal::DepthStencilState       _writeNonSky;
        const Metal::ShaderProgram*    _perSampleMask;

        Metal::SamplerState            _shadowComparisonSampler;
        Metal::SamplerState            _shadowDepthSampler;

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _validationCallback; }

        LightingResolveResources(const Desc& desc);
    private:
        std::shared_ptr<::Assets::DependencyValidation>  _validationCallback;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    static ShaderLightDesc::Ambient AsShaderDesc(const GlobalLightingDesc& desc)
    {
        return ShaderLightDesc::Ambient 
            { 
                desc._ambientLight, desc._skyReflectionScale, desc._skyReflectionBlurriness, {0,0,0},
            };
    }

    static ShaderLightDesc::RangeFog AsRangeFogDesc(const GlobalLightingDesc& desc)
    {
        if (desc._doRangeFog)
            return ShaderLightDesc::RangeFog { desc._rangeFogInscatter, desc._rangeFogThickness };
        return ShaderLightDesc::RangeFog { Float3(0.f, 0.f, 0.f), 0 };
    }

    static ShaderLightDesc::Light AsShaderDesc(const LightDesc& light)
    {
        return ShaderLightDesc::Light 
            {
                light._position, light._cutoffRange, 
                light._diffuseColor, light._radii[0],
                light._specularColor, light._radii[1],
                ExtractRight(light._orientation), light._diffuseWideningMin, 
                ExtractForward(light._orientation), light._diffuseWideningMax, 
                ExtractUp(light._orientation), 0
            };
    }

    static ShaderLightDesc::Light BlankLightDesc()
    {
        return ShaderLightDesc::Light
            {   Float3(0.f, 0.f, 0.f), 0.f, 
                Float3(0.f, 0.f, 0.f), 0.f,
                Float3(0.f, 0.f, 0.f), 0.f,
                Float3(0.f, 0.f, 0.f), 0.f,
                Float3(0.f, 0.f, 0.f), 0.f,
                Float3(0.f, 0.f, 0.f), 0 };
    }

    static ShaderLightDesc::VolumeFog BlankVolumeFogDesc()
    {
        return ShaderLightDesc::VolumeFog
            {   0.f, 0.f, 0.f, 0,
                Float3(0.f, 0.f, 0.f), 0, 
                Float3(0.f, 0.f, 0.f), 0 };
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static unsigned FindDMShadowFrustum(const LightingParserContext& parsingContext, unsigned lightId);
    static unsigned FindRTShadowFrustum(const LightingParserContext& parsingContext, unsigned lightId);

    static Metal::ConstantBufferPacket BuildLightConstants(const LightDesc& light);
    static void ResolveLights(  Metal::DeviceContext& context,
								Techniques::ParsingContext& parsingContext,
                                LightingParserContext& lightingParserContext,
                                const LightingResolveContext& resolveContext,
                                bool debugging = false);

    static void SetupStateForDeferredLightingResolve(   
        Metal::DeviceContext& context, 
        LightingParserContext& lightingParserContext, 
        LightingResolveResources& resolveRes,
		Techniques::RenderPassFragment& rpi,
        bool doSampleFrequencyOptimisation,
		bool precisionTargets)
    {
        const unsigned samplingCount = lightingParserContext._sampleCount;

        SetupVertexGeneratorShader(context);

        // context.Bind(
        //     MakeResourceList(lightingResTargets._lightingResolveRTV), 
        //     (doSampleFrequencyOptimisation && samplingCount>1)?&mainTargets._secondaryDepthBuffer:nullptr);
        if (doSampleFrequencyOptimisation && samplingCount > 1) {
            context.Bind(Techniques::CommonResources()._cullDisable);
            context.Bind(resolveRes._writePixelFrequencyPixels, StencilSampleCount);
        } else {
            context.Bind(resolveRes._writeNonSky, 0x0);
        }

        TextureViewDesc justDepthWindow{
            TextureViewDesc::Aspect::Depth,
            TextureViewDesc::All, TextureViewDesc::All,
			TextureDesc::Dimensionality::Undefined, 
			TextureViewDesc::Flags::JustDepth};

		auto diffuseAspect = (!precisionTargets) ? TextureViewDesc::Aspect::ColorSRGB : TextureViewDesc::Aspect::ColorLinear;
        context.GetNumericUniforms(ShaderStage::Pixel).Bind(
			MakeResourceList(
				*rpi.GetInputAttachmentSRV(0, {diffuseAspect}), // IMainTargets::GBufferDiffuse),
				*rpi.GetInputAttachmentSRV(1), // IMainTargets::GBufferNormals),
				*rpi.GetInputAttachmentSRV(2), // IMainTargets::GBufferParameters),
				Metal::ShaderResourceView(), 
				*rpi.GetInputAttachmentSRV(3, justDepthWindow))); // IMainTargets::MultisampledDepth, justDepthWindow)));
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    void BindShadowsForForwardResolve(
        Metal::DeviceContext& metalContext,
        Techniques::ParsingContext& parsingContext,
        const PreparedDMShadowFrustum& dominantLight)
    {
        // Bind the settings in "dominantLight" to the pipeline, so they
        // can be used by forward lighting shaders. Note that this is a different
        // path from deferred lighting -- which binds elsewhere.
        //
        // Resources
        //  3: ShadowTextures
        // 10: NoiseTexture     (this is bound by LightingParser_BindLightResolveResources)
        // 
        // Samplers
        //  4: ShadowSampler
        //  5: ShadowDepthSampler
        // 
        // CBs
        // 11: ShadowResolveParameters
        // 12: ShadowParameters

        auto* shadowSRV = &dominantLight.GetSRV();
        assert(shadowSRV);
        metalContext.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(3, *shadowSRV));

        auto samplingCount = 1; // ...?
        auto& resolveRes = ConsoleRig::FindCachedBoxDep2<LightingResolveResources>(samplingCount);
        MetalStubs::GetGlobalNumericUniforms(metalContext, ShaderStage::Pixel).Bind(MakeResourceList(4, resolveRes._shadowComparisonSampler, resolveRes._shadowDepthSampler));

        metalContext.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(11, 
            dominantLight._resolveParametersCB,
			ConsoleRig::FindCachedBox2<ShadowResourcesBox>()._sampleKernel32));

        parsingContext.SetGlobalCB(
            metalContext, Techniques::TechniqueContext::CB_ShadowProjection, 
            &dominantLight._arbitraryCBSource, sizeof(dominantLight._arbitraryCBSource));
        
        parsingContext.SetGlobalCB(
            metalContext, Techniques::TechniqueContext::CB_OrthoShadowProjection, 
            &dominantLight._orthoCBSource, sizeof(dominantLight._orthoCBSource));

        auto& rtState = parsingContext.GetSubframeShaderSelectors();
        rtState.SetParameter(u("SHADOW_CASCADE_MODE"), dominantLight._mode == ShadowProjectionDesc::Projections::Mode::Ortho?2:1);
        rtState.SetParameter(u("SHADOW_ENABLE_NEAR_CASCADE"), dominantLight._enableNearCascade?1:0);
    }

    void UnbindShadowsForForwardResolve(
        Metal::DeviceContext& metalContext,
        Techniques::ParsingContext& parsingContext)
    {
        MetalStubs::UnbindPS<Metal::ShaderResourceView>(metalContext, 3,1);   // unbind shadow textures
        auto& rtState = parsingContext.GetSubframeShaderSelectors();
        rtState.SetParameter(u("SHADOW_CASCADE_MODE"), 0);
        rtState.SetParameter(u("SHADOW_ENABLE_NEAR_CASCADE"), 0);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
    
    class LightResolveResourcesRes
    {
    public:
        unsigned    _skyTextureProjection;
        bool        _hasDiffuseIBL;
        bool        _hasSpecularIBL;
    };

    LightResolveResourcesRes LightingParser_BindLightResolveResources( 
        Metal::DeviceContext& context,
        Techniques::ParsingContext& parsingContext,
		const ILightingParserDelegate& delegate)
    {
            // bind resources and constants that are required for lighting resolve operations
            // these are needed in both deferred and forward shading modes... But they are
            // bound at different times in different modes

        LightResolveResourcesRes result = { 0, false };

        CATCH_ASSETS_BEGIN
            const auto& globalDesc = delegate.GetGlobalLightingDesc();
            result._skyTextureProjection = SkyTextureParts(globalDesc).BindPS_G(context, 11);

            if (globalDesc._diffuseIBL[0]) {
                MetalStubs::GetGlobalNumericUniforms(context, ShaderStage::Pixel).Bind(MakeResourceList(19, ::Assets::MakeAsset<RenderCore::Techniques::DeferredShaderResource>(globalDesc._diffuseIBL)->Actualize()->GetShaderResource()));
                result._hasDiffuseIBL = true;
            }

            if (globalDesc._specularIBL[0]) {
                MetalStubs::GetGlobalNumericUniforms(context, ShaderStage::Pixel).Bind(MakeResourceList(20, ::Assets::MakeAsset<RenderCore::Techniques::DeferredShaderResource>(globalDesc._specularIBL)->Actualize()->GetShaderResource()));
                result._hasSpecularIBL = true;
                DEBUG_ONLY(CheckSpecularIBLMipMapCount(::Assets::MakeAsset<RenderCore::Techniques::DeferredShaderResource>(globalDesc._specularIBL)->Actualize()->GetShaderResource()));
            }

            MetalStubs::GetGlobalNumericUniforms(context, ShaderStage::Pixel).Bind(MakeResourceList(10, ::Assets::MakeAsset<RenderCore::Techniques::DeferredShaderResource>("xleres/DefaultResources/balanced_noise.dds:LT")->Actualize()->GetShaderResource()));
            MetalStubs::GetGlobalNumericUniforms(context, ShaderStage::Pixel).Bind(MakeResourceList(16, ::Assets::MakeAsset<RenderCore::Techniques::DeferredShaderResource>("xleres/DefaultResources/GGXTable.dds:LT")->Actualize()->GetShaderResource()));
            MetalStubs::GetGlobalNumericUniforms(context, ShaderStage::Pixel).Bind(MakeResourceList(21, 
				::Assets::MakeAsset<RenderCore::Techniques::DeferredShaderResource>("xleres/DefaultResources/glosslut.dds:LT")->Actualize()->GetShaderResource(),
				::Assets::MakeAsset<RenderCore::Techniques::DeferredShaderResource>("xleres/DefaultResources/glosstranslut.dds:LT")->Actualize()->GetShaderResource()));

            // context.BindPS_G(MakeResourceList(9, Metal::ConstantBuffer(&GlobalMaterialOverride, sizeof(GlobalMaterialOverride))));
        CATCH_ASSETS_END(parsingContext)

        return result;
    }

    void LightingParser_ResolveGBuffer(
        IThreadContext& threadContext, Techniques::ParsingContext& parsingContext, LightingParserContext& lightingParserContext,
		RenderCore::Techniques::RenderPassFragment& rpi)
    {
		auto& metalContext = *Metal::DeviceContext::Get(threadContext);
        Metal::GPUAnnotation anno2(metalContext, "ResolveGBuffer");

        const bool doSampleFrequencyOptimisation = Tweakable("SampleFrequencyOptimisation", true);
		const bool lightResolveDebugging = Tweakable("LightResolveDebugging", false);

        LightingResolveContext lightingResolveContext(lightingParserContext);
        const unsigned samplingCount = lightingParserContext.GetMainTargets().GetSamplingCount();
        const bool useMsaaSamplers = lightingResolveContext.UseMsaaSamplers();

        bool precisionTargets = Tweakable("PrecisionTargets", false);
        // auto sampling = TextureSamples::Create(
        //     uint8(std::max(mainTargets.GetQualitySettings()._samplingCount, 1u)), 
        //     uint8(mainTargets.GetQualitySettings()._samplingQuality));

        auto& resolveRes = ConsoleRig::FindCachedBoxDep2<LightingResolveResources>(samplingCount);
		auto& delegate = *lightingParserContext._delegate;

            //
            //    Our inputs is the prepared gbuffer 
            //        -- we resolve the lighting and write out a "lighting resolve texture"
            //

#if 0 // platformtemp
        if (doSampleFrequencyOptimisation && samplingCount>1) {
            metalContext.Bind(resolveRes._dssPrepareSampleCount, StencilSampleCount);

                // todo --  instead of clearing the stencil every time, how 
                //          about doing a progressive walk through all of the bits!
            metalContext.ClearStencil(mainTargets._secondaryDepthBuffer, 0);
            metalContext.Bind(ResourceList<Metal::RenderTargetView, 0>(), &mainTargets._secondaryDepthBuffer);
            metalContext.BindPS(MakeResourceList(mainTargets._msaaDepthBufferSRV, mainTargets._gbufferRTVsSRV[1]));
            SetupVertexGeneratorShader(metalContext);
            CATCH_ASSETS_BEGIN
                metalContext.Bind(*resolveRes._perSampleMask);
                metalContext.Draw(4);
            CATCH_ASSETS_END(parsingContext)
        }
#endif

        {
            Metal::GPUAnnotation anno(metalContext, "Prepare");
            for (auto i=lightingParserContext._plugins.cbegin(); i!=lightingParserContext._plugins.cend(); ++i) {
                CATCH_ASSETS_BEGIN
                    (*i)->OnLightingResolvePrepare(threadContext, parsingContext, lightingParserContext, lightingResolveContext);
                CATCH_ASSETS_END(parsingContext)
            }
        }

                //
                //      We must do lighting resolve in two steps:
                //          1. per-pixel resolve operations
                //          2. per-sample resolve operations
                //
                //      In each step, there are several sub-steps.
                //      Order is important. But we want to provide some flexibility for 
                //      customisation of this pipeline. For example, we need the ability to
                //      put certain resolve steps in another library or DLL.
                //
                //          1. Dynamic lights (defined by the scene engine GetLight... methods)
                //          2. Ambient light resolve
                //              (this also incorporates some other resolve operations, such as 
                //              tiled lighting, ambient occlusion & screen space reflections)
                //          3. Sky render
                //          4. Fog resolve (distance and volumetric fog)
                //
                //      During lighting resolve, there are a few permanent binds:
                //          PS SRV:
                //              0-2 : gbuffer targets 0-2
                //              4 : gbuffer depth
                //              10 : balanced_noise
                //              11-13 : sky textures
                //              16 : ggx helper
                //
                //          PS Constants:
                //              9 : global material override
                //
                //          Blend mode:
                //              One + SrcAlpha      (todo -- change to One + InvSrcAlpha for better consistency)
                //
                //          Also, we should remain in "vertex generator mode" (ie, no bound vertex inputs)
                //

        CATCH_ASSETS_BEGIN
            // note --  the gbuffer isn't considered an "input attachment" here...
            //          If we combined the gbuffer generation and lighting resolve into a single render pass,
            //          we could just use the gbuffer as an input attachment

            const unsigned passCount = (doSampleFrequencyOptimisation && samplingCount > 1)?2:1;

                // -------- -------- -------- -------- -------- --------
                //          E M I S S I V E

            // Emissive is mostly the sky.
            // We will stencil the sky here, so that we can avoid running the light & emissive shaders
            // over these pixels.
            // Note that we have to do MSAA stuff when rendering the sky (even though the color result 
            // for each sample within a pixel is identical).
            if (Tweakable("DoSky", true)) {
                Metal::GPUAnnotation anno(metalContext, "Sky");
                for (unsigned c=0; c<passCount; ++c) {
                    metalContext.Bind(resolveRes._dssPrepareSky, StencilSky);
                    Sky_Render(threadContext, parsingContext, lightingParserContext._delegate->GetGlobalLightingDesc(), false);
                }
            }

            rpi.NextSubpass();      // (in the second subpass the depth buffer is only used for stencil)

            // set light resolve state (note that we have to bind the depth buffer as a shader input here)
            SetupStateForDeferredLightingResolve(metalContext, lightingParserContext, resolveRes, rpi, doSampleFrequencyOptimisation, precisionTargets);
            auto resourceBindRes = LightingParser_BindLightResolveResources(metalContext, parsingContext, delegate);
            MetalStubs::GetGlobalNumericUniforms(metalContext, ShaderStage::Pixel).Bind(MakeResourceList(4, resolveRes._shadowComparisonSampler, resolveRes._shadowDepthSampler));
            metalContext.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(5, lightingResolveContext._ambientOcclusionResult));

                // -------- -------- -------- -------- -------- --------
                //          A M B I E N T
            
            // We do ambient first with opaque blending. This will overwrite all non-sky pixels and cover up
            // the results from last frame.
            metalContext.Bind(Techniques::CommonResources()._blendOpaque);
            for (unsigned c=0; c<passCount; ++c) {
                CATCH_ASSETS_BEGIN
                    Metal::GPUAnnotation anno(metalContext, "Ambient");

                    lightingResolveContext.SetPass((LightingResolveContext::Pass::Enum)c);

                    auto globalLightDesc = delegate.GetGlobalLightingDesc();

                        //-------- ambient light shader --------
                    auto& ambientResolveShaders = 
						ConsoleRig::FindCachedBoxDep2<AmbientResolveShaders>(
                            lightingParserContext._gbufferType,
                            (c==0)?samplingCount:1, useMsaaSamplers, c==1,
                            lightingResolveContext._ambientOcclusionResult.IsGood(),
                            lightingResolveContext._tiledLightingResult.IsGood(),
                            lightingResolveContext._screenSpaceReflectionsResult.IsGood(),
                            resourceBindRes._skyTextureProjection, resourceBindRes._hasDiffuseIBL && resourceBindRes._hasSpecularIBL,
                            globalLightDesc._doRangeFog, Tweakable("IBLRef", false));

                    Metal::ViewportDesc vdesc(metalContext);
                    struct AmbientResolveCBuffer
                    {
                        ShaderLightDesc::Ambient light;
                        ShaderLightDesc::RangeFog fog;
                        Float2 reciprocalViewportDims;
                    } ambientcbuffer {
                        AsShaderDesc(globalLightDesc),
                        AsRangeFogDesc(globalLightDesc),
                        Float2(1.f / float(vdesc.Width), 1.f / float(vdesc.Height))
                    };

                    auto ambientLightPacket = MakeSharedPkt(ambientcbuffer);
                    ambientResolveShaders._ambientLightUniforms->Apply(metalContext, 0, parsingContext.GetGlobalUniformsStream());
					ConstantBufferView cbvs[] = {ambientLightPacket};
					ambientResolveShaders._ambientLightUniforms->Apply(metalContext, 1, UniformsStream{MakeIteratorRange(cbvs)});

                    #if 0 // GFXAPI_ACTIVE == GFXAPI_DX11	// platformtemp
                            //  When screen space reflections are enabled, we need to take a copy of the lighting
                            //  resolve target. This is because we want to reflect the post-lighting resolve pixels.
                        if (lightingResolveContext._screenSpaceReflectionsResult.IsGood())
                            Metal::Copy(
                                metalContext,
                                lightingResTargets._lightingResolveCopy->GetUnderlying(), 
                                lightingResTargets._lightingResolveTexture->GetUnderlying());

                        metalContext.BindPS(MakeResourceList(6, 
                            lightingResolveContext._tiledLightingResult, 
                            lightingResolveContext._screenSpaceReflectionsResult,
                            lightingResTargets._lightingResolveCopySRV));
                    #endif

                    metalContext.Bind(*ambientResolveShaders._ambientLight);
                    metalContext.Draw(4);
                CATCH_ASSETS_END(parsingContext)
            }

                // -------- -------- -------- -------- -------- --------
                //          L I G H T S

            metalContext.Bind(Techniques::CommonResources()._blendOneSrcAlpha);
            for (unsigned c=0; c<passCount; ++c) {
                lightingResolveContext.SetPass((LightingResolveContext::Pass::Enum)c);
                ResolveLights(metalContext, parsingContext, lightingParserContext, lightingResolveContext, lightResolveDebugging);

                for (auto i=lightingResolveContext._queuedResolveFunctions.cbegin();
                    i!=lightingResolveContext._queuedResolveFunctions.cend(); ++i) {
                    (*i)(threadContext, parsingContext, lightingParserContext, lightingResolveContext, c);
                }
            }

                // -------- -------- -------- -------- -------- --------

        CATCH_ASSETS_END(parsingContext)

        MetalStubs::UnbindPS<Metal::ShaderResourceView>(metalContext, 0, 9);

            // reset some of the states to more common defaults...
        metalContext.Bind(Techniques::CommonResources()._defaultRasterizer);
        metalContext.Bind(Techniques::CommonResources()._dssReadWrite);
        
        // note -- we need to change the frame buffer desc if any of these are enabled
        // because the gbuffer needs to be retained and read from in a debugging phase
        auto debugging = Tweakable("DeferredDebugging", 0);
        if (debugging > 0) {
            parsingContext._pendingOverlays.push_back(
                std::bind(
					&Deferred_DrawDebugging, 
					std::placeholders::_1, std::placeholders::_2,
					lightingParserContext.GetMainTargets(),
					lightingParserContext._sampleCount > 1, debugging));
        }

        if (Tweakable("RTShadowMetrics", false)) {
            parsingContext._pendingOverlays.push_back(
                std::bind(&RTShadows_DrawMetrics, std::placeholders::_1, std::placeholders::_2, std::ref(lightingParserContext)));
        }

#if 0
        if (Tweakable("LightResolveDebugging", false)) {
                // we use the lamdba to store a copy of lightingResolveContext
            parsingContext._pendingOverlays.push_back(
                [&mainTargets, &lightingParserContext, &delegate, lightingResolveContext, &resolveRes, doSampleFrequencyOptimisation](RenderCore::Metal::DeviceContext& context, Techniques::ParsingContext& parsingContext)
                {
                    SavedTargets savedTargets(context);
                    auto restoreMarker = savedTargets.MakeResetMarker(context);

					#if GFXAPI_ACTIVE == GFXAPI_DX11
						context.GetUnderlying()->OMSetRenderTargets(1, savedTargets.GetRenderTargets(), nullptr); // (unbind depth)
					#endif

                    SetupVertexGeneratorShader(context);
                    context.Bind(Techniques::CommonResources()._blendOneSrcAlpha);
                    context.Bind(Techniques::CommonResources()._dssDisable);
                    context.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(
						mainTargets.GetSRV(IMainTargets::GBufferDiffuse, {diffuseAspect}),
                        mainTargets.GetSRV(IMainTargets::GBufferNormals),
                        mainTargets.GetSRV(IMainTargets::GBufferParameters),
                        Metal::ShaderResourceView(), 
                        mainTargets.GetSRV(IMainTargets::MultisampledDepth)));

                    LightingParser_BindLightResolveResources(context, parsingContext, delegate);

                    context.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(4, resolveRes._shadowComparisonSampler, resolveRes._shadowDepthSampler));
                    context.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(5, lightingResolveContext._ambientOcclusionResult));

                    ResolveLights(context, parsingContext, lightingParserContext, delegate, lightingResolveContext, true);

                    MetalStubs::UnbindPS<Metal::ShaderResourceView>(context, 0, 9);
                    context.Bind(Techniques::CommonResources()._defaultRasterizer);
                    context.Bind(Techniques::CommonResources()._dssReadWrite);
                });
        }
#endif
    }

    static void ResolveLights(  Metal::DeviceContext& context,
								Techniques::ParsingContext& parsingContext,
                                LightingParserContext& lightingParserContext,
                                const LightingResolveContext& resolveContext,
                                bool debugging)
    {
        const unsigned samplingCount = resolveContext.GetSamplingCount();
        const bool useMsaaSamplers = resolveContext.UseMsaaSamplers();

        auto& delegate = *lightingParserContext._delegate;
		auto lightCount = delegate.GetLightCount();
        if (!lightCount) return;

        Metal::GPUAnnotation anno(context, "Lights");

        using CB = LightingResolveShaders::CB;
        using SR = LightingResolveShaders::SR;
        ConstantBufferView cbvs[CB::Max];

        const Metal::ShaderResourceView* srvs[] = { nullptr, nullptr, nullptr, nullptr };
        static_assert(dimof(srvs)==SR::Max, "Shader resource array incorrect size");
        
        cbvs[CB::ShadowParam] = &ConsoleRig::FindCachedBox2<ShadowResourcesBox>()._sampleKernel32;

        Metal::ConstantBuffer debuggingCB;
        if (debugging) {
            Metal::ViewportDesc vdesc(context);
            struct DebuggingGlobals
            {
                UInt2 viewportSize; 
                Int2 MousePosition;
            } debuggingGlobals = { UInt2(unsigned(vdesc.Width), unsigned(vdesc.Height)), GetCursorPos() };
            debuggingCB = MakeMetalCB(&debuggingGlobals, sizeof(debuggingGlobals));
            cbvs[CB::Debugging] = &debuggingCB;
        }

            ////////////////////////////////////////////////////////////////////////

        auto& lightingResolveShaders = 
			ConsoleRig::FindCachedBoxDep2<LightingResolveShaders>(
                lightingParserContext._gbufferType,
                (resolveContext.GetCurrentPass()==LightingResolveContext::Pass::PerSample)?samplingCount:1, useMsaaSamplers, 
                resolveContext.GetCurrentPass()==LightingResolveContext::Pass::PerPixel,
                Tweakable("LightResolveDynamic", 0), debugging);

        const bool allowOrthoShadowResolve = Tweakable("AllowOrthoShadowResolve", true);

            //-------- do lights --------
        for (unsigned l=0; l<lightCount; ++l) {
            auto& i = delegate.GetLightDesc(l);
            cbvs[1] = BuildLightConstants(i);

            CATCH_ASSETS_BEGIN
                LightingResolveShaders::LightShaderType shaderType;
                shaderType._shape = (LightingResolveShaders::Shape)i._shape;
                shaderType._shadows = LightingResolveShaders::NoShadows;
                shaderType._hasScreenSpaceAO = resolveContext._ambientOcclusionResult.IsGood();

                    //  We only support a limited set of different light types so far.
                    //  Perhaps this will be extended to support more lights with custom
                    //  shaders and resources.
                auto shadowFrustumIndex = FindDMShadowFrustum(lightingParserContext, l);
                if (shadowFrustumIndex < lightingParserContext._preparedDMShadows.size()) {
                    assert(lightingParserContext._preparedDMShadows[shadowFrustumIndex].second.IsReady());

                    const auto& preparedShadows = lightingParserContext._preparedDMShadows[shadowFrustumIndex].second;
                    srvs[SR::DMShadow] = &preparedShadows.GetSRV();
                    assert(srvs[SR::DMShadow]);
                    cbvs[CB::ShadowProj_Arbit] = &preparedShadows._arbitraryCB;
                    cbvs[CB::ShadowProj_Ortho] = &preparedShadows._orthoCB;
                    cbvs[CB::ShadowResolveParam] = &preparedShadows._resolveParametersCB;

                        //
                        //      We need an accurate way to get from screen coords into 
                        //      shadow projection coords. To get the most accurate conversion,
                        //      let's calculate the basis vectors of the shadow projection,
                        //      in camera space.
                        //
                        //      Note -- when rendering the lights here, we're always doing a
                        //              full screen pass... Really we should be rendering a 
                        //              basic version of the light shape, with depth modes
                        //              set so that we are limited to just the pixels that 
                        //              are affected by this light.
                        //
                    
                    auto& mainCamProjDesc = parsingContext.GetProjectionDesc();
                    cbvs[CB::ScreenToShadow] = BuildScreenToShadowConstants(
                        preparedShadows, mainCamProjDesc._cameraToWorld, mainCamProjDesc._cameraToProjection);

                    if (preparedShadows._mode == ShadowProjectionDesc::Projections::Mode::Ortho && allowOrthoShadowResolve) {
                        if (preparedShadows._enableNearCascade) {
                            shaderType._shadows = LightingResolveShaders::OrthShadowsNearCascade;
                        } else shaderType._shadows = LightingResolveShaders::OrthShadows;
                    } else 
                        shaderType._shadows = LightingResolveShaders::PerspectiveShadows;

                }

                    // check for additional RT shadows
                {
                    auto rtShadowIndex = FindRTShadowFrustum(lightingParserContext, l);
                    if (rtShadowIndex < lightingParserContext._preparedRTShadows.size()) {
                        const auto& preparedRTShadows = lightingParserContext._preparedRTShadows[rtShadowIndex].second;
                        auto& mainCamProjDesc = parsingContext.GetProjectionDesc();
                        cbvs[CB::ScreenToRTShadow] = BuildScreenToShadowConstants(
                            preparedRTShadows, mainCamProjDesc._cameraToWorld, mainCamProjDesc._cameraToProjection);

                        srvs[SR::RTShadow_ListHead] = &preparedRTShadows._listHeadSRV;
                        srvs[SR::RTShadow_LinkedLists] = &preparedRTShadows._linkedListsSRV;
                        srvs[SR::RTShadow_Triangles] = &preparedRTShadows._trianglesSRV;

                        shaderType._shadows = LightingResolveShaders::OrthHybridShadows;
                    }
                }

                shaderType._diffuseModel = (uint8)i._diffuseModel;
                shaderType._shadowResolveModel = (uint8)i._shadowResolveModel;

                const auto* shader = lightingResolveShaders.GetShader(shaderType);
                assert(shader);
                if (!shader->_shader) continue;

                shader->_uniforms.Apply(context, 0, parsingContext.GetGlobalUniformsStream());
				shader->_uniforms.Apply(context, 1, UniformsStream{
					MakeIteratorRange(cbvs),
					UniformsStream::MakeResources(MakeIteratorRange(srvs))});
                if (shader->_dynamicLinking)
                    context.Bind(*shader->_shader, shader->_boundClassInterfaces);
                else
                    context.Bind(*shader->_shader);
                context.Draw(4);
            CATCH_ASSETS_END(parsingContext)
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	void RenderStep_LightingResolve::Execute(
		RenderCore::IThreadContext& threadContext,
		RenderCore::Techniques::ParsingContext& parsingContext,
		LightingParserContext& lightingParserContext,
		RenderCore::Techniques::RenderPassFragment& rpi,
		IViewDelegate* viewDelegate)
	{
		LightingParser_ResolveGBuffer(threadContext, parsingContext, lightingParserContext, rpi);
	}

	RenderStep_LightingResolve::RenderStep_LightingResolve(bool precisionTargets)
	{
		// Now, this is awkward because we want to first write to the stencil buffer using the depth information,
        // and then we want to enable a stencil pass while simulanteously reading from the depth buffer in a shader.
        // This requires that we have the depth buffer bound as a DSV and a SRV at the same time... But we can explicitly
        // separately the "aspects" so the DSV has stencil, and the SRV has depth.
        // Perhaps we need an input attachment for the depth buffer in the second pass?
            
		AttachmentDesc lightResolveAttachmentDesc =
			{	(!precisionTargets) ? Format::R16G16B16A16_FLOAT : Format::R32G32B32A32_FLOAT,
				1.f, 1.f, 0u,
				AttachmentDesc::DimensionsMode::OutputRelative,
				AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::ShaderResource | AttachmentDesc::Flags::RenderTarget };
		auto lightResolveTarget = _fragment.DefineAttachment(Techniques::AttachmentSemantics::ColorHDR, lightResolveAttachmentDesc);
		auto depthTarget = _fragment.DefineAttachment(Techniques::AttachmentSemantics::MultisampleDepth);

		TextureViewDesc justStencilWindow {
			TextureViewDesc::Aspect::Stencil,
			TextureViewDesc::All, TextureViewDesc::All,
			TextureDesc::Dimensionality::Undefined,
			TextureViewDesc::Flags::JustStencil};

		TextureViewDesc justDepthWindow {
			TextureViewDesc::Aspect::Depth,
			TextureViewDesc::All, TextureViewDesc::All,
			TextureDesc::Dimensionality::Undefined,
			TextureViewDesc::Flags::JustDepth};

		SubpassDesc firstSubpass {
			{AttachmentViewDesc{lightResolveTarget, LoadStore::DontCare, LoadStore::Retain}}, 
			AttachmentViewDesc{depthTarget, LoadStore::Retain_ClearStencil, LoadStore::Retain_RetainStencil}};

			// In the second subpass, the depth buffer is bound as stencil-only (so we can read the depth values as shader inputs)
		SubpassDesc secondSubpass {
			{AttachmentViewDesc { lightResolveTarget, LoadStore::Retain, LoadStore::Retain } }, 
			AttachmentViewDesc { depthTarget, LoadStore::Retain_RetainStencil, LoadStore::Retain_RetainStencil, justStencilWindow } };
		auto gbufferStore = LoadStore::Retain;	// (technically only need retain when we're going to use these for debugging)
		auto diffuseAspect = (!precisionTargets) ? TextureViewDesc::Aspect::ColorSRGB : TextureViewDesc::Aspect::ColorLinear;
		secondSubpass._input.push_back(
			AttachmentViewDesc {
				_fragment.DefineAttachment(Techniques::AttachmentSemantics::GBufferDiffuse),
				LoadStore::Retain, gbufferStore,
				{diffuseAspect}
			});
		secondSubpass._input.push_back(
			AttachmentViewDesc {
				_fragment.DefineAttachment(Techniques::AttachmentSemantics::GBufferNormal),
				LoadStore::Retain, gbufferStore
			});
		secondSubpass._input.push_back(
			AttachmentViewDesc {
				_fragment.DefineAttachment(Techniques::AttachmentSemantics::GBufferParameter),
				LoadStore::Retain, gbufferStore
			});
		secondSubpass._input.push_back(
			AttachmentViewDesc { depthTarget, LoadStore::Retain_RetainStencil, LoadStore::Retain_RetainStencil, justDepthWindow });

		_fragment.AddSubpass(std::move(firstSubpass));
		_fragment.AddSubpass(std::move(secondSubpass));
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

    LightingResolveResources::LightingResolveResources(const Desc& desc)
    {
        _dssPrepareSampleCount = Metal::DepthStencilState(
            false, false, 0x0, StencilSampleCount, Metal::StencilMode::AlwaysWrite, Metal::StencilMode::AlwaysWrite);

        _dssPrepareSky = Metal::DepthStencilState(
            true, false, 0xff, StencilSky, Metal::StencilMode::AlwaysWrite);

        // when StencilSky is set, the stencil test should always fail
        // So, we want "StencilSampleCount" to succeed on the front size,
        // and "0" to succeed on the back size
        _writePixelFrequencyPixels = Metal::DepthStencilState(
            false, false, StencilSky|StencilSampleCount, 0xff, 
            Metal::StencilMode(CompareOp::Equal, StencilOp::DontWrite),
            Metal::StencilMode(CompareOp::Less, StencilOp::DontWrite));

        _writeNonSky = Metal::DepthStencilState(
            false, false, StencilSky, 0xff, 
            Metal::StencilMode(CompareOp::Equal, StencilOp::DontWrite));

        Metal::SamplerState shadowComparisonSampler(
            FilterMode::ComparisonBilinear, AddressMode::Clamp, AddressMode::Clamp, AddressMode::Clamp,
			CompareOp::LessEqual);
        Metal::SamplerState shadowDepthSampler(
            FilterMode::Bilinear, AddressMode::Clamp, AddressMode::Clamp, AddressMode::Clamp);

        char definesTable[256];
        Utility::XlFormatString(definesTable, dimof(definesTable), "MSAA_SAMPLES=%i", desc._samplingCount);
        auto* perSampleMask = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            "xleres/basic2D.vsh:fullscreen:vs_*", 
            "xleres/deferred/persamplemask.psh:main:ps_*", definesTable);

        auto validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(validationCallback, perSampleMask->GetDependencyValidation());

        _perSampleMask = std::move(perSampleMask);
        _shadowComparisonSampler = std::move(shadowComparisonSampler);
        _shadowDepthSampler = std::move(shadowDepthSampler);
        _validationCallback = std::move(validationCallback);
    }

    auto            LightingResolveContext::GetCurrentPass() const -> Pass::Enum { return _pass; }
    bool            LightingResolveContext::UseMsaaSamplers() const     { return _useMsaaSamplers; }
    unsigned        LightingResolveContext::GetSamplingCount() const    { return _samplingCount; }
    void            LightingResolveContext::AppendResolve(std::function<ResolveFn>&& fn) 
    {
            // It's not safe to all this function after the "prepare" step
            //  (mostly because we iterative it through it after the prepare step,
            //  so there's a chance we might be attempting to change it while it's
            //  currently being used)
        assert(_pass == Pass::Prepare);
        _queuedResolveFunctions.push_back(fn); 
    }
    void            LightingResolveContext::SetPass(Pass::Enum newPass) { _pass = newPass; }

    LightingResolveContext::LightingResolveContext(const LightingParserContext& lightingParserContext)
    : _pass(Pass::Prepare)
    {
        _samplingCount = lightingParserContext._sampleCount;
        _useMsaaSamplers = _samplingCount > 1;
    }
    LightingResolveContext::~LightingResolveContext() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    static unsigned FindDMShadowFrustum(const LightingParserContext& parsingContext, unsigned lightId)
    {
        for (unsigned c=0; c<unsigned(parsingContext._preparedDMShadows.size()); ++c)
            if (parsingContext._preparedDMShadows[c].first==lightId)
                return c;
        return ~0u;
    }

    static unsigned FindRTShadowFrustum(const LightingParserContext& parsingContext, unsigned lightId)
    {
        for (unsigned c=0; c<unsigned(parsingContext._preparedRTShadows.size()); ++c)
            if (parsingContext._preparedRTShadows[c].first==lightId)
                return c;
        return ~0u;
    }

    static Metal::ConstantBufferPacket BuildLightConstants(const LightDesc& light)
    {
        return MakeSharedPkt(AsShaderDesc(light));
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    void LightingParser_InitBasicLightEnv(  
        RenderCore::IThreadContext& threadContext,
        Techniques::ParsingContext& parsingContext,
		LightingParserContext& lightingParserContext)
    {
        ShaderLightDesc::BasicEnvironment env;
        auto globalDesc = lightingParserContext._delegate->GetGlobalLightingDesc();
        env._ambient = AsShaderDesc(globalDesc);
        env._rangeFog = AsRangeFogDesc(globalDesc);
        env._volumeFog = BlankVolumeFogDesc();

        auto lightCount = lightingParserContext._delegate->GetLightCount();
        for (unsigned l=0; l<dimof(env._dominant); ++l)
            env._dominant[l] = (lightCount > l) ? AsShaderDesc(lightingParserContext._delegate->GetLightDesc(l)) : BlankLightDesc();

        for (const auto& p:lightingParserContext._plugins)
            p->InitBasicLightEnvironment(threadContext, parsingContext, lightingParserContext, env);

        parsingContext.SetGlobalCB(
            *RenderCore::Metal::DeviceContext::Get(threadContext), Techniques::TechniqueContext::CB_BasicLightingEnvironment,
            &env, sizeof(env));
    }

}



