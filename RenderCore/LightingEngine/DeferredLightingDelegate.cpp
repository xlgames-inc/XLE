// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DeferredLightingDelegate.h"
#include "DeferredLightingResolve.h"
#include "LightingEngineInternal.h"
#include "LightingEngineApparatus.h"
#include "LightUniforms.h"
#include "ShadowPreparer.h"
#include "RenderStepFragments.h"
#include "../Techniques/DrawableDelegates.h"
#include "../Techniques/CommonBindings.h"
#include "../Techniques/DeferredShaderResource.h"
#include "../Techniques/ParsingContext.h"
#include "../Techniques/PipelineCollection.h"
#include "../UniformsStream.h"
#include "../../Assets/Assets.h"
#include "../../Utility/MemoryUtils.h"
#include "../../xleres/FileList.h"


#include "../Metal/DeviceContext.h"
#include "../Metal/ObjectFactory.h"
#include "../Metal/InputLayout.h"
#include "../Techniques/CommonResources.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../ConsoleRig/Console.h"

namespace RenderCore { namespace LightingEngine
{
	class LightResolveOperators;

	class DeferredLightingCaptures
	{
	public:
		std::vector<PreparedShadowFrustum> _preparedDMShadows;
		std::shared_ptr<ShadowPreparationOperators> _shadowPreparationOperators;
		std::shared_ptr<LightResolveOperators> _lightResolveOperators;
		std::shared_ptr<Techniques::FrameBufferPool> _shadowGenFrameBufferPool;
		std::shared_ptr<Techniques::AttachmentPool> _shadowGenAttachmentPool;
		const SceneLightingDesc* _sceneLightDesc;

		void DoShadowPrepare(LightingTechniqueIterator& iterator);
		void DoLightResolve(LightingTechniqueIterator& iterator);
		void DoToneMap(LightingTechniqueIterator& iterator);

	private:
		void SetupDMShadowPrepare(LightingTechniqueIterator& iterator, const ShadowProjectionDesc& proj, unsigned shadowIdx);
	};

	class BuildGBufferResourceDelegate : public Techniques::IShaderResourceDelegate
	{
	public:
		virtual const UniformsStreamInterface& GetInterface() { return _interf; }

        virtual void WriteResourceViews(Techniques::ParsingContext& context, const void* objectContext, uint64_t bindingFlags, IteratorRange<IResourceView**> dst)
		{
			assert(bindingFlags == 1<<0);
			dst[0] = _normalsFitting.get();
		}

		BuildGBufferResourceDelegate(Techniques::DeferredShaderResource& normalsFittingResource)
		{
			_interf.BindResourceView(0, Utility::Hash64("NormalsFittingTexture"));
			_normalsFitting = normalsFittingResource.GetShaderResource();
		}
		UniformsStreamInterface _interf;
		std::shared_ptr<IResourceView> _normalsFitting;
	};

	static ::Assets::FuturePtr<RenderStepFragmentInterface> CreateBuildGBufferSceneFragment(
		SharedTechniqueDelegateBox& techDelBox,
		GBufferType gbufferType, 
		bool precisionTargets = false)
	{
		auto result = std::make_shared<::Assets::AssetFuture<RenderStepFragmentInterface>>("build-gbuffer");
		auto normalsFittingTexture = ::Assets::MakeAsset<Techniques::DeferredShaderResource>(NORMALS_FITTING_TEXTURE);

		::Assets::WhenAll(normalsFittingTexture).ThenConstructToFuture<RenderStepFragmentInterface>(
			*result,
			[defIllumDel = techDelBox._deferredIllumDelegate, gbufferType, precisionTargets](std::shared_ptr<Techniques::DeferredShaderResource> deferredShaderResource) {

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

				auto createGBuffer = std::make_shared<RenderStepFragmentInterface>(RenderCore::PipelineType::Graphics);
				auto msDepth = createGBuffer->DefineAttachment(
					Techniques::AttachmentSemantics::MultisampleDepth,
					// Main multisampled depth stencil
					{ RenderCore::Format::D24_UNORM_S8_UINT });

						// Generally the deferred pixel shader will just copy information from the albedo
						// texture into the first deferred buffer. So the first deferred buffer should
						// have the same pixel format as much input textures.
						// Usually this is an 8 bit SRGB format, so the first deferred buffer should also
						// be 8 bit SRGB. So long as we don't do a lot of processing in the deferred pixel shader
						// that should be enough precision.
						//      .. however, it possible some clients might prefer 10 or 16 bit albedo textures
						//      In these cases, the first buffer should be a matching format.
				auto diffuse = createGBuffer->DefineAttachment(
					Techniques::AttachmentSemantics::GBufferDiffuse,
					{ (!precisionTargets) ? Format::R8G8B8A8_UNORM_SRGB : Format::R32G32B32A32_FLOAT });

				auto normal = createGBuffer->DefineAttachment(
					Techniques::AttachmentSemantics::GBufferNormal,
					{ (!precisionTargets) ? Format::R8G8B8A8_SNORM : Format::R32G32B32A32_FLOAT });

				auto parameter = createGBuffer->DefineAttachment(
					Techniques::AttachmentSemantics::GBufferParameter,
					{ (!precisionTargets) ? Format::R8G8B8A8_UNORM : Format::R32G32B32A32_FLOAT });

				SubpassDesc subpass;
				subpass.AppendOutput(diffuse, LoadStore::Clear, LoadStore::Retain);
				subpass.AppendOutput(normal, LoadStore::Clear, LoadStore::Retain);
				if (gbufferType == GBufferType::PositionNormalParameters)
					subpass.AppendOutput(parameter, LoadStore::Clear, LoadStore::Retain);
				subpass.SetDepthStencil(msDepth, LoadStore::Clear_ClearStencil, LoadStore::Retain);

				auto srDelegate = std::make_shared<BuildGBufferResourceDelegate>(*deferredShaderResource);

				ParameterBox box;
				box.SetParameter("GBUFFER_TYPE", (unsigned)gbufferType);
				createGBuffer->AddSubpass(std::move(subpass), defIllumDel, Techniques::BatchFilter::General, std::move(box), std::move(srDelegate));
				return createGBuffer;
			});
		return result;
	}

	static RenderStepFragmentInterface CreateLightingResolveFragment(
		std::function<void(LightingTechniqueIterator&)>&& fn,
		bool precisionTargets = false)
	{
		RenderStepFragmentInterface fragment { RenderCore::PipelineType::Graphics };
		auto depthTarget = fragment.DefineAttachment(Techniques::AttachmentSemantics::MultisampleDepth);
		auto lightResolveTarget = fragment.DefineAttachment(
			Techniques::AttachmentSemantics::ColorHDR,
			{ (!precisionTargets) ? Format::R16G16B16A16_FLOAT : Format::R32G32B32A32_FLOAT });

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

		SubpassDesc subpasses[2];
		subpasses[0].AppendOutput(lightResolveTarget, LoadStore::Clear);
		subpasses[0].SetDepthStencil(depthTarget, LoadStore::Retain_ClearStencil, LoadStore::Retain_RetainStencil);

			// In the second subpass, the depth buffer is bound as stencil-only (so we can read the depth values as shader inputs)
		subpasses[1].AppendOutput(lightResolveTarget);
		subpasses[1].SetDepthStencil({ depthTarget, LoadStore::Retain_RetainStencil, LoadStore::Retain_RetainStencil, justStencilWindow });

		auto gbufferStore = LoadStore::Retain;	// (technically only need retain when we're going to use these for debugging)
		auto diffuseAspect = (!precisionTargets) ? TextureViewDesc::Aspect::ColorSRGB : TextureViewDesc::Aspect::ColorLinear;
		subpasses[1].AppendInput(
			AttachmentViewDesc {
				fragment.DefineAttachment(Techniques::AttachmentSemantics::GBufferDiffuse),
				LoadStore::Retain, gbufferStore,
				{diffuseAspect}
			});
		subpasses[1].AppendInput(
			AttachmentViewDesc {
				fragment.DefineAttachment(Techniques::AttachmentSemantics::GBufferNormal),
				LoadStore::Retain, gbufferStore
			});
		subpasses[1].AppendInput(
			AttachmentViewDesc {
				fragment.DefineAttachment(Techniques::AttachmentSemantics::GBufferParameter),
				LoadStore::Retain, gbufferStore
			});
		subpasses[1].AppendInput(
			AttachmentViewDesc { depthTarget, LoadStore::Retain_RetainStencil, LoadStore::Retain_RetainStencil, justDepthWindow });

		// fragment.AddSubpasses(MakeIteratorRange(subpasses), std::move(fn));
		fragment.AddSkySubpass(std::move(subpasses[0]));
		fragment.AddSubpass(std::move(subpasses[1]), std::move(fn));
		return fragment;
	}

	static RenderStepFragmentInterface CreateToneMapFragment(
		std::function<void(LightingTechniqueIterator&)>&& fn,
		bool precisionTargets = false)
	{
		RenderStepFragmentInterface fragment { RenderCore::PipelineType::Graphics };
		auto hdrInput = fragment.DefineAttachment(Techniques::AttachmentSemantics::ColorHDR);
		auto ldrOutput = fragment.DefineAttachment(Techniques::AttachmentSemantics::ColorLDR);

		SubpassDesc subpass;
		subpass.AppendOutput(ldrOutput, LoadStore::DontCare, LoadStore::Retain);
		subpass.AppendInput(hdrInput, LoadStore::Retain_RetainStencil, LoadStore::DontCare);
		fragment.AddSubpass(std::move(subpass), std::move(fn));
		return fragment;
	}
	
	void DeferredLightingCaptures::SetupDMShadowPrepare(
		LightingTechniqueIterator& iterator,
		const ShadowProjectionDesc& proj,
		unsigned shadowIdx)
	{
		if (_shadowPreparationOperators->_operators.empty()) return;

		auto& shadowPreparer = *_shadowPreparationOperators->_operators[0]._preparer;
		iterator.PushFollowingStep(
			[this, &shadowPreparer, proj](LightingTechniqueIterator& iterator) {
				iterator._rpi = shadowPreparer.Begin(
					*iterator._threadContext,
					*iterator._parsingContext,
					proj,
					*this->_shadowGenFrameBufferPool,
					*this->_shadowGenAttachmentPool);
			});
		iterator.PushFollowingStep(Techniques::BatchFilter::General);
		auto cfg = shadowPreparer.GetSequencerConfig();
		iterator.PushFollowingStep(std::move(cfg.first), std::move(cfg.second));
		iterator.PushFollowingStep(
			[this, &shadowPreparer](LightingTechniqueIterator& iterator) {
				iterator._rpi.End();
				auto preparedShadow = shadowPreparer.End(
					*iterator._threadContext,
					*iterator._parsingContext,
					iterator._rpi);
				this->_preparedDMShadows.push_back(std::move(preparedShadow));
			});
	}

	void DeferredLightingCaptures::DoShadowPrepare(LightingTechniqueIterator& iterator)
	{
		const auto& lightingDesc = iterator._sceneLightingDesc;

		_preparedDMShadows.reserve(lightingDesc._shadowProjections.size());
		for (unsigned c=0; c<lightingDesc._shadowProjections.size(); ++c)
			SetupDMShadowPrepare(iterator, lightingDesc._shadowProjections[c], c);
	}

	void DeferredLightingCaptures::DoLightResolve(LightingTechniqueIterator& iterator)
	{
		// Light subpass
		ResolveLights(
			*iterator._threadContext, *iterator._parsingContext, iterator._rpi,
			iterator._sceneLightingDesc, *_lightResolveOperators);
	}

	void DeferredLightingCaptures::DoToneMap(LightingTechniqueIterator& iterator)
	{
		// Very simple stand-in for tonemap -- just use a copy shader to write the HDR values directly to the LDR texture
		auto pipelineLayout = _lightResolveOperators->_pipelineLayout;
		auto& copyShader = *::Assets::Actualize<Metal::ShaderProgram>(
			pipelineLayout,
			BASIC2D_VERTEX_HLSL ":fullscreen",
			BASIC_PIXEL_HLSL ":copy");
		auto& metalContext = *Metal::DeviceContext::Get(*iterator._threadContext);
		auto encoder = metalContext.BeginGraphicsEncoder_ProgressivePipeline(pipelineLayout);
		UniformsStreamInterface usi;
		usi.BindResourceView(0, Utility::Hash64("InputTexture"));
		Metal::BoundUniforms uniforms(copyShader, usi);
		encoder.Bind(copyShader);
		encoder.Bind(Techniques::CommonResourceBox::s_dsDisable);
		encoder.Bind({&Techniques::CommonResourceBox::s_abOpaque, &Techniques::CommonResourceBox::s_abOpaque+1});
		UniformsStream us;
		IResourceView* srvs[] = { iterator._rpi.GetInputAttachmentSRV(0) };
		us._resourceViews = MakeIteratorRange(srvs);
		uniforms.ApplyLooseUniforms(metalContext, encoder, us);
		encoder.Bind({}, Topology::TriangleStrip);
		encoder.Draw(4);
	}

	::Assets::FuturePtr<CompiledLightingTechnique> CreateDeferredLightingTechnique(
		const std::shared_ptr<IDevice>& device,
		const std::shared_ptr<Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
		const std::shared_ptr<SharedTechniqueDelegateBox>& techDelBox,
		const std::shared_ptr<Techniques::GraphicsPipelineCollection>& pipelineCollection,
		IteratorRange<const LightResolveOperatorDesc*> resolveOperatorsInit,
		IteratorRange<const ShadowGeneratorDesc*> shadowGenerators,
		IteratorRange<const Techniques::PreregisteredAttachment*> preregisteredAttachmentsInit,
		const FrameBufferProperties& fbProps)
	{
		auto buildGBufferFragment = CreateBuildGBufferSceneFragment(*techDelBox, GBufferType::PositionNormalParameters);
		auto shadowPreparationOperators = CreateShadowPreparationOperators(shadowGenerators, pipelineAccelerators, techDelBox);
		std::vector<LightResolveOperatorDesc> resolveOperators { resolveOperatorsInit.begin(), resolveOperatorsInit.end() };

		auto result = std::make_shared<::Assets::AssetFuture<CompiledLightingTechnique>>("deferred-lighting-technique");
		std::vector<Techniques::PreregisteredAttachment> preregisteredAttachments { preregisteredAttachmentsInit.begin(), preregisteredAttachmentsInit.end() };
		::Assets::WhenAll(buildGBufferFragment, shadowPreparationOperators).ThenConstructToFuture<CompiledLightingTechnique>(
			*result,
			[device, pipelineAccelerators, techDelBox, fbProps, 
			preregisteredAttachments=std::move(preregisteredAttachments),
			resolveOperators=std::move(resolveOperators), pipelineCollection](
				::Assets::AssetFuture<CompiledLightingTechnique>& thatFuture,
				std::shared_ptr<RenderStepFragmentInterface> buildGbuffer,
				std::shared_ptr<ShadowPreparationOperators> shadowPreparationOperators) {

				auto lightingTechnique = std::make_shared<CompiledLightingTechnique>(pipelineAccelerators, fbProps, preregisteredAttachments);
				auto captures = std::make_shared<DeferredLightingCaptures>();
				captures->_shadowGenAttachmentPool = std::make_shared<Techniques::AttachmentPool>(device);
				captures->_shadowGenFrameBufferPool = Techniques::CreateFrameBufferPool();
				captures->_shadowPreparationOperators = std::move(shadowPreparationOperators);

				// Reset captures
				lightingTechnique->CreateStep_CallFunction(
					[captures](LightingTechniqueIterator& iterator) {
						captures->_sceneLightDesc = &iterator._sceneLightingDesc;
					});

				// Prepare shadows
				lightingTechnique->CreateStep_CallFunction(
					[captures](LightingTechniqueIterator& iterator) {
						captures->DoShadowPrepare(iterator);
					});

				// Draw main scene
				lightingTechnique->CreateStep_RunFragments(std::move(*buildGbuffer));

				// Lighting resolve (gbuffer -> HDR color image)
				auto lightingResolveFragment = CreateLightingResolveFragment(
					[captures](LightingTechniqueIterator& iterator) {
						// do lighting resolve here
						captures->DoLightResolve(iterator);
					});
				auto resolveFragmentRegistration = lightingTechnique->CreateStep_RunFragments(std::move(lightingResolveFragment));

				auto toneMapFragment = CreateToneMapFragment(
					[captures](LightingTechniqueIterator& iterator) {
						captures->DoToneMap(iterator);
					});
				lightingTechnique->CreateStep_RunFragments(std::move(toneMapFragment));

				// unbind operations
				lightingTechnique->CreateStep_CallFunction(
					[captures](LightingTechniqueIterator& iterator) {
					});

				lightingTechnique->CompleteConstruction();

				//
				// Now that we've finalized the frame buffer layout, build the light resolve operators
				// And then we'll complete the technique when the future from BuildLightResolveOperators() is completed
				//
				auto resolvedFB = lightingTechnique->GetResolvedFrameBufferDesc(resolveFragmentRegistration);
				auto lightResolveOperators = BuildLightResolveOperators(
					*pipelineCollection, resolveOperators,
					*resolvedFB.first, resolvedFB.second,
					false, 0, Shadowing::NoShadows, GBufferType::PositionNormalParameters);

				::Assets::WhenAll(lightResolveOperators).ThenConstructToFuture<CompiledLightingTechnique>(
					thatFuture,
					[lightingTechnique, captures](const std::shared_ptr<LightResolveOperators>& resolveOperators) {
						captures->_lightResolveOperators = resolveOperators;
						return lightingTechnique;
					});
			});

		return result;
	}

	::Assets::FuturePtr<CompiledLightingTechnique> CreateDeferredLightingTechnique(
		const std::shared_ptr<LightingEngineApparatus>& apparatus,
		IteratorRange<const LightResolveOperatorDesc*> resolveOperators,
		IteratorRange<const ShadowGeneratorDesc*> shadowGenerators,
		IteratorRange<const Techniques::PreregisteredAttachment*> preregisteredAttachments,
		const FrameBufferProperties& fbProps)
	{
		return CreateDeferredLightingTechnique(
			apparatus->_device,
			apparatus->_pipelineAccelerators,
			apparatus->_sharedDelegates,
			apparatus->_lightingOperatorCollection,
			resolveOperators, shadowGenerators, preregisteredAttachments,
			fbProps);		
	}
}}
