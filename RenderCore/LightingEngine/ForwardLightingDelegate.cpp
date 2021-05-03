// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)


#include "ForwardLightingDelegate.h"
#include "LightingEngineInternal.h"
#include "LightingEngineApparatus.h"
#include "LightUniforms.h"
#include "ShadowPreparer.h"
#include "RenderStepFragments.h"
#include "../Techniques/ParsingContext.h"
#include "../Techniques/DrawableDelegates.h"
#include "../Techniques/RenderPass.h"
#include "../Techniques/CommonBindings.h"
#include "../Techniques/TechniqueDelegates.h"

namespace RenderCore { namespace LightingEngine
{
	class ForwardLightingCaptures
	{
	public:
		std::vector<PreparedShadowFrustum> _preparedDMShadows;
		std::shared_ptr<ICompiledShadowPreparer> _shadowPreparer;
		std::shared_ptr<Techniques::FrameBufferPool> _shadowGenFrameBufferPool;
		std::shared_ptr<Techniques::AttachmentPool> _shadowGenAttachmentPool;
		const SceneLightingDesc* _sceneLightDesc;

		class UniformsDelegate : public Techniques::IShaderResourceDelegate
		{
		public:
			virtual const UniformsStreamInterface& GetInterface() override { return _interface; }
			void WriteImmediateData(Techniques::ParsingContext& context, const void* objectContext, unsigned idx, IteratorRange<void*> dst) override
			{
				assert(idx==0);
				assert(dst.size() == sizeof(CB_BasicEnvironment));
				*(CB_BasicEnvironment*)dst.begin() = MakeBasicEnvironmentUniforms(*_captures->_sceneLightDesc);
			}

			size_t GetImmediateDataSize(Techniques::ParsingContext& context, const void* objectContext, unsigned idx) override
			{
				assert(idx==0);
				return sizeof(CB_BasicEnvironment);
			}
		
			UniformsDelegate(ForwardLightingCaptures& captures) : _captures(&captures)
			{
				_interface.BindImmediateData(0, Utility::Hash64("BasicLightingEnvironment"), {});
			}
			UniformsStreamInterface _interface;
			ForwardLightingCaptures* _captures;
		};
		std::shared_ptr<UniformsDelegate> _uniformsDelegate;
	};

	static void SetupDMShadowPrepare(
		LightingTechniqueIterator& iterator,
		std::shared_ptr<ForwardLightingCaptures> captures,
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

	static RenderStepFragmentInterface CreateForwardSceneFragment(
		std::shared_ptr<Techniques::ITechniqueDelegate> forwardIllumDelegate,
		std::shared_ptr<Techniques::ITechniqueDelegate> depthOnlyDelegate,
		bool precisionTargets, bool writeDirectToLDR)
	{
		AttachmentDesc lightResolveAttachmentDesc =
			{	(!precisionTargets) ? Format::R16G16B16A16_FLOAT : Format::R32G32B32A32_FLOAT,
				1.f, 1.f, 0u,
				AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::OutputRelativeDimensions };

		AttachmentDesc msDepthDesc =
            {   Format::D24_UNORM_S8_UINT, 1.f, 1.f, 0u,
                AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::OutputRelativeDimensions,
				BindFlag::ShaderResource };

		RenderStepFragmentInterface result(PipelineType::Graphics);
        AttachmentName output;
		if (!writeDirectToLDR)
			output = result.DefineAttachment(Techniques::AttachmentSemantics::ColorHDR, lightResolveAttachmentDesc);
		else
			output = result.DefineAttachment(Techniques::AttachmentSemantics::ColorLDR);
		auto depth = result.DefineAttachment(Techniques::AttachmentSemantics::MultisampleDepth, msDepthDesc);

		SubpassDesc depthOnlySubpass;
		depthOnlySubpass.SetDepthStencil(depth, LoadStore::Clear_ClearStencil);

		SubpassDesc mainSubpass;
		mainSubpass.AppendOutput(output, LoadStore::Clear);
		mainSubpass.SetDepthStencil(depth);

		result.AddSubpass(depthOnlySubpass.SetName("DepthOnly"), depthOnlyDelegate, Techniques::BatchFilter::General);

		// todo -- parameters should be configured based on how the scene is set up
		ParameterBox box;
		// box.SetParameter((const utf8*)"SKY_PROJECTION", lightBindRes._skyTextureProjection);
		box.SetParameter((const utf8*)"HAS_DIFFUSE_IBL", 1);
		box.SetParameter((const utf8*)"HAS_SPECULAR_IBL", 1);
		result.AddSubpass(mainSubpass.SetName("MainForward"), forwardIllumDelegate, Techniques::BatchFilter::General);
		return result;
	}

	std::shared_ptr<CompiledLightingTechnique> CreateForwardLightingTechnique(
		const std::shared_ptr<Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
		const std::shared_ptr<LightingEngineApparatus>& apparatus,
		IteratorRange<const Techniques::PreregisteredAttachment*> preregisteredAttachments,
		const FrameBufferProperties& fbProps)
	{
		return CreateForwardLightingTechnique(apparatus->_device, pipelineAccelerators, apparatus->_sharedDelegates, preregisteredAttachments, fbProps);
	}

	std::shared_ptr<CompiledLightingTechnique> CreateForwardLightingTechnique(
		const std::shared_ptr<IDevice>& device,
		const std::shared_ptr<Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
		const std::shared_ptr<SharedTechniqueDelegateBox>& techDelBox,
		IteratorRange<const Techniques::PreregisteredAttachment*> preregisteredAttachments,
		const FrameBufferProperties& fbProps)
	{
		auto lightingTechnique = std::make_shared<CompiledLightingTechnique>(pipelineAccelerators, fbProps, preregisteredAttachments);
		auto captures = std::make_shared<ForwardLightingCaptures>();
		captures->_shadowGenAttachmentPool = std::make_shared<Techniques::AttachmentPool>(device);
		captures->_shadowGenFrameBufferPool = Techniques::CreateFrameBufferPool();
		captures->_uniformsDelegate = std::make_shared<ForwardLightingCaptures::UniformsDelegate>(*captures.get());

		// Reset captures
		lightingTechnique->Push(
			[captures](LightingTechniqueIterator& iterator) {
				captures->_sceneLightDesc = &iterator._sceneLightingDesc;
				iterator._parsingContext->AddShaderResourceDelegate(captures->_uniformsDelegate);
			});

		// Prepare shadows
		ShadowGeneratorDesc defaultShadowGenerator;
		captures->_shadowPreparer = CreateCompiledShadowPreparer(defaultShadowGenerator, pipelineAccelerators, techDelBox);
		lightingTechnique->Push(
			[captures](LightingTechniqueIterator& iterator) {
				const auto& lightingDesc = iterator._sceneLightingDesc;

				captures->_preparedDMShadows.reserve(lightingDesc._shadowProjections.size());
				for (unsigned c=0; c<lightingDesc._shadowProjections.size(); ++c)
					SetupDMShadowPrepare(iterator, captures, lightingDesc._shadowProjections[c], c);
			});

		// Draw main scene
		const bool writeDirectToLDR = true;
		lightingTechnique->Push(CreateForwardSceneFragment(
			techDelBox->_forwardIllumDelegate_DisableDepthWrite,
			techDelBox->_depthOnlyDelegate, 
			false, writeDirectToLDR));

		lightingTechnique->Push(
			[captures](LightingTechniqueIterator& iterator) {
				iterator._parsingContext->RemoveShaderResourceDelegate(*captures->_uniformsDelegate);
			});
			
		return lightingTechnique;
	}

}}

