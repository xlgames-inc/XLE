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
#include "Sky.h"
#include "SunFlare.h"
#include "Rain.h"
#include "Noise.h"
#include "RayTracedShadows.h"

#include "../RenderCore/RenderUtils.h"
#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/RenderStateResolver.h"
#include "../RenderCore/Assets/DeferredShaderResource.h"
#include "../RenderCore/Metal/GPUProfiler.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/Shader.h"
#include "../ConsoleRig/Console.h"
#include "../Math/Transformations.h"
#include "../Math/ProjectionMath.h"
#include "../Utility/FunctionUtils.h"

#include "../RenderCore/DX11/Metal/DX11Utils.h"

namespace SceneEngine
{
    using namespace RenderCore;
    using SPS = SceneParseSettings;

    DeepOceanSimSettings GlobalOceanSettings; 
    OceanLightingSettings GlobalOceanLightingSettings; 

    void LightingParser_ResolveGBuffer( 
        Metal::DeviceContext& context,
        LightingParserContext& parserContext,
        MainTargetsBox& mainTargets,
        LightingResolveTextureBox& lightingResTargets);

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

    StateSetResolvers& GetStateSetResolvers() { return Techniques::FindCachedBox2<StateSetResolvers>(); }

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
            samplerClamp(Metal::FilterMode::Trilinear, Metal::AddressMode::Clamp, Metal::AddressMode::Clamp, Metal::AddressMode::Clamp), 
            samplerAnisotrophic(Metal::FilterMode::Anisotropic),
            samplerPoint(Metal::FilterMode::Point, Metal::AddressMode::Clamp, Metal::AddressMode::Clamp, Metal::AddressMode::Clamp),
            samplerWrapU(Metal::FilterMode::Trilinear, Metal::AddressMode::Wrap, Metal::AddressMode::Clamp);
        context.BindPS(RenderCore::MakeResourceList(samplerDefault, samplerClamp, samplerAnisotrophic, samplerPoint));
        context.BindVS(RenderCore::MakeResourceList(samplerDefault, samplerClamp, samplerAnisotrophic, samplerPoint));
        context.BindPS(RenderCore::MakeResourceList(6, samplerWrapU));

        const auto& normalsFittingResource = ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("game/xleres/DefaultResources/normalsfitting.dds:LT").GetShaderResource();
		const auto& distintColors = ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("game/xleres/DefaultResources/distinctcolors.dds:T").GetShaderResource();
        context.BindPS(RenderCore::MakeResourceList(14, normalsFittingResource, distintColors));
        context.BindCS(RenderCore::MakeResourceList(14, normalsFittingResource));

            // perlin noise resources in standard slots
        auto& perlinNoiseRes = Techniques::FindCachedBox2<PerlinNoiseResources>();
        context.BindPS(MakeResourceList(12, perlinNoiseRes._gradShaderResource, perlinNoiseRes._permShaderResource));

            // procedural scratch texture for scratches test
        // context.BindPS(MakeResourceList(18, Assets::GetAssetDep<Metal::DeferredShaderResource>("game/xleres/scratchnorm.dds:L").GetShaderResource()));
        // context.BindPS(MakeResourceList(19, Assets::GetAssetDep<Metal::DeferredShaderResource>("game/xleres/scratchocc.dds:L").GetShaderResource()));
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
        context.Bind(Metal::Topology::TriangleList);
        context.BindVS(RenderCore::MakeResourceList(Metal::ConstantBuffer(), Metal::ConstantBuffer(), Metal::ConstantBuffer(), Metal::ConstantBuffer(), Metal::ConstantBuffer()));
        context.BindPS(RenderCore::MakeResourceList(Metal::ConstantBuffer(), Metal::ConstantBuffer(), Metal::ConstantBuffer(), Metal::ConstantBuffer(), Metal::ConstantBuffer()));
        context.Unbind<Metal::GeometryShader>();
    }

    static void ClearDeferredBuffers(Metal::DeviceContext& context, MainTargetsBox& mainTargets)
    {
        if (mainTargets._gbufferRTVs[0].GetUnderlying())
            context.Clear(mainTargets._gbufferRTVs[0], Float4(0.33f, 0.33f, 0.33f, 1.f));
        if (mainTargets._gbufferRTVs[1].GetUnderlying())
            context.Clear(mainTargets._gbufferRTVs[1], Float4(0.f, 0.f, 0.f, 0.f));
        if (mainTargets._gbufferRTVs[2].GetUnderlying())
            context.Clear(mainTargets._gbufferRTVs[2], Float4(0.f, 0.f, 0.f, 0.f));
        context.Clear(mainTargets._msaaDepthBuffer, 1.f, 0);
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
        UInt2 viewportDims, const Float4x4* specialProjectionMatrix)
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

    void LightingParser_LateGBufferRender(
        Metal::DeviceContext& context, 
        LightingParserContext& parserContext,
        MainTargetsBox& mainTargets)
    {
        Metal::GPUProfiler::DebugAnnotation anno(context, L"LateGBuffer");

        #if defined(_DEBUG)
            auto& saveGBuffer = Tweakable("SaveGBuffer", false);
            if (saveGBuffer) {
                SaveGBuffer(context, mainTargets);
                saveGBuffer = false;
            }
        #endif
    }

    class FinalResolveResources
    {
    public:
        class Desc
        {
        public:
            Desc(unsigned width, unsigned height, Metal::NativeFormat::Enum format)
                : _width(width), _height(height), _format(format) {}

            unsigned            _width, _height;
            Metal::NativeFormat::Enum  _format;
        };

        ResourcePtr         _postMsaaResolveTexture;
        Metal::RenderTargetView    _postMsaaResolveTarget;
        Metal::ShaderResourceView  _postMsaaResolveSRV;

        FinalResolveResources(const Desc& desc);
    };

    FinalResolveResources::FinalResolveResources(const Desc& desc)
    {
        using namespace BufferUploads;
        auto bufferUploadsDesc = BuildRenderTargetDesc(
            BindFlag::ShaderResource|BindFlag::RenderTarget,
            BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, Metal::AsDXGIFormat(desc._format)),
            "FinalResolve");
        auto postMsaaResolveTexture = CreateResourceImmediate(bufferUploadsDesc);

        Metal::RenderTargetView postMsaaResolveTarget(postMsaaResolveTexture.get());
        Metal::ShaderResourceView postMsaaResolveSRV(postMsaaResolveTexture.get());

        _postMsaaResolveTexture = std::move(postMsaaResolveTexture);
        _postMsaaResolveTarget = std::move(postMsaaResolveTarget);
        _postMsaaResolveSRV = std::move(postMsaaResolveSRV);
    }

    void LightingParser_ResolveMSAA(
        Metal::DeviceContext& context, 
        LightingParserContext& parserContext,
        ID3D::Resource* destinationTexture,
        ID3D::Resource* sourceTexture,
        Metal::NativeFormat::Enum resolveFormat)
    {
            // todo -- support custom resolve (tone-map aware)
        context.GetUnderlying()->ResolveSubresource(
            destinationTexture, D3D11CalcSubresource(0,0,0),
            sourceTexture, D3D11CalcSubresource(0,0,0),
            Metal::AsDXGIFormat(resolveFormat));
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
        Metal::ShaderResourceView& depthsSRV)
    {
        Metal::GPUProfiler::DebugAnnotation anno(context, L"PreTranslucency");

            // note --  these things can be executed by the scene parser? Are they better
            //          off handled by the scene parser, or the lighting parser?
        if (Tweakable("OceanDoSimulation", false)) {
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
        Metal::ShaderResourceView& depthsSRV,
        Metal::ShaderResourceView& normalsSRV)
    {
        Metal::GPUProfiler::DebugAnnotation anno(context, L"PostGBuffer");
        
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

    void LightingParser_Overlays(   Metal::DeviceContext* context,
                                    LightingParserContext& parserContext)
    {
        Metal::GPUProfiler::DebugAnnotation anno(*context, L"Overlays");

        Metal::ViewportDesc mainViewportDesc(*context);
        auto& refractionBox = Techniques::FindCachedBox2<RefractionsBuffer>(unsigned(mainViewportDesc.Width/2), unsigned(mainViewportDesc.Height/2));
        refractionBox.Build(*context, parserContext, 4.f);
        context->BindPS(MakeResourceList(12, refractionBox.GetSRV()));

        for (auto i=parserContext._pendingOverlays.cbegin(); i!=parserContext._pendingOverlays.cend(); ++i) {
            CATCH_ASSETS_BEGIN
                (*i)(*context, parserContext);
            CATCH_ASSETS_END(parserContext)
        }
                    
        if (Tweakable("FFTDebugging", false)) {
            FFT_DoDebugging(context);
        }

        if (Tweakable("MetricsRender", false) && parserContext.GetMetricsBox()) {
            CATCH_ASSETS_BEGIN

                using namespace RenderCore;
                using namespace RenderCore::Metal;
                auto& metricsShader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                        "game/xleres/utility/metricsrender.vsh:main:vs_*", 
                        "game/xleres/utility/metricsrender.gsh:main:gs_*",
                        "game/xleres/utility/metricsrender.psh:main:ps_*",
                        "");
                context->Bind(metricsShader);
                context->BindPS(MakeResourceList(
                    3, ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("game/xleres/DefaultResources/metricsdigits.dds:T").GetShaderResource()));
                context->Bind(BlendState(BlendOp::Add, Blend::One, Blend::InvSrcAlpha));
                context->Bind(DepthStencilState(false));
                context->BindVS(MakeResourceList(parserContext.GetMetricsBox()->_metricsBufferSRV));
                unsigned dimensions[4] = { unsigned(mainViewportDesc.Width), unsigned(mainViewportDesc.Height), 0, 0 };
                context->BindVS(MakeResourceList(ConstantBuffer(dimensions, sizeof(dimensions))));
                context->BindGS(MakeResourceList(ConstantBuffer(dimensions, sizeof(dimensions))));
                SetupVertexGeneratorShader(*context);
                context->Bind(Topology::PointList);
                context->Draw(9);

                context->UnbindPS<ShaderResourceView>(3, 1);
                context->UnbindVS<ShaderResourceView>(0, 1);

            CATCH_ASSETS_END(parserContext)
        }
    }

    void LightingParser_PrepareShadows(
        IThreadContext& context, Metal::DeviceContext& metalContext, 
        LightingParserContext& parserContext, PreparedScene& preparedScene);

    static void ExecuteScene(
        IThreadContext& context,
        LightingParserContext& parserContext,
        const SPS& parseSettings,
        PreparedScene& preparedScene,
        unsigned techniqueIndex,
        const wchar_t* name)
    {
        CATCH_ASSETS_BEGIN
            #if defined(GPUANNOTATIONS_ENABLE)
                Metal::GPUProfiler::DebugAnnotation anno(*RenderCore::Metal::DeviceContext::Get(context), name);
            #endif
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
        ForwardTargetsBox& targetsBox,
        unsigned sampleCount)
    {
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
                TechniqueIndex_DepthOnly, L"MainScene-DepthOnly");
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
            TechniqueIndex_General, L"MainScene-General");

            /////

        Metal::ShaderResourceView duplicatedDepthBuffer;
        TransparencyTargetsBox* oiTransTargets = nullptr;
        if (useOrderIndependentTransparency) {
            duplicatedDepthBuffer = 
                BuildDuplicatedDepthBuffer(&metalContext, targetsBox._msaaDepthBufferTexture.get());
                
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
                TechniqueIndex_OrderIndependentTransparency, L"MainScene-OITrans");
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
    }

    static void LightingParser_DeferredPostGBuffer(
        IThreadContext& context,
        Metal::DeviceContext& metalContext, 
        LightingParserContext& parserContext,
        PreparedScene& preparedScene,
        MainTargetsBox& mainTargets)
    {
        //////////////////////////////////////////////////////////////////////////////////////////////////
            //  Render translucent objects (etc)
            //  everything after the gbuffer resolve
        LightingParser_PreTranslucency(
            metalContext, parserContext, 
            mainTargets._msaaDepthBufferSRV);

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
        const auto enabledSortedTrans = 
            Tweakable("UseOITrans", false)
            && (mainTargets._desc._sampling._sampleCount <= 1)
            && hasOITrans;

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
        if (enabledSortedTrans && Tweakable("TransPrePass", false)) {
            StateSetChangeMarker marker(parserContext, GetStateSetResolvers()._depthOnly);
            ExecuteScene(
                context, parserContext, SPS::BatchFilter::TransparentPreDepth,
                preparedScene,
                TechniqueIndex_DepthOnly, L"MainScene-TransPreDepth");
        }

        if (BatchHasContent(parserContext, SPS::BatchFilter::Transparent)) {
            StateSetChangeMarker marker(parserContext, GetStateSetResolvers()._forward);
            ExecuteScene(
                context, parserContext, SPS::BatchFilter::Transparent,
                preparedScene,
                TechniqueIndex_General, L"MainScene-PostGBuffer");
        }
        
        if (hasOITrans) {
            StateSetChangeMarker marker(parserContext, GetStateSetResolvers()._depthOnly);
            if (enabledSortedTrans) {
                auto duplicatedDepthBuffer = BuildDuplicatedDepthBuffer(&metalContext, mainTargets._msaaDepthBufferTexture.get());
                auto* transTargets = OrderIndependentTransparency_Prepare(metalContext, parserContext, duplicatedDepthBuffer);

                ExecuteScene(
                    context, parserContext, SPS::BatchFilter::OITransparent,
                    preparedScene,
                    TechniqueIndex_OrderIndependentTransparency, L"MainScene-PostGBuffer-OI");

                    // note; we use the main depth buffer for this call (not the duplicated buffer)
                OrderIndependentTransparency_Resolve(metalContext, parserContext, *transTargets, mainTargets._msaaDepthBufferSRV);
            } else if (Tweakable("UseStochasticTrans", true)) {
                SavedTargets savedTargets(metalContext);
                auto resetMarker = savedTargets.MakeResetMarker(metalContext);

                StochasticTransparencyOp stochTransOp(metalContext, parserContext);

                    // We do 2 passes through the ordered transparency geometry
                    //  1) we write the multi-sample occlusion buffer
                    //  2) we draw the pixels, out of order, using a forward-lighting approach  
                    //
                    // Note that we may need to modify this a little bit while rendering with
                    // MSAA
                stochTransOp.PrepareFirstPass(mainTargets._msaaDepthBufferSRV);
                ExecuteScene(
                    context, parserContext, SPS::BatchFilter::OITransparent,
                    preparedScene,
                    TechniqueIndex_DepthOnly, L"MainScene-PostGBuffer-OI");

                stochTransOp.PrepareSecondPass(mainTargets._msaaDepthBuffer);
                ExecuteScene(
                    context, parserContext, SPS::BatchFilter::OITransparent,
                    preparedScene,
                    TechniqueIndex_StochasticTransparency, L"MainScene-PostGBuffer-OI-Res");

                resetMarker = SavedTargets::ResetMarker();  // back to normal targets now
                stochTransOp.Resolve();
            }
        }

        //////////////////////////////////////////////////////////////////////////////////////////////////
        LightingParser_PostGBufferEffects(
            metalContext, parserContext, 
            mainTargets._msaaDepthBufferSRV, mainTargets._gbufferRTVsSRV[1]);

        for (auto p=parserContext._plugins.cbegin(); p!=parserContext._plugins.cend(); ++p)
            (*p)->OnPostSceneRender(metalContext, parserContext, SPS::BatchFilter::Transparent, TechniqueIndex_General);
    }

    void LightingParser_MainScene(
        IThreadContext& context,
        Metal::DeviceContext& metalContext, 
        LightingParserContext& parserContext,
        PreparedScene& preparedScene,
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

        Metal::ShaderResourceView postLightingResolveSRV;
        Metal::ShaderResourceView sceneDepthsSRV;
        Metal::ShaderResourceView sceneSecondaryDepthsSRV;
        Metal::RenderTargetView postLightingResolveRTV;
        Metal::DepthStencilView sceneDepthsDSV;
        ID3D::Resource* postLightingResolveTexture = nullptr;

        SavedTargets savedTargets(metalContext);

        Metal::ViewportDesc mainViewport(  
            0.f, 0.f, float(qualitySettings._dimensions[0]), float(qualitySettings._dimensions[1]), 0.f, 1.f);
        
        bool precisionTargets = Tweakable("PrecisionTargets", false);

        typedef Metal::NativeFormat::Enum NativeFormat;
        auto sampling = BufferUploads::TextureSamples::Create(
            uint8(std::max(qualitySettings._samplingCount, 1u)), uint8(qualitySettings._samplingQuality));
        auto& lightingResTargets = Techniques::FindCachedBox2<LightingResolveTextureBox>(
            unsigned(mainViewport.Width), unsigned(mainViewport.Height),
            (!precisionTargets) ? FormatStack(NativeFormat::R16G16B16A16_FLOAT) : FormatStack(NativeFormat::R32G32B32A32_FLOAT),
            sampling);

        if (qualitySettings._lightingModel == RenderingQualitySettings::LightingModel::Deferred) {

                //
            //////////////////////////////////////////////////////////////////////////////////////
                //      Get the gbuffer render targets for this frame
                //

                // Generally the deferred pixel shader will just copy information from the albedo
                // texture into the first deferred buffer. So the first deferred buffer should
                // have the same pixel format as much input textures.
                // Usually this is an 8 bit SRGB format, so the first deferred buffer should also
                // be 8 bit SRGB. So long as we don't do a lot of processing in the deferred pixel shader
                // that should be enough precision.
                //      .. however, it possible some clients might prefer 10 or 16 bit albedo textures
                //      In these cases, the first buffer should be a matching format.
            const bool enableParametersBuffer = Tweakable("EnableParametersBuffer", true);
            auto& mainTargets = Techniques::FindCachedBox2<MainTargetsBox>(
                unsigned(mainViewport.Width), unsigned(mainViewport.Height),
                (!precisionTargets) ? FormatStack(NativeFormat::R8G8B8A8_UNORM_SRGB) : FormatStack(NativeFormat::R32G32B32A32_FLOAT),
                (!precisionTargets) ? FormatStack(NativeFormat::R8G8B8A8_SNORM) : FormatStack(NativeFormat::R32G32B32A32_FLOAT),
                FormatStack(enableParametersBuffer ? ((!precisionTargets) ? FormatStack(NativeFormat::R8G8B8A8_UNORM) : FormatStack(NativeFormat::R32G32B32A32_FLOAT)) : NativeFormat::Unknown),
                FormatStack(
                    NativeFormat(DXGI_FORMAT_R24G8_TYPELESS), 
                    NativeFormat(DXGI_FORMAT_R24_UNORM_X8_TYPELESS), 
                    NativeFormat(DXGI_FORMAT_D24_UNORM_S8_UINT)),
                sampling);

            auto& globalState = parserContext.GetTechniqueContext()._globalEnvironmentState;
            globalState.SetParameter((const utf8*)"GBUFFER_TYPE", enableParametersBuffer?1:2);

            TRY {

                    //
                //////////////////////////////////////////////////////////////////////////////////////
                    //      Bind and clear gbuffer
                    //

                metalContext.Bind(
                    MakeResourceList(   
                        mainTargets._gbufferRTVs[0], 
                        mainTargets._gbufferRTVs[1], 
                        mainTargets._gbufferRTVs[2]), 
                    &mainTargets._msaaDepthBuffer);
                ClearDeferredBuffers(metalContext, mainTargets);
                metalContext.Bind(mainViewport);

                    //
                //////////////////////////////////////////////////////////////////////////////////////
                    //      Render full scene to gbuffer
                    //

                ReturnToSteadyState(metalContext);
                StateSetChangeMarker marker(parserContext, GetStateSetResolvers()._deferred);

                ExecuteScene(
                    context, parserContext, SPS::BatchFilter::General,
                    preparedScene,
                    TechniqueIndex_Deferred, L"MainScene-OpaqueGBuffer");
                for (auto p=parserContext._plugins.cbegin(); p!=parserContext._plugins.cend(); ++p) {
                    (*p)->OnPostSceneRender(metalContext, parserContext, SPS::BatchFilter::General, TechniqueIndex_Deferred);
                }
                LightingParser_LateGBufferRender(metalContext, parserContext, mainTargets);
                LightingParser_ResolveGBuffer(metalContext, parserContext, mainTargets, lightingResTargets);
            } 
            CATCH(const ::Assets::Exceptions::AssetException& e) { parserContext.Process(e); savedTargets.ResetToOldTargets(metalContext); }
            CATCH_END

                // Post lighting resolve operations... (must rebind the depth buffer)
            metalContext.Bind(ResourceList<Metal::RenderTargetView, 0>(), nullptr);
            metalContext.Bind(
                MakeResourceList(lightingResTargets._lightingResolveRTV), 
                &mainTargets._msaaDepthBuffer);
            metalContext.Bind(Techniques::CommonResources()._dssReadOnly);

            TRY {
                LightingParser_DeferredPostGBuffer(context, metalContext, parserContext, preparedScene, mainTargets);
            }
            CATCH(const ::Assets::Exceptions::AssetException& e) { parserContext.Process(e); savedTargets.ResetToOldTargets(metalContext); }
            CATCH_END

            sceneDepthsSRV              = mainTargets._msaaDepthBufferSRV;
            sceneSecondaryDepthsSRV     = mainTargets._secondaryDepthBufferSRV;
            sceneDepthsDSV              = mainTargets._msaaDepthBuffer;

            postLightingResolveSRV      = lightingResTargets._lightingResolveSRV;
            postLightingResolveTexture  = lightingResTargets._lightingResolveTexture.get();
            postLightingResolveRTV      = lightingResTargets._lightingResolveRTV;

        } else if (qualitySettings._lightingModel == RenderingQualitySettings::LightingModel::Forward) {

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

            ForwardLightingModel_Render(context, metalContext, parserContext, preparedScene, mainTargets, sampling._sampleCount);

            sceneDepthsSRV              = mainTargets._msaaDepthBufferSRV;
            sceneSecondaryDepthsSRV     = mainTargets._secondaryDepthBufferSRV;
            sceneDepthsDSV              = mainTargets._msaaDepthBuffer;

            postLightingResolveSRV      = lightingResTargets._lightingResolveSRV;
            postLightingResolveTexture  = lightingResTargets._lightingResolveTexture.get();
            postLightingResolveRTV      = lightingResTargets._lightingResolveRTV;

        }

        {
            Metal::GPUProfiler::DebugAnnotation anno(metalContext, L"Resolve-MSAA-HDR");

            auto toneMapSettings = parserContext.GetSceneParser()->GetToneMapSettings();
            LuminanceResult luminanceResult;
            if (parserContext.GetSceneParser()->GetToneMapSettings()._flags & ToneMapSettings::Flags::EnableToneMap) {
                    //  (must resolve luminance early, because we use it during the MSAA resolve)
                luminanceResult = ToneMap_SampleLuminance(
                    metalContext, parserContext, toneMapSettings, postLightingResolveSRV);
            }

                //
                //      Post lighting resolve operations...
                //          we must bind the depth buffer to whatever
                //          buffer contained the correct depth information from the
                //          previous rendering (might be the default depth buffer, or might
                //          not be)
                //
            if (qualitySettings._samplingCount > 1) {
                Metal::TextureDesc2D inputTextureDesc(postLightingResolveTexture);
				auto& msaaResolveRes = Techniques::FindCachedBox2<FinalResolveResources>(
					inputTextureDesc.Width, inputTextureDesc.Height, Metal::AsNativeFormat(inputTextureDesc.Format));
                LightingParser_ResolveMSAA(
                    metalContext, parserContext,
                    msaaResolveRes._postMsaaResolveTexture.get(),
                    postLightingResolveTexture,
					Metal::AsNativeFormat(inputTextureDesc.Format));

                    // todo -- also resolve the depth buffer...!
                    //      here; we switch the active textures to the msaa resolved textures
                postLightingResolveTexture = msaaResolveRes._postMsaaResolveTexture.get();
                postLightingResolveSRV = msaaResolveRes._postMsaaResolveSRV;
                postLightingResolveRTV = msaaResolveRes._postMsaaResolveTarget;
            }
            metalContext.Bind(MakeResourceList(postLightingResolveRTV), nullptr);       // we don't have a single-sample depths target at this time (only multisample)
            LightingParser_PostProcess(metalContext, parserContext);

                //  Write final colour to output texture
                //  We have to be careful about whether "SRGB" is enabled
                //  on the back buffer we're writing to. Depending on the
                //  tone mapping method, sometimes we want the SRGB conversion,
                //  other times we don't (because some tone map operations produce
                //  SRGB results, others give linear results)

            const bool hardwareSRGBDisabled = Tweakable("Tonemap_DisableHardwareSRGB", true);
            if (hardwareSRGBDisabled) {
                auto res = Metal::ExtractResource<ID3D::Resource>(savedTargets.GetRenderTargets()[0]);
                if (res) {
                    auto currentFormat = Metal::AsNativeFormat(Metal::TextureDesc2D(res.get()).Format);
                    if (Metal::GetComponentType(currentFormat) == Metal::FormatComponentType::UNorm_SRGB) {
                            // create a render target view with SRGB disabled (but the same colour format)
                            // todo -- make sure we're using the correct format here -- 
                        Metal::RenderTargetView rtv(res.get(), Metal::NativeFormat::R8G8B8A8_UNORM);
                        auto* drtv = rtv.GetUnderlying();
                        metalContext.GetUnderlying()->OMSetRenderTargets(1, &drtv, savedTargets.GetDepthStencilView());
                    } else
                        savedTargets.ResetToOldTargets(metalContext);
                }
            } else {
                savedTargets.ResetToOldTargets(metalContext);
            }

            ToneMap_Execute(metalContext, parserContext, luminanceResult, toneMapSettings, postLightingResolveSRV);

                //  if we're not in MSAA mode, we can rebind the main depth buffer. But if we're in MSAA mode, we have to
                //  resolve the depth buffer before we can do that...
            if (qualitySettings._samplingCount >= 1) {
                savedTargets.SetDepthStencilView(sceneDepthsDSV.GetUnderlying());
            }
            savedTargets.ResetToOldTargets(metalContext);
        }

        LightingParser_Overlays(&metalContext, parserContext);
    }

    static const utf8* StringShadowCascadeMode = u("SHADOW_CASCADE_MODE");
    static const utf8* StringShadowEnableNearCascade = u("SHADOW_ENABLE_NEAR_CASCADE");

    PreparedDMShadowFrustum LightingParser_PrepareDMShadow(
        IThreadContext& context,
        Metal::DeviceContext& metalContext, LightingParserContext& parserContext, 
        PreparedScene& preparedScene,
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
        preparedResult._resolveParametersCB = Metal::ConstantBuffer(
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

        auto& targetsBox = Techniques::FindCachedBox2<ShadowTargetsBox>(
            frustum._width, frustum._height, MaxShadowTexturesPerLight, 
            FormatStack(frustum._typelessFormat, frustum._readFormat, frustum._writeFormat));

        RenderCore::Techniques::RSDepthBias singleSidedBias(
            frustum._rasterDepthBias, frustum._depthBiasClamp, frustum._slopeScaledBias);
        RenderCore::Techniques::RSDepthBias doubleSidedBias(
            frustum._dsRasterDepthBias, frustum._dsDepthBiasClamp, frustum._dsSlopeScaledBias);
        auto& resources = Techniques::FindCachedBox2<ShadowWriteResources>(
            singleSidedBias, doubleSidedBias, unsigned(frustum._windingCull));

        preparedResult._shadowTextureSRV = targetsBox._shaderResource;

            /////////////////////////////////////////////

        SavedTargets savedTargets(metalContext);
        auto resetMarker = savedTargets.MakeResetMarker(metalContext);
        metalContext.Bind(RenderCore::ResourceList<Metal::RenderTargetView,0>(), &targetsBox._depthStencilView);
        metalContext.Bind(Metal::ViewportDesc(0.f, 0.f, float(frustum._width), float(frustum._height)));

        for (unsigned c=0; c<projectionCount; ++c) {
                // note --  do we need to clear each slice individually? Or can we clear a single DSV 
                //          representing the whole thing? What if we don't need every slice for this 
                //          frame? Is there a benefit to skipping unnecessary slices?
            metalContext.Clear(targetsBox._dsvBySlice[c], 1.f, 0);  
        }

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
            TechniqueIndex_ShadowGen, L"ShadowGen-Prepare");

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
        LightingParserContext& parserContext, PreparedScene& preparedScene)
    {
        auto* scene = parserContext.GetSceneParser();
        if (!scene) {
            parserContext._preparedDMShadows.clear();
            parserContext._preparedRTShadows.clear();
            return;
        }

        Metal::GPUProfiler::DebugAnnotation anno(metalContext, L"Prepare-Shadows");

            // todo --  we should be using a temporary frame heap for this vector
        auto shadowFrustumCount = scene->GetShadowProjectionCount();
        parserContext._preparedDMShadows.reserve(shadowFrustumCount);

        for (unsigned c=0; c<shadowFrustumCount; ++c) {
            auto frustum = scene->GetShadowProjectionDesc(c, parserContext.GetProjectionDesc());
            CATCH_ASSETS_BEGIN
                if (frustum._resolveType == ShadowProjectionDesc::ResolveType::DepthTexture) {

                    auto shadow = LightingParser_PrepareDMShadow(context, metalContext, parserContext, preparedScene, frustum, c);
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

        auto& metricsBox = Techniques::FindCachedBox2<MetricsBox>();
        unsigned clearValues[] = {0,0,0,0};
        context.Clear(metricsBox._metricsBufferUAV, clearValues);
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

        {
            Metal::GPUProfiler::DebugAnnotation anno(*metalContext, L"Prepare");
            for (auto i=parserContext._plugins.cbegin(); i!=parserContext._plugins.cend(); ++i) {
                CATCH_ASSETS_BEGIN
                    (*i)->OnPreScenePrepare(context, parserContext, preparedScene);
                CATCH_ASSETS_END(parserContext)
            }

            LightingParser_PrepareShadows(context, *metalContext, parserContext, preparedScene);
        }

        GetBufferUploads().Update(context, true);

        CATCH_ASSETS_BEGIN
            LightingParser_MainScene(context, *metalContext, parserContext, preparedScene, qualitySettings);
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

    LightingParserContext::LightingParserContext(const Techniques::TechniqueContext& techniqueContext)
    : ParsingContext(techniqueContext)
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
        _dimensions = UInt2(0,0);
        _lightingModel = LightingModel::Deferred;
        _samplingCount = _samplingQuality = 0;
    }

    RenderingQualitySettings::RenderingQualitySettings(
        UInt2 dimensions, 
        LightingModel lightingModel, unsigned samplingCount, unsigned samplingQuality)
    : _dimensions(dimensions), _lightingModel(lightingModel), _samplingCount(samplingCount), _samplingQuality(samplingQuality)
    {}


    ISceneParser::~ISceneParser() {}

}

