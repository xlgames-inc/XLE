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
#include "Shadows.h"
#include "LightInternal.h"

#include "Sky.h"
#include "VolumetricFog.h"

#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/RenderUtils.h"
#include "../RenderCore/Assets/DeferredShaderResource.h"
#include "../RenderCore/Metal/DeviceContextImpl.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/GPUProfiler.h"
#include "../RenderCore/Metal/Shader.h"

#include "../ConsoleRig/Console.h"
#include "../Math/Transformations.h"
#include "../Utility/StringFormat.h"

namespace SceneEngine
{
    using namespace RenderCore;
    using namespace RenderCore::Metal;

///////////////////////////////////////////////////////////////////////////////////////////////////

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

        DepthStencilState       _alwaysWriteToStencil;
        DepthStencilState       _writePixelFrequencyPixels;
        const ShaderProgram*    _perSampleMask;

        SamplerState            _shadowComparisonSampler;
        SamplerState            _shadowDepthSampler;

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _validationCallback; }

        LightingResolveResources(const Desc& desc);
    private:
        std::shared_ptr<::Assets::DependencyValidation>  _validationCallback;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    namespace ShaderLightDesc
    {
        struct Ambient
        { 
            Float3      _ambientColour; 
            float       _skyReflectionScale; 
            float       _skyReflectionBlurriness; 
            unsigned    _dummy[3];

            Float3      _rangeFogInscatter; unsigned _dummy1;
            Float3      _rangeFogOpticalThickness; unsigned _dummy2;
        };

        class Light
        {
        public:
            Float3 _negativeDirection; float _radius;
            Float3 _diffuse; float dummy;
            Float3 _specular; float nonMetalSpecularBrightness;
            float _lightPower; 
            float _diffuseWideningMin, _diffuseWideningMax;
            float dummy1[1];
        };
    }

    static ShaderLightDesc::Ambient AsShaderDesc(const GlobalLightingDesc& desc)
    {
        return ShaderLightDesc::Ambient 
            { 
                desc._ambientLight, desc._skyReflectionScale, desc._skyReflectionBlurriness, {0,0,0},
                desc._rangeFogInscatter, 0, desc._rangeFogThickness, 0
            };
    }

    static ShaderLightDesc::Light AsShaderDesc(const LightDesc& light)
    {
        return ShaderLightDesc::Light 
            {
                light._negativeLightDirection, 
                light._radius, 
                light._diffuseColor, 0.f,
                light._specularColor,
                light._nonMetalSpecularBrightness,
                PowerForHalfRadius(light._radius, 0.05f),
                light._diffuseWideningMin, light._diffuseWideningMax
            };
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static ConstantBufferPacket BuildScreenToShadowConstants(LightingParserContext& parserContext, unsigned shadowFrustumIndex);
    static ConstantBufferPacket BuildLightConstants(const LightDesc& light);
    static void ResolveLights(  DeviceContext* context,
                                LightingParserContext& parserContext,
                                MainTargetsBox& mainTargets,
                                LightingResolveContext& resolveContext);

    static void SetupStateForDeferredLightingResolve(   DeviceContext* context, 
                                                MainTargetsBox& mainTargets, 
                                                LightingResolveTextureBox& lightingResTargets,
                                                LightingResolveResources& resolveRes,
                                                bool doSampleFrequencyOptimisation)
    {
        const unsigned samplingCount = mainTargets._desc._sampling._sampleCount;

        SetupVertexGeneratorShader(context);
        context->Bind(Techniques::CommonResources()._blendOneSrcAlpha);

                //      Bind lighting resolve texture
                //      (in theory, in LDR/non-MSAA modes we could write directly to the 
                //      default back buffer now)
        context->Bind(
            MakeResourceList(lightingResTargets._lightingResolveRTV), 
            (doSampleFrequencyOptimisation && samplingCount>1)?&mainTargets._secondaryDepthBuffer:nullptr);
        if (doSampleFrequencyOptimisation && samplingCount > 1) {
            context->Bind(Techniques::CommonResources()._cullDisable);
            context->Bind(resolveRes._writePixelFrequencyPixels, 0xff);
        } else {
            context->Bind(Techniques::CommonResources()._dssDisable);
        }

        context->BindPS(MakeResourceList(
            mainTargets._gbufferRTVsSRV[0], mainTargets._gbufferRTVsSRV[1], mainTargets._gbufferRTVsSRV[2], 
            Metal::ShaderResourceView(), mainTargets._msaaDepthBufferSRV));
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
    
    static unsigned GBufferType(MainTargetsBox& mainTargets) { return (mainTargets._gbufferTextures[2]) ? 1 : 2; }

    unsigned LightingParser_BindLightResolveResources( 
        DeviceContext* context,
        LightingParserContext& parserContext)
    {
            // bind resources and constants that are required for lighting resolve operations
            // these are needed in both deferred and forward shading modes... But they are
            // bound at different times in different modes

        auto skyTexture = parserContext.GetSceneParser()->GetGlobalLightingDesc()._skyTexture;
        unsigned skyTextureProjection = 0;
        if (skyTexture[0]) {
            SkyTextureParts parts(skyTexture);
            skyTextureProjection = SkyTexture_BindPS(context, parserContext, parts, 11);
        }

        TRY {
            context->BindPS(MakeResourceList(10, ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("game/xleres/DefaultResources/balanced_noise.dds:LT").GetShaderResource()));
            context->BindPS(MakeResourceList(16, ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("game/xleres/DefaultResources/GGXTable.dds:LT").GetShaderResource()));
        }
        CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
        CATCH_END

        context->BindPS(MakeResourceList(9, ConstantBuffer(&GlobalMaterialOverride, sizeof(GlobalMaterialOverride))));

        return skyTextureProjection;
    }

    void LightingParser_ResolveGBuffer(
        DeviceContext* context, LightingParserContext& parserContext,
        MainTargetsBox& mainTargets, LightingResolveTextureBox& lightingResTargets)
    {
        GPUProfiler::DebugAnnotation anno(*context, L"ResolveGBuffer");

        const bool doSampleFrequencyOptimisation = Tweakable("SampleFrequencyOptimisation", true);

        LightingResolveContext lightingResolveContext(mainTargets);
        const unsigned samplingCount = lightingResolveContext.GetSamplingCount();
        const bool useMsaaSamplers = lightingResolveContext.UseMsaaSamplers();

        auto& resolveRes = Techniques::FindCachedBoxDep2<LightingResolveResources>(samplingCount);

            //
            //    Our inputs is the prepared gbuffer 
            //        -- we resolve the lighting and write out a "lighting resolve texture"
            //

        if (doSampleFrequencyOptimisation && samplingCount>1) {
            context->Bind(resolveRes._alwaysWriteToStencil, 0xff);

                // todo --  instead of clearing the stencil every time, how 
                //          about doing a progressive walk through all of the bits!
            context->ClearStencil(mainTargets._secondaryDepthBuffer, 0);
            context->Bind(ResourceList<Metal::RenderTargetView, 0>(), &mainTargets._secondaryDepthBuffer);
            context->BindPS(MakeResourceList(mainTargets._msaaDepthBufferSRV, mainTargets._gbufferRTVsSRV[1]));
            SetupVertexGeneratorShader(context);
            TRY {
                context->Bind(*resolveRes._perSampleMask);
                context->Draw(4);
            } 
            CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
            CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
            CATCH_END
        }

        {
            GPUProfiler::DebugAnnotation anno(*context, L"Prepare");
            for (auto i=parserContext._plugins.cbegin(); i!=parserContext._plugins.cend(); ++i) {
                TRY {
                    (*i)->OnLightingResolvePrepare(context, parserContext, lightingResolveContext);
                } 
                CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
                CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
                CATCH_END
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

        TRY {
            SetupStateForDeferredLightingResolve(context, mainTargets, lightingResTargets, resolveRes, doSampleFrequencyOptimisation);
            auto skyTextureProjection = LightingParser_BindLightResolveResources(context, parserContext);

            context->BindPS(MakeResourceList(4, resolveRes._shadowComparisonSampler, resolveRes._shadowDepthSampler));

                // note -- if we do ambient first, we can avoid this clear (by rendering the ambient opaque)
            float clearColour[] = { 0.f, 0.f, 0.f, 1.f };
            context->Clear(lightingResTargets._lightingResolveRTV, clearColour);
                       
            const unsigned passCount = (doSampleFrequencyOptimisation && samplingCount > 1)?2:1;
            for (unsigned c=0; c<passCount; ++c) {

                lightingResolveContext.SetPass((LightingResolveContext::Pass::Enum)c);

                    // -------- -------- -------- -------- -------- --------

                TRY {
                    ResolveLights(context, parserContext, mainTargets, lightingResolveContext);
                }
                CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
                CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
                CATCH_END

                TRY {
                    GPUProfiler::DebugAnnotation anno(*context, L"Ambient");

                    auto globalLightDesc = parserContext.GetSceneParser()->GetGlobalLightingDesc();

                        //-------- ambient light shader --------
                    auto& ambientResolveShaders = 
                        Techniques::FindCachedBoxDep2<AmbientResolveShaders>(
                            GBufferType(mainTargets),
                            (c==0)?samplingCount:1, useMsaaSamplers, c==1,
                            lightingResolveContext._ambientOcclusionResult.IsGood(),
                            lightingResolveContext._tiledLightingResult.IsGood(),
                            lightingResolveContext._screenSpaceReflectionsResult.IsGood(),
                            skyTextureProjection, globalLightDesc._doRangeFog);

                    auto ambientLightPacket = MakeSharedPkt(AsShaderDesc(globalLightDesc));
                    ambientResolveShaders._ambientLightUniforms->Apply(
                        *context, parserContext.GetGlobalUniformsStream(),
                        UniformsStream(&ambientLightPacket, nullptr, 1));

                        //  When screen space reflections are enabled, we need to take a copy of the lighting
                        //  resolve target. This is because we want to reflect the post-lighting resolve pixels.
                    if (lightingResolveContext._screenSpaceReflectionsResult.IsGood())
                        context->GetUnderlying()->CopyResource(
                            lightingResTargets._lightingResolveCopy.get(), 
                            lightingResTargets._lightingResolveTexture.get());

                    context->BindPS(MakeResourceList(5, 
                        lightingResolveContext._tiledLightingResult, 
                        lightingResolveContext._ambientOcclusionResult,
                        lightingResolveContext._screenSpaceReflectionsResult,
                        lightingResTargets._lightingResolveCopySRV));

                    context->Bind(*ambientResolveShaders._ambientLight);
                    context->Draw(4);
                }
                CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
                CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
                CATCH_END

                    //-------- do sky --------
                if (Tweakable("DoSky", true)) {
                    GPUProfiler::DebugAnnotation anno(*context, L"Sky");

                        //  Hack -- stop and render the sky at this point!
                        //          we have to change all of the render states. We need
                        //          a better way to manage render states during lighting resolve
                        //          (pending refactoring)
                    context->Bind(Techniques::CommonResources()._dssReadOnly);
                    context->UnbindPS<ShaderResourceView>(4, 1);
                    context->Bind(MakeResourceList(lightingResTargets._lightingResolveRTV), &mainTargets._msaaDepthBuffer);
                    Sky_Render(context, parserContext, false);     // we do a first pass of the sky here, and then follow up with a second pass after lighting resolve

                        // have to reset our state (because Sky_Render changed everything)
                    SetupStateForDeferredLightingResolve(
                        context, mainTargets, lightingResTargets, resolveRes, doSampleFrequencyOptimisation);
                }

                for (auto i=lightingResolveContext._queuedResolveFunctions.cbegin();
                    i!=lightingResolveContext._queuedResolveFunctions.cend(); ++i) {
                    (*i)(context, parserContext, lightingResolveContext, c);
                }

                    // -------- -------- -------- -------- -------- --------

            }
        } 
        CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
        CATCH_END

        context->UnbindPS<ShaderResourceView>(0, 9);

            // reset some of the states to more common defaults...
        context->Bind(Techniques::CommonResources()._defaultRasterizer);
        context->Bind(Techniques::CommonResources()._dssReadWrite);
        
        if (Tweakable("DeferredDebugging", false)) {
            parserContext._pendingOverlays.push_back(
                std::bind(&Deferred_DrawDebugging, std::placeholders::_1, std::placeholders::_2, std::ref(mainTargets)));
        }
    }

    static void ResolveLights(  DeviceContext* context,
                                LightingParserContext& parserContext,
                                MainTargetsBox& mainTargets,
                                LightingResolveContext& resolveContext)
    {
        GPUProfiler::DebugAnnotation anno(*context, L"Lights");

        const unsigned samplingCount = resolveContext.GetSamplingCount();
        const bool useMsaaSamplers = resolveContext.UseMsaaSamplers();

        ConstantBufferPacket constantBufferPackets[6];
        const Metal::ConstantBuffer* prebuiltConstantBuffers[6] = { nullptr, nullptr, nullptr, nullptr, nullptr };
        prebuiltConstantBuffers[2] = &Techniques::FindCachedBox2<ShadowResourcesBox>()._sampleKernel32;

            ////////////////////////////////////////////////////////////////////////

        auto& lightingResolveShaders = 
            Techniques::FindCachedBoxDep2<LightingResolveShaders>(
                GBufferType(mainTargets),
                (resolveContext.GetCurrentPass()==LightingResolveContext::Pass::PerSample)?samplingCount:1, useMsaaSamplers, 
                resolveContext.GetCurrentPass()==LightingResolveContext::Pass::PerPixel);

        const bool allowOrthoShadowResolve = Tweakable("AllowOrthoShadowResolve", true);

            //-------- do lights --------
        auto lightCount = parserContext.GetSceneParser()->GetLightCount();
        for (unsigned l=0; l<lightCount; ++l) {
            auto& i = parserContext.GetSceneParser()->GetLightDesc(l);
            constantBufferPackets[1] = BuildLightConstants(i);

            TRY {
                LightingResolveShaders::LightShaderType shaderType;
                shaderType._projection = 
                    (i._type == LightDesc::Directional) 
                    ? LightingResolveShaders::Directional 
                    : LightingResolveShaders::Point;

                    //  We only support a limited set of different light types so far.
                    //  Perhaps this will be extended to support more lights with custom
                    //  shaders and resources.
                if (    i._shadowFrustumIndex < parserContext._preparedShadows.size() 
                    &&  parserContext._preparedShadows[i._shadowFrustumIndex].IsReady()) {

                    const auto& preparedShadows = parserContext._preparedShadows[i._shadowFrustumIndex];
                    context->BindPS(MakeResourceList(3, preparedShadows._shadowTextureSRV));
                    prebuiltConstantBuffers[0] = &preparedShadows._arbitraryCB;
                    prebuiltConstantBuffers[4] = &preparedShadows._orthoCB;
                    constantBufferPackets[5] = MakeSharedPkt(preparedShadows._resolveParameters);

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
                    
                    constantBufferPackets[3] = BuildScreenToShadowConstants(parserContext, i._shadowFrustumIndex);

                    if (preparedShadows._mode == ShadowProjectionDesc::Projections::Mode::Ortho && allowOrthoShadowResolve) {
                        shaderType._shadows = LightingResolveShaders::OrthShadows;
                    } else 
                        shaderType._shadows = LightingResolveShaders::PerspectiveShadows;

                } else
                    shaderType._shadows = LightingResolveShaders::NoShadows;

                shaderType._diffuseModel = (uint8)i._diffuseModel;
                shaderType._shadowResolveModel = (uint8)i._shadowResolveModel;

                const auto* shader = lightingResolveShaders.GetShader(shaderType);
                assert(shader && shader->_shader);

                shader->_uniforms.Apply(
                    *context, parserContext.GetGlobalUniformsStream(), 
                    UniformsStream(constantBufferPackets, prebuiltConstantBuffers));
                context->Bind(*shader->_shader);
                context->Draw(4);
            } 
            CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
            CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
            CATCH_END
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    LightingResolveResources::LightingResolveResources(const Desc& desc)
    {
        DepthStencilState alwaysWriteToStencil(
            false, false, 0x0, 0xff, StencilMode::AlwaysWrite, StencilMode::AlwaysWrite);

        DepthStencilState writePixelFrequencyPixels(
            false, false, 0xff, 0xff, 
            StencilMode(Comparison::Equal, StencilOp::DontWrite),
            StencilMode(Comparison::NotEqual, StencilOp::DontWrite));

        SamplerState shadowComparisonSampler(
            FilterMode::ComparisonBilinear, AddressMode::Clamp, AddressMode::Clamp, AddressMode::Clamp,
            Comparison::LessEqual);
        SamplerState shadowDepthSampler(
            FilterMode::Bilinear, AddressMode::Clamp, AddressMode::Clamp, AddressMode::Clamp);

        char definesTable[256];
        Utility::XlFormatString(definesTable, dimof(definesTable), "MSAA_SAMPLES=%i", desc._samplingCount);
        auto* perSampleMask = &::Assets::GetAssetDep<ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*", 
            "game/xleres/deferred/persamplemask.psh:main:ps_*", definesTable);

        auto validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(validationCallback, perSampleMask->GetDependencyValidation());

        _alwaysWriteToStencil = std::move(alwaysWriteToStencil);
        _perSampleMask = std::move(perSampleMask);
        _shadowComparisonSampler = std::move(shadowComparisonSampler);
        _shadowDepthSampler = std::move(shadowDepthSampler);
        _writePixelFrequencyPixels = std::move(writePixelFrequencyPixels);
        _validationCallback = std::move(validationCallback);
    }

    auto            LightingResolveContext::GetCurrentPass() const -> Pass::Enum { return _pass; }
    bool            LightingResolveContext::UseMsaaSamplers() const     { return _useMsaaSamplers; }
    unsigned        LightingResolveContext::GetSamplingCount() const    { return _samplingCount; }
    MainTargetsBox& LightingResolveContext::GetMainTargets() const      { return *_mainTargets; }
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

    LightingResolveContext::LightingResolveContext(MainTargetsBox& mainTargets)
    : _mainTargets(&mainTargets)
    , _pass(Pass::Prepare)
    {
        _samplingCount = mainTargets._desc._sampling._sampleCount;
        _useMsaaSamplers = _samplingCount > 1;
    }
    LightingResolveContext::~LightingResolveContext() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    static ConstantBufferPacket BuildScreenToShadowConstants(
        LightingParserContext& parserContext, unsigned shadowFrustumIndex)
    {
        return BuildScreenToShadowConstants(
            parserContext._preparedShadows[shadowFrustumIndex]._frustumCount,
            parserContext._preparedShadows[shadowFrustumIndex]._arbitraryCBSource,
            parserContext._preparedShadows[shadowFrustumIndex]._orthoCBSource,
            parserContext.GetProjectionDesc()._cameraToWorld);
    }

    static ConstantBufferPacket BuildLightConstants(const LightDesc& light)
    {
        return MakeSharedPkt(AsShaderDesc(light));
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    void LightingParser_InitBasicLightEnv(  
        DeviceContext* context,
        LightingParserContext& parserContext,
        ISceneParser& sceneParser)
    {
        const unsigned lightCount = 1;
        struct BasicEnv
        {
            ShaderLightDesc::Ambient _ambient;
            ShaderLightDesc::Light _dominant[lightCount];
        } env;

        env._ambient = AsShaderDesc(sceneParser.GetGlobalLightingDesc());

        for (unsigned l=0; l<lightCount; ++l)
            if (sceneParser.GetLightCount() > l) {
                env._dominant[l] = AsShaderDesc(sceneParser.GetLightDesc(l));
            } else {
                env._dominant[l] = ShaderLightDesc::Light
                    {   Float3(0.f, 0.f, 0.f), 0.f, Float3(0.f, 0.f, 0.f), 0.f,
                        Float3(0.f, 0.f, 0.f), 0.f,
                        0.f, 0.f, 0.f, { 0.f } };
            }

        parserContext.SetGlobalCB(5, context, &env, sizeof(env));
    }

}



