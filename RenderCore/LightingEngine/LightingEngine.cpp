// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LightingEngine.h"
#include "LightingEngineInternal.h"
#include "LightDesc.h"
#include "RenderStepFragments.h"
#include "LightingEngineApparatus.h"
#include "../Techniques/RenderPass.h"
#include "../Techniques/PipelineAccelerator.h"
#include "../Techniques/Techniques.h"
#include "../Techniques/ParsingContext.h"
#include "../FrameBufferDesc.h"
#include "../../Assets/AssetFutureContinuation.h"
#include "../../Assets/AsyncMarkerGroup.h"
#include "../../OSServices/Log.h"

namespace RenderCore { namespace LightingEngine
{
	void CompiledLightingTechnique::CreateStep_CallFunction(std::function<StepFnSig>&& fn)
	{
		assert(!_isConstructionCompleted);
		ResolvePendingCreateFragmentSteps();
		Step newStep;
		newStep._type = Step::Type::CallFunction;
		newStep._function = std::move(fn);
		_steps.emplace_back(std::move(newStep));
	}

	void CompiledLightingTechnique::CreateStep_ParseScene(Techniques::BatchFilter batch)
	{
		assert(!_isConstructionCompleted);
		ResolvePendingCreateFragmentSteps();
		Step newStep;
		newStep._type = Step::Type::ParseScene;
		newStep._batch = batch;
		_steps.emplace_back(std::move(newStep));
	}

	void CompiledLightingTechnique::CreateStep_ExecuteDrawables(
		std::shared_ptr<Techniques::SequencerConfig> sequencerConfig,
		std::shared_ptr<Techniques::IShaderResourceDelegate> uniformDelegate)
	{
		assert(!_isConstructionCompleted);
		ResolvePendingCreateFragmentSteps();
		Step newStep;
		newStep._type = Step::Type::ParseScene;
		newStep._sequencerConfig = std::move(sequencerConfig);
		newStep._shaderResourceDelegate = std::move(uniformDelegate);
		_steps.emplace_back(std::move(newStep));
	}

	static FragmentLinkResults LinkFragmentOutputsToSystemAttachments(
		RenderCore::Techniques::MergeFragmentsResult& merged,
		IteratorRange<RenderCore::Techniques::PreregisteredAttachment*> systemAttachments)
	{
		// Attachments that are in _outputAttachments, but not in _inputAttachments are considered generated
		// Attachments that are in _inputAttachments, but not in _outputAttachments are considered consumed
		FragmentLinkResults result;
		for (const auto& o:merged._outputAttachments) {
			auto q = std::find_if(merged._inputAttachments.begin(), merged._inputAttachments.end(), [o](const auto& a) { return a.first == o.first; });
			if (q != merged._inputAttachments.end()) continue;
			result._generatedAttachments.push_back(o);
		}
		for (const auto& i:merged._inputAttachments) {
			auto q = std::find_if(merged._outputAttachments.begin(), merged._outputAttachments.end(), [i](const auto& a) { return a.first == i.first; });
			if (q != merged._outputAttachments.end()) continue;
			result._consumedAttachments.push_back(i.first);
		}
		// If there are system attachments that are no considered initialized, and not written to by the fragment
		// at all; generate a warning
		for (const auto& s:systemAttachments) {
			if (s._state == RenderCore::Techniques::PreregisteredAttachment::State::Initialized) continue;
			auto i = std::find_if(merged._outputAttachments.begin(), merged._outputAttachments.end(), [s](const auto& a) { return a.first == s._semantic; });
			if (i != merged._outputAttachments.end()) continue;
			Log(Warning) << "System attachment with semantic (";
			auto* dehash = RenderCore::Techniques::AttachmentSemantics::TryDehash(s._semantic);
			if (dehash) {
				Log(Warning) << dehash;
			} else
				Log(Warning) << "0x" << std::hex << s._semantic << std::dec;
			Log(Warning) << ") is not written to by merged fragment" << std::endl;
		}
		return result;
	}

	auto CompiledLightingTechnique::CreateStep_RunFragments(RenderStepFragmentInterface&& fragments) -> FragmentInterfaceRegistration
	{
		assert(!_isConstructionCompleted);
		_pendingCreateFragmentSteps.emplace_back(std::make_pair(std::move(fragments), _nextFragmentInterfaceRegistration));
		++_nextFragmentInterfaceRegistration;
		return _nextFragmentInterfaceRegistration-1;
	}

	void CompiledLightingTechnique::ResolvePendingCreateFragmentSteps()
	{
		if (_pendingCreateFragmentSteps.empty()) return;

		UInt2 dimensionsForCompatibilityTests { _fbProps._outputWidth, _fbProps._outputHeight };
		assert(dimensionsForCompatibilityTests[0] * dimensionsForCompatibilityTests[1]);

		std::vector<Techniques::FrameBufferDescFragment> fragments;
		for (auto& step:_pendingCreateFragmentSteps)
			fragments.emplace_back(Techniques::FrameBufferDescFragment{step.first.GetFrameBufferDescFragment()});

		auto merged = Techniques::MergeFragments(
			MakeIteratorRange(_workingAttachments),
			MakeIteratorRange(fragments),
			_fbProps);

		auto linkResults = LinkFragmentOutputsToSystemAttachments(merged, MakeIteratorRange(_workingAttachments));

		#if defined(_DEBUG)
			Log(Warning) << "Merged fragment in lighting technique:" << std::endl << merged._log << std::endl;
			if (RenderCore::Techniques::CanBeSimplified(merged._mergedFragment, _workingAttachments, _fbProps))
				Log(Warning) << "Detected a frame buffer fragment which be simplified while building lighting technique. This usually means one or more of the attachments can be reused, thereby reducing the total number of attachments required." << std::endl;
		#endif

		assert(0);
#if 0
		// Update _workingAttachments
		RenderCore::Techniques::MergeInOutputs(_workingAttachments, merged._mergedFragment, _fbProps);

		auto fbDesc = Techniques::BuildFrameBufferDesc(std::move(merged._mergedFragment), _fbProps);
		_fbDescs.emplace_back(std::move(fbDesc));

		// Generate commands for walking through the render pass
		Step beginStep;
		beginStep._type = Step::Type::BeginRenderPassInstance;
		beginStep._fbDescIdx = (unsigned)_fbDescs.size()-1;
		_steps.emplace_back(std::move(beginStep));
		
		unsigned stepCounter = 0;
		for (auto& fragments:_pendingCreateFragmentSteps) {
			assert(_fragmentInterfaceMappings.size() == fragments.second);
			_fragmentInterfaceMappings.push_back({beginStep._fbDescIdx, stepCounter});

			assert(!fragments.first.GetSubpassAddendums().empty());
			for (unsigned c=0; c<fragments.first.GetSubpassAddendums().size(); ++c) {
				if (stepCounter != 0) _steps.push_back({Step::Type::NextRenderPassStep});
				auto& sb = fragments.first.GetSubpassAddendums()[c];

				using SubpassExtension = RenderStepFragmentInterface::SubpassExtension;
				if (sb._type == SubpassExtension::Type::ExecuteDrawables) {
					assert(sb._techniqueDelegate);

					Step drawStep;
					drawStep._type = Step::Type::ParseScene;
					drawStep._batch = sb._batchFilter;
					_steps.emplace_back(std::move(drawStep));

					drawStep._type = Step::Type::ExecuteDrawables;
					drawStep._sequencerConfig = _pipelineAccelerators->CreateSequencerConfig(sb._techniqueDelegate, sb._sequencerSelectors, fbDesc, c);
					drawStep._shaderResourceDelegate = sb._shaderResourceDelegate;
					_steps.emplace_back(std::move(drawStep));
				} else if (sb._type == SubpassExtension::Type::ExecuteSky) {
					_steps.push_back({Step::Type::DrawSky});
				} else if (sb._type == SubpassExtension::Type::CallLightingIteratorFunction) {
					Step newStep;
					newStep._type = Step::Type::CallFunction;
					newStep._function = std::move(sb._lightingIteratorFunction);
					_steps.emplace_back(std::move(newStep));
				} else {
					assert(sb._type == SubpassExtension::Type::HandledByPrevious);
				}

				++stepCounter;
			}
		}

		Step endStep;
		endStep._type = Step::Type::EndRenderPassInstance;
		endStep._fragmentLinkResults = std::move(linkResults);
		_steps.push_back(endStep);
#endif

		_pendingCreateFragmentSteps.clear();
	}

	void CompiledLightingTechnique::CompleteConstruction()
	{
		assert(!_isConstructionCompleted);
		ResolvePendingCreateFragmentSteps();
		_isConstructionCompleted = true;
	}

	std::pair<const FrameBufferDesc*, unsigned> CompiledLightingTechnique::GetResolvedFrameBufferDesc(FragmentInterfaceRegistration regId) const
	{
		assert(_isConstructionCompleted);
		assert(regId < _fragmentInterfaceMappings.size());
		return std::make_pair(
			&_fbDescs[_fragmentInterfaceMappings[regId]._fbDesc],
			_fragmentInterfaceMappings[regId]._subpassBegin);
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

	CompiledLightingTechnique::~CompiledLightingTechnique() {}

	void LightingTechniqueIterator::PushFollowingStep(std::function<CompiledLightingTechnique::StepFnSig>&& fn)
	{
		CompiledLightingTechnique::Step newStep;
		newStep._type = CompiledLightingTechnique::Step::Type::CallFunction;
		newStep._function = std::move(fn);
		size_t d0 = std::distance(_steps.begin(), _stepIterator);
		_pushFollowingIterator = _steps.insert(_pushFollowingIterator, std::move(newStep)) + 1;
		_stepIterator = _steps.begin() + d0;
	}

	void LightingTechniqueIterator::PushFollowingStep(Techniques::BatchFilter batchFilter)
	{
		CompiledLightingTechnique::Step newStep;
		newStep._type = CompiledLightingTechnique::Step::Type::ParseScene;
		newStep._batch = batchFilter;
		size_t d0 = std::distance(_steps.begin(), _stepIterator);
		_pushFollowingIterator = _steps.insert(_pushFollowingIterator, std::move(newStep)) + 1;
		_stepIterator = _steps.begin() + d0;
	}

	void LightingTechniqueIterator::PushFollowingStep(std::shared_ptr<Techniques::SequencerConfig> seqConfig, std::shared_ptr<Techniques::IShaderResourceDelegate> uniformDelegate)
	{
		CompiledLightingTechnique::Step newStep;
		newStep._type = CompiledLightingTechnique::Step::Type::ExecuteDrawables;
		newStep._sequencerConfig = std::move(seqConfig);
		newStep._shaderResourceDelegate = std::move(uniformDelegate);
		size_t d0 = std::distance(_steps.begin(), _stepIterator);
		_pushFollowingIterator = _steps.insert(_pushFollowingIterator, std::move(newStep)) + 1;
		_stepIterator = _steps.begin() + d0;
	}

	LightingTechniqueIterator::LightingTechniqueIterator(
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
	, _sceneLightingDesc(sceneLightingDesc)
	{
		// If you hit this, it probably means that there's a missing call to CompiledLightingTechnique::CompleteConstruction()
		// (which should have happened at the end of the technique construction process)
		assert(compiledTechnique._isConstructionCompleted); 
		_steps = compiledTechnique._steps;
		_stepIterator = _steps.begin();
	}

	static void Remove(std::vector<Techniques::PreregisteredAttachment>& prereg, uint64_t semantic)
	{
		auto i = std::find_if(prereg.begin(), prereg.end(), [semantic](const auto& c) { return c._semantic == semantic; });
		if (i != prereg.end()) prereg.erase(i);
	}

	auto LightingTechniqueInstance::GetNextStep() -> Step
	{
		if (!_iterator)
			return GetNextPrepareResourcesStep();

		while (_iterator->_stepIterator != _iterator->_steps.end()) {
			auto next = _iterator->_stepIterator;
			++_iterator->_stepIterator;
			_iterator->_pushFollowingIterator = _iterator->_stepIterator;
			switch (next->_type) {
			case CompiledLightingTechnique::Step::Type::ParseScene:
				return { StepType::ParseScene, next->_batch, &_iterator->_drawablePkt };

			case CompiledLightingTechnique::Step::Type::CallFunction:
				next->_function(*_iterator);
				break;

			case CompiledLightingTechnique::Step::Type::ExecuteDrawables:
				{
					Techniques::SequencerContext context;
					context._sequencerConfig = next->_sequencerConfig.get();
					if (next->_shaderResourceDelegate)
						context._sequencerResources.push_back(next->_shaderResourceDelegate);

					Techniques::Draw(
						*_iterator->_threadContext,
						*_iterator->_parsingContext,
						*_iterator->_pipelineAcceleratorPool,
						context,
						_iterator->_drawablePkt);
					_iterator->_drawablePkt.Reset();
				}
				break;

			case CompiledLightingTechnique::Step::Type::DrawSky:
				return { StepType::DrawSky };

			case CompiledLightingTechnique::Step::Type::BeginRenderPassInstance:
				{
					assert(next->_fbDescIdx < _iterator->_compiledTechnique->_fbDescs.size());
					assert(0);
					_iterator->_rpi = Techniques::RenderPassInstance{
						*_iterator->_threadContext,
						_iterator->_compiledTechnique->_fbDescs[next->_fbDescIdx],
						IteratorRange<const Techniques::PreregisteredAttachment*>{},
						*_iterator->_frameBufferPool,
						*_iterator->_attachmentPool};
				}
				break;

			case CompiledLightingTechnique::Step::Type::EndRenderPassInstance:
				{
					_iterator->_rpi.End();
					assert(0);
#if 0
					auto& attachmentPool = *_iterator->_attachmentPool;
					for (auto consumed:next->_fragmentLinkResults._consumedAttachments) {
						auto* bound = attachmentPool.GetBoundResource(consumed).get();
						if (bound) attachmentPool.Unbind(*bound);
						Remove(_iterator->_parsingContext->_preregisteredAttachments, consumed);
					}
					for (auto generated:next->_fragmentLinkResults._generatedAttachments) {
						auto res = _iterator->_rpi.GetResourceForAttachmentName(generated.second);
						attachmentPool.Bind(generated.first, res);
						Remove(_iterator->_parsingContext->_preregisteredAttachments, generated.first);
						_iterator->_parsingContext->_preregisteredAttachments.push_back({generated.first, res->GetDesc(), Techniques::PreregisteredAttachment::State::Initialized, Techniques::PreregisteredAttachment::State::Initialized});
					}
#endif
					_iterator->_rpi = {};
				}
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


	class LightingTechniqueInstance::PrepareResourcesIterator
	{
	public:
		Techniques::DrawablesPacket _drawablePkt;
		std::vector<std::shared_ptr<::Assets::IAsyncMarker>> _requiredResources;

		std::vector<CompiledLightingTechnique::Step> _steps;
		std::vector<CompiledLightingTechnique::Step>::iterator _stepIterator;

		Techniques::IPipelineAcceleratorPool* _pipelineAcceleratorPool = nullptr;
	};

	auto LightingTechniqueInstance::GetNextPrepareResourcesStep() -> Step
	{
		assert(_prepareResourcesIterator);
		while (_prepareResourcesIterator->_stepIterator != _prepareResourcesIterator->_steps.end()) {
			auto next = _prepareResourcesIterator->_stepIterator;
			++_prepareResourcesIterator->_stepIterator;
			switch (next->_type) {
			case CompiledLightingTechnique::Step::Type::ParseScene:
				return { StepType::ParseScene, next->_batch, &_prepareResourcesIterator->_drawablePkt };

			case CompiledLightingTechnique::Step::Type::DrawSky:
				return { StepType::DrawSky };

			case CompiledLightingTechnique::Step::Type::ExecuteDrawables:
				{
					auto preparation = Techniques::PrepareResources(*_prepareResourcesIterator->_pipelineAcceleratorPool, *next->_sequencerConfig, _prepareResourcesIterator->_drawablePkt);
					if (preparation)
						_prepareResourcesIterator->_requiredResources.push_back(std::move(preparation));
					_prepareResourcesIterator->_drawablePkt.Reset();
				}
				break;

			case CompiledLightingTechnique::Step::Type::CallFunction:
			case CompiledLightingTechnique::Step::Type::BeginRenderPassInstance:
			case CompiledLightingTechnique::Step::Type::EndRenderPassInstance:
			case CompiledLightingTechnique::Step::Type::NextRenderPassStep:
				break;

			case CompiledLightingTechnique::Step::Type::None:
				assert(0);
				break;
			}
		}

		return Step { StepType::None };
	}

	std::shared_ptr<::Assets::IAsyncMarker> LightingTechniqueInstance::GetResourcePreparationMarker()
	{
		if (!_prepareResourcesIterator || _prepareResourcesIterator->_requiredResources.empty()) return {};
		
		auto marker = std::make_shared<::Assets::AsyncMarkerGroup>();
		for (const auto& c:_prepareResourcesIterator->_requiredResources)
			marker->Add(c, {});
		return marker;
	}

	LightingTechniqueInstance::LightingTechniqueInstance(
		Techniques::IPipelineAcceleratorPool& pipelineAccelerators,
		CompiledLightingTechnique& technique)
	{
		_prepareResourcesIterator = std::make_unique<PrepareResourcesIterator>();
		_prepareResourcesIterator->_pipelineAcceleratorPool = &pipelineAccelerators;
		_prepareResourcesIterator->_steps = technique._steps;
		_prepareResourcesIterator->_stepIterator = _prepareResourcesIterator->_steps.begin();
	}


}}
