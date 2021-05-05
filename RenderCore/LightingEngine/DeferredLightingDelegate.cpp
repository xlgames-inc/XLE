// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DeferredLightingDelegate.h"
#include "LightingEngineInternal.h"
#include "LightingEngineApparatus.h"
#include "LightUniforms.h"
#include "ShadowPreparer.h"
#include "RenderStepFragments.h"
#include "../Techniques/DrawableDelegates.h"
#include "../Techniques/CommonBindings.h"
#include "../Techniques/DeferredShaderResource.h"
#include "../Techniques/ParsingContext.h"
#include "../UniformsStream.h"
#include "../../Assets/Assets.h"
#include "../../Utility/MemoryUtils.h"
#include "../../xleres/FileList.h"

namespace RenderCore { namespace LightingEngine
{
	class DeferredLightingCaptures
	{
	public:
		std::vector<PreparedShadowFrustum> _preparedDMShadows;
		std::shared_ptr<ICompiledShadowPreparer> _shadowPreparer;
		std::shared_ptr<Techniques::FrameBufferPool> _shadowGenFrameBufferPool;
		std::shared_ptr<Techniques::AttachmentPool> _shadowGenAttachmentPool;
		const SceneLightingDesc* _sceneLightDesc;
	};

	static void SetupDMShadowPrepare(
		LightingTechniqueIterator& iterator,
		std::shared_ptr<DeferredLightingCaptures> captures,
		const ShadowProjectionDesc& proj,
		unsigned shadowIdx)
	{
		iterator.PushFollowingStep(
			[captures, proj](LightingTechniqueIterator& iterator) {
				iterator._rpi = captures->_shadowPreparer->Begin(
					*iterator._threadContext,
					*iterator._parsingContext,
					proj,
					*captures->_shadowGenFrameBufferPool,
					*captures->_shadowGenAttachmentPool);
			});
		iterator.PushFollowingStep(Techniques::BatchFilter::General);
		auto cfg = captures->_shadowPreparer->GetSequencerConfig();
		iterator.PushFollowingStep(std::move(cfg.first), std::move(cfg.second));
		iterator.PushFollowingStep(
			[captures](LightingTechniqueIterator& iterator) {
				iterator._rpi.End();
				auto preparedShadow = captures->_shadowPreparer->End(
					*iterator._threadContext,
					*iterator._parsingContext,
					iterator._rpi);
				captures->_preparedDMShadows.push_back(std::move(preparedShadow));
			});
	}

	enum class GBufferType
	{
		PositionNormal,
		PositionNormalParameters
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
			[defIllumDel = techDelBox._deferredIllumDelegate, gbufferType, precisionTargets](const std::shared_ptr<Techniques::DeferredShaderResource>& deferredShaderResource) {

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

	static RenderStepFragmentInterface CreateLightingResolveFragment(bool precisionTargets = false)
	{
		RenderStepFragmentInterface postOpaque { RenderCore::PipelineType::Graphics };
		auto msDepth = postOpaque.DefineAttachment(Techniques::AttachmentSemantics::MultisampleDepth);
		auto lightingResolve = postOpaque.DefineAttachment(
			Techniques::AttachmentSemantics::ColorHDR,
			{ (!precisionTargets) ? Format::R16G16B16A16_FLOAT : Format::R32G32B32A32_FLOAT });

		SubpassDesc subpass;
		subpass.AppendOutput(lightingResolve, LoadStore::Clear);
		subpass.SetDepthStencil(msDepth);

		// todo -- parameters should be configured based on how the scene is set up
		ParameterBox box;
		// box.SetParameter((const utf8*)"SKY_PROJECTION", lightBindRes._skyTextureProjection);
		box.SetParameter((const utf8*)"HAS_DIFFUSE_IBL", 1);
		box.SetParameter((const utf8*)"HAS_SPECULAR_IBL", 1);
		postOpaque.AddSubpass(std::move(subpass), nullptr, Techniques::BatchFilter::Max, std::move(box));
		return postOpaque;
	}

	::Assets::FuturePtr<CompiledLightingTechnique> CreateDeferredLightingTechnique(
		const std::shared_ptr<IDevice>& device,
		const std::shared_ptr<Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
		const std::shared_ptr<SharedTechniqueDelegateBox>& techDelBox,
		IteratorRange<const Techniques::PreregisteredAttachment*> preregisteredAttachmentsInit,
		const FrameBufferProperties& fbProps)
	{
		auto result = std::make_shared<::Assets::AssetFuture<CompiledLightingTechnique>>("deferred-lighting-technique");

		auto buildGBufferFragment = CreateBuildGBufferSceneFragment(*techDelBox, GBufferType::PositionNormalParameters);

		std::vector<Techniques::PreregisteredAttachment> preregisteredAttachments { preregisteredAttachmentsInit.begin(), preregisteredAttachmentsInit.end() };
		::Assets::WhenAll(buildGBufferFragment).ThenConstructToFuture<CompiledLightingTechnique>(
			*result,
			[device, pipelineAccelerators, techDelBox, fbProps, preregisteredAttachments=std::move(preregisteredAttachments)](const std::shared_ptr<RenderStepFragmentInterface>& buildGbuffer) {
				auto lightingTechnique = std::make_shared<CompiledLightingTechnique>(pipelineAccelerators, fbProps, preregisteredAttachments);
				auto captures = std::make_shared<DeferredLightingCaptures>();
				captures->_shadowGenAttachmentPool = std::make_shared<Techniques::AttachmentPool>(device);
				captures->_shadowGenFrameBufferPool = Techniques::CreateFrameBufferPool();

				// Reset captures
				lightingTechnique->CreateStep_CallFunction(
					[captures](LightingTechniqueIterator& iterator) {
						captures->_sceneLightDesc = &iterator._sceneLightingDesc;
					});

				// Prepare shadows
				ShadowGeneratorDesc defaultShadowGenerator;
				captures->_shadowPreparer = CreateCompiledShadowPreparer(defaultShadowGenerator, pipelineAccelerators, techDelBox);
				lightingTechnique->CreateStep_CallFunction(
					[captures](LightingTechniqueIterator& iterator) {
						const auto& lightingDesc = iterator._sceneLightingDesc;

						captures->_preparedDMShadows.reserve(lightingDesc._shadowProjections.size());
						for (unsigned c=0; c<lightingDesc._shadowProjections.size(); ++c)
							SetupDMShadowPrepare(iterator, captures, lightingDesc._shadowProjections[c], c);
					});

				// Draw main scene
				lightingTechnique->CreateStep_RunFragmentsAndExecuteDrawables(std::move(*buildGbuffer));

				// Lighting resolve (gbuffer -> HDR color image)
				auto lightingResolveFragment = CreateLightingResolveFragment();
				lightingTechnique->CreateStep_RunFragmentsAndCallFunction(
					std::move(lightingResolveFragment),
					[](LightingTechniqueIterator& iterator) {
						// do lighting resolve here
					});

				// unbind operations
				lightingTechnique->CreateStep_CallFunction(
					[captures](LightingTechniqueIterator& iterator) {
					});

				lightingTechnique->CompleteConstruction();
					
				return lightingTechnique;
			});

		return result;
	}
}}
