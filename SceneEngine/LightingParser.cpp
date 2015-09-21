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
#include "Shadows.h"
#include "MetricsBox.h"
#include "Ocean.h"
#include "DeepOceanSim.h"
#include "RefractionsBuffer.h"
#include "OrderIndependentTransparency.h"
#include "Sky.h"
#include "SunFlare.h"
#include "Rain.h"
#include "Noise.h"
#include "RayTracedShadows.h"

#include "../RenderCore/RenderUtils.h"
#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Assets/DeferredShaderResource.h"
#include "../RenderCore/Metal/GPUProfiler.h"
#include "../RenderCore/Metal/DeviceContextImpl.h"
#include "../RenderCore/Metal/Shader.h"
#include "../ConsoleRig/Console.h"
#include "../Math/Transformations.h"
#include "../Math/ProjectionMath.h"

namespace SceneEngine
{
    using namespace RenderCore;
    using namespace RenderCore::Metal;

    DeepOceanSimSettings GlobalOceanSettings; 
    OceanLightingSettings GlobalOceanLightingSettings; 

    void LightingParser_ResolveGBuffer( 
        DeviceContext* context,
        LightingParserContext& parserContext,
        MainTargetsBox& mainTargets,
        LightingResolveTextureBox& lightingResTargets);

    unsigned LightingParser_BindLightResolveResources( 
        DeviceContext* context,
        LightingParserContext& parserContext);

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
    void SetFrameGlobalStates(DeviceContext* context, LightingParserContext& parserContext)
    {
        TRY {
            SamplerState samplerDefault, 
                samplerClamp(FilterMode::Trilinear, AddressMode::Clamp, AddressMode::Clamp, AddressMode::Clamp), 
                samplerAnisotrophic(FilterMode::Anisotropic),
                samplerPoint(FilterMode::Point, AddressMode::Clamp, AddressMode::Clamp, AddressMode::Clamp),
                samplerWrapU(FilterMode::Trilinear, AddressMode::Wrap, AddressMode::Clamp);
            context->BindPS(RenderCore::MakeResourceList(samplerDefault, samplerClamp, samplerAnisotrophic, samplerPoint));
            context->BindVS(RenderCore::MakeResourceList(samplerDefault, samplerClamp, samplerAnisotrophic, samplerPoint));
            context->BindPS(RenderCore::MakeResourceList(6, samplerWrapU));

            auto normalsFittingResource = ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("game/xleres/DefaultResources/normalsfitting.dds:LT").GetShaderResource();
            context->BindPS(RenderCore::MakeResourceList(14, normalsFittingResource));
            context->BindCS(RenderCore::MakeResourceList(14, normalsFittingResource));

                // perlin noise resources in standard slots
            auto& perlinNoiseRes = Techniques::FindCachedBox<PerlinNoiseResources>(PerlinNoiseResources::Desc());
            context->BindPS(MakeResourceList(12, perlinNoiseRes._gradShaderResource, perlinNoiseRes._permShaderResource));

                // procedural scratch texture for scratches test
            // context->BindPS(MakeResourceList(17, Assets::GetAssetDep<Metal::DeferredShaderResource>("game/xleres/scratchnorm.dds:L").GetShaderResource()));
            // context->BindPS(MakeResourceList(18, Assets::GetAssetDep<Metal::DeferredShaderResource>("game/xleres/scratchocc.dds:L").GetShaderResource()));
        }
        CATCH(const ::Assets::Exceptions::InvalidAsset& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingAsset& e) { parserContext.Process(e); }
        CATCH_END
    }

    void ReturnToSteadyState(DeviceContext* context)
    {
            //
            //      Change some frequently changed states back
            //      to their defaults.
            //      Most rendering operations assume these states
            //      are at their defaults.
            //

        context->Bind(Techniques::CommonResources()._dssReadWrite);
        context->Bind(Techniques::CommonResources()._blendOpaque);
        context->Bind(Techniques::CommonResources()._defaultRasterizer);
        context->Bind(Topology::TriangleList);
        context->BindVS(RenderCore::MakeResourceList(ConstantBuffer(), ConstantBuffer(), ConstantBuffer(), ConstantBuffer(), ConstantBuffer()));
        context->BindPS(RenderCore::MakeResourceList(ConstantBuffer(), ConstantBuffer(), ConstantBuffer(), ConstantBuffer(), ConstantBuffer()));
        context->Unbind<GeometryShader>();
    }

    static void ClearDeferredBuffers(DeviceContext* context, MainTargetsBox& mainTargets)
    {
        if (mainTargets._gbufferRTVs[0].GetUnderlying())
            context->Clear(mainTargets._gbufferRTVs[0], Float4(0.33f, 0.33f, 0.33f, 1.f));
        if (mainTargets._gbufferRTVs[1].GetUnderlying())
            context->Clear(mainTargets._gbufferRTVs[1], Float4(0.f, 0.f, 0.f, 0.f));
        if (mainTargets._gbufferRTVs[2].GetUnderlying())
            context->Clear(mainTargets._gbufferRTVs[2], Float4(0.f, 0.f, 0.f, 0.f));
        context->Clear(mainTargets._msaaDepthBuffer, 1.f, 0);
    }

    void LightingParser_SetProjectionDesc(  
        LightingParserContext& parserContext, 
        const Techniques::CameraDesc& sceneCamera,
        UInt2 viewportDims,
        const Float4x4* specialProjectionMatrix)
    {
            //  Setup our projection matrix... Scene parser should give us some camera
            //  parameters... But perhaps the projection matrix should be created here,
            //  from the lighting parser. The reason is because the size of the output
            //  texture matters... The scene parser doesn't know what we're rendering
            //  do, so can't know the complete rendering output.
        const float aspectRatio = viewportDims[0] / float(viewportDims[1]);
        auto cameraToProjection = Techniques::Projection(sceneCamera, aspectRatio);

        if (specialProjectionMatrix) {
            cameraToProjection = *specialProjectionMatrix;
        }

        auto& projDesc = parserContext.GetProjectionDesc();
        projDesc._verticalFov = sceneCamera._verticalFieldOfView;
        projDesc._aspectRatio = aspectRatio;
        projDesc._nearClip = sceneCamera._nearClip;
        projDesc._farClip = sceneCamera._farClip;
        projDesc._worldToProjection = Combine(InvertOrthonormalTransform(sceneCamera._cameraToWorld), cameraToProjection);
        projDesc._cameraToProjection = cameraToProjection;
        projDesc._cameraToWorld = sceneCamera._cameraToWorld;
    }

    void LightingParser_SetGlobalTransform( DeviceContext* context, 
                                            LightingParserContext& parserContext, 
                                            const Techniques::CameraDesc& sceneCamera,
                                            UInt2 viewportDims,
                                            const Float4x4* specialProjectionMatrix)
    {
        LightingParser_SetProjectionDesc(parserContext, sceneCamera, viewportDims, specialProjectionMatrix);
        auto& projDesc = parserContext.GetProjectionDesc();
        auto globalTransform = BuildGlobalTransformConstants(projDesc);
        parserContext.SetGlobalCB(
            *context, Techniques::TechniqueContext::CB_GlobalTransform,
            &globalTransform, sizeof(globalTransform));
    }

    void LightingParser_SetGlobalTransform(
        MetalContext* context,
        LightingParserContext& parserContext,
        const Float4x4& cameraToWorld,
        float l, float t, float r, float b,
        float nearClip, float farClip)
    {
        auto cameraToProjection = OrthogonalProjection(l, t, r, b, nearClip, farClip, Techniques::GetDefaultClipSpaceType());

        auto& projDesc = parserContext.GetProjectionDesc();
        projDesc._verticalFov = 0.f;
        projDesc._aspectRatio = 1.f;
        projDesc._nearClip = nearClip;
        projDesc._farClip = farClip;
        projDesc._worldToProjection = Combine(InvertOrthonormalTransform(cameraToWorld), cameraToProjection);
        projDesc._cameraToProjection = cameraToProjection;
        projDesc._cameraToWorld = cameraToWorld;

        auto globalTransform = BuildGlobalTransformConstants(projDesc);
        parserContext.SetGlobalCB(
            *context, Techniques::TechniqueContext::CB_GlobalTransform,
            &globalTransform, sizeof(globalTransform));
    }

    void LightingParser_LateGBufferRender(  DeviceContext* context, 
                                            LightingParserContext& parserContext,
                                            MainTargetsBox& mainTargets)
    {
        GPUProfiler::DebugAnnotation anno(*context, L"LateGBuffer");

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
            Desc(unsigned width, unsigned height, NativeFormat::Enum format)
                : _width(width), _height(height), _format(format) {}

            unsigned            _width, _height;
            NativeFormat::Enum  _format;
        };

        ResourcePtr         _postMsaaResolveTexture;
        RenderTargetView    _postMsaaResolveTarget;
        ShaderResourceView  _postMsaaResolveSRV;

        FinalResolveResources(const Desc& desc);
    };

    FinalResolveResources::FinalResolveResources(const Desc& desc)
    {
        using namespace BufferUploads;
        auto bufferUploadsDesc = BuildRenderTargetDesc(
            BindFlag::ShaderResource|BindFlag::RenderTarget,
            BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, AsDXGIFormat(desc._format)),
            "FinalResolve");
        auto postMsaaResolveTexture = CreateResourceImmediate(bufferUploadsDesc);

        RenderTargetView postMsaaResolveTarget(postMsaaResolveTexture.get());
        ShaderResourceView postMsaaResolveSRV(postMsaaResolveTexture.get());

        _postMsaaResolveTexture = std::move(postMsaaResolveTexture);
        _postMsaaResolveTarget = std::move(postMsaaResolveTarget);
        _postMsaaResolveSRV = std::move(postMsaaResolveSRV);
    }

    void LightingParser_ResolveMSAA(    DeviceContext* context, 
                                        LightingParserContext& parserContext,
                                        ID3D::Resource* destinationTexture,
                                        ID3D::Resource* sourceTexture,
                                        NativeFormat::Enum resolveFormat)
    {
            // todo -- support custom resolve (tone-map aware)
        context->GetUnderlying()->ResolveSubresource(
            destinationTexture, D3D11CalcSubresource(0,0,0),
            sourceTexture, D3D11CalcSubresource(0,0,0),
            Metal::AsDXGIFormat(resolveFormat));
    }

    void LightingParser_PostProcess(    DeviceContext* context, 
                                        LightingParserContext& parserContext)
    {
        // nothing here yet!
    }

    void LightingParser_ResolveHDR(     DeviceContext* context, 
                                        LightingParserContext& parserContext,
                                        ShaderResourceView& inputHDR,
                                        int samplingCount)
    {
        ToneMap_Execute(context, parserContext, inputHDR, samplingCount);
    }

    void LightingParser_PostGBuffer(    DeviceContext* context, 
                                        LightingParserContext& parserContext,
                                        ShaderResourceView& depthsSRV,
                                        ShaderResourceView& normalsSRV)
    {
        GPUProfiler::DebugAnnotation anno(*context, L"PostGBuffer");

            // note --  these things can be executed by the scene parser? Are they better
            //          off handled by the scene parser, or the lighting parser?
        if (Tweakable("OceanDoSimulation", false)) {
            Ocean_Execute(context, parserContext, GlobalOceanSettings, GlobalOceanLightingSettings, depthsSRV);
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

        if (Tweakable("DoRain", false)) {
            Rain_Render(context, parserContext);
            Rain_RenderSimParticles(context, parserContext, depthsSRV, normalsSRV);
            // SparkParticleTest_RenderSimParticles(context, parserContext, depthsSRV, normalsSRV);
        }

        if (Tweakable("DoSun", false)) {
            SunFlare_Execute(context, parserContext, depthsSRV);
        }
    }

    void LightingParser_Overlays(   DeviceContext* context,
                                    LightingParserContext& parserContext)
    {
        GPUProfiler::DebugAnnotation anno(*context, L"Overlays");

        ViewportDesc mainViewportDesc(*context);
        auto& refractionBox = Techniques::FindCachedBox<RefractionsBuffer>(RefractionsBuffer::Desc(unsigned(mainViewportDesc.Width/2), unsigned(mainViewportDesc.Height/2)));
        refractionBox.Build(*context, parserContext, 4.f);
        context->BindPS(MakeResourceList(12, refractionBox.GetSRV()));

        for (auto i=parserContext._pendingOverlays.cbegin(); i!=parserContext._pendingOverlays.cend(); ++i) {
            (*i)(context, parserContext);
        }
                    
        if (Tweakable("FFTDebugging", false)) {
            FFT_DoDebugging(context);
        }

        if (Tweakable("MetricsRender", false) && parserContext.GetMetricsBox()) {
            TRY {

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
                SetupVertexGeneratorShader(context);
                context->Bind(Topology::PointList);
                context->Draw(9);

                context->UnbindPS<ShaderResourceView>(3, 1);
                context->UnbindVS<ShaderResourceView>(0, 1);

            } 
            CATCH(const ::Assets::Exceptions::InvalidAsset& e) { parserContext.Process(e); }
            CATCH(const ::Assets::Exceptions::PendingAsset& e) { parserContext.Process(e); }
            CATCH_END
        }
    }

    void LightingParser_PrepareShadows(DeviceContext* context, LightingParserContext& parserContext);

    static void ForwardLightingModel_Render(    DeviceContext* context, 
                                                LightingParserContext& parserContext,
                                                ForwardTargetsBox& targetsBox,
                                                unsigned sampleCount)
    {
        auto skyProj = LightingParser_BindLightResolveResources(context, parserContext);
        parserContext.GetTechniqueContext()._globalEnvironmentState.SetParameter((const utf8*)"SKY_PROJECTION", skyProj);

            //  Order independent transparency disabled when
            //  using MSAA modes... Still some problems in related to MSAA buffers
        const bool useOrderIndependentTransparency = Tweakable("UseOrderIndependentTransparency", false) && (sampleCount <= 1);
        SceneParseSettings::Toggles::BitField normalRenderToggles = ~SceneParseSettings::Toggles::BitField(0);
        if (useOrderIndependentTransparency) {
                // Skip non-terrain during normal render (this will be rendered later using OIT mode)
            normalRenderToggles &= ~SceneParseSettings::Toggles::NonTerrain;
        }

        ReturnToSteadyState(context);
        {
            GPUProfiler::DebugAnnotation anno(*context, L"MainScene-DepthOnly");
            parserContext.GetSceneParser()->ExecuteScene(
                context, parserContext, 
                SceneParseSettings(SceneParseSettings::BatchFilter::Depth, normalRenderToggles),
                TechniqueIndex_DepthOnly);
        }

            /////

        ReturnToSteadyState(context);
            //  We must disable z write (so all shaders can be early-depth-stencil)
            //      (this is because early-depth-stencil will normally write to the depth
            //      buffer before the alpha test has been performed. The pre-depth pass
            //      will switch early-depth-stencil on and off as necessary, but in the second
            //      pass we want it on permanently because the depth reject will end up performing
            //      the same job as alpha testing)
        context->Bind(Techniques::CommonResources()._dssReadOnly);

            /////
            
        {
            GPUProfiler::DebugAnnotation anno(*context, L"MainScene-General");
            parserContext.GetSceneParser()->ExecuteScene(
                context, parserContext, 
                SceneParseSettings(SceneParseSettings::BatchFilter::General, normalRenderToggles),
                TechniqueIndex_General);
        }

            /////

        ShaderResourceView duplicatedDepthBuffer;
        if (useOrderIndependentTransparency) {
            duplicatedDepthBuffer = 
                BuildDuplicatedDepthBuffer(context, targetsBox._msaaDepthBufferTexture.get());
                
            OrderIndependentTransparency_Prepare(context, parserContext, duplicatedDepthBuffer);

                //
                //      (render the parts of the scene that we want to use
                //       the order independent translucent shader)
                //      --  we can also do this in one ExecuteScene() pass, because
                //          it's ok to bind the order independent uav outputs during
                //          the normal render. But to do that requires some way to
                //          select the technique index used deeper within the
                //          scene parser.
                //
            ReturnToSteadyState(context);
            parserContext.GetSceneParser()->ExecuteScene(
                context, parserContext, 
                SceneParseSettings(
                    SceneParseSettings::BatchFilter::General, 
                    SceneParseSettings::Toggles::NonTerrain|SceneParseSettings::Toggles::Opaque|SceneParseSettings::Toggles::Transparent),
                TechniqueIndex_OrderIndependentTransparency);
        }

            /////

            // note --  we have to careful about when we render the sky. 
            //          If we're rendering some transparent stuff, it must come
            //          after the transparent stuff. In the case of order independent
            //          transparency, we can do the "prepare" step before the
            //          sky (because the prepare step will only write opaque stuff, 
            //          but the "resolve" step must come after sky rendering.
        if (Tweakable("DoSky", true)) {
            Sky_Render(context, parserContext, true);
            Sky_RenderPostFog(context, parserContext);
        }

        if (useOrderIndependentTransparency) {
                //  feed the "duplicated depth buffer" into the resolve operation here...
                //  this a non-MSAA blend over a MSAA buffer; so it might kill the samples information.
                //  We can try to do this after the MSAA resolve... But we do the luminance sample
                //  before MSAA resolve, so transluent objects don't contribute to the luminance sampling.
            OrderIndependentTransparency_Resolve(context, parserContext, duplicatedDepthBuffer); // mainTargets._msaaDepthBufferSRV);
        }
    }

    void LightingParser_MainScene(  DeviceContext* context, 
                                    LightingParserContext& parserContext,
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

        ShaderResourceView postLightingResolveSRV;
        ShaderResourceView sceneDepthsSRV;
        ShaderResourceView sceneSecondaryDepthsSRV;
        RenderTargetView postLightingResolveRTV;
        DepthStencilView sceneDepthsDSV;
        ID3D::Resource* postLightingResolveTexture = nullptr;

        SavedTargets savedTargets(context);

        ViewportDesc mainViewport(  
            0.f, 0.f, float(qualitySettings._dimensions[0]), float(qualitySettings._dimensions[1]), 0.f, 1.f);
        
        bool precisionTargets = Tweakable("PrecisionTargets", false);

        typedef Metal::NativeFormat::Enum NativeFormat;
        auto sampling = BufferUploads::TextureSamples::Create(
            uint8(std::max(qualitySettings._samplingCount, 1u)), uint8(qualitySettings._samplingQuality));
        auto& lightingResTargets = Techniques::FindCachedBox<LightingResolveTextureBox>(
            LightingResolveTextureBox::Desc(
                unsigned(mainViewport.Width), unsigned(mainViewport.Height),
                (!precisionTargets) ? FormatStack(NativeFormat::R16G16B16A16_FLOAT) : FormatStack(NativeFormat::R32G32B32A32_FLOAT),
                sampling));

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
            auto& mainTargets = Techniques::FindCachedBox<MainTargetsBox>(
                MainTargetsBox::Desc(
                    unsigned(mainViewport.Width), unsigned(mainViewport.Height),
                    (!precisionTargets) ? FormatStack(NativeFormat::R8G8B8A8_UNORM_SRGB) : FormatStack(NativeFormat::R32G32B32A32_FLOAT),
                    (!precisionTargets) ? FormatStack(NativeFormat::R8G8B8A8_UNORM) : FormatStack(NativeFormat::R32G32B32A32_FLOAT),
                    FormatStack(enableParametersBuffer ? ((!precisionTargets) ? FormatStack(NativeFormat::R8G8B8A8_UNORM) : FormatStack(NativeFormat::R32G32B32A32_FLOAT)) : NativeFormat::Unknown),
                    FormatStack(
                        NativeFormat(DXGI_FORMAT_R24G8_TYPELESS), 
                        NativeFormat(DXGI_FORMAT_R24_UNORM_X8_TYPELESS), 
                        NativeFormat(DXGI_FORMAT_D24_UNORM_S8_UINT)),
                    sampling));

            auto& globalState = parserContext.GetTechniqueContext()._globalEnvironmentState;
            globalState.SetParameter((const utf8*)"GBUFFER_TYPE", enableParametersBuffer?1:2);

            TRY {

                    //
                //////////////////////////////////////////////////////////////////////////////////////
                    //      Bind and clear gbuffer
                    //

                context->Bind(
                    MakeResourceList(   
                        mainTargets._gbufferRTVs[0], 
                        mainTargets._gbufferRTVs[1], 
                        mainTargets._gbufferRTVs[2]), 
                    &mainTargets._msaaDepthBuffer);
                ClearDeferredBuffers(context, mainTargets);
                context->Bind(mainViewport);

                    //
                //////////////////////////////////////////////////////////////////////////////////////
                    //      Render full scene to gbuffer
                    //

                ReturnToSteadyState(context);
                SceneParseSettings sceneParseSettings( 
                    SceneParseSettings::BatchFilter::General, ~SceneParseSettings::Toggles::BitField(0));
                {
                    GPUProfiler::DebugAnnotation anno(*context, L"MainScene-OpaqueGBuffer");
                    parserContext.GetSceneParser()->ExecuteScene(
                        context, parserContext, sceneParseSettings, TechniqueIndex_Deferred);
                }
                for (auto p=parserContext._plugins.cbegin(); p!=parserContext._plugins.cend(); ++p) {
                    (*p)->OnPostSceneRender(context, parserContext, sceneParseSettings, TechniqueIndex_Deferred);
                }
                LightingParser_LateGBufferRender(context, parserContext, mainTargets);
                LightingParser_ResolveGBuffer(context, parserContext, mainTargets, lightingResTargets);
            } 
            CATCH(const ::Assets::Exceptions::InvalidAsset& e) { parserContext.Process(e); savedTargets.ResetToOldTargets(context); }
            CATCH(const ::Assets::Exceptions::PendingAsset& e) { parserContext.Process(e); savedTargets.ResetToOldTargets(context); }
            CATCH_END

                // Post lighting resolve operations... (must rebind the depth buffer)
            context->Bind(ResourceList<Metal::RenderTargetView, 0>(), nullptr);
            context->Bind(
                MakeResourceList(lightingResTargets._lightingResolveRTV), 
                &mainTargets._msaaDepthBuffer);
            context->Bind(Techniques::CommonResources()._dssReadOnly);

            TRY {

                    //  Render translucent objects (etc)
                    //  everything after the gbuffer resolve
                LightingParser_PostGBuffer(context, parserContext, mainTargets._msaaDepthBufferSRV, mainTargets._gbufferRTVsSRV[1]);

                ReturnToSteadyState(context);
                SceneParseSettings sceneParseSettings(SceneParseSettings::BatchFilter::Transparent, ~SceneParseSettings::Toggles::BitField(0));
                {
                    GPUProfiler::DebugAnnotation anno(*context, L"MainScene-PostGBuffer");
                    parserContext.GetSceneParser()->ExecuteScene(
                        context, parserContext, sceneParseSettings, TechniqueIndex_General);
                }
                for (auto p=parserContext._plugins.cbegin(); p!=parserContext._plugins.cend(); ++p) {
                    (*p)->OnPostSceneRender(context, parserContext, sceneParseSettings, TechniqueIndex_General);
                }

            } 
            CATCH(const ::Assets::Exceptions::InvalidAsset& e) { parserContext.Process(e); savedTargets.ResetToOldTargets(context); }
            CATCH(const ::Assets::Exceptions::PendingAsset& e) { parserContext.Process(e); savedTargets.ResetToOldTargets(context); }
            CATCH_END

            sceneDepthsSRV              = mainTargets._msaaDepthBufferSRV;
            sceneSecondaryDepthsSRV     = mainTargets._secondaryDepthBufferSRV;
            sceneDepthsDSV              = mainTargets._msaaDepthBuffer;

            postLightingResolveSRV      = lightingResTargets._lightingResolveSRV;
            postLightingResolveTexture  = lightingResTargets._lightingResolveTexture.get();
            postLightingResolveRTV      = lightingResTargets._lightingResolveRTV;

        } else if (qualitySettings._lightingModel == RenderingQualitySettings::LightingModel::Forward) {

            auto& mainTargets = Techniques::FindCachedBox<ForwardTargetsBox>(
                ForwardTargetsBox::Desc(
                    unsigned(mainViewport.Width), unsigned(mainViewport.Height),
                    FormatStack(NativeFormat(DXGI_FORMAT_R24G8_TYPELESS), 
                                NativeFormat(DXGI_FORMAT_R24_UNORM_X8_TYPELESS), 
                                NativeFormat(DXGI_FORMAT_D24_UNORM_S8_UINT)),
                    sampling));

            context->Clear(mainTargets._msaaDepthBuffer, 1.f, 0);
            context->Bind(
                MakeResourceList(lightingResTargets._lightingResolveRTV),
                &mainTargets._msaaDepthBuffer);

            ForwardLightingModel_Render(context, parserContext, mainTargets, sampling._sampleCount);

            sceneDepthsSRV              = mainTargets._msaaDepthBufferSRV;
            sceneSecondaryDepthsSRV     = mainTargets._secondaryDepthBufferSRV;
            sceneDepthsDSV              = mainTargets._msaaDepthBuffer;

            postLightingResolveSRV      = lightingResTargets._lightingResolveSRV;
            postLightingResolveTexture  = lightingResTargets._lightingResolveTexture.get();
            postLightingResolveRTV      = lightingResTargets._lightingResolveRTV;

        }

        {
            GPUProfiler::DebugAnnotation anno(*context, L"Resolve-MSAA-HDR");

            if (parserContext.GetSceneParser()->GetToneMapSettings()._flags & ToneMapSettings::Flags::EnableToneMap) {
                ToneMap_SampleLuminance(context, parserContext, postLightingResolveSRV, qualitySettings._samplingCount);   //  (must resolve luminance early, because we use it during the MSAA resolve)
            }

                //
                //      Post lighting resolve operations...
                //          we must bind the depth buffer to whatever
                //          buffer contained the correct depth information from the
                //          previous rendering (might be the default depth buffer, or might
                //          not be)
                //
            if (qualitySettings._samplingCount > 1) {
                TextureDesc2D inputTextureDesc(postLightingResolveTexture);
				auto& msaaResolveRes = Techniques::FindCachedBox<FinalResolveResources>(
					FinalResolveResources::Desc(inputTextureDesc.Width, inputTextureDesc.Height, (Metal::NativeFormat::Enum)inputTextureDesc.Format));
                LightingParser_ResolveMSAA(
                    context, parserContext,
                    msaaResolveRes._postMsaaResolveTexture.get(),
                    postLightingResolveTexture,
					(Metal::NativeFormat::Enum)inputTextureDesc.Format);

                    // todo -- also resolve the depth buffer...!
                    //      here; we switch the active textures to the msaa resolved textures
                postLightingResolveTexture = msaaResolveRes._postMsaaResolveTexture.get();
                postLightingResolveSRV = msaaResolveRes._postMsaaResolveSRV;
                postLightingResolveRTV = msaaResolveRes._postMsaaResolveTarget;
            }
            context->Bind(MakeResourceList(postLightingResolveRTV), nullptr);       // we don't have a single-sample depths target at this time (only multisample)
            LightingParser_PostProcess(context, parserContext);

                //  Write final colour to output texture
                //  We have to be careful about whether "SRGB" is enabled
                //  on the back buffer we're writing to. Depending on the
                //  tone mapping method, sometimes we want the SRGB conversion,
                //  other times we don't...

            const bool hardwareSRGBDisabled = Tweakable("Tonemap_DisableHardwareSRGB", true);
            if (hardwareSRGBDisabled) {
                auto res = ExtractResource<ID3D::Resource>(savedTargets.GetRenderTargets()[0]);
                if (res) {
                        // create a render target view with SRGB disabled (but the same colour format)
                    Metal::RenderTargetView rtv(res.get(), Metal::NativeFormat::R8G8B8A8_UNORM);
                    auto* drtv = rtv.GetUnderlying();
                    context->GetUnderlying()->OMSetRenderTargets(1, &drtv, savedTargets.GetDepthStencilView());
                }
            } else {
                savedTargets.ResetToOldTargets(context);
            }

            LightingParser_ResolveHDR(context, parserContext, postLightingResolveSRV, qualitySettings._samplingCount);

                //  if we're not in MSAA mode, we can rebind the main depth buffer. But if we're in MSAA mode, we have to
                //  resolve the depth buffer before we can do that...
            if (qualitySettings._samplingCount >= 1) {
                savedTargets.SetDepthStencilView(sceneDepthsDSV.GetUnderlying());
            }
            savedTargets.ResetToOldTargets(context);
        }

        LightingParser_Overlays(context, parserContext);
    }

    static const std::string StringShadowCascadeMode = "SHADOW_CASCADE_MODE";

    PreparedDMShadowFrustum LightingParser_PrepareDMShadow(
        DeviceContext* context, LightingParserContext& parserContext, 
        const ShadowProjectionDesc& frustum,
        unsigned shadowFrustumIndex)
    {
        auto projectionCount = std::min(frustum._projections._count, MaxShadowTexturesPerLight);
        if (!projectionCount)
            return PreparedDMShadowFrustum();

        ViewportDesc newViewport[MaxShadowTexturesPerLight];
        for (unsigned c=0; c<frustum._projections._count; ++c) {
            newViewport[c].TopLeftX = newViewport[c].TopLeftY = 0;
            newViewport[c].Width = float(frustum._width);
            newViewport[c].Height = float(frustum._height);
            newViewport[c].MinDepth = 0.f;
            newViewport[c].MaxDepth = 1.f;
        }

        PreparedDMShadowFrustum preparedResult;
        preparedResult.InitialiseConstants(context, frustum._projections);
        using TC = Techniques::TechniqueContext;
        parserContext.SetGlobalCB(*context, TC::CB_ShadowProjection, &preparedResult._arbitraryCBSource, sizeof(preparedResult._arbitraryCBSource));
        parserContext.SetGlobalCB(*context, TC::CB_OrthoShadowProjection, &preparedResult._orthoCBSource, sizeof(preparedResult._orthoCBSource));
        preparedResult._resolveParameters._worldSpaceBias = frustum._worldSpaceResolveBias;
        preparedResult._resolveParameters._tanBlurAngle = frustum._tanBlurAngle;
        preparedResult._resolveParameters._minBlurSearch = frustum._minBlurSearch;
        preparedResult._resolveParameters._maxBlurSearch = frustum._maxBlurSearch;
        preparedResult._resolveParameters._shadowTextureSize = (float)std::min(frustum._width, frustum._height);
        XlZeroMemory(preparedResult._resolveParameters._dummy);

            //  we need to set the "shadow cascade mode" settings to the right
            //  mode for this prepare step;
        parserContext.GetTechniqueContext()._runtimeState.SetParameter(
            (const utf8*)StringShadowCascadeMode.c_str(), 
            preparedResult._mode == ShadowProjectionDesc::Projections::Mode::Ortho?2:1);

            /////////////////////////////////////////////

        auto& targetsBox = Techniques::FindCachedBox2<ShadowTargetsBox>(
            frustum._width, frustum._height, MaxShadowTexturesPerLight, 
            FormatStack(frustum._typelessFormat, frustum._readFormat, frustum._writeFormat));
        auto& resources = Techniques::FindCachedBox2<ShadowWriteResources>(
            frustum._shadowSlopeScaledBias, frustum._shadowDepthBiasClamp, 
            frustum._shadowRasterDepthBias, unsigned(frustum._windingCull));

        preparedResult._shadowTextureSRV = targetsBox._shaderResource;

            /////////////////////////////////////////////

        using namespace RenderCore::Metal;
        SavedTargets savedTargets(context);

        TRY
        {
            context->Bind(RenderCore::ResourceList<RenderTargetView,0>(), &targetsBox._depthStencilView);
            context->Bind(newViewport[0]);
            context->Bind(resources._rasterizerState);
            for (unsigned c=0; c<projectionCount; ++c) {
                    // note --  do we need to clear each slice individually? Or can we clear a single DSV 
                    //          representing the whole thing? What if we don't need every slice for this 
                    //          frame? Is there a benefit to skipping unnecessary slices?
                context->Clear(targetsBox._dsvBySlice[c], 1.f, 0);  
            }

                /////////////////////////////////////////////

            Float4x4 savedWorldToProjection = parserContext.GetProjectionDesc()._worldToProjection;
            parserContext.GetProjectionDesc()._worldToProjection = frustum._worldToClip;

            SceneParseSettings sceneParseSettings(SceneParseSettings::BatchFilter::General, ~SceneParseSettings::Toggles::BitField(0));
            parserContext.GetSceneParser()->ExecuteShadowScene(
                context, parserContext, sceneParseSettings, shadowFrustumIndex, TechniqueIndex_ShadowGen);

            for (auto p=parserContext._plugins.cbegin(); p!=parserContext._plugins.cend(); ++p) {
                (*p)->OnPostSceneRender(context, parserContext, sceneParseSettings, TechniqueIndex_ShadowGen);
            }

            parserContext.GetProjectionDesc()._worldToProjection = savedWorldToProjection;
   
                /////////////////////////////////////////////
        }
        CATCH(const ::Assets::Exceptions::InvalidAsset& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingAsset& e) { parserContext.Process(e); }
        CATCH_END

        savedTargets.ResetToOldTargets(context);
        context->Bind(Techniques::CommonResources()._defaultRasterizer);

        parserContext.GetTechniqueContext()._runtimeState.SetParameter((const utf8*)StringShadowCascadeMode.c_str(), 0);

        return std::move(preparedResult);
    }

    PreparedRTShadowFrustum LightingParser_PrepareRTShadow(
        DeviceContext* context, LightingParserContext& parserContext, 
        const ShadowProjectionDesc& frustum,
        unsigned shadowFrustumIndex)
    {
        return PrepareRTShadows(*context, parserContext, frustum, shadowFrustumIndex);
    }

    void LightingParser_PrepareShadows(DeviceContext* context, LightingParserContext& parserContext)
    {
        GPUProfiler::DebugAnnotation anno(*context, L"Prepare-Shadows");

            // todo --  we should be using a temporary frame heap for this vector
        auto shadowFrustumCount = parserContext.GetSceneParser()->GetShadowProjectionCount();
        parserContext._preparedDMShadows.reserve(shadowFrustumCount);

        for (unsigned c=0; c<shadowFrustumCount; ++c) {
            auto frustum = parserContext.GetSceneParser()->GetShadowProjectionDesc(
                c, parserContext.GetProjectionDesc());

            if (frustum._resolveType == ShadowProjectionDesc::ResolveType::DepthTexture) {

                auto shadow = LightingParser_PrepareDMShadow(context, parserContext, frustum, c);
                if (shadow.IsReady())
                    parserContext._preparedDMShadows.push_back(std::make_pair(frustum._lightId, std::move(shadow)));

            } else if (frustum._resolveType == ShadowProjectionDesc::ResolveType::RayTraced) {

                auto shadow = LightingParser_PrepareRTShadow(context, parserContext, frustum, c);
                if (shadow.IsReady())
                    parserContext._preparedRTShadows.push_back(std::make_pair(frustum._lightId, std::move(shadow)));

            }
        }
    }

    void LightingParser_InitBasicLightEnv(  
        DeviceContext* context,
        LightingParserContext& parserContext,
        ISceneParser& sceneParser);

    void LightingParser_SetupScene( DeviceContext* context, 
                                    LightingParserContext& parserContext,
                                    ISceneParser* sceneParser,
                                    const RenderCore::Techniques::CameraDesc& camera,
                                    const RenderingQualitySettings& qualitySettings)
    {
        TRY {
            SetFrameGlobalStates(context, parserContext);
            ReturnToSteadyState(context);

            Float4 time(0.f, 0.f, 0.f, 0.f);
            if (sceneParser)
                time[0] = sceneParser->GetTimeValue();
            parserContext.SetGlobalCB(
                *context, Techniques::TechniqueContext::CB_GlobalState,
                &time, sizeof(time));

            if (sceneParser)
                LightingParser_InitBasicLightEnv(context, parserContext, *sceneParser);

            auto& metricsBox = Techniques::FindCachedBox<MetricsBox>(MetricsBox::Desc());
            unsigned clearValues[] = {0,0,0,0};
            context->Clear(metricsBox._metricsBufferUAV, clearValues);
            parserContext.SetMetricsBox(&metricsBox);

            LightingParser_SetGlobalTransform(
                context, parserContext, camera, qualitySettings._dimensions);
        } 
        CATCH(const ::Assets::Exceptions::InvalidAsset& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingAsset& e) { parserContext.Process(e); }
        CATCH_END
    }

    void LightingParser_ExecuteScene(
        RenderCore::IThreadContext& context, 
        LightingParserContext& parserContext,
        ISceneParser& scene,
        const RenderingQualitySettings& qualitySettings)
    {
        auto metalContext = DeviceContext::Get(context);
        parserContext.SetSceneParser(&scene);
        LightingParser_SetupScene(metalContext.get(), parserContext, &scene, scene.GetCameraDesc(), qualitySettings);

        {
            GPUProfiler::DebugAnnotation anno(*metalContext.get(), L"Prepare");
            for (auto i=parserContext._plugins.cbegin(); i!=parserContext._plugins.cend(); ++i) {
                TRY {
                    (*i)->OnPreScenePrepare(metalContext.get(), parserContext);
                }
                CATCH(const ::Assets::Exceptions::InvalidAsset& e) { parserContext.Process(e); }
                CATCH(const ::Assets::Exceptions::PendingAsset& e) { parserContext.Process(e); }
                CATCH_END
            }

            LightingParser_PrepareShadows(metalContext.get(), parserContext);
        }

        TRY {
            LightingParser_MainScene(metalContext.get(), parserContext, qualitySettings);
        }
        CATCH(const ::Assets::Exceptions::InvalidAsset& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingAsset& e) { parserContext.Process(e); }
        CATCH_END

        parserContext.SetSceneParser(nullptr);
    }

    void LightingParserContext::SetMetricsBox(MetricsBox* box)
    {
        _metricsBox = box;
    }

    LightingParserContext::LightingParserContext(const Techniques::TechniqueContext& techniqueContext)
    : ParsingContext(techniqueContext)
    , _sceneParser(nullptr)
    {
        _metricsBox = nullptr;
    }

    LightingParserContext::~LightingParserContext() {}

    void LightingParserContext::SetSceneParser(ISceneParser* sceneParser)
    {
        _sceneParser = sceneParser;
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

