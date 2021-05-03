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
#include "../../OSServices/Log.h"

// For invalid asset report:
/*#include "../../Assets/AssetSetManager.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/AssetHeap.h"
#include "../../Assets/IAsyncMarker.h"
#include "../../OSServices/Log.h" */

namespace RenderCore { namespace LightingEngine
{
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

	void CompiledLightingTechnique::Push(RenderStepFragmentInterface&& fragments)
	{
		// Generate a FrameBufferDesc from the input
		UInt2 dimensionsForCompatibilityTests { _fbProps._outputWidth, _fbProps._outputHeight };
		assert(dimensionsForCompatibilityTests[0] * dimensionsForCompatibilityTests[1]);
		auto merged = Techniques::MergeFragments(
			MakeIteratorRange(_workingAttachments),
			MakeIteratorRange(&fragments.GetFrameBufferDescFragment(), &fragments.GetFrameBufferDescFragment()+1),
			dimensionsForCompatibilityTests);

		auto linkResults = LinkFragmentOutputsToSystemAttachments(merged, MakeIteratorRange(_workingAttachments));

		#if 0 // defined(_DEBUG)
			Log(Warning) << "Merged fragment in lighting technique:" << std::endl << merged._log << std::endl;
			if (RenderCore::Techniques::CanBeSimplified(merged._mergedFragment, _workingAttachments))
				Log(Warning) << "Detected a frame buffer fragment which be simplified while building lighting technique. This usually means one or more of the attachments can be reused, thereby reducing the total number of attachments required." << std::endl;
		#endif

		// Update _workingAttachments
		RenderCore::Techniques::MergeInOutputs(_workingAttachments, merged._mergedFragment, _fbProps);

		auto fbDesc = Techniques::BuildFrameBufferDesc(std::move(merged._mergedFragment), _fbProps);

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

		Step endStep;
		endStep._type = Step::Type::EndRenderPassInstance;
		endStep._fragmentLinkResults = std::move(linkResults);
		_steps.push_back(endStep);
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

	void LightingTechniqueIterator::PushFollowingStep(std::function<CompiledLightingTechnique::Step::FnSig>&& fn)
	{
		CompiledLightingTechnique::Step newStep;
		newStep._type = CompiledLightingTechnique::Step::Type::ExecuteFunction;
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

					/*auto preparation = Techniques::PrepareResources(*_iterator->_pipelineAcceleratorPool, *context._sequencerConfig, _iterator->_drawablePkt);
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
					}*/

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
						*_iterator->_attachmentPool,
						MakeIteratorRange(_iterator->_parsingContext->_preregisteredAttachments)};
				}
				break;

			case CompiledLightingTechnique::Step::Type::EndRenderPassInstance:
				{
					_iterator->_rpi.End();
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

}}
