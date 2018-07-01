// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LightingParser.h"
#include "LightingParserContext.h"
#include "SceneParser.h"
#include "SceneEngineUtils.h"

#include "LightingTargets.h"
#include "LightInternal.h"
#include "Tonemap.h"
#include "VolumetricFog.h"
#include "ShadowResources.h"
#include "MetricsBox.h"
#include "Ocean.h"
#include "DeepOceanSim.h"
#include "RefractionsBuffer.h"
#include "OrderIndependentTransparency.h"
#include "StochasticTransparency.h"
#include "DepthWeightedTransparency.h"
#include "Sky.h"
#include "SunFlare.h"
#include "Rain.h"
#include "Noise.h"
#include "RayTracedShadows.h"

#include "../RenderCore/RenderUtils.h"
#include "../RenderCore/IThreadContext.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/RenderStateResolver.h"
#include "../RenderCore/Techniques/RenderPass.h"
#include "../RenderCore/Assets/DeferredShaderResource.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/QueryPool.h"
#include "../RenderCore/IAnnotator.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../Assets/Assets.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/ResourceBox.h"
#include "../Math/Transformations.h"
#include "../Math/ProjectionMath.h"
#include "../Utility/FunctionUtils.h"

#if GFXAPI_ACTIVE == GFXAPI_DX11
    #include "../RenderCore/DX11/Metal/IncludeDX11.h"
#endif

// temporary warning disable
#pragma warning(disable:4189)   // 'hasOITrans' : local variable is initialized but not referenced
#pragma warning(disable:4505)   // 'SceneEngine::LightingParser_ResolveMSAA' : unreferenced local function has been removed

namespace SceneEngine
{
    using namespace RenderCore;
    using SPS = SceneParseSettings;

    DeepOceanSimSettings GlobalOceanSettings; 
    OceanLightingSettings GlobalOceanLightingSettings; 

    void LightingParser_ResolveGBuffer( 
        IThreadContext& context,
        LightingParserContext& parserContext,
        IMainTargets& mainTargets);

    class LightResolveResourcesRes
    {
    public:
        unsigned    _skyTextureProjection;
        bool        _hasDiffuseIBL;
        bool        _hasSpecularIBL;
    };

    LightResolveResourcesRes LightingParser_BindLightResolveResources( 
        Metal::DeviceContext& context,
        LightingParserContext& parserContext);

    class StateSetResolvers
    {
    public:
        class Desc {};

        using Resolver = std::shared_ptr<Techniques::IStateSetResolver>;
        Resolver _forward, _deferred, _depthOnly;

        StateSetResolvers(const Desc&)
        {
            _forward = Techniques::CreateStateSetResolver_Forward();
            _deferred = Techniques::CreateStateSetResolver_Deferred();
            _depthOnly = Techniques::CreateStateSetResolver_DepthOnly();
        }
    };

    StateSetResolvers& GetStateSetResolvers() { return ConsoleRig::FindCachedBox2<StateSetResolvers>(); }

    class StateSetChangeMarker
    {
    public:
        StateSetChangeMarker(
            Techniques::ParsingContext& parsingContext,
            std::shared_ptr<Techniques::IStateSetResolver> newResolver)
        {
            _parsingContext = &parsingContext;
            _oldResolver = parsingContext.SetStateSetResolver(std::move(newResolver));
        }
        ~StateSetChangeMarker()
        {
            if (_parsingContext)
                _parsingContext->SetStateSetResolver(std::move(_oldResolver));
        }
        StateSetChangeMarker(const StateSetChangeMarker&);
        StateSetChangeMarker& operator=(const StateSetChangeMarker&);
    private:
        std::shared_ptr<Techniques::IStateSetResolver> _oldResolver;
        Techniques::ParsingContext* _parsingContext;
    };

        //
        //      Reserve some states as global states
        //      These remain with some fixed value throughout
        //      the entire scene. Don't change the values bound
        //      here -- we are expecting these to be set once, and 
        //      remain bound
        //
        //          PS t14 -- normals fitting texture
        //          CS t14 -- normals fitting texture
        //          
        //          PS s0, s1, s2, s4 -- default sampler, clamping sampler, anisotropic wrapping sampler, point sampler
        //          VS s0, s1, s2, s4 -- default sampler, clamping sampler, anisotropic wrapping sampler, point sampler
        //          PS s6 -- samplerWrapU
        //
    void SetFrameGlobalStates(Metal::DeviceContext& context)
    {
        Metal::SamplerState 
            samplerDefault, 
            samplerClamp(FilterMode::Trilinear, AddressMode::Clamp, AddressMode::Clamp, AddressMode::Clamp), 
            samplerAnisotrophic(FilterMode::Anisotropic),
            samplerPoint(FilterMode::Point, AddressMode::Clamp, AddressMode::Clamp, AddressMode::Clamp),
            samplerWrapU(FilterMode::Trilinear, AddressMode::Wrap, AddressMode::Clamp);
        context.BindPS_G(RenderCore::MakeResourceList(samplerDefault, samplerClamp, samplerAnisotrophic, samplerPoint));
        context.BindVS_G(RenderCore::MakeResourceList(samplerDefault, samplerClamp, samplerAnisotrophic, samplerPoint));
        context.BindPS_G(RenderCore::MakeResourceList(6, samplerWrapU));

        const auto& normalsFittingResource = ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("xleres/DefaultResources/normalsfitting.dds:LT").GetShaderResource();
		const auto& distintColors = ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("xleres/DefaultResources/distinctcolors.dds:T").GetShaderResource();
        context.BindPS_G(RenderCore::MakeResourceList(14, normalsFittingResource, distintColors));
        context.BindCS_G(RenderCore::MakeResourceList(14, normalsFittingResource));

            // perlin noise resources in standard slots
        auto& perlinNoiseRes = ConsoleRig::FindCachedBox2<PerlinNoiseResources>();
        context.BindPS_G(MakeResourceList(12, perlinNoiseRes._gradShaderResource, perlinNoiseRes._permShaderResource));

            // procedural scratch texture for scratches test
        // context.BindPS(MakeResourceList(18, Assets::GetAssetDep<Metal::DeferredShaderResource>("xleres/scratchnorm.dds:L").GetShaderResource()));
        // context.BindPS(MakeResourceList(19, Assets::GetAssetDep<Metal::DeferredShaderResource>("xleres/scratchocc.dds:L").GetShaderResource()));
    }

    void ReturnToSteadyState(Metal::DeviceContext& context)
    {
            //
            //      Change some frequently changed states back
            //      to their defaults.
            //      Most rendering operations assume these states
            //      are at their defaults.
            //

        context.Bind(Techniques::CommonResources()._dssReadWrite);
        context.Bind(Techniques::CommonResources()._blendOpaque);
        context.Bind(Techniques::CommonResources()._defaultRasterizer);
        context.Bind(Topology::TriangleList);
        context.GetNumericUniforms(ShaderStage::Vertex).Bind(RenderCore::MakeResourceList(Metal::ConstantBuffer(), Metal::ConstantBuffer(), Metal::ConstantBuffer(), Metal::ConstantBuffer(), Metal::ConstantBuffer()));
        context.GetNumericUniforms(ShaderStage::Pixel).Bind(RenderCore::MakeResourceList(Metal::ConstantBuffer(), Metal::ConstantBuffer(), Metal::ConstantBuffer(), Metal::ConstantBuffer(), Metal::ConstantBuffer()));
        context.Unbind<Metal::GeometryShader>();
    }

    void LightingParser_SetGlobalTransform(
        Metal::DeviceContext& context, 
        LightingParserContext& parserContext, 
        const RenderCore::Techniques::ProjectionDesc& projDesc)
    {
        parserContext.GetProjectionDesc() = projDesc;
        auto globalTransform = BuildGlobalTransformConstants(projDesc);
        parserContext.SetGlobalCB(
            context, Techniques::TechniqueContext::CB_GlobalTransform,
            &globalTransform, sizeof(globalTransform));
    }

    RenderCore::Techniques::ProjectionDesc BuildProjectionDesc(
        const RenderCore::Techniques::CameraDesc& sceneCamera,
        VectorPattern<unsigned, 2> viewportDims, const Float4x4* specialProjectionMatrix)
    {
        const float aspectRatio = viewportDims[0] / float(viewportDims[1]);
        auto cameraToProjection = Techniques::Projection(sceneCamera, aspectRatio);

        if (specialProjectionMatrix) {
            cameraToProjection = *specialProjectionMatrix;
        }

        RenderCore::Techniques::ProjectionDesc projDesc;
        projDesc._verticalFov = sceneCamera._verticalFieldOfView;
        projDesc._aspectRatio = aspectRatio;
        projDesc._nearClip = sceneCamera._nearClip;
        projDesc._farClip = sceneCamera._farClip;
        projDesc._worldToProjection = Combine(InvertOrthonormalTransform(sceneCamera._cameraToWorld), cameraToProjection);
        projDesc._cameraToProjection = cameraToProjection;
        projDesc._cameraToWorld = sceneCamera._cameraToWorld;
        return projDesc;
    }

    RenderCore::Techniques::ProjectionDesc BuildProjectionDesc(
        const Float4x4& cameraToWorld,
        float l, float t, float r, float b,
        float nearClip, float farClip)
    {
        auto cameraToProjection = OrthogonalProjection(l, t, r, b, nearClip, farClip, Techniques::GetDefaultClipSpaceType());

        RenderCore::Techniques::ProjectionDesc projDesc;
        projDesc._verticalFov = 0.f;
        projDesc._aspectRatio = 1.f;
        projDesc._nearClip = nearClip;
        projDesc._farClip = farClip;
        projDesc._worldToProjection = Combine(InvertOrthonormalTransform(cameraToWorld), cameraToProjection);
        projDesc._cameraToProjection = cameraToProjection;
        projDesc._cameraToWorld = cameraToWorld;
        return projDesc;
    }

    static void LightingParser_ResolveMSAA(
        Metal::DeviceContext& context, 
        LightingParserContext& parserContext,
        RenderCore::Resource& destinationTexture,
        RenderCore::Resource& sourceTexture,
        Format resolveFormat)
    {
		#if GFXAPI_ACTIVE == GFXAPI_DX11	// platformtemp
				// todo -- support custom resolve (tone-map aware)
				// See AMD post on this topic:
				//      http://gpuopen.com/optimized-reversible-tonemapper-for-resolve/
			context.GetUnderlying()->ResolveSubresource(
				Metal::AsResource(destinationTexture).GetUnderlying().get(), D3D11CalcSubresource(0,0,0),
				Metal::AsResource(sourceTexture).GetUnderlying().get(), D3D11CalcSubresource(0,0,0),
				Metal::AsDXGIFormat(resolveFormat));
		#endif
    }

    void LightingParser_PostProcess(
        Metal::DeviceContext& context, 
        LightingParserContext& parserContext)
    {
        // nothing here yet!
    }

    void LightingParser_PreTranslucency(    
        Metal::DeviceContext& context, 
        LightingParserContext& parserContext,
        const Metal::ShaderResourceView& depthsSRV)
    {
        Metal::GPUAnnotation anno(context, "PreTranslucency");

            // note --  these things can be executed by the scene parser? Are they better
            //          off handled by the scene parser, or the lighting parser?
        if (Tweakable("OceanDoSimulation", true)) {
            Ocean_Execute(&context, parserContext, GlobalOceanSettings, GlobalOceanLightingSettings, depthsSRV);
        }

        if (Tweakable("DoSky", true)) {
            Sky_RenderPostFog(context, parserContext);
        }

        auto gblLighting = parserContext.GetSceneParser()->GetGlobalLightingDesc();
        if (Tweakable("DoAtmosBlur", true) && gblLighting._doAtmosphereBlur) {
            float farClip = parserContext.GetProjectionDesc()._farClip;
            AtmosphereBlur_Execute(
                context, parserContext,
                AtmosphereBlurSettings {
                    gblLighting._atmosBlurStdDev, 
                    gblLighting._atmosBlurStart / farClip, gblLighting._atmosBlurEnd / farClip
                });
        }
    }
        
    void LightingParser_PostGBufferEffects(    
        Metal::DeviceContext& context, 
        LightingParserContext& parserContext,
        const Metal::ShaderResourceView& depthsSRV,
        const Metal::ShaderResourceView& normalsSRV)
    {
        Metal::GPUAnnotation anno(context, "PostGBuffer");
        
        if (Tweakable("DoRain", false)) {
            Rain_Render(&context, parserContext);
            Rain_RenderSimParticles(&context, parserContext, depthsSRV, normalsSRV);
        }

        if (Tweakable("DoSparks", false)) {
            SparkParticleTest_RenderSimParticles(&context, parserContext, depthsSRV, normalsSRV);
        }

        if (Tweakable("DoSun", false)) {
            SunFlare_Execute(&context, parserContext, depthsSRV);
        }
    }

    void LightingParser_Overlays(   IThreadContext& context,
                                    LightingParserContext& parserContext)
    {
        GPUAnnotation anno(context, "Overlays");

		auto metalContext = Metal::DeviceContext::Get(context);
        Metal::ViewportDesc mainViewportDesc(*metalContext);
        auto& refractionBox = ConsoleRig::FindCachedBox2<RefractionsBuffer>(unsigned(mainViewportDesc.Width/2), unsigned(mainViewportDesc.Height/2));
        refractionBox.Build(*metalContext, parserContext, 4.f);
        metalContext->BindPS_G(MakeResourceList(12, refractionBox.GetSRV()));

        for (auto i=parserContext._pendingOverlays.cbegin(); i!=parserContext._pendingOverlays.cend(); ++i) {
            CATCH_ASSETS_BEGIN
                (*i)(*metalContext, parserContext);
            CATCH_ASSETS_END(parserContext)
        }
                    
        if (Tweakable("FFTDebugging", false)) {
            FFT_DoDebugging(metalContext.get());
        }

        if (Tweakable("MetricsRender", false) && parserContext.GetMetricsBox()) {
            CATCH_ASSETS_BEGIN

                using namespace RenderCore;
                using namespace RenderCore::Metal;
                auto& metricsShader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                        "xleres/utility/metricsrender.vsh:main:vs_*", 
                        "xleres/utility/metricsrender.gsh:main:gs_*",
                        "xleres/utility/metricsrender.psh:main:ps_*",
                        "");
                metalContext->Bind(metricsShader);
                metalContext->GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(
                    3, ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("xleres/DefaultResources/metricsdigits.dds:T").GetShaderResource()));
                metalContext->Bind(BlendState(BlendOp::Add, Blend::One, Blend::InvSrcAlpha));
                metalContext->Bind(DepthStencilState(false));
                metalContext->GetNumericUniforms(ShaderStage::Vertex).Bind(MakeResourceList(parserContext.GetMetricsBox()->_metricsBufferSRV));
                unsigned dimensions[4] = { unsigned(mainViewportDesc.Width), unsigned(mainViewportDesc.Height), 0, 0 };
                metalContext->GetNumericUniforms(ShaderStage::Vertex).Bind(MakeResourceList(MakeMetalCB(dimensions, sizeof(dimensions))));
                metalContext->GetNumericUniforms(ShaderStage::Geometry).Bind(MakeResourceList(MakeMetalCB(dimensions, sizeof(dimensions))));
                SetupVertexGeneratorShader(*metalContext);
                metalContext->Bind(Topology::PointList);
                metalContext->Draw(9);

                metalContext->UnbindPS<ShaderResourceView>(3, 1);
                metalContext->UnbindVS<ShaderResourceView>(0, 1);

            CATCH_ASSETS_END(parserContext)
        }
    }

    void LightingParser_PrepareShadows(
        IThreadContext& context, Metal::DeviceContext& metalContext, 
        LightingParserContext& parserContext, PreparedScene& preparedScene,
        IMainTargets& mainTargets);

    static void ExecuteScene(
        IThreadContext& context,
        LightingParserContext& parserContext,
        const SPS& parseSettings,
        PreparedScene& preparedScene,
        unsigned techniqueIndex,
		const char name[])
    {
        CATCH_ASSETS_BEGIN
            GPUAnnotation anno(context, name);
            parserContext.GetSceneParser()->ExecuteScene(
                context, parserContext, parseSettings, preparedScene, techniqueIndex);
        CATCH_ASSETS_END(parserContext)
    }

    static bool BatchHasContent(LightingParserContext& parserContext, const SPS& parseSettings)
    {
        return parserContext.GetSceneParser()->HasContent(parseSettings);
    }

    static void ForwardLightingModel_Render(
        IThreadContext& context,
        Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        PreparedScene& preparedScene,
        IMainTargets& targets)
    {
        const auto sampleCount = targets.GetSampling()._sampleCount;
        auto lightBindRes = LightingParser_BindLightResolveResources(metalContext, parserContext);
        parserContext.GetTechniqueContext()._globalEnvironmentState.SetParameter((const utf8*)"SKY_PROJECTION", lightBindRes._skyTextureProjection);
        parserContext.GetTechniqueContext()._globalEnvironmentState.SetParameter((const utf8*)"HAS_DIFFUSE_IBL", lightBindRes._hasDiffuseIBL?1:0);
        parserContext.GetTechniqueContext()._globalEnvironmentState.SetParameter((const utf8*)"HAS_SPECULAR_IBL", lightBindRes._hasSpecularIBL?1:0);

            //  Order independent transparency disabled when
            //  using MSAA modes... Still some problems in related to MSAA buffers
        const bool useOrderIndependentTransparency = Tweakable("UseOITrans", false) && (sampleCount <= 1);
        auto normalRenderToggles = ~SPS::Toggles::BitField(0);
        if (useOrderIndependentTransparency) {
                // Skip non-terrain during normal render (this will be rendered later using OIT mode)
            normalRenderToggles &= ~SceneParseSettings::Toggles::NonTerrain;
        }

        ReturnToSteadyState(metalContext);
        {
            StateSetChangeMarker marker(parserContext, GetStateSetResolvers()._depthOnly);
            ExecuteScene(
                context, parserContext, SPS(SPS::BatchFilter::PreDepth, normalRenderToggles),
                preparedScene,
                TechniqueIndex_DepthOnly, "MainScene-DepthOnly");
        }

            /////

        ReturnToSteadyState(metalContext);
        StateSetChangeMarker marker(parserContext, GetStateSetResolvers()._forward);

            //  We must disable z write (so all shaders can be early-depth-stencil)
            //      (this is because early-depth-stencil will normally write to the depth
            //      buffer before the alpha test has been performed. The pre-depth pass
            //      will switch early-depth-stencil on and off as necessary, but in the second
            //      pass we want it on permanently because the depth reject will end up performing
            //      the same job as alpha testing)
        metalContext.Bind(Techniques::CommonResources()._dssReadOnly);

            /////
            
        ExecuteScene(
            context, parserContext, SPS(SPS::BatchFilter::General, normalRenderToggles),
            preparedScene,
            TechniqueIndex_General, "MainScene-General");

            /////

#if 0 // platformtemp
        Metal::ShaderResourceView duplicatedDepthBuffer;
        TransparencyTargetsBox* oiTransTargets = nullptr;
        if (useOrderIndependentTransparency) {
            duplicatedDepthBuffer = 
                BuildDuplicatedDepthBuffer(&metalContext, *targetsBox._msaaDepthBufferTexture->GetUnderlying());
                
            oiTransTargets = OrderIndependentTransparency_Prepare(metalContext, parserContext, duplicatedDepthBuffer);

                //
                //      (render the parts of the scene that we want to use
                //       the order independent translucent shader)
                //      --  we can also do this in one ExecuteScene() pass, because
                //          it's ok to bind the order independent uav outputs during
                //          the normal render. But to do that requires some way to
                //          select the technique index used deeper within the
                //          scene parser.
                //
            ReturnToSteadyState(metalContext);
            ExecuteScene(
                context, parserContext, SPS::BatchFilter::OITransparent, preparedScene,
                TechniqueIndex_OrderIndependentTransparency, "MainScene-OITrans");
        }

            /////

            // note --  we have to careful about when we render the sky. 
            //          If we're rendering some transparent stuff, it must come
            //          after the transparent stuff. In the case of order independent
            //          transparency, we can do the "prepare" step before the
            //          sky (because the prepare step will only write opaque stuff, 
            //          but the "resolve" step must come after sky rendering.
        if (Tweakable("DoSky", true)) {
            Sky_Render(metalContext, parserContext, true);
            Sky_RenderPostFog(metalContext, parserContext);
        }

        if (useOrderIndependentTransparency) {
                //  feed the "duplicated depth buffer" into the resolve operation here...
                //  this a non-MSAA blend over a MSAA buffer; so it might kill the samples information.
                //  We can try to do this after the MSAA resolve... But we do the luminance sample
                //  before MSAA resolve, so transluent objects don't contribute to the luminance sampling.
            OrderIndependentTransparency_Resolve(metalContext, parserContext, *oiTransTargets, duplicatedDepthBuffer); // mainTargets._msaaDepthBufferSRV);
        }
#endif
    }

    static void LightingParser_DeferredPostGBuffer(
        IThreadContext& context,
        Metal::DeviceContext& metalContext, 
        LightingParserContext& parserContext,
        PreparedScene& preparedScene,
        IMainTargets& mainTargets)
    {
        //////////////////////////////////////////////////////////////////////////////////////////////////
            //  Render translucent objects (etc)
            //  everything after the gbuffer resolve
        LightingParser_PreTranslucency(
            metalContext, parserContext, 
            mainTargets.GetSRV(IMainTargets::MultisampledDepth));

        ReturnToSteadyState(metalContext);

            // We must bind all of the lighting resolve resources here
            //  -- because we'll be doing lighting operations in the pixel
            //      shaders in a forward-lit way
        auto lightBindRes = LightingParser_BindLightResolveResources(metalContext, parserContext);
        parserContext.GetTechniqueContext()._globalEnvironmentState.SetParameter((const utf8*)"SKY_PROJECTION", lightBindRes._skyTextureProjection);
        parserContext.GetTechniqueContext()._globalEnvironmentState.SetParameter((const utf8*)"HAS_DIFFUSE_IBL", lightBindRes._hasDiffuseIBL?1:0);
        parserContext.GetTechniqueContext()._globalEnvironmentState.SetParameter((const utf8*)"HAS_SPECULAR_IBL", lightBindRes._hasSpecularIBL?1:0);

        AutoCleanup bindShadowsCleanup;
        if (!parserContext._preparedDMShadows.empty()) {
            BindShadowsForForwardResolve(metalContext, parserContext, parserContext._preparedDMShadows[0].second);
            bindShadowsCleanup = MakeAutoCleanup(
                [&metalContext, &parserContext]() 
                { UnbindShadowsForForwardResolve(metalContext, parserContext); });
        }
                    
        //////////////////////////////////////////////////////////////////////////////////////////////////
        const bool hasOITrans = BatchHasContent(parserContext, SPS::BatchFilter::OITransparent);

        enum OIMode { Unordered, Stochastic, DepthWeighted, SortedRef } oiMode;
        switch (Tweakable("OITransMode", 1)) {
        case 1:     oiMode = OIMode::Stochastic; break;
        case 2:     oiMode = OIMode::DepthWeighted; break;
        case 3:     oiMode = (mainTargets.GetSampling()._sampleCount <= 1) ? OIMode::SortedRef : OIMode::Stochastic; break;
        default:    oiMode = OIMode::Unordered; break;
        }

            // When enable OI transparency is enabled, we do a pre-depth pass
            // on all transparent geometry.
            // This pass should only draw in the opaque (or very close to opaque)
            // parts of the geometry we draw. This is important for occluding
            // transparent fragments from the sorting algorithm. For scenes with a
            // lot of sortable vegetation, it should reduce the total number of 
            // sortable fragments significantly.
            //
            // The depth pre-pass helps a little bit for stochastic transparency,
            // but it's not clear that it helps overall.
        if (oiMode == OIMode::SortedRef && Tweakable("TransPrePass", false)) {
            StateSetChangeMarker marker(parserContext, GetStateSetResolvers()._depthOnly);
            ExecuteScene(
                context, parserContext, SPS::BatchFilter::TransparentPreDepth,
                preparedScene,
                TechniqueIndex_DepthOnly, "MainScene-TransPreDepth");
        }

        if (BatchHasContent(parserContext, SPS::BatchFilter::Transparent)) {
            StateSetChangeMarker marker(parserContext, GetStateSetResolvers()._forward);
            ExecuteScene(
                context, parserContext, SPS::BatchFilter::Transparent,
                preparedScene,
                TechniqueIndex_General, "MainScene-PostGBuffer");
        }
        
#if 0 // platformtemp
        if (hasOITrans) {
            if (oiMode == OIMode::SortedRef) {
                StateSetChangeMarker marker(parserContext, GetStateSetResolvers()._depthOnly);
                auto duplicatedDepthBuffer = BuildDuplicatedDepthBuffer(&metalContext, *mainTargets._msaaDepthBufferTexture->GetUnderlying());
                auto* transTargets = OrderIndependentTransparency_Prepare(metalContext, parserContext, duplicatedDepthBuffer);

                ExecuteScene(
                    context, parserContext, SPS::BatchFilter::OITransparent,
                    preparedScene,
                    TechniqueIndex_OrderIndependentTransparency, "MainScene-PostGBuffer-OI");

                    // note; we use the main depth buffer for this call (not the duplicated buffer)
                OrderIndependentTransparency_Resolve(metalContext, parserContext, *transTargets, mainTargets.GetSRV(IMainTargets::MultisampledDepth));
            } else if (oiMode == OIMode::Stochastic) {
                StateSetChangeMarker marker(parserContext, GetStateSetResolvers()._depthOnly);
                SavedTargets savedTargets(metalContext);
                auto resetMarker = savedTargets.MakeResetMarker(metalContext);

                StochasticTransparencyOp stochTransOp(metalContext, parserContext);

                    // We do 2 passes through the ordered transparency geometry
                    //  1) we write the multi-sample occlusion buffer
                    //  2) we draw the pixels, out of order, using a forward-lighting approach  
                    //
                    // Note that we may need to modify this a little bit while rendering with
                    // MSAA
                stochTransOp.PrepareFirstPass(mainTargets.GetSRV(IMainTargets::MultisampledDepth));
                ExecuteScene(
                    context, parserContext, SPS::BatchFilter::OITransparent,
                    preparedScene,
                    TechniqueIndex_DepthOnly, "MainScene-PostGBuffer-OI");

                stochTransOp.PrepareSecondPass(mainTargets._msaaDepthBuffer);
                ExecuteScene(
                    context, parserContext, SPS::BatchFilter::OITransparent,
                    preparedScene,
                    TechniqueIndex_StochasticTransparency, "MainScene-PostGBuffer-OI-Res");

                resetMarker = SavedTargets::ResetMarker();  // back to normal targets now
                stochTransOp.Resolve();
            } else if (oiMode == OIMode::DepthWeighted) {
                StateSetChangeMarker marker(parserContext, GetStateSetResolvers()._depthOnly);
                SavedTargets savedTargets(metalContext);
                auto resetMarker = savedTargets.MakeResetMarker(metalContext);

                DepthWeightedTransparencyOp transOp(metalContext, parserContext);

                Metal::DepthStencilView dsv(savedTargets.GetDepthStencilView());
                transOp.PrepareFirstPass(&dsv);
                ExecuteScene(
                    context, parserContext, SPS::BatchFilter::OITransparent,
                    preparedScene,
                    TechniqueIndex_DepthWeightedTransparency, "MainScene-PostGBuffer-OI");

                resetMarker = SavedTargets::ResetMarker();  // back to normal targets now
                transOp.Resolve();
            } else if (oiMode == OIMode::Unordered) {
                StateSetChangeMarker marker(parserContext, GetStateSetResolvers()._forward);
                ExecuteScene(
                    context, parserContext, SPS::BatchFilter::OITransparent,
                    preparedScene,
                    TechniqueIndex_General, "MainScene-PostGBuffer-OI");
            }
        }
#endif

        //////////////////////////////////////////////////////////////////////////////////////////////////
        LightingParser_PostGBufferEffects(
            metalContext, parserContext, 
            mainTargets.GetSRV(IMainTargets::MultisampledDepth), 
            mainTargets.GetSRV(IMainTargets::GBufferNormals));

        for (auto p=parserContext._plugins.cbegin(); p!=parserContext._plugins.cend(); ++p)
            (*p)->OnPostSceneRender(metalContext, parserContext, SPS::BatchFilter::Transparent, TechniqueIndex_General);
    }

    class FrameBufferDescBox
    {
    public:
        class Desc
        {
        public:
            TextureSamples      _samples;
            bool                _precisionTargets;
            unsigned            _gbufferMode;
            Desc(const TextureSamples& samples, bool precisionTargets, unsigned gbufferMode) 
            {
                std::fill((char*)this, PtrAdd((char*)this, sizeof(*this)), '\0');
                _samples = samples;
                _precisionTargets = precisionTargets;
                _gbufferMode = gbufferMode;
            }
        };

        FrameBufferDesc _createGBuffer;
        std::vector<AttachmentDesc> _attachments;

        FrameBufferDescBox(const Desc& d);
    };

    // static const RenderPassFragment::SystemName s_renderToGBuffer = 1u;
    // static const RenderPassFragment::SystemName s_resolveLighting = 2u;

    FrameBufferDescBox::FrameBufferDescBox(const Desc& desc)
    {
        // This render pass will include just rendering to the gbuffer and doing the initial
        // lighting resolve.
        //
        // Typically after this we have a number of smaller render passes (such as rendering
        // transparent geometry, performing post processing, MSAA resolve, tone mapping, etc)
        //
        // We could attempt to combine more steps into this one render pass.. But it might become
        // awkward. For example, if we know we have only simple translucent geometry, we could
        // add in a subpass for rendering that geometry.
        //
        // We can elect to retain or discard the gbuffer contents after the lighting resolve. Frequently
        // the gbuffer contents are useful for various effects.

        // note --  All of these attachments must be marked with "ShaderResource"
        //          flags, because they will be used in the lighting pipeline later. However,
        //          this may have a consequence on the efficiency when writing to them.
        _attachments = std::vector<AttachmentDesc>{
            // Main multisampled depth stencil
            {   IMainTargets::MultisampledDepth, AttachmentDesc::DimensionsMode::OutputRelative, 1.f, 1.f, 0u,
                RenderCore::Format::R24G8_TYPELESS,
                TextureViewDesc::Aspect::DepthStencil,
                AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::ShaderResource | AttachmentDesc::Flags::DepthStencil },

                // Generally the deferred pixel shader will just copy information from the albedo
                // texture into the first deferred buffer. So the first deferred buffer should
                // have the same pixel format as much input textures.
                // Usually this is an 8 bit SRGB format, so the first deferred buffer should also
                // be 8 bit SRGB. So long as we don't do a lot of processing in the deferred pixel shader
                // that should be enough precision.
                //      .. however, it possible some clients might prefer 10 or 16 bit albedo textures
                //      In these cases, the first buffer should be a matching format.
            {   IMainTargets::GBufferDiffuse, AttachmentDesc::DimensionsMode::OutputRelative, 1.f, 1.f, 0u,
                (!desc._precisionTargets) ? Format::R8G8B8A8_UNORM_SRGB : Format::R32G32B32A32_FLOAT,
                (!desc._precisionTargets) ? TextureViewDesc::Aspect::ColorSRGB : TextureViewDesc::Aspect::ColorLinear,
                AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::ShaderResource | AttachmentDesc::Flags::RenderTarget },

            {   IMainTargets::GBufferNormals, AttachmentDesc::DimensionsMode::OutputRelative, 1.f, 1.f, 0u,
                (!desc._precisionTargets) ? Format::R8G8B8A8_SNORM : Format::R32G32B32A32_FLOAT,
                TextureViewDesc::Aspect::ColorLinear,
                AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::ShaderResource | AttachmentDesc::Flags::RenderTarget },

            {   IMainTargets::GBufferParameters, AttachmentDesc::DimensionsMode::OutputRelative, 1.f, 1.f, 0u,
                (!desc._precisionTargets) ? Format::R8G8B8A8_UNORM : Format::R32G32B32A32_FLOAT,
                TextureViewDesc::Aspect::ColorLinear,
                AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::ShaderResource | AttachmentDesc::Flags::RenderTarget }
        };

        AttachmentViewDesc gbufferAttaches[] = 
        {
            // Main multisampled depth stencil
            {   IMainTargets::MultisampledDepth, IMainTargets::MultisampledDepth, 
                TextureViewDesc(),
                AttachmentViewDesc::LoadStore::Clear_ClearStencil, AttachmentViewDesc::LoadStore::Retain },

                // Generally the deferred pixel shader will just copy information from the albedo
                // texture into the first deferred buffer. So the first deferred buffer should
                // have the same pixel format as much input textures.
                // Usually this is an 8 bit SRGB format, so the first deferred buffer should also
                // be 8 bit SRGB. So long as we don't do a lot of processing in the deferred pixel shader
                // that should be enough precision.
                //      .. however, it possible some clients might prefer 10 or 16 bit albedo textures
                //      In these cases, the first buffer should be a matching format.
            {   IMainTargets::GBufferDiffuse, IMainTargets::GBufferDiffuse, 
                TextureViewDesc(),
                AttachmentViewDesc::LoadStore::DontCare, AttachmentViewDesc::LoadStore::Retain },

            {   IMainTargets::GBufferNormals, IMainTargets::GBufferNormals, 
                TextureViewDesc(),
                AttachmentViewDesc::LoadStore::DontCare, AttachmentViewDesc::LoadStore::Retain },

            {   IMainTargets::GBufferParameters, IMainTargets::GBufferParameters, 
                TextureViewDesc(),
                AttachmentViewDesc::LoadStore::DontCare, AttachmentViewDesc::LoadStore::Retain }
        };

        if (desc._gbufferMode == 1) {

            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            _createGBuffer = FrameBufferDesc(
                {
                    // render first to the gbuffer
					SubpassDesc{
						{IMainTargets::GBufferDiffuse, IMainTargets::GBufferNormals, IMainTargets::GBufferParameters},
						IMainTargets::MultisampledDepth}
                },
				std::vector<AttachmentViewDesc>{&gbufferAttaches[0], &gbufferAttaches[4]});
            
        } else {

            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            _createGBuffer = FrameBufferDesc(
                {
                    // render first to the gbuffer
					SubpassDesc{{IMainTargets::GBufferDiffuse, IMainTargets::GBufferNormals}, IMainTargets::MultisampledDepth}
                },
				std::vector<AttachmentViewDesc>{&gbufferAttaches[0], &gbufferAttaches[3]});
        }
    }

    class MainTargets : public IMainTargets
    {
    public:
        unsigned                        GetGBufferType() const;
        RenderCore::TextureSamples      GetSampling() const;
        const RenderingQualitySettings& GetQualitySettings() const;
        VectorPattern<unsigned, 2>      GetDimensions() const;
        const SRV&                      GetSRV(Name) const;
        const SRV&                      GetSRV(Name, Name, const TextureViewDesc&) const;
        bool                            HasSRV(Name) const;

        MainTargets(
            Techniques::ParsingContext& parsingContext,
            const RenderingQualitySettings& qualSettings,
            unsigned gbufferType);
        ~MainTargets();

    private:
        Techniques::ParsingContext* _parsingContext;
        RenderingQualitySettings    _qualSettings;
        unsigned                    _gbufferType;
    };

    unsigned                        MainTargets::GetGBufferType() const
    {
        return _gbufferType;
    }

    RenderCore::TextureSamples      MainTargets::GetSampling() const
    {
        return TextureSamples::Create(
            uint8(std::max(_qualSettings._samplingCount, 1u)), uint8(_qualSettings._samplingQuality));
    }

    const RenderingQualitySettings& MainTargets::GetQualitySettings() const
    {
        return _qualSettings;
    }

    VectorPattern<unsigned, 2>      MainTargets::GetDimensions() const
    {
        return _qualSettings._dimensions;
    }

    auto  MainTargets::GetSRV(Name name) const -> const SRV&
    {
        auto result = _parsingContext->GetNamedResources().GetSRV(name);
        assert(result);
        return *result;
    }

    bool MainTargets::HasSRV(Name name) const
    {
        return _parsingContext->GetNamedResources().GetSRV(name) != nullptr;
    }

    auto MainTargets::GetSRV(Name viewName, Name resName, const TextureViewDesc& viewWindow) const -> const SRV&
    {
        auto result = _parsingContext->GetNamedResources().GetSRV(viewName, resName, viewWindow);
        assert(result);
        return *result;
    }

    MainTargets::MainTargets(
        Techniques::ParsingContext& parsingContext,
        const RenderingQualitySettings& qualSettings,
        unsigned gbufferType)
    : _parsingContext(&parsingContext)
    , _qualSettings(qualSettings)
    , _gbufferType(gbufferType)
    {}

    MainTargets::~MainTargets() {}

    void LightingParser_MainScene(
        IThreadContext& context,
        Metal::DeviceContext& metalContext, 
        LightingParserContext& parserContext,
        PreparedScene& preparedScene,
        MainTargets& mainTargets,
        const RenderingQualitySettings& qualitySettings)
    {
            ////////////////////////////////////////////////////////////////////
                //      1. Resolve lighting
                //          -> outputs  1. postLightingResolveTexture (HDR colour)
                //                      2. depth buffer (NDC depths)
                //                      3. secondary depth buffer (contains per 
                //                          pixel/per sample stencil)
                //
            ////////............................................................
                //      2. Resolve MSAA (if needed)
                //          -> outputs  1. single sample HDR colour
                //                      2. single sample depth buffer
                //
            ////////............................................................
                //      3. Post processing operations
                //
            ////////............................................................
                //      4. Resolve HDR (tone mapping, bloom, etc)
                //          -> outputs  1. LDR SRGB colour
                //
            ////////............................................................
                //      5. Debugging / overlays
                //
            ////////////////////////////////////////////////////////////////////

        const bool precisionTargets = Tweakable("PrecisionTargets", false);

        if (qualitySettings._lightingModel == RenderingQualitySettings::LightingModel::Deferred) {

                //
            //////////////////////////////////////////////////////////////////////////////////////
                //      Get the gbuffer render targets for this frame
                //

            auto& globalState = parserContext.GetTechniqueContext()._globalEnvironmentState;
            globalState.SetParameter((const utf8*)"GBUFFER_TYPE", mainTargets.GetGBufferType());

                //
            //////////////////////////////////////////////////////////////////////////////////////
                //      Bind the gbuffer, begin the render pass
                //

            auto& fbDescBox = ConsoleRig::FindCachedBox2<FrameBufferDescBox>(
                mainTargets.GetSampling(), precisionTargets, mainTargets.GetGBufferType());

            parserContext.GetNamedResources().DefineAttachments(MakeIteratorRange(fbDescBox._attachments));

            ReturnToSteadyState(metalContext);
            StateSetChangeMarker marker(parserContext, GetStateSetResolvers()._deferred);

            CATCH_ASSETS_BEGIN {
                Techniques::RenderPassInstance rpi(
                    metalContext,
                    fbDescBox._createGBuffer,
                    0u, parserContext.GetNamedResources(),
                    Techniques::RenderPassBeginDesc{{RenderCore::MakeClearValue(1.f, 0)}});
                metalContext.Bind(Metal::ViewportDesc(0.f, 0.f, (float)qualitySettings._dimensions[0], (float)qualitySettings._dimensions[1]));

                ExecuteScene(
                    context, parserContext, SPS::BatchFilter::General,
                    preparedScene,
                    TechniqueIndex_Deferred, "MainScene-OpaqueGBuffer");
            } CATCH_ASSETS_END(parserContext)

            for (auto p=parserContext._plugins.cbegin(); p!=parserContext._plugins.cend(); ++p)
                (*p)->OnPostSceneRender(metalContext, parserContext, SPS::BatchFilter::General, TechniqueIndex_Deferred);

                //
            //////////////////////////////////////////////////////////////////////////////////////
                //      Now resolve lighting
                //

            CATCH_ASSETS_BEGIN {
                LightingParser_ResolveGBuffer(context, parserContext, mainTargets);
            } CATCH_ASSETS_END(parserContext)

                // Post lighting resolve operations... (must rebind the depth buffer)
            metalContext.Bind(Techniques::CommonResources()._dssReadOnly);

            CATCH_ASSETS_BEGIN
                LightingParser_DeferredPostGBuffer(context, metalContext, parserContext, preparedScene, mainTargets);
            CATCH_ASSETS_END(parserContext)

        } else if (qualitySettings._lightingModel == RenderingQualitySettings::LightingModel::Forward) {

#if 0   // platformtemp
            auto& mainTargets = Techniques::FindCachedBox2<ForwardTargetsBox>(
                unsigned(mainViewport.Width), unsigned(mainViewport.Height),
                FormatStack(NativeFormat(DXGI_FORMAT_R24G8_TYPELESS), 
                            NativeFormat(DXGI_FORMAT_R24_UNORM_X8_TYPELESS), 
                            NativeFormat(DXGI_FORMAT_D24_UNORM_S8_UINT)),
                sampling);

            metalContext.Clear(mainTargets._msaaDepthBuffer, 1.f, 0);
            metalContext.Bind(
                MakeResourceList(lightingResTargets._lightingResolveRTV),
                &mainTargets._msaaDepthBuffer);

            if (!parserContext._preparedDMShadows.empty())
                BindShadowsForForwardResolve(metalContext, parserContext, parserContext._preparedDMShadows[0].second);

            ForwardLightingModel_Render(context, metalContext, parserContext, preparedScene, mainTargets);
#endif

        }

        {
            GPUAnnotation anno(context, "Resolve-MSAA-HDR");

            auto postLightingResolve = IMainTargets::LightResolve;
            if (!mainTargets.HasSRV(postLightingResolve))
                return;

#if 0   // platformtemp
                //
                //      Post lighting resolve operations...
                //          we must bind the depth buffer to whatever
                //          buffer contained the correct depth information from the
                //          previous rendering (might be the default depth buffer, or might
                //          not be)
                //
            if (qualitySettings._samplingCount > 1) {
                auto inputTextureDesc = Metal::ExtractDesc(postLightingResolveTexture->GetUnderlying());
				auto& msaaResolveRes = Techniques::FindCachedBox2<FinalResolveResources>(
					inputTextureDesc._textureDesc._width, inputTextureDesc._textureDesc._height, inputTextureDesc._textureDesc._format);
                LightingParser_ResolveMSAA(
                    metalContext, parserContext,
                    *msaaResolveRes._postMsaaResolveTexture->GetUnderlying(),
                    *postLightingResolveTexture->GetUnderlying(),
					inputTextureDesc._textureDesc._format);

                    // todo -- also resolve the depth buffer...!
                    //      here; we switch the active textures to the msaa resolved textures
                postLightingResolve = IMainTargets::PostMSAALightResolve;
            }

            metalContext.Bind(MakeResourceList(postLightingResolveRTV), nullptr);       // we don't have a single-sample depths target at this time (only multisample)
            LightingParser_PostProcess(metalContext, parserContext);
#endif

            auto toneMapSettings = parserContext.GetSceneParser()->GetToneMapSettings();
            LuminanceResult luminanceResult;
            if (parserContext.GetSceneParser()->GetToneMapSettings()._flags & ToneMapSettings::Flags::EnableToneMap) {
                    //  (must resolve luminance early, because we use it during the MSAA resolve)
                luminanceResult = ToneMap_SampleLuminance(
                    context, parserContext, toneMapSettings, 
                    mainTargets.GetSRV(postLightingResolve));
            }

                //  Write final colour to output texture
                //  We have to be careful about whether "SRGB" is enabled
                //  on the back buffer we're writing to. Depending on the
                //  tone mapping method, sometimes we want the SRGB conversion,
                //  other times we don't (because some tone map operations produce
                //  SRGB results, others give linear results)

            const bool hardwareSRGBDisabled = Tweakable("Tonemap_DisableHardwareSRGB", true);
            FrameBufferDesc applyToneMapping(
				{ SubpassDesc{{IMainTargets::PresentationTarget_ToneMapWrite}} },
                {
                    // We want to reuse the presentation target texture, except with the format modified for SRGB/Linear
                    {   IMainTargets::PresentationTarget, IMainTargets::PresentationTarget_ToneMapWrite,
                        {{ hardwareSRGBDisabled ? TextureViewDesc::ColorLinear : TextureViewDesc::ColorSRGB }},
                        LoadStore::DontCare, LoadStore::Retain }
                });
            
            ToneMap_Execute(
                context, parserContext, luminanceResult, toneMapSettings, 
                applyToneMapping,
                mainTargets.GetSRV(postLightingResolve));
        }
    }

    static const utf8* StringShadowCascadeMode = u("SHADOW_CASCADE_MODE");
    static const utf8* StringShadowEnableNearCascade = u("SHADOW_ENABLE_NEAR_CASCADE");

    PreparedDMShadowFrustum LightingParser_PrepareDMShadow(
        IThreadContext& context,
        Metal::DeviceContext& metalContext, LightingParserContext& parserContext, 
        PreparedScene& preparedScene,
        IMainTargets& mainTargets,
        const ShadowProjectionDesc& frustum,
        unsigned shadowFrustumIndex)
    {
        auto projectionCount = std::min(frustum._projections.Count(), MaxShadowTexturesPerLight);
        if (!projectionCount)
            return PreparedDMShadowFrustum();

        SPS sceneParseSettings(SPS::BatchFilter::DMShadows, ~SPS::Toggles::BitField(0), shadowFrustumIndex);
        if (!BatchHasContent(parserContext, sceneParseSettings))
            return PreparedDMShadowFrustum();

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
        parserContext.GetTechniqueContext()._runtimeState.SetParameter(
            StringShadowCascadeMode, 
            preparedResult._mode == ShadowProjectionDesc::Projections::Mode::Ortho?2:1);
        parserContext.GetTechniqueContext()._runtimeState.SetParameter(
            StringShadowEnableNearCascade,  preparedResult._enableNearCascade?1:0);

        auto cleanup = MakeAutoCleanup(
            [&parserContext]() {
                parserContext.GetTechniqueContext()._runtimeState.SetParameter(StringShadowCascadeMode, 0);
                parserContext.GetTechniqueContext()._runtimeState.SetParameter(StringShadowEnableNearCascade, 0);
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

        AttachmentDesc attachments[] =
            {
                {   IMainTargets::ShadowDepthMap + shadowFrustumIndex, 
                    AttachmentDesc::DimensionsMode::Absolute, float(frustum._width), float(frustum._height),
                    frustum._projections.Count(),
                    AsTypelessFormat(frustum._format),
                    TextureViewDesc::DepthStencil,
                    AttachmentDesc::Flags::ShaderResource | AttachmentDesc::Flags::DepthStencil }
            };

		parserContext.GetNamedResources().DefineAttachments(MakeIteratorRange(attachments));

        FrameBufferDesc resolveLighting(
			{ SubpassDesc{{}, IMainTargets::ShadowDepthMap + shadowFrustumIndex} },
            {
                {   IMainTargets::ShadowDepthMap + shadowFrustumIndex, IMainTargets::ShadowDepthMap + shadowFrustumIndex, 
                    TextureViewDesc(),
                    AttachmentViewDesc::LoadStore::Clear, AttachmentViewDesc::LoadStore::Retain }
            });

        Techniques::RenderPassInstance rpi(
            metalContext,
            resolveLighting,
            0u, parserContext.GetNamedResources(),
            Techniques::RenderPassBeginDesc{{MakeClearValue(1.f, 0x0)}});

        preparedResult._shadowTextureName = IMainTargets::ShadowDepthMap + shadowFrustumIndex;

            /////////////////////////////////////////////

        Float4x4 savedWorldToProjection = parserContext.GetProjectionDesc()._worldToProjection;
        parserContext.GetProjectionDesc()._worldToProjection = frustum._worldToClip;
        auto cleanup2 = MakeAutoCleanup(
            [&parserContext, &savedWorldToProjection]() {
                parserContext.GetProjectionDesc()._worldToProjection = savedWorldToProjection;
            });

            /////////////////////////////////////////////

        StateSetChangeMarker stateMarker(parserContext, resources._stateResolver);
        metalContext.Bind(resources._rasterizerState);
        ExecuteScene(
            context, parserContext, sceneParseSettings,
            preparedScene,
            TechniqueIndex_ShadowGen, "ShadowGen-Prepare");

        for (auto p=parserContext._plugins.cbegin(); p!=parserContext._plugins.cend(); ++p)
            (*p)->OnPostSceneRender(metalContext, parserContext, sceneParseSettings, TechniqueIndex_ShadowGen);
        
        return std::move(preparedResult);
    }

    PreparedRTShadowFrustum LightingParser_PrepareRTShadow(
        IThreadContext& context,
        Metal::DeviceContext& metalContext, LightingParserContext& parserContext,
        PreparedScene& preparedScene, const ShadowProjectionDesc& frustum,
        unsigned shadowFrustumIndex)
    {
        return PrepareRTShadows(context, metalContext, parserContext, preparedScene, frustum, shadowFrustumIndex);
    }

    void LightingParser_PrepareShadows(
        IThreadContext& context, Metal::DeviceContext& metalContext, 
        LightingParserContext& parserContext, PreparedScene& preparedScene,
        IMainTargets& mainTargets)
    {
        auto* scene = parserContext.GetSceneParser();
        if (!scene) {
            parserContext._preparedDMShadows.clear();
            parserContext._preparedRTShadows.clear();
            return;
        }

        GPUAnnotation anno(context, "Prepare-Shadows");

            // todo --  we should be using a temporary frame heap for this vector
        auto shadowFrustumCount = scene->GetShadowProjectionCount();
        parserContext._preparedDMShadows.reserve(shadowFrustumCount);

        for (unsigned c=0; c<shadowFrustumCount; ++c) {
            auto frustum = scene->GetShadowProjectionDesc(c, parserContext.GetProjectionDesc());
            CATCH_ASSETS_BEGIN
                if (frustum._resolveType == ShadowProjectionDesc::ResolveType::DepthTexture) {

                    auto shadow = LightingParser_PrepareDMShadow(context, metalContext, parserContext, preparedScene, mainTargets, frustum, c);
                    if (shadow.IsReady())
                        parserContext._preparedDMShadows.push_back(std::make_pair(frustum._lightId, std::move(shadow)));

                } else if (frustum._resolveType == ShadowProjectionDesc::ResolveType::RayTraced) {

                    auto shadow = LightingParser_PrepareRTShadow(context, metalContext, parserContext, preparedScene, frustum, c);
                    if (shadow.IsReady())
                        parserContext._preparedRTShadows.push_back(std::make_pair(frustum._lightId, std::move(shadow)));

                }
            CATCH_ASSETS_END(parserContext)
        }
    }

    void LightingParser_InitBasicLightEnv(  
        Metal::DeviceContext& context,
        LightingParserContext& parserContext,
        ISceneParser& sceneParser);

    AttachedSceneMarker LightingParser_SetupScene(
        Metal::DeviceContext& context, 
        LightingParserContext& parserContext,
        ISceneParser* sceneParser,
        unsigned samplingPassIndex, unsigned samplingPassCount)
    {
        struct GlobalCBuffer
        {
            float _time; unsigned _samplingPassIndex; 
            unsigned _samplingPassCount; unsigned _dummy;
        } time { 0.f, samplingPassIndex, samplingPassCount, 0 };
        if (sceneParser)
            time._time = sceneParser->GetTimeValue();
        parserContext.SetGlobalCB(
            context, Techniques::TechniqueContext::CB_GlobalState,
            &time, sizeof(time));

        if (sceneParser)
            LightingParser_InitBasicLightEnv(context, parserContext, *sceneParser);

        auto& metricsBox = ConsoleRig::FindCachedBox2<MetricsBox>();
        context.ClearUInt(metricsBox._metricsBufferUAV, { 0,0,0,0 });
        parserContext.SetMetricsBox(&metricsBox);

        return parserContext.SetSceneParser(sceneParser);
    }

    void LightingParser_ExecuteScene(
        RenderCore::IThreadContext& context, 
        LightingParserContext& parserContext,
        ISceneParser& scene,
        const RenderCore::Techniques::CameraDesc& camera,
        const RenderingQualitySettings& qualitySettings)
    {
        auto metalContext = Metal::DeviceContext::Get(context);
        auto marker = LightingParser_SetupScene(*metalContext.get(), parserContext, &scene);
        LightingParser_SetGlobalTransform(
            *metalContext.get(), parserContext, 
            BuildProjectionDesc(camera, qualitySettings._dimensions));
        scene.PrepareScene(context, parserContext, marker.GetPreparedScene());

        // Throw in a "frame priority barrier" here, right after the prepare scene. This will
        // force all uploads started during PrepareScene to be completed when we next call
        // bufferUploads.Update(). Normally we want a gap between FramePriority_Barrier() and
        // Update(), because this gives some time for the background thread to run.
        GetBufferUploads().FramePriority_Barrier();
        LightingParser_ExecuteScene(context, parserContext, qualitySettings, marker.GetPreparedScene());
    }

    void LightingParser_ExecuteScene(
        RenderCore::IThreadContext& context, 
        LightingParserContext& parserContext,
        const RenderingQualitySettings& qualitySettings,
        PreparedScene& preparedScene)
    {
        auto metalContext = Metal::DeviceContext::Get(context);
        CATCH_ASSETS_BEGIN
            ReturnToSteadyState(*metalContext);
            SetFrameGlobalStates(*metalContext);
        CATCH_ASSETS_END(parserContext)

        const bool enableParametersBuffer = Tweakable("EnableParametersBuffer", true);
        MainTargets mainTargets(parserContext, qualitySettings, enableParametersBuffer?1:2);

        {
            GPUAnnotation anno(context, "Prepare");
            for (auto i=parserContext._plugins.cbegin(); i!=parserContext._plugins.cend(); ++i) {
                CATCH_ASSETS_BEGIN
                    (*i)->OnPreScenePrepare(context, parserContext, preparedScene);
                CATCH_ASSETS_END(parserContext)
            }

            LightingParser_PrepareShadows(context, *metalContext, parserContext, preparedScene, mainTargets);
        }

        GetBufferUploads().Update(context, true);

        CATCH_ASSETS_BEGIN
            LightingParser_MainScene(context, *metalContext, parserContext, preparedScene, mainTargets, qualitySettings);
        CATCH_ASSETS_END(parserContext)
    }

    void LightingParserContext::SetMetricsBox(MetricsBox* box)
    {
        _metricsBox = box;
    }

    void LightingParserContext::Reset()
    {
        _preparedDMShadows.clear();
        _preparedRTShadows.clear();
    }

    LightingParserContext::LightingParserContext(
        const Techniques::TechniqueContext& techniqueContext, 
        Techniques::AttachmentPool* namedResources)
    : ParsingContext(techniqueContext, namedResources)
    , _sceneParser(nullptr)
    {
        _metricsBox = nullptr;
    }

    LightingParserContext::~LightingParserContext() {}

    AttachedSceneMarker LightingParserContext::SetSceneParser(ISceneParser* sceneParser)
    {
        assert(_sceneParser == nullptr);
        _sceneParser = sceneParser;
        return AttachedSceneMarker(*this);
    }

    RenderingQualitySettings::RenderingQualitySettings()
    {
        _dimensions = {0,0};
        _lightingModel = LightingModel::Deferred;
        _samplingCount = _samplingQuality = 0;
    }

    RenderingQualitySettings::RenderingQualitySettings(
        VectorPattern<unsigned, 2> dimensions, 
        LightingModel lightingModel, unsigned samplingCount, unsigned samplingQuality)
    : _dimensions(dimensions), _lightingModel(lightingModel), _samplingCount(samplingCount), _samplingQuality(samplingQuality)
    {}


    ISceneParser::~ISceneParser() {}

}

