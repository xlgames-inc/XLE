// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LightingEngine.h"
#include "LightDesc.h"
#include "LightUniforms.h"		// might be able to remove if we shift the forward delegate out
#include "ShadowPreparer.h"
#include "RenderStepFragments.h"
#include "SharedTechniqueDelegates.h"
#include "../Techniques/RenderPass.h"
#include "../Techniques/CommonBindings.h"
#include "../Techniques/TechniqueDelegates.h"
#include "../Techniques/PipelineAccelerator.h"
#include "../Techniques/ParsingContext.h"		// likewise could remove when shifting the forward delegate 
#include "../Techniques/DrawableDelegates.h"
#include "../Techniques/Techniques.h"
#include "../FrameBufferDesc.h"
#include "../../Assets/AssetFutureContinuation.h"

// For invalid asset report:
#include "../../Assets/AssetSetManager.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/AssetHeap.h"
#include "../../Assets/IAsyncMarker.h"
#include "../../OSServices/Log.h"

namespace RenderCore { namespace LightingEngine
{
	class LightingTechniqueIterator;

	class CompiledLightingTechnique
	{
	public:
		struct Step
		{
			enum class Type { ParseScene, ExecuteFunction, ExecuteDrawables, BeginRenderPassInstance, EndRenderPassInstance, NextRenderPassStep, None };
			Type _type = Type::None;
			Techniques::BatchFilter _batch = Techniques::BatchFilter::Max;
			std::shared_ptr<Techniques::SequencerConfig> _sequencerConfig;
			std::shared_ptr<Techniques::IShaderResourceDelegate> _shaderResourceDelegate;
			FrameBufferDesc _fbDesc;

			using FnSig = void(LightingTechniqueIterator&);
			std::function<FnSig> _function;
		};
		std::vector<Step> _steps;

		void Push(std::function<Step::FnSig>&&);
		void PushParseScene(Techniques::BatchFilter);
		void PushExecuteDrawables(
			std::shared_ptr<Techniques::SequencerConfig> sequencerConfig,
			std::shared_ptr<Techniques::IShaderResourceDelegate> uniformDelegate);
		void Push(RenderStepFragmentInterface&& fragmentInterface);

		std::vector<Techniques::PreregisteredAttachment> _workingAttachments;
		FrameBufferProperties _fbProps;

		std::shared_ptr<Techniques::IPipelineAcceleratorPool> _pipelineAccelerators;

		const ::Assets::DependencyValidation& GetDependencyValidation() const { return _depVal; }
		::Assets::DependencyValidation _depVal;

		CompiledLightingTechnique(
			const std::shared_ptr<Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
			const FrameBufferProperties& fbProps,
			IteratorRange<const Techniques::PreregisteredAttachment*> preregisteredAttachments);
	};

	void CompiledLightingTechnique::Push(std::function<Step::FnSig>&& fn)
	{
		Step newStep;
		newStep._type = Step::Type::ExecuteFunction;
		newStep._function = std::move(fn);
		_steps.emplace_back(std::move(newStep));
	}

	void CompiledLightingTechnique::PushParseScene(Techniques::BatchFilter batch)
	{
		Step newStep;
		newStep._type = Step::Type::ParseScene;
		newStep._batch = batch;
		_steps.emplace_back(std::move(newStep));
	}

	void CompiledLightingTechnique::PushExecuteDrawables(
		std::shared_ptr<Techniques::SequencerConfig> sequencerConfig,
		std::shared_ptr<Techniques::IShaderResourceDelegate> uniformDelegate)
	{
		Step newStep;
		newStep._type = Step::Type::ParseScene;
		newStep._sequencerConfig = std::move(sequencerConfig);
		newStep._shaderResourceDelegate = std::move(uniformDelegate);
		_steps.emplace_back(std::move(newStep));
	}

	void CompiledLightingTechnique::Push(RenderStepFragmentInterface&& fragments)
	{
		// Generate a FrameBufferDesc from the input
		UInt2 dimensionsForCompatibilityTests { _fbProps._outputWidth, _fbProps._outputHeight };
		auto merged = Techniques::MergeFragments(
			MakeIteratorRange(_workingAttachments),
			MakeIteratorRange(&fragments.GetFrameBufferDescFragment(), &fragments.GetFrameBufferDescFragment()+1),
			dimensionsForCompatibilityTests);
		auto fbDesc = Techniques::BuildFrameBufferDesc(std::move(merged._mergedFragment), _fbProps);

		// Update _workingAttachments
		_workingAttachments.reserve(merged._outputAttachments.size());
		for (const auto&o:merged._outputAttachments) {
			auto i = std::find_if(
				_workingAttachments.begin(), _workingAttachments.end(),
				[&o](const Techniques::PreregisteredAttachment& p) { return p._semantic == o.first; });
			if (i != _workingAttachments.end()) {
				assert(merged._mergedFragment._attachments[o.second]._outputSemanticBinding == o.first);
				i->_desc = merged._mergedFragment._attachments[o.second]._desc;
				i->_state = i->_stencilState = Techniques::PreregisteredAttachment::State::Initialized;
			} else {
				assert(merged._mergedFragment._attachments[o.second]._outputSemanticBinding == o.first);
				_workingAttachments.push_back(
					Techniques::PreregisteredAttachment { 
						o.first,
						merged._mergedFragment._attachments[o.second]._desc,
						Techniques::PreregisteredAttachment::State::Initialized,
						Techniques::PreregisteredAttachment::State::Initialized
					});
			}
		}

		// Generate commands for walking through the render pass
		Step beginStep;
		beginStep._type = Step::Type::BeginRenderPassInstance;
		beginStep._fbDesc = fbDesc;
		_steps.emplace_back(std::move(beginStep));

		assert(!fragments.GetSubpassAddendums().empty());
		for (unsigned c=0; c<fragments.GetSubpassAddendums().size(); ++c) {
			if (c != 0)
				_steps.push_back({Step::Type::NextRenderPassStep});
			auto& sb = fragments.GetSubpassAddendums()[c];
			assert(sb._techniqueDelegate);

			Step drawStep;
			drawStep._type = Step::Type::ParseScene;
			drawStep._batch = sb._batchFilter;
			_steps.emplace_back(std::move(drawStep));

			drawStep._type = Step::Type::ExecuteDrawables;
			drawStep._sequencerConfig = _pipelineAccelerators->CreateSequencerConfig(sb._techniqueDelegate, sb._sequencerSelectors, fbDesc, c);
			_steps.emplace_back(std::move(drawStep));
		}

		_steps.push_back({Step::Type::EndRenderPassInstance});
	}

	CompiledLightingTechnique::CompiledLightingTechnique(
		const std::shared_ptr<Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
		const FrameBufferProperties& fbProps,
		IteratorRange<const Techniques::PreregisteredAttachment*> preregisteredAttachments)
	: _pipelineAccelerators(pipelineAccelerators)
	, _fbProps(fbProps)
	, _workingAttachments(preregisteredAttachments.begin(), preregisteredAttachments.end())
	{
	}

	class LightingTechniqueIterator
	{
	public:
		Techniques::RenderPassInstance _rpi;
		Techniques::DrawablesPacket _drawablePkt;

		IThreadContext* _threadContext = nullptr;
		Techniques::ParsingContext* _parsingContext = nullptr;
		Techniques::IPipelineAcceleratorPool* _pipelineAcceleratorPool = nullptr;
		Techniques::AttachmentPool* _attachmentPool = nullptr;
		Techniques::FrameBufferPool* _frameBufferPool = nullptr;
		const CompiledLightingTechnique* _compiledTechnique = nullptr;
		const SceneLightingDesc* _sceneLightingDesc = nullptr;

		void PushFollowingStep(std::function<CompiledLightingTechnique::Step::FnSig>&& fn)
		{
			CompiledLightingTechnique::Step newStep;
			newStep._type = CompiledLightingTechnique::Step::Type::ExecuteFunction;
			newStep._function = std::move(fn);
			size_t d0 = std::distance(_steps.begin(), _stepIterator);
			_pushFollowingIterator = _steps.insert(_pushFollowingIterator, std::move(newStep)) + 1;
			_stepIterator = _steps.begin() + d0;
		}

		void PushFollowingStep(Techniques::BatchFilter batchFilter)
		{
			CompiledLightingTechnique::Step newStep;
			newStep._type = CompiledLightingTechnique::Step::Type::ParseScene;
			newStep._batch = batchFilter;
			size_t d0 = std::distance(_steps.begin(), _stepIterator);
			_pushFollowingIterator = _steps.insert(_pushFollowingIterator, std::move(newStep)) + 1;
			_stepIterator = _steps.begin() + d0;
		}

		void PushFollowingStep(std::shared_ptr<Techniques::SequencerConfig> seqConfig, std::shared_ptr<Techniques::IShaderResourceDelegate> uniformDelegate)
		{
			CompiledLightingTechnique::Step newStep;
			newStep._type = CompiledLightingTechnique::Step::Type::ExecuteDrawables;
			newStep._sequencerConfig = std::move(seqConfig);
			newStep._shaderResourceDelegate = std::move(uniformDelegate);
			size_t d0 = std::distance(_steps.begin(), _stepIterator);
			_pushFollowingIterator = _steps.insert(_pushFollowingIterator, std::move(newStep)) + 1;
			_stepIterator = _steps.begin() + d0;
		}

		LightingTechniqueIterator(
			IThreadContext& threadContext,
			Techniques::ParsingContext& parsingContext,
			Techniques::IPipelineAcceleratorPool& pipelineAcceleratorPool,
			Techniques::AttachmentPool& attachmentPool,
			Techniques::FrameBufferPool& frameBufferPool,
			const CompiledLightingTechnique& compiledTechnique,
			const SceneLightingDesc& sceneLightingDesc)
		: _threadContext(&threadContext)
		, _parsingContext(&parsingContext)
		, _pipelineAcceleratorPool(&pipelineAcceleratorPool)
		, _attachmentPool(&attachmentPool)
		, _frameBufferPool(&frameBufferPool)
		, _compiledTechnique(&compiledTechnique)
		, _sceneLightingDesc(&sceneLightingDesc)
		{
			_steps = compiledTechnique._steps;
			_stepIterator = _steps.begin();
		}

	private:
		std::vector<CompiledLightingTechnique::Step> _steps;
		std::vector<CompiledLightingTechnique::Step>::iterator _stepIterator;
		std::vector<CompiledLightingTechnique::Step>::iterator _pushFollowingIterator;
		size_t currentStepIdx = 0;

		friend class LightingTechniqueInstance;
	};

	auto LightingTechniqueInstance::GetNextStep() -> Step
	{
		while (_iterator->_stepIterator != _iterator->_steps.end()) {
			auto next = _iterator->_stepIterator;
			++_iterator->_stepIterator;
			_iterator->_pushFollowingIterator = _iterator->_stepIterator;
			switch (next->_type) {
			case CompiledLightingTechnique::Step::Type::ParseScene:
				return { StepType::ParseScene, next->_batch, &_iterator->_drawablePkt };

			case CompiledLightingTechnique::Step::Type::ExecuteFunction:
				next->_function(*_iterator);
				break;

			case CompiledLightingTechnique::Step::Type::ExecuteDrawables:
				{
					Techniques::SequencerContext context;
					context._sequencerConfig = next->_sequencerConfig.get();
					if (next->_shaderResourceDelegate)
						context._sequencerResources.push_back(next->_shaderResourceDelegate);

					auto preparation = Techniques::PrepareResources(*_iterator->_pipelineAcceleratorPool, *context._sequencerConfig, _iterator->_drawablePkt);
					if (preparation) {
						auto state = preparation->StallWhilePending();
						if (state.value() == ::Assets::AssetState::Invalid) {
							auto records = ::Assets::Services::GetAssetSets().LogRecords();
							Log(Error) << "Invalid assets list: " << std::endl;
							for (const auto&r:records) {
								if (r._state != ::Assets::AssetState::Invalid) continue;
								Log(Error) << r._initializer << ": " << ::Assets::AsString(r._actualizationLog) << std::endl;
							}
						}
						assert(state.has_value() && state.value() == ::Assets::AssetState::Ready);
					}

					Techniques::Draw(
						*_iterator->_threadContext,
						*_iterator->_parsingContext,
						*_iterator->_pipelineAcceleratorPool,
						context,
						_iterator->_drawablePkt);
					_iterator->_drawablePkt.Reset();
				}
				break;

			case CompiledLightingTechnique::Step::Type::BeginRenderPassInstance:
				{
					_iterator->_rpi = Techniques::RenderPassInstance{
						*_iterator->_threadContext,
						next->_fbDesc,
						*_iterator->_frameBufferPool,
						*_iterator->_attachmentPool};
				}
				break;

			case CompiledLightingTechnique::Step::Type::EndRenderPassInstance:
				_iterator->_rpi = {};
				break;

			case CompiledLightingTechnique::Step::Type::NextRenderPassStep:
				_iterator->_rpi.NextSubpass();
				break;

			case CompiledLightingTechnique::Step::Type::None:
				assert(0);
				break;
			}
		}

		return Step { StepType::None };
	}

	LightingTechniqueInstance::LightingTechniqueInstance(
		IThreadContext& threadContext,
		Techniques::ParsingContext& parsingContext,
		Techniques::IPipelineAcceleratorPool& pipelineAcceleratorPool,
		const SceneLightingDesc& lightingDesc,
		CompiledLightingTechnique& compiledTechnique)
	{
		_iterator = std::make_unique<LightingTechniqueIterator>(
			threadContext, parsingContext, pipelineAcceleratorPool,
			*parsingContext.GetTechniqueContext()._attachmentPool,
			*parsingContext.GetTechniqueContext()._frameBufferPool,
			compiledTechnique, lightingDesc);
	}

	LightingTechniqueInstance::~LightingTechniqueInstance() {}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

	class ForwardLightingCaptures
	{
	public:
		std::vector<PreparedShadowFrustum> _preparedDMShadows;
		std::shared_ptr<ICompiledShadowPreparer> _shadowPreparer;
		std::shared_ptr<Techniques::FrameBufferPool> _shadowGenFrameBufferPool;
		std::shared_ptr<Techniques::AttachmentPool> _shadowGenAttachmentPool;
		SceneLightingDesc _sceneLightDesc;

		class UniformsDelegate : public Techniques::IShaderResourceDelegate
		{
		public:
			virtual const UniformsStreamInterface& GetInterface() override { return _interface; }
			void WriteImmediateData(Techniques::ParsingContext& context, const void* objectContext, unsigned idx, IteratorRange<void*> dst) override
			{
				assert(idx==0);
				assert(dst.size() == sizeof(CB_BasicEnvironment));
				*(CB_BasicEnvironment*)dst.begin() = MakeBasicEnvironmentUniforms(_captures->_sceneLightDesc);
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
		bool precisionTargets)
	{
		AttachmentDesc lightResolveAttachmentDesc =
			{	(!precisionTargets) ? Format::R16G16B16A16_FLOAT : Format::R32G32B32A32_FLOAT,
				1.f, 1.f, 0u,
				AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::OutputRelativeDimensions };

		AttachmentDesc msDepthDesc =
            {   Format::D24_UNORM_S8_UINT, 1.f, 1.f, 0u,
                AttachmentDesc::Flags::Multisampled | AttachmentDesc::Flags::OutputRelativeDimensions };

		RenderStepFragmentInterface result(PipelineType::Graphics);
        auto output = result.DefineAttachment(Techniques::AttachmentSemantics::ColorHDR, lightResolveAttachmentDesc);
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
		const std::shared_ptr<LightingEngineApparatus>& apparatus)
	{
		return CreateForwardLightingTechnique(apparatus->_device, pipelineAccelerators, apparatus->_sharedDelegates);
	}

	std::shared_ptr<CompiledLightingTechnique> CreateForwardLightingTechnique(
		const std::shared_ptr<IDevice>& device,
		const std::shared_ptr<Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
		const std::shared_ptr<SharedTechniqueDelegateBox>& techDelBox)
	{
		std::vector<Techniques::PreregisteredAttachment> predefinedAttachments = {
			Techniques::PreregisteredAttachment { 
				Techniques::AttachmentSemantics::ColorLDR,
				AttachmentDesc { Format::R8G8B8A8_UNORM, 256, 256 },
				Techniques::PreregisteredAttachment::State::Uninitialized
			}
		};
		FrameBufferProperties fbProps { 256, 256 };

		auto lightingTechnique = std::make_shared<CompiledLightingTechnique>(pipelineAccelerators, fbProps, MakeIteratorRange(predefinedAttachments));
		auto captures = std::make_shared<ForwardLightingCaptures>();
		captures->_shadowGenAttachmentPool = std::make_shared<Techniques::AttachmentPool>(device);
		captures->_shadowGenFrameBufferPool = std::make_shared<Techniques::FrameBufferPool>();
		captures->_uniformsDelegate = std::make_shared<ForwardLightingCaptures::UniformsDelegate>(*captures.get());

		// Reset captures
		lightingTechnique->Push(
			[captures](LightingTechniqueIterator& iterator) {
				captures->_sceneLightDesc = *iterator._sceneLightingDesc;
				iterator._parsingContext->AddShaderResourceDelegate(captures->_uniformsDelegate);
			});

		// Prepare shadows
		ShadowGeneratorDesc defaultShadowGenerator;
		captures->_shadowPreparer = CreateCompiledShadowPreparer(defaultShadowGenerator, pipelineAccelerators, techDelBox);
		lightingTechnique->Push(
			[captures](LightingTechniqueIterator& iterator) {
				const auto& lightingDesc = *iterator._sceneLightingDesc;

				captures->_preparedDMShadows.reserve(lightingDesc._shadowProjections.size());
				for (unsigned c=0; c<lightingDesc._shadowProjections.size(); ++c)
					SetupDMShadowPrepare(iterator, captures, lightingDesc._shadowProjections[c], c);
			});

		// Draw main scene
		lightingTechnique->Push(CreateForwardSceneFragment(
			techDelBox->_forwardIllumDelegate_DisableDepthWrite,
			techDelBox->_depthOnlyDelegate, 
			false));

		lightingTechnique->Push(
			[captures](LightingTechniqueIterator& iterator) {
				iterator._parsingContext->RemoveShaderResourceDelegate(*captures->_uniformsDelegate);
			});
			
		return lightingTechnique;
	}

}}
