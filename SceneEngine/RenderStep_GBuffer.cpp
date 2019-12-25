// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)


#include "RenderStep.h"
#include "LightingTargets.h"
#include "LightingParserContext.h"
#include "SceneEngineUtils.h"
#include "RenderStepUtils.h"
#include "LightingParser.h"
#include "LightInternal.h"

#include "Ocean.h"
#include "DeepOceanSim.h"
#include "Sky.h"
#include "Tonemap.h"
#include "Rain.h"
#include "SunFlare.h"

#include "../RenderCore/Techniques/RenderPass.h"
#include "../RenderCore/Techniques/CommonBindings.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Format.h"
#include "../RenderCore/FrameBufferDesc.h"
#include "../RenderCore/IAnnotator.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../ConsoleRig/Console.h"
#include "../Utility/FunctionUtils.h"

namespace SceneEngine
{
	using namespace RenderCore;

	DeepOceanSimSettings GlobalOceanSettings; 
    OceanLightingSettings GlobalOceanLightingSettings; 

	void LightingParser_ResolveGBuffer( 
        IThreadContext& context,
		Techniques::ParsingContext& parserContext,
        LightingParserContext& lightingParserContext);
	
	const RenderCore::Techniques::FrameBufferDescFragment& RenderStep_GBuffer::GetInterface() const
	{
		return _createGBuffer;
	}

	RenderStep_GBuffer::RenderStep_GBuffer(unsigned gbufferType, bool precisionTargets)
	: _gbufferType(gbufferType)
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
        auto msDepth = _createGBuffer.DefineAttachment(
			Techniques::AttachmentSemantics::MultisampleDepth,
            // Main multisampled depth stencil
            {   RenderCore::Format::D24_UNORM_S8_UINT, 1.f, 1.f, 0u,		// ,
				AttachmentDesc::DimensionsMode::OutputRelative, 
                AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::ShaderResource | AttachmentDesc::Flags::DepthStencil });

                // Generally the deferred pixel shader will just copy information from the albedo
                // texture into the first deferred buffer. So the first deferred buffer should
                // have the same pixel format as much input textures.
                // Usually this is an 8 bit SRGB format, so the first deferred buffer should also
                // be 8 bit SRGB. So long as we don't do a lot of processing in the deferred pixel shader
                // that should be enough precision.
                //      .. however, it possible some clients might prefer 10 or 16 bit albedo textures
                //      In these cases, the first buffer should be a matching format.
		auto diffuse = _createGBuffer.DefineAttachment(
			Techniques::AttachmentSemantics::GBufferDiffuse,
            {   (!precisionTargets) ? Format::R8G8B8A8_UNORM_SRGB : Format::R32G32B32A32_FLOAT,
				1.f, 1.f, 0u,
				AttachmentDesc::DimensionsMode::OutputRelative,
                AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::ShaderResource | AttachmentDesc::Flags::RenderTarget });

		auto normal = _createGBuffer.DefineAttachment(
			Techniques::AttachmentSemantics::GBufferNormal,
            {   (!precisionTargets) ? Format::R8G8B8A8_SNORM : Format::R32G32B32A32_FLOAT,
				1.f, 1.f, 0u,
				AttachmentDesc::DimensionsMode::OutputRelative,
                AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::ShaderResource | AttachmentDesc::Flags::RenderTarget });

		auto parameter = _createGBuffer.DefineAttachment(
			Techniques::AttachmentSemantics::GBufferParameter,
            {   (!precisionTargets) ? Format::R8G8B8A8_UNORM : Format::R32G32B32A32_FLOAT,
				1.f, 1.f, 0u,
				AttachmentDesc::DimensionsMode::OutputRelative,
                AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::ShaderResource | AttachmentDesc::Flags::RenderTarget });

		auto diffuseAspect = (!precisionTargets) ? TextureViewDesc::Aspect::ColorSRGB : TextureViewDesc::Aspect::ColorLinear;

        if (gbufferType == 1) {

			_createGBuffer.AddSubpass(
				SubpassDesc {
					std::vector<AttachmentViewDesc> {
						{ diffuse, LoadStore::Clear, LoadStore::Retain, {diffuseAspect} },
						{ normal, LoadStore::Clear, LoadStore::Retain },
						{ parameter, LoadStore::Clear, LoadStore::Retain }
					},
					{msDepth, LoadStore::Clear_ClearStencil, LoadStore::Retain}
				});
            
        } else {

			_createGBuffer.AddSubpass(
				SubpassDesc {
					std::vector<AttachmentViewDesc> {
						{ diffuse, LoadStore::DontCare, LoadStore::Retain, {diffuseAspect} },
						{ normal, LoadStore::DontCare, LoadStore::Retain },
					},
					{msDepth, LoadStore::Clear_ClearStencil, LoadStore::Retain}
				});

        }
	}

	RenderStep_GBuffer::~RenderStep_GBuffer() {}

	class ViewDelegate_Deferred : public IViewDelegate
	{
	public:
		Techniques::DrawablesPacket _transparentPreDepth;		// BatchFilter::TransparentPreDepth
		Techniques::DrawablesPacket _transparent;				// BatchFilter::Transparent
		Techniques::DrawablesPacket _oiTransparent;				// BatchFilter::OITransparent

		Techniques::DrawablesPacket _gbufferOpaque;				// BatchFilter::General

		RenderCore::Techniques::DrawablesPacket* GetDrawablesPacket(Techniques::BatchFilter batch) 
		{
			switch (batch) {
			case Techniques::BatchFilter::General:
				return &_gbufferOpaque;

			case Techniques::BatchFilter::Transparent:
				return &_transparent;

			case Techniques::BatchFilter::OITransparent:
				return &_oiTransparent;

			case Techniques::BatchFilter::TransparentPreDepth:
				return &_transparentPreDepth;

			case Techniques::BatchFilter::PreDepth:
			default:
				return nullptr;
			}
		}
	};

	std::shared_ptr<IViewDelegate> RenderStep_GBuffer::CreateViewDelegate()
	{
		return std::make_shared<ViewDelegate_Deferred>();
	}

	/*
			auto fb = parsingContext.GetFrameBufferPool().BuildFrameBuffer(fbDescBox._createGBuffer, parsingContext.GetNamedResources());
				ClearValue clearValues[] = {
					RenderCore::MakeClearValue(0.f, 0.f, 0.f),
					RenderCore::MakeClearValue(0.f, 0.f, 0.f, 0.f),
					RenderCore::MakeClearValue(0.f, 0.f, 0.f, 0.f),
					RenderCore::MakeClearValue(1.f, 0)};
                Techniques::RenderPassInstance rpi(
                    context, fb, fbDescBox._createGBuffer,
                    parsingContext.GetNamedResources(),
                    (Techniques::RenderPassBeginDesc)MakeIteratorRange(clearValues));
                metalContext.Bind(Metal::ViewportDesc(0.f, 0.f, (float)qualitySettings._dimensions[0], (float)qualitySettings._dimensions[1]));
	*/

	void RenderStep_GBuffer::Execute(
		RenderCore::IThreadContext& threadContext,
		RenderCore::Techniques::ParsingContext& parsingContext,
		LightingParserContext& lightingParserContext,
		RenderCore::Techniques::RenderPassFragment& rpi,
		IViewDelegate* viewDelegate)
	{
		auto& metalContext = *Metal::DeviceContext::Get(threadContext);

		assert(viewDelegate);
		const auto& drawables = *checked_cast<ViewDelegate_Deferred*>(viewDelegate);

			//
        //////////////////////////////////////////////////////////////////////////////////////
            //      Get the gbuffer render targets for this frame
            //

        auto& globalState = parsingContext.GetTechniqueContext()._globalEnvironmentState;
        globalState.SetParameter((const utf8*)"GBUFFER_TYPE", _gbufferType);

            //
        //////////////////////////////////////////////////////////////////////////////////////
            //      Bind the gbuffer, begin the render pass
            //

		ReturnToSteadyState(metalContext);

        CATCH_ASSETS_BEGIN {
			RenderStateDelegateChangeMarker marker(parsingContext, GetStateSetResolvers()._deferred);
			ExecuteDrawablesContext executeDrawablesContext(parsingContext);
			ExecuteDrawables(
				threadContext, parsingContext, executeDrawablesContext,
				drawables._gbufferOpaque,
				TechniqueIndex_Deferred, "MainScene-OpaqueGBuffer");
        } CATCH_ASSETS_END(parsingContext)

        for (auto p=lightingParserContext._plugins.cbegin(); p!=lightingParserContext._plugins.cend(); ++p)
            (*p)->OnPostSceneRender(threadContext, parsingContext, lightingParserContext, Techniques::BatchFilter::General, TechniqueIndex_Deferred);

            //
        //////////////////////////////////////////////////////////////////////////////////////
            //      Now resolve lighting
            //

        /*CATCH_ASSETS_BEGIN {
            LightingParser_ResolveGBuffer(threadContext, parsingContext, lightingParserContext);
        } CATCH_ASSETS_END(parsingContext)

            // Post lighting resolve operations... (must rebind the depth buffer)
        metalContext.Bind(Techniques::CommonResources()._dssReadOnly);

        CATCH_ASSETS_BEGIN
            LightingParser_DeferredPostGBuffer(threadContext, parsingContext, lightingParserContext, sceneParser, preparedScene, mainTargets);
        CATCH_ASSETS_END(parsingContext)*/
	}

	void LightingParser_PreTranslucency(
        IThreadContext& context, 
        Techniques::ParsingContext& parserContext,
        const ILightingParserDelegate& delegate,
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
        const ILightingParserDelegate& delegate,
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

	static void LightingParser_DeferredPostGBuffer(
        IThreadContext& context,
		Techniques::ParsingContext& parserContext,
        LightingParserContext& lightingParserContext,
		ViewDelegate_Deferred& executedScene)
    {
		auto& mainTargets = lightingParserContext.GetMainTargets();

        //////////////////////////////////////////////////////////////////////////////////////////////////
            //  Render translucent objects (etc)
            //  everything after the gbuffer resolve
        LightingParser_PreTranslucency(
            context, parserContext, *lightingParserContext._delegate,
            mainTargets.GetSRV(parserContext, Techniques::AttachmentSemantics::MultisampleDepth));

		auto& metalContext = *RenderCore::Metal::DeviceContext::Get(context);
        ReturnToSteadyState(metalContext);

            // We must bind all of the lighting resolve resources here
            //  -- because we'll be doing lighting operations in the pixel
            //      shaders in a forward-lit way
        auto lightBindRes = LightingParser_BindLightResolveResources(metalContext, parserContext, *lightingParserContext._delegate);
		if (lightBindRes._skyTextureProjection != ~0u) {
			parserContext.GetTechniqueContext()._globalEnvironmentState.SetParameter((const utf8*)"SKY_PROJECTION", lightBindRes._skyTextureProjection);
			parserContext.GetTechniqueContext()._globalEnvironmentState.SetParameter((const utf8*)"HAS_DIFFUSE_IBL", lightBindRes._hasDiffuseIBL?1:0);
			parserContext.GetTechniqueContext()._globalEnvironmentState.SetParameter((const utf8*)"HAS_SPECULAR_IBL", lightBindRes._hasSpecularIBL?1:0);
		}

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
        case 3:     oiMode = (mainTargets.GetSamplingCount(parserContext) <= 1) ? OIMode::SortedRef : OIMode::Stochastic; break;
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
			ExecuteDrawablesContext executeDrawablesContext(parserContext);
            ExecuteDrawables(
                context, parserContext, executeDrawablesContext,
                executedScene._transparentPreDepth,
                TechniqueIndex_DepthOnly, "MainScene-TransPreDepth");
        }

        if (BatchHasContent(executedScene._transparent)) {
            RenderStateDelegateChangeMarker marker(parserContext, GetStateSetResolvers()._forward);
			ExecuteDrawablesContext executeDrawablesContext(parserContext);
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
                    context, parserContext, Techniques::BatchFilter::OITransparent,
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
                    context, parserContext, Techniques::BatchFilter::OITransparent,
                    preparedScene,
                    TechniqueIndex_DepthOnly, "MainScene-PostGBuffer-OI");

                stochTransOp.PrepareSecondPass(mainTargets._msaaDepthBuffer);
                ExecuteScene(
                    context, parserContext, Techniques::BatchFilter::OITransparent,
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
                    context, parserContext, Techniques::BatchFilter::OITransparent,
                    preparedScene,
                    TechniqueIndex_DepthWeightedTransparency, "MainScene-PostGBuffer-OI");

                resetMarker = SavedTargets::ResetMarker();  // back to normal targets now
                transOp.Resolve();
            } else if (oiMode == OIMode::Unordered) {
                RenderStateDelegateChangeMarker marker(parserContext, GetStateSetResolvers()._forward);
                ExecuteScene(
                    context, parserContext, Techniques::BatchFilter::OITransparent,
                    preparedScene,
                    TechniqueIndex_General, "MainScene-PostGBuffer-OI");
            }
        }
#endif

        //////////////////////////////////////////////////////////////////////////////////////////////////
        LightingParser_PostGBufferEffects(
            context, parserContext, *lightingParserContext._delegate,
            mainTargets.GetSRV(parserContext, Techniques::AttachmentSemantics::MultisampleDepth), 
            mainTargets.GetSRV(parserContext, Techniques::AttachmentSemantics::GBufferNormal));

        for (auto p=lightingParserContext._plugins.cbegin(); p!=lightingParserContext._plugins.cend(); ++p)
            (*p)->OnPostSceneRender(context, parserContext, lightingParserContext, Techniques::BatchFilter::Transparent, TechniqueIndex_General);
    }
}
