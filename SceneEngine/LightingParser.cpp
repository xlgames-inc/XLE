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
#include "RayTracedShadows.h"
#include "MetalStubs.h"

#include "../RenderCore/RenderUtils.h"
#include "../RenderCore/IThreadContext.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/RenderStateResolver.h"
#include "../RenderCore/Techniques/RenderPass.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Techniques/DeferredShaderResource.h"
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

#include <map>

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
		Techniques::ParsingContext& parserContext,
        LightingParserContext& lightingParserContext,
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
		Techniques::ParsingContext& parserContext,
        ILightingParserDelegate& delegate);

    class StateSetResolvers
    {
    public:
        class Desc {};

        using Resolver = std::shared_ptr<Techniques::IRenderStateDelegate>;
        Resolver _forward, _deferred, _depthOnly;

        StateSetResolvers(const Desc&)
        {
            _forward = Techniques::CreateRenderStateDelegate_Forward();
            _deferred = Techniques::CreateRenderStateDelegate_Deferred();
            _depthOnly = Techniques::CreateRenderStateDelegate_DepthOnly();
        }
    };

    StateSetResolvers& GetStateSetResolvers() { return ConsoleRig::FindCachedBox2<StateSetResolvers>(); }

    class RenderStateDelegateChangeMarker
    {
    public:
        RenderStateDelegateChangeMarker(
            Techniques::ParsingContext& parsingContext,
            std::shared_ptr<Techniques::IRenderStateDelegate> newResolver)
        {
            _parsingContext = &parsingContext;
            _oldResolver = parsingContext.SetRenderStateDelegate(std::move(newResolver));
        }
        ~RenderStateDelegateChangeMarker()
        {
            if (_parsingContext)
                _parsingContext->SetRenderStateDelegate(std::move(_oldResolver));
        }
        RenderStateDelegateChangeMarker(const RenderStateDelegateChangeMarker&);
        RenderStateDelegateChangeMarker& operator=(const RenderStateDelegateChangeMarker&);
    private:
        std::shared_ptr<Techniques::IRenderStateDelegate> _oldResolver;
        Techniques::ParsingContext* _parsingContext;
    };

    void LightingParser_SetGlobalTransform(
        RenderCore::IThreadContext& context, 
        Techniques::ParsingContext& parserContext, 
        const RenderCore::Techniques::ProjectionDesc& projDesc)
    {
        parserContext.GetProjectionDesc() = projDesc;
        auto globalTransform = BuildGlobalTransformConstants(projDesc);
        parserContext.SetGlobalCB(
            *Metal::DeviceContext::Get(context), Techniques::TechniqueContext::CB_GlobalTransform,
            &globalTransform, sizeof(globalTransform));
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
        IThreadContext& context, 
		Techniques::ParsingContext& parserContext,
        LightingParserContext& lightingParserContext)
    {
        // nothing here yet!
    }

    void LightingParser_PreTranslucency(    
        IThreadContext& context, 
        Techniques::ParsingContext& parserContext,
        ILightingParserDelegate& delegate,
        const Metal::ShaderResourceView& depthsSRV)
    {
        GPUAnnotation anno(context, "PreTranslucency");

            // note --  these things can be executed by the scene parser? Are they better
            //          off handled by the scene parser, or the lighting parser?
        if (Tweakable("OceanDoSimulation", true)) {
            Ocean_Execute(context, parserContext, delegate, GlobalOceanSettings, GlobalOceanLightingSettings, depthsSRV);
        }

        if (Tweakable("DoSky", true)) {
            Sky_RenderPostFog(context, parserContext, delegate.GetGlobalLightingDesc());
        }

        auto gblLighting = delegate.GetGlobalLightingDesc();
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
        IThreadContext& context, 
		Techniques::ParsingContext& parserContext,
        ILightingParserDelegate& delegate,
        const Metal::ShaderResourceView& depthsSRV,
        const Metal::ShaderResourceView& normalsSRV)
    {
        GPUAnnotation anno(context, "PostGBuffer");
        
        if (Tweakable("DoRain", false)) {
            Rain_Render(context, parserContext);
            Rain_RenderSimParticles(context, parserContext, depthsSRV, normalsSRV);
        }

        if (Tweakable("DoSparks", false)) {
            SparkParticleTest_RenderSimParticles(context, parserContext, delegate.GetTimeValue(), depthsSRV, normalsSRV);
        }

        if (Tweakable("DoSun", false)) {
            SunFlare_Execute(context, parserContext, depthsSRV, delegate.GetLightDesc(0));
        }
    }

    void LightingParser_Overlays(   IThreadContext& context,
									Techniques::ParsingContext& parserContext,
                                    LightingParserContext& lightingParserContext)
    {
        GPUAnnotation anno(context, "Overlays");

		auto metalContext = Metal::DeviceContext::Get(context);
        Metal::ViewportDesc mainViewportDesc(*metalContext);
        auto& refractionBox = ConsoleRig::FindCachedBox2<RefractionsBuffer>(unsigned(mainViewportDesc.Width/2), unsigned(mainViewportDesc.Height/2));
        refractionBox.Build(*metalContext, parserContext, 4.f);
        MetalStubs::GetGlobalNumericUniforms(*metalContext, ShaderStage::Pixel).Bind(MakeResourceList(12, refractionBox.GetSRV()));

        for (auto i=parserContext._pendingOverlays.cbegin(); i!=parserContext._pendingOverlays.cend(); ++i) {
            CATCH_ASSETS_BEGIN
                (*i)(*metalContext, parserContext);
            CATCH_ASSETS_END(parserContext)
        }
                    
        if (Tweakable("FFTDebugging", false)) {
            FFT_DoDebugging(metalContext.get());
        }

        if (Tweakable("MetricsRender", false) && lightingParserContext.GetMetricsBox()) {
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
                    3, ::Assets::MakeAsset<RenderCore::Techniques::DeferredShaderResource>("xleres/DefaultResources/metricsdigits.dds:T")->Actualize()->GetShaderResource()));
                metalContext->Bind(BlendState(BlendOp::Add, Blend::One, Blend::InvSrcAlpha));
                metalContext->Bind(DepthStencilState(false));
                metalContext->GetNumericUniforms(ShaderStage::Vertex).Bind(MakeResourceList(lightingParserContext.GetMetricsBox()->_metricsBufferSRV));
                unsigned dimensions[4] = { unsigned(mainViewportDesc.Width), unsigned(mainViewportDesc.Height), 0, 0 };
                metalContext->GetNumericUniforms(ShaderStage::Vertex).Bind(MakeResourceList(MakeMetalCB(dimensions, sizeof(dimensions))));
                metalContext->GetNumericUniforms(ShaderStage::Geometry).Bind(MakeResourceList(MakeMetalCB(dimensions, sizeof(dimensions))));
                SetupVertexGeneratorShader(*metalContext);
                metalContext->Bind(Topology::PointList);
                metalContext->Draw(9);

                MetalStubs::UnbindPS<ShaderResourceView>(*metalContext, 3, 1);
                MetalStubs::UnbindVS<ShaderResourceView>(*metalContext, 0, 1);

            CATCH_ASSETS_END(parserContext)
        }
    }

    void LightingParser_PrepareShadows(
        IThreadContext& threadContext,
		RenderCore::Techniques::ParsingContext& parsingContext,
        LightingParserContext& lightingParserContext, 
		SceneExecuteContext_Main& executedScene, PreparedScene& preparedScene, IMainTargets& mainTargets);

	class ViewDrawables_Forward : public SceneExecuteContext
	{
	public:
		Techniques::DrawablesPacket _preDepth;
		Techniques::DrawablesPacket _general;
	};

	class ExecuteDrawablesContext
	{
	public:
		Techniques::SequencerTechnique _sequencerTechnique;
		ParameterBox _seqShaderSelectors;
	};

    static void ExecuteDrawables(
        IThreadContext& threadContext,
		Techniques::ParsingContext& parserContext,
		ExecuteDrawablesContext& context,
		const Techniques::DrawablesPacket& drawables,
        unsigned techniqueIndex,
		const char name[])
    {
        CATCH_ASSETS_BEGIN
            GPUAnnotation anno(threadContext, name);
			for (auto d=drawables._drawables.begin(); d!=drawables._drawables.end(); ++d)
				RenderCore::Techniques::Draw(
					threadContext, 
					parserContext,
					techniqueIndex,
					context._sequencerTechnique,
					&context._seqShaderSelectors,
					*(Techniques::Drawable*)d.get());
        CATCH_ASSETS_END(parserContext)
    }

    static bool BatchHasContent(const Techniques::DrawablesPacket& drawables)
    {
        return !drawables._drawables.empty();
    }

    static void ForwardLightingModel_Render(
        IThreadContext& threadContext,
		Techniques::ParsingContext& parserContext,
        LightingParserContext& lightingParserContext,
		ViewDrawables_Forward& executedScene,
        PreparedScene& preparedScene,
        unsigned sampleCount)
    {
		auto& metalContext = *Metal::DeviceContext::Get(threadContext);

        auto lightBindRes = LightingParser_BindLightResolveResources(metalContext, parserContext, *lightingParserContext._delegate);
        parserContext.GetTechniqueContext()._globalEnvironmentState.SetParameter((const utf8*)"SKY_PROJECTION", lightBindRes._skyTextureProjection);
        parserContext.GetTechniqueContext()._globalEnvironmentState.SetParameter((const utf8*)"HAS_DIFFUSE_IBL", lightBindRes._hasDiffuseIBL?1:0);
        parserContext.GetTechniqueContext()._globalEnvironmentState.SetParameter((const utf8*)"HAS_SPECULAR_IBL", lightBindRes._hasSpecularIBL?1:0);

            //  Order independent transparency disabled when
            //  using MSAA modes... Still some problems in related to MSAA buffers
        const bool useOrderIndependentTransparency = Tweakable("UseOITrans", false) && (sampleCount <= 1);
		assert(!useOrderIndependentTransparency);	// (order independent transparency broken during refactoring)

		ExecuteDrawablesContext executeDrawablesContext;

        ReturnToSteadyState(metalContext);

		if (BatchHasContent(executedScene._preDepth)) {
			RenderStateDelegateChangeMarker marker(parserContext, GetStateSetResolvers()._depthOnly);
			ExecuteDrawables(
				threadContext, parserContext, executeDrawablesContext, executedScene._preDepth,
				TechniqueIndex_DepthOnly, "MainScene-DepthOnly");
			ReturnToSteadyState(metalContext);
		}

            /////

        RenderStateDelegateChangeMarker marker(parserContext, GetStateSetResolvers()._forward);

            //  We must disable z write (so all shaders can be early-depth-stencil)
            //      (this is because early-depth-stencil will normally write to the depth
            //      buffer before the alpha test has been performed. The pre-depth pass
            //      will switch early-depth-stencil on and off as necessary, but in the second
            //      pass we want it on permanently because the depth reject will end up performing
            //      the same job as alpha testing)
        metalContext.Bind(Techniques::CommonResources()._dssReadOnly);

            /////
            
        ExecuteDrawables(
            threadContext, parserContext, executeDrawablesContext, executedScene._general,
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
            Sky_RenderPostFog(metalContext, parserContext, lightingParserContext.GetSceneParser()->GetGlobalLightingDesc());
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

	class ViewDrawables_Deferred
	{
	public:
		Techniques::DrawablesPacket _transparentPreDepth;		// BatchFilter::TransparentPreDepth
		Techniques::DrawablesPacket _transparent;				// BatchFilter::Transparent
		Techniques::DrawablesPacket _oiTransparent;				// BatchFilter::OITransparent
	};

    static void LightingParser_DeferredPostGBuffer(
        IThreadContext& context,
		Techniques::ParsingContext& parserContext,
        LightingParserContext& lightingParserContext,
		ViewDrawables_Deferred& executedScene,
        PreparedScene& preparedScene,
        IMainTargets& mainTargets)
    {
        //////////////////////////////////////////////////////////////////////////////////////////////////
            //  Render translucent objects (etc)
            //  everything after the gbuffer resolve
        LightingParser_PreTranslucency(
            context, parserContext, *lightingParserContext._delegate,
            mainTargets.GetSRV(IMainTargets::MultisampledDepth));

		auto& metalContext = *RenderCore::Metal::DeviceContext::Get(context);
        ReturnToSteadyState(metalContext);

		ExecuteDrawablesContext executeDrawablesContext;

            // We must bind all of the lighting resolve resources here
            //  -- because we'll be doing lighting operations in the pixel
            //      shaders in a forward-lit way
        auto lightBindRes = LightingParser_BindLightResolveResources(metalContext, parserContext, delegate);
        parserContext.GetTechniqueContext()._globalEnvironmentState.SetParameter((const utf8*)"SKY_PROJECTION", lightBindRes._skyTextureProjection);
        parserContext.GetTechniqueContext()._globalEnvironmentState.SetParameter((const utf8*)"HAS_DIFFUSE_IBL", lightBindRes._hasDiffuseIBL?1:0);
        parserContext.GetTechniqueContext()._globalEnvironmentState.SetParameter((const utf8*)"HAS_SPECULAR_IBL", lightBindRes._hasSpecularIBL?1:0);

        AutoCleanup bindShadowsCleanup;
        if (!lightingParserContext._preparedDMShadows.empty()) {
            BindShadowsForForwardResolve(metalContext, parserContext, lightingParserContext._preparedDMShadows[0].second);
            bindShadowsCleanup = MakeAutoCleanup(
                [&metalContext, &parserContext]() 
                { UnbindShadowsForForwardResolve(metalContext, parserContext); });
        }
                    
        //////////////////////////////////////////////////////////////////////////////////////////////////
        const bool hasOITrans = BatchHasContent(executedScene._oiTransparent);

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
        if (oiMode == OIMode::SortedRef && Tweakable("TransPrePass", false) && BatchHasContent(executedScene._transparentPreDepth)) {
            RenderStateDelegateChangeMarker marker(parserContext, GetStateSetResolvers()._depthOnly);
            ExecuteDrawables(
                context, parserContext, executeDrawablesContext,
                executedScene._transparentPreDepth,
                TechniqueIndex_DepthOnly, "MainScene-TransPreDepth");
        }

        if (BatchHasContent(executedScene._transparent)) {
            RenderStateDelegateChangeMarker marker(parserContext, GetStateSetResolvers()._forward);
            ExecuteDrawables(
                context, parserContext, executeDrawablesContext,
                executedScene._transparent,
                TechniqueIndex_General, "MainScene-PostGBuffer");
        }
        
#if 0 // platformtemp
        if (hasOITrans) {
            if (oiMode == OIMode::SortedRef) {
                RenderStateDelegateChangeMarker marker(parserContext, GetStateSetResolvers()._depthOnly);
                auto duplicatedDepthBuffer = BuildDuplicatedDepthBuffer(&metalContext, *mainTargets._msaaDepthBufferTexture->GetUnderlying());
                auto* transTargets = OrderIndependentTransparency_Prepare(metalContext, parserContext, duplicatedDepthBuffer);

                ExecuteScene(
                    context, parserContext, SPS::BatchFilter::OITransparent,
                    preparedScene,
                    TechniqueIndex_OrderIndependentTransparency, "MainScene-PostGBuffer-OI");

                    // note; we use the main depth buffer for this call (not the duplicated buffer)
                OrderIndependentTransparency_Resolve(metalContext, parserContext, *transTargets, mainTargets.GetSRV(IMainTargets::MultisampledDepth));
            } else if (oiMode == OIMode::Stochastic) {
                RenderStateDelegateChangeMarker marker(parserContext, GetStateSetResolvers()._depthOnly);
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
                RenderStateDelegateChangeMarker marker(parserContext, GetStateSetResolvers()._depthOnly);
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
                RenderStateDelegateChangeMarker marker(parserContext, GetStateSetResolvers()._forward);
                ExecuteScene(
                    context, parserContext, SPS::BatchFilter::OITransparent,
                    preparedScene,
                    TechniqueIndex_General, "MainScene-PostGBuffer-OI");
            }
        }
#endif

        //////////////////////////////////////////////////////////////////////////////////////////////////
        LightingParser_PostGBufferEffects(
            context, parserContext, delegate,
            mainTargets.GetSRV(IMainTargets::MultisampledDepth), 
            mainTargets.GetSRV(IMainTargets::GBufferNormals));

        for (auto p=lightingParserContext._plugins.cbegin(); p!=lightingParserContext._plugins.cend(); ++p)
            (*p)->OnPostSceneRender(context, parserContext, lightingParserContext, delegate, BatchFilter::Transparent, TechniqueIndex_General);
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
        std::map<AttachmentName, AttachmentDesc> _attachments;

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
        _attachments[IMainTargets::MultisampledDepth] =
            // Main multisampled depth stencil
            {   RenderCore::Format::D24_UNORM_S8_UINT, 1.f, 1.f, 0u,		// ,
                TextureViewDesc::Aspect::DepthStencil,
				AttachmentDesc::DimensionsMode::OutputRelative, 
                AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::ShaderResource | AttachmentDesc::Flags::DepthStencil };

                // Generally the deferred pixel shader will just copy information from the albedo
                // texture into the first deferred buffer. So the first deferred buffer should
                // have the same pixel format as much input textures.
                // Usually this is an 8 bit SRGB format, so the first deferred buffer should also
                // be 8 bit SRGB. So long as we don't do a lot of processing in the deferred pixel shader
                // that should be enough precision.
                //      .. however, it possible some clients might prefer 10 or 16 bit albedo textures
                //      In these cases, the first buffer should be a matching format.
		_attachments[IMainTargets::GBufferDiffuse] =
            {   (!desc._precisionTargets) ? Format::R8G8B8A8_UNORM_SRGB : Format::R32G32B32A32_FLOAT,
				1.f, 1.f, 0u,
                (!desc._precisionTargets) ? TextureViewDesc::Aspect::ColorSRGB : TextureViewDesc::Aspect::ColorLinear,
				AttachmentDesc::DimensionsMode::OutputRelative,
                AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::ShaderResource | AttachmentDesc::Flags::RenderTarget };

		_attachments[IMainTargets::GBufferNormals] =
            {   (!desc._precisionTargets) ? Format::R8G8B8A8_SNORM : Format::R32G32B32A32_FLOAT,
				1.f, 1.f, 0u,
                TextureViewDesc::Aspect::ColorLinear,
				AttachmentDesc::DimensionsMode::OutputRelative,
                AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::ShaderResource | AttachmentDesc::Flags::RenderTarget };

		_attachments[IMainTargets::GBufferParameters] =
            {   (!desc._precisionTargets) ? Format::R8G8B8A8_UNORM : Format::R32G32B32A32_FLOAT,
				1.f, 1.f, 0u,
                TextureViewDesc::Aspect::ColorLinear,
				AttachmentDesc::DimensionsMode::OutputRelative,
                AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::ShaderResource | AttachmentDesc::Flags::RenderTarget };

        if (desc._gbufferMode == 1) {

			SubpassDesc subpasses[] = {
				SubpassDesc {
					std::vector<AttachmentViewDesc> {
						{ IMainTargets::GBufferDiffuse, LoadStore::Clear, LoadStore::Retain },
						{ IMainTargets::GBufferNormals, LoadStore::Clear, LoadStore::Retain },
						{ IMainTargets::GBufferParameters, LoadStore::Clear, LoadStore::Retain }
					},
					{IMainTargets::MultisampledDepth, LoadStore::Clear_ClearStencil, LoadStore::Retain}
				}
			};

            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            _createGBuffer = (FrameBufferDesc)MakeIteratorRange(subpasses);
            
        } else {

			SubpassDesc subpasses[] = {
				SubpassDesc {
					std::vector<AttachmentViewDesc> {
						{ IMainTargets::GBufferDiffuse, LoadStore::DontCare, LoadStore::Retain },
						{ IMainTargets::GBufferNormals, LoadStore::DontCare, LoadStore::Retain },
					},
					{IMainTargets::MultisampledDepth, LoadStore::Clear_ClearStencil, LoadStore::Retain}
				}
			};

            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            _createGBuffer = (FrameBufferDesc)MakeIteratorRange(subpasses);

        }
    }

    class MainTargets : public IMainTargets
    {
    public:
        unsigned                        GetGBufferType() const;
        RenderCore::TextureSamples      GetSampling() const;
        const RenderSceneSettings&		GetRenderSceneSettings() const;
        UInt2							GetDimensions() const;
		const SRV&                      GetSRV(Name, const TextureViewDesc& window = {}) const;
        bool                            HasSRV(Name) const;

        MainTargets(
            Techniques::ParsingContext& parsingContext,
            const RenderSceneSettings& qualSettings,
            unsigned gbufferType);
        ~MainTargets();

    private:
        Techniques::ParsingContext* _parsingContext;
        RenderSceneSettings			_qualSettings;
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

    const RenderSceneSettings& MainTargets::GetRenderSceneSettings() const
    {
        return _qualSettings;
    }

    UInt2 MainTargets::GetDimensions() const
    {
        return _qualSettings._dimensions;
    }

    auto  MainTargets::GetSRV(Name name, const TextureViewDesc& window) const -> const SRV&
    {
        auto result = _parsingContext->GetNamedResources().GetSRV(name, window);
        assert(result);
        return *result;
    }

    bool MainTargets::HasSRV(Name name) const
    {
        return _parsingContext->GetNamedResources().GetSRV(name) != nullptr;
    }

    MainTargets::MainTargets(
        Techniques::ParsingContext& parsingContext,
        const RenderSceneSettings& qualSettings,
        unsigned gbufferType)
    : _parsingContext(&parsingContext)
    , _qualSettings(qualSettings)
    , _gbufferType(gbufferType)
    {}

    MainTargets::~MainTargets() {}

    void LightingParser_MainScene(
        IThreadContext& context,
        Metal::DeviceContext& metalContext, 
		Techniques::ParsingContext& parserContext,
        LightingParserContext& lightingParserContext,
        PreparedScene& preparedScene,
        MainTargets& mainTargets,
        const RenderSceneSettings& qualitySettings)
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

        if (qualitySettings._lightingModel == RenderSceneSettings::LightingModel::Deferred) {

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

			for (const auto&a:fbDescBox._attachments)
				parserContext.GetNamedResources().DefineAttachment(a.first, a.second);

            ReturnToSteadyState(metalContext);
            RenderStateDelegateChangeMarker marker(parserContext, GetStateSetResolvers()._deferred);

            CATCH_ASSETS_BEGIN {
				auto fb = parserContext.GetFrameBufferPool().BuildFrameBuffer(fbDescBox._createGBuffer, parserContext.GetNamedResources());
				ClearValue clearValues[] = {
					RenderCore::MakeClearValue(0.f, 0.f, 0.f),
					RenderCore::MakeClearValue(0.f, 0.f, 0.f, 0.f),
					RenderCore::MakeClearValue(0.f, 0.f, 0.f, 0.f),
					RenderCore::MakeClearValue(1.f, 0)};
                Techniques::RenderPassInstance rpi(
                    context, fb, fbDescBox._createGBuffer,
                    parserContext.GetNamedResources(),
                    (Techniques::RenderPassBeginDesc)MakeIteratorRange(clearValues));
                metalContext.Bind(Metal::ViewportDesc(0.f, 0.f, (float)qualitySettings._dimensions[0], (float)qualitySettings._dimensions[1]));

                ExecuteScene(
                    context, parserContext, lightingParserContext, sceneParser, SPS::BatchFilter::General,
                    preparedScene,
                    TechniqueIndex_Deferred, "MainScene-OpaqueGBuffer");
            } CATCH_ASSETS_END(parserContext)

            for (auto p=lightingParserContext._plugins.cbegin(); p!=lightingParserContext._plugins.cend(); ++p)
                (*p)->OnPostSceneRender(context, parserContext, lightingParserContext, delegate, BatchFilter::General, TechniqueIndex_Deferred);

                //
            //////////////////////////////////////////////////////////////////////////////////////
                //      Now resolve lighting
                //

            CATCH_ASSETS_BEGIN {
                LightingParser_ResolveGBuffer(context, parserContext, lightingParserContext, mainTargets);
            } CATCH_ASSETS_END(parserContext)

                // Post lighting resolve operations... (must rebind the depth buffer)
            metalContext.Bind(Techniques::CommonResources()._dssReadOnly);

            CATCH_ASSETS_BEGIN
                LightingParser_DeferredPostGBuffer(context, parserContext, lightingParserContext, sceneParser, preparedScene, mainTargets);
            CATCH_ASSETS_END(parserContext)

        } else if (qualitySettings._lightingModel == RenderSceneSettings::LightingModel::Forward) {

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

            const auto sampleCount = mainTargets.GetSampling()._sampleCount;
			ForwardLightingModel_Render(context, parserContext, lightingParserContext, preparedScene, sampleCount);
#endif

        } else if (qualitySettings._lightingModel == RenderSceneSettings::LightingModel::Direct) {

			auto rpi = RenderPassToPresentationTarget(context, parserContext);
			metalContext.Bind(Metal::ViewportDesc{0.f, 0.f, (float)qualitySettings._dimensions[0], (float)qualitySettings._dimensions[1]});

			if (!lightingParserContext._preparedDMShadows.empty())
                BindShadowsForForwardResolve(metalContext, parserContext, lightingParserContext._preparedDMShadows[0].second);

			const auto sampleCount = mainTargets.GetSampling()._sampleCount;
			ForwardLightingModel_Render(context, parserContext, lightingParserContext, sceneParser, preparedScene, sampleCount);

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

            auto toneMapSettings = delegate.GetToneMapSettings();
            LuminanceResult luminanceResult;
            if (toneMapSettings._flags & ToneMapSettings::Flags::EnableToneMap) {
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

			SubpassDesc subpasses[] = {
				SubpassDesc {
					{ {IMainTargets::PresentationTarget, LoadStore::DontCare, LoadStore::Retain} },
				}
			};
            FrameBufferDesc applyToneMapping = MakeIteratorRange(subpasses);
            
            ToneMap_Execute(
                context, parserContext, luminanceResult, toneMapSettings, 
                applyToneMapping,
                mainTargets.GetSRV(postLightingResolve));
        }
    }

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

    void LightingParser_InitBasicLightEnv(  
        RenderCore::IThreadContext& context,
        Techniques::ParsingContext& parserContext,
		LightingParserContext& lightingParserContext);

    static LightingParserContext LightingParser_SetupContext(
        RenderCore::IThreadContext& context, 
		RenderCore::Techniques::ParsingContext& parserContext,
        ILightingParserDelegate& delegate,
		const RenderSceneSettings& qualitySettings,
        unsigned samplingPassIndex = 0, unsigned samplingPassCount = 1)
    {
        struct GlobalCBuffer
        {
            float _time; unsigned _samplingPassIndex; 
            unsigned _samplingPassCount; unsigned _dummy;
        } time { delegate.GetTimeValue(), samplingPassIndex, samplingPassCount, 0 };
		auto& metalContext = *Metal::DeviceContext::Get(context);
        parserContext.SetGlobalCB(
            metalContext, Techniques::TechniqueContext::CB_GlobalState,
            &time, sizeof(time));

		LightingParserContext lightingParserContext;
		lightingParserContext._delegate = &delegate;
		lightingParserContext._plugins.insert(
			lightingParserContext._plugins.end(),
			qualitySettings._lightingPlugins.begin(), qualitySettings._lightingPlugins.end());
        LightingParser_InitBasicLightEnv(context, parserContext, lightingParserContext, delegate);

        auto& metricsBox = ConsoleRig::FindCachedBox2<MetricsBox>();
        metalContext.ClearUInt(metricsBox._metricsBufferUAV, { 0,0,0,0 });
        lightingParserContext.SetMetricsBox(&metricsBox);

        return lightingParserContext;
    }

	class SceneExecuteContext_Main : public SceneExecuteContext
	{
	public:
		unsigned _shadowViewCount;
		PreparedScene _preparedScene;
	};

    static void LightingParser_Render(
        RenderCore::IThreadContext& context, 
		RenderCore::IResource& renderTarget,
        Techniques::ParsingContext& parserContext,
		LightingParserContext& lightingParserContext,
        const RenderSceneSettings& qualitySettings,
		SceneExecuteContext_Main& executedScene)
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
            for (auto i=lightingParserContext._plugins.cbegin(); i!=lightingParserContext._plugins.cend(); ++i) {
                CATCH_ASSETS_BEGIN
                    (*i)->OnPreScenePrepare(context, parserContext, lightingParserContext, sceneParser, preparedScene);
                CATCH_ASSETS_END(parserContext)
            }

            LightingParser_PrepareShadows(context, parserContext, lightingParserContext, &sceneParser, preparedScene, mainTargets);
        }

        GetBufferUploads().Update(context, true);

        CATCH_ASSETS_BEGIN
            LightingParser_MainScene(context, *metalContext, parserContext, lightingParserContext, sceneParser, preparedScene, mainTargets, qualitySettings);
        CATCH_ASSETS_END(parserContext)
    }

	LightingParserContext LightingParser_ExecuteScene(
        RenderCore::IThreadContext& context, 
		RenderCore::IResource& renderTarget,
        Techniques::ParsingContext& parserContext,
        IScene& scene,
		ILightingParserDelegate& delegate,
        const RenderCore::Techniques::CameraDesc& camera,
        const RenderSceneSettings& renderSettings)
    {
		SceneExecuteContext_Main executeContext;
        scene.ExecuteScene(context, executeContext);

        // Throw in a "frame priority barrier" here, right after the prepare scene. This will
        // force all uploads started during PrepareScene to be completed when we next call
        // bufferUploads.Update(). Normally we want a gap between FramePriority_Barrier() and
        // Update(), because this gives some time for the background thread to run.
        GetBufferUploads().FramePriority_Barrier();

		auto lightingParserContext = LightingParser_SetupContext(context, parserContext, delegate, renderSettings);
        LightingParser_SetGlobalTransform(
            context, parserContext, 
            RenderCore::Techniques::BuildProjectionDesc(camera, renderSettings._dimensions));
        LightingParser_Render(context, renderTarget, parserContext, lightingParserContext, renderSettings, executeContext);

		return lightingParserContext;
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

	LightingParserContext::LightingParserContext() {}
	LightingParserContext::~LightingParserContext() {}
	LightingParserContext::LightingParserContext(LightingParserContext&& moveFrom)
	: _preparedDMShadows(std::move(moveFrom._preparedDMShadows))
	, _preparedRTShadows(std::move(moveFrom._preparedRTShadows))
	, _plugins(std::move(moveFrom._plugins))
	, _metricsBox(moveFrom._metricsBox)
	{
		moveFrom._metricsBox = nullptr;
	}

	LightingParserContext& LightingParserContext::operator=(LightingParserContext&& moveFrom)
	{
		_preparedDMShadows = std::move(moveFrom._preparedDMShadows);
		_preparedRTShadows = std::move(moveFrom._preparedRTShadows);
		_plugins = std::move(moveFrom._plugins);
		_metricsBox = moveFrom._metricsBox;
		moveFrom._metricsBox = nullptr;
		return *this;
	}

    IScene::~IScene() {}
	ILightingParserDelegate::~ILightingParserDelegate() {}

}

