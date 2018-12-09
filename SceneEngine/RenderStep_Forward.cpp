// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RenderStep.h"
#include "LightingParserContext.h"
#include "LightInternal.h"
#include "SceneEngineUtils.h"
#include "RenderStepUtils.h"
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

	class ViewDelegate_Forward : public IViewDelegate
	{
	public:
		Techniques::DrawablesPacket _preDepth;
		Techniques::DrawablesPacket _general;

		RenderCore::Techniques::DrawablesPacket* GetDrawablesPacket(Techniques::BatchFilter batch)
		{
			switch (batch) {
			case Techniques::BatchFilter::General:
				return &_general;
			case Techniques::BatchFilter::PreDepth:
				return &_preDepth;
			default:
				return nullptr;
			}
		}
	};

	const RenderCore::Techniques::FrameBufferDescFragment& RenderStep_Forward::GetInterface() const
	{
		return _forward;
	}

	std::shared_ptr<IViewDelegate> RenderStep_Forward::CreateViewDelegate()
	{
		return std::make_shared<ViewDelegate_Forward>();
	}

	RenderStep_Forward::RenderStep_Forward(bool precisionTargets)
	{
		AttachmentDesc lightResolveAttachmentDesc =
			{	(!precisionTargets) ? Format::R16G16B16A16_FLOAT : Format::R32G32B32A32_FLOAT,
				1.f, 1.f, 0u,
				TextureViewDesc::Aspect::ColorLinear,AttachmentDesc::DimensionsMode::OutputRelative,
				AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::ShaderResource | AttachmentDesc::Flags::RenderTarget };

		AttachmentDesc msDepthDesc =
            {   RenderCore::Format::D24_UNORM_S8_UINT, 1.f, 1.f, 0u,		// ,
                TextureViewDesc::Aspect::DepthStencil,
				AttachmentDesc::DimensionsMode::OutputRelative, 
                AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::ShaderResource | AttachmentDesc::Flags::DepthStencil };

        auto output = _forward.DefineAttachment(Techniques::AttachmentSemantics::ColorHDR, lightResolveAttachmentDesc);
		auto depth = _forward.DefineAttachment(Techniques::AttachmentSemantics::MultisampleDepth, msDepthDesc);

		_forward.AddSubpass(
			SubpassDesc {
				std::vector<AttachmentViewDesc> {
					{ output, LoadStore::Clear, LoadStore::Retain },
				},
				{depth, LoadStore::Clear_ClearStencil, LoadStore::Retain}
			});
	}

	RenderStep_Forward::~RenderStep_Forward() {}

	

	static void ForwardLightingModel_Render(
        IThreadContext& threadContext,
		Techniques::ParsingContext& parsingContext,
        LightingParserContext& lightingParserContext,
		ViewDelegate_Forward& executedScene)
    {
		auto& metalContext = *Metal::DeviceContext::Get(threadContext);

		if (!lightingParserContext._preparedDMShadows.empty())
            BindShadowsForForwardResolve(metalContext, parsingContext, lightingParserContext._preparedDMShadows[0].second);
        auto lightBindRes = LightingParser_BindLightResolveResources(metalContext, parsingContext, *lightingParserContext._delegate);
        parsingContext.GetTechniqueContext()._globalEnvironmentState.SetParameter((const utf8*)"SKY_PROJECTION", lightBindRes._skyTextureProjection);
        parsingContext.GetTechniqueContext()._globalEnvironmentState.SetParameter((const utf8*)"HAS_DIFFUSE_IBL", lightBindRes._hasDiffuseIBL?1:0);
        parsingContext.GetTechniqueContext()._globalEnvironmentState.SetParameter((const utf8*)"HAS_SPECULAR_IBL", lightBindRes._hasSpecularIBL?1:0);

            //  Order independent transparency disabled when
            //  using MSAA modes... Still some problems in related to MSAA buffers
        const bool useOrderIndependentTransparency = Tweakable("UseOITrans", false) && (lightingParserContext._sampleCount <= 1);
		assert(!useOrderIndependentTransparency);	// (order independent transparency broken during refactoring)

        ReturnToSteadyState(metalContext);

		if (BatchHasContent(executedScene._preDepth)) {
			RenderStateDelegateChangeMarker marker(parsingContext, GetStateSetResolvers()._depthOnly);
			ExecuteDrawablesContext executeDrawablesContext(parsingContext);
			ExecuteDrawables(
				threadContext, parsingContext, executeDrawablesContext, executedScene._preDepth,
				TechniqueIndex_DepthOnly, "MainScene-DepthOnly");
			ReturnToSteadyState(metalContext);
		}

            /////

        RenderStateDelegateChangeMarker marker(parsingContext, GetStateSetResolvers()._forward);

            //  We must disable z write (so all shaders can be early-depth-stencil)
            //      (this is because early-depth-stencil will normally write to the depth
            //      buffer before the alpha test has been performed. The pre-depth pass
            //      will switch early-depth-stencil on and off as necessary, but in the second
            //      pass we want it on permanently because the depth reject will end up performing
            //      the same job as alpha testing)
        metalContext.Bind(Techniques::CommonResources()._dssReadOnly);

            /////
            
		ExecuteDrawablesContext executeDrawablesContext(parsingContext);
        ExecuteDrawables(
            threadContext, parsingContext, executeDrawablesContext, executedScene._general,
            TechniqueIndex_General, "MainScene-General");

            /////

#if 0 // platformtemp
        Metal::ShaderResourceView duplicatedDepthBuffer;
        TransparencyTargetsBox* oiTransTargets = nullptr;
        if (useOrderIndependentTransparency) {
            duplicatedDepthBuffer = 
                BuildDuplicatedDepthBuffer(&metalContext, *targetsBox._msaaDepthBufferTexture->GetUnderlying());
                
            oiTransTargets = OrderIndependentTransparency_Prepare(metalContext, parsingContext, duplicatedDepthBuffer);

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
                context, parsingContext, Techniques::BatchFilter::OITransparent, preparedScene,
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
            Sky_Render(metalContext, parsingContext, true);
            Sky_RenderPostFog(metalContext, parsingContext, lightingParserContext.GetSceneParser()->GetGlobalLightingDesc());
        }

        if (useOrderIndependentTransparency) {
                //  feed the "duplicated depth buffer" into the resolve operation here...
                //  this a non-MSAA blend over a MSAA buffer; so it might kill the samples information.
                //  We can try to do this after the MSAA resolve... But we do the luminance sample
                //  before MSAA resolve, so transluent objects don't contribute to the luminance sampling.
            OrderIndependentTransparency_Resolve(metalContext, parsingContext, *oiTransTargets, duplicatedDepthBuffer); // mainTargets._msaaDepthBufferSRV);
        }
#endif
    }

	void RenderStep_Forward::Execute(
		RenderCore::IThreadContext& threadContext,
		RenderCore::Techniques::ParsingContext& parsingContext,
		LightingParserContext& lightingParserContext,
		RenderCore::Techniques::RenderPassFragment& rpi,
		IViewDelegate* viewDelegate)
	{
		assert(viewDelegate);
		ForwardLightingModel_Render(threadContext, parsingContext, lightingParserContext, *checked_cast<ViewDelegate_Forward*>(viewDelegate));
	}

}
