// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PipelineAccelerator.h"
#include "../FrameBufferDesc.h"
#include "../Metal/DeviceContext.h"
#include "../Metal/InputLayout.h"
#include "../Assets/MaterialScaffold.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/StringFormat.h"
#include <cctype>

#include "Techniques.h"
#include "TechniqueDelegates.h"

namespace RenderCore { namespace Techniques
{
	using SequencerConfigId = uint64_t;

	class SequencerConfig
	{
	public:
		SequencerConfigId _cfgId = ~0ull;

		std::shared_ptr<ITechniqueDelegate> _delegate;
		ParameterBox _sequencerSelectors;

		FrameBufferProperties _fbProps;
		FrameBufferDesc _fbDesc;
		uint64_t _fbRelevanceValue = 0;
	};

	class PipelineAccelerator : public std::enable_shared_from_this<PipelineAccelerator>
	{
	public:
		PipelineAccelerator(
			unsigned ownerPoolId,
			const std::shared_ptr<CompiledShaderPatchCollection>& shaderPatches,
			const ParameterBox& materialSelectors,
			IteratorRange<const InputElementDesc*> inputAssembly,
			RenderCore::Topology topology,
			const RenderCore::Assets::RenderStateSet& stateSet);
		~PipelineAccelerator();
	
		struct Pipeline
		{
			::Assets::FuturePtr<Metal::GraphicsPipeline> _future;
		};
		std::vector<Pipeline> _finalPipelines;

		Pipeline CreatePipelineForSequencerState(
			const SequencerConfig& cfg,
			const ParameterBox& globalSelectors);

		Pipeline& PipelineForCfgId(SequencerConfigId cfgId);

		const std::shared_ptr<CompiledShaderPatchCollection> _shaderPatches;
		ParameterBox _materialSelectors;
		ParameterBox _geoSelectors;

		std::vector<InputElementDesc> _inputAssembly;
		RenderCore::Topology _topology;
		RenderCore::Assets::RenderStateSet _stateSet;

		unsigned _ownerPoolId;

		std::shared_ptr<Metal::GraphicsPipeline> InternalCreatePipeline(
			const Metal::ShaderProgram& shader,
			const RenderCore::DepthStencilDesc& depthStencil,
			const RenderCore::AttachmentBlendDesc& blend,
			const RenderCore::RasterizationDesc& rasterization,
			const SequencerConfig& sequencerCfg)
		{
			Metal::GraphicsPipelineBuilder builder;
			builder.Bind(shader);
			builder.Bind(blend);
			builder.Bind(depthStencil);
			builder.Bind(rasterization);
			builder.Bind(_topology);

			Metal::BoundInputLayout ia(MakeIteratorRange(_inputAssembly), shader);
			builder.SetInputLayout(ia);

			builder.SetRenderPassConfiguration(sequencerCfg._fbProps, sequencerCfg._fbDesc);

			return builder.CreatePipeline(Metal::GetObjectFactory());
		}

	};

	auto PipelineAccelerator::CreatePipelineForSequencerState(
		const SequencerConfig& cfg,
		const ParameterBox& globalSelectors) -> Pipeline
	{
		Pipeline result;
		result._future = std::make_shared<::Assets::AssetFuture<Metal::GraphicsPipeline>>("PipelineAccelerator Pipeline");

		// Consider the pipeline accelerator required for the given that, and store it with
		// the id provided

		// The list here defines the override order. Note that the global settings are last
		// because they can actually override everything
		const ParameterBox* paramBoxes[] = {
			&cfg._sequencerSelectors,
			&_geoSelectors,
			&_materialSelectors,
			&globalSelectors
		};

		auto shader = cfg._delegate->Resolve(
			_shaderPatches,
			MakeIteratorRange(paramBoxes),
			_stateSet);
		
		auto state = shader._shaderProgram->GetAssetState();
		if (state == ::Assets::AssetState::Invalid) {
			result._future->SetInvalidAsset(shader._shaderProgram->GetDependencyValidation(), shader._shaderProgram->GetActualizationLog());
		} else if (state == ::Assets::AssetState::Ready) {
			//
			// Since we're ready, let's take an accelerated path and just construct
			// the pipeline right here and now
			//
			auto pipeline = InternalCreatePipeline(*shader._shaderProgram->Actualize(), shader._depthStencil, shader._blend, shader._rasterization, cfg);
			result._future->SetAsset(std::move(pipeline), nullptr);
		} else {
			//
			// Our final pipeline is dependant on the shader compilation
			// We can create a future that will trigger and complete processing after
			// the shader has finished compilating
			//
			std::weak_ptr<PipelineAccelerator> weakThis = shared_from_this();
			result._future->SetPollingFunction(
				[shader, cfg, weakThis](::Assets::AssetFuture<Metal::GraphicsPipeline>& thatFuture) {
					auto shaderActual = shader._shaderProgram->TryActualize();
					if (!shaderActual) {
						auto state = shader._shaderProgram->GetAssetState();
						if (state == ::Assets::AssetState::Invalid) {
							thatFuture.SetInvalidAsset(shader._shaderProgram->GetDependencyValidation(), shader._shaderProgram->GetActualizationLog());
							return false;
						}
						return true;
					}

					auto containingPipelineAccelerator = weakThis.lock();
					if (!containingPipelineAccelerator) {
						thatFuture.SetInvalidAsset(
							std::make_shared<::Assets::DependencyValidation>(),
							::Assets::AsBlob("Containing GraphicsPipeline builder has been destroyed"));
						return false;
					}

					auto pipeline = containingPipelineAccelerator->InternalCreatePipeline(*shader._shaderProgram->Actualize(), shader._depthStencil, shader._blend, shader._rasterization, cfg);
					thatFuture.SetAsset(std::move(pipeline), nullptr);
					return false;
				});
		}
		
		return result;
	}

	auto PipelineAccelerator::PipelineForCfgId(SequencerConfigId cfgId) -> Pipeline&
	{
		unsigned poolId = unsigned(cfgId >> 32ull);
		unsigned sequencerIdx = unsigned(cfgId);
		if (poolId != _ownerPoolId)
			Throw(std::runtime_error("Mixing a pipeline accelerator from an incorrect pool"));

		if (_finalPipelines.size() < sequencerIdx+1)
			_finalPipelines.resize(sequencerIdx+1);
		return _finalPipelines[sequencerIdx];
	}

	PipelineAccelerator::PipelineAccelerator(
		unsigned ownerPoolId,
		const std::shared_ptr<CompiledShaderPatchCollection>& shaderPatches,
		const ParameterBox& materialSelectors,
		IteratorRange<const InputElementDesc*> inputAssembly,
		RenderCore::Topology topology,
		const RenderCore::Assets::RenderStateSet& stateSet)
	: _shaderPatches(shaderPatches)
	, _materialSelectors(materialSelectors)
	, _inputAssembly(inputAssembly.begin(), inputAssembly.end())
	, _topology(topology)
	, _stateSet(stateSet)
	, _ownerPoolId(ownerPoolId)
	{
		std::vector<InputElementDesc> sortedIA = _inputAssembly;
		std::sort(
			sortedIA.begin(), sortedIA.end(),
			[](const InputElementDesc& lhs, const InputElementDesc& rhs) {
				if (lhs._semanticName < rhs._semanticName) return true;
				if (lhs._semanticName > rhs._semanticName) return false;
				return lhs._semanticIndex > rhs._semanticIndex;	// note -- reversing order
			});

		// Build up the geometry selectors. 
		for (auto i = sortedIA.begin(); i!=sortedIA.end(); ++i) {
			// If we have the same name as the last one, we should just skip (because the
			// previous one would have had a larger semantic index, and effectively took
			// care of this selector)
			if (i!=sortedIA.begin() && (i-1)->_semanticName == i->_semanticName)
				continue;

			char buffer[256] = "GEO_HAS_";
			unsigned c=0;
			for (; c<i->_semanticName.size() && c < 255-8; ++c)
				buffer[8+c] = (char)std::toupper(i->_semanticName[c]);	// ensure that we're using upper case for the full semantic
			buffer[8+c] = '\0';
			_geoSelectors.SetParameter((const utf8*)buffer, i->_semanticIndex+1);
		}
	}

	PipelineAccelerator::~PipelineAccelerator()
	{}

	const ::Assets::FuturePtr<Metal::GraphicsPipeline>& PipelineAcceleratorPool::GetPipeline(
		PipelineAccelerator& pipelineAccelerator, 
		const SequencerConfig& sequencerConfig) const
	{
		unsigned sequencerIdx = unsigned(sequencerConfig._cfgId);
		#if defined(_DEBUG)
			unsigned poolId = unsigned(sequencerConfig._cfgId >> 32ull);
			if (poolId != pipelineAccelerator._ownerPoolId)
				Throw(std::runtime_error("Mixing a pipeline accelerator from an incorrect pool"));
			if (sequencerIdx >= pipelineAccelerator._finalPipelines.size())
				Throw(std::runtime_error("Bad sequencer config id"));
		#endif
		
		return pipelineAccelerator._finalPipelines[sequencerIdx]._future;
	}

	const Metal::GraphicsPipeline* PipelineAcceleratorPool::TryGetPipeline(
		PipelineAccelerator& pipelineAccelerator, 
		const SequencerConfig& sequencerConfig) const
	{
		unsigned sequencerIdx = unsigned(sequencerConfig._cfgId);
		#if defined(_DEBUG)
			unsigned poolId = unsigned(sequencerConfig._cfgId >> 32ull);
			if (poolId != pipelineAccelerator._ownerPoolId)
				Throw(std::runtime_error("Mixing a pipeline accelerator from an incorrect pool"));
			if (sequencerIdx >= pipelineAccelerator._finalPipelines.size())
				Throw(std::runtime_error("Bad sequencer config id"));
		#endif
		
		return pipelineAccelerator._finalPipelines[sequencerIdx]._future->TryActualize().get();
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//		P   O   O   L
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class PipelineAcceleratorPool::Pimpl
	{
	public:
		ParameterBox _globalSelectors;
		std::vector<std::pair<uint64_t, std::weak_ptr<SequencerConfig>>> _sequencerConfigById;
		std::vector<std::pair<uint64_t, std::weak_ptr<PipelineAccelerator>>> _pipelineAccelerators;

		SequencerConfig MakeSequencerConfig(
			/*out*/ uint64_t& hash,
			const std::shared_ptr<ITechniqueDelegate>& delegate,
			const ParameterBox& sequencerSelectors,
			const FrameBufferProperties& fbProps,
			const FrameBufferDesc& fbDesc,
			unsigned subpassIndex);

		void RebuildAllPipelines(unsigned poolGuid);
		void RebuildAllPipelines(unsigned poolGuid, PipelineAccelerator& pipeline);
	};

	SequencerConfig PipelineAcceleratorPool::Pimpl::MakeSequencerConfig(
		/*out*/ uint64_t& hash,
		const std::shared_ptr<ITechniqueDelegate>& delegate,
		const ParameterBox& sequencerSelectors,
		const FrameBufferProperties& fbProps,
		const FrameBufferDesc& fbDesc,
		unsigned subpassIndex)
	{
		// Search for an identical sequencer config already registered, and return it
		// if it's here already. Other create it and return the result
		assert(!fbDesc.GetSubpasses().empty());

		SequencerConfig cfg;
		cfg._delegate = delegate;
		cfg._sequencerSelectors = sequencerSelectors;
		cfg._fbProps = fbProps;

		cfg._fbDesc = fbDesc;
		if (subpassIndex != 0 || fbDesc.GetSubpasses().size() > 1)
			cfg._fbDesc = SeparateSingleSubpass(fbDesc, subpassIndex);

		cfg._fbRelevanceValue = Metal::GraphicsPipelineBuilder::CalculateFrameBufferRelevance(cfg._fbProps, cfg._fbDesc);

		hash = HashCombine(sequencerSelectors.GetHash(), sequencerSelectors.GetParameterNamesHash());
		hash = HashCombine(cfg._fbRelevanceValue, hash);

		// todo -- we must take into account the delegate itself; it must impact the hash
		hash = HashCombine(uint64_t(&delegate), hash);

		return cfg;
	}

	static uint64_t Hash(IteratorRange<const InputElementDesc*> inputAssembly)
	{
		auto norm = NormalizeInputAssembly(inputAssembly);
		uint64_t result = DefaultSeed64;
		for (const auto&a:norm) {
			result = Hash64(a._semanticName, result);

			assert((uint64_t(a._nativeFormat) & ~0xffull) == 0);
			assert((uint64_t(a._alignedByteOffset) & ~0xffull) == 0);
			assert((uint64_t(a._semanticIndex) & ~0xfull) == 0);
			assert((uint64_t(a._inputSlot) & ~0xfull) == 0);
			assert((uint64_t(a._inputSlotClass) & ~0xfull) == 0);
			assert((uint64_t(a._instanceDataStepRate) & ~0xfull) == 0);
			uint64_t paramHash = 
					((uint64_t(a._nativeFormat) & 0xffull) << 0ull)
				|	((uint64_t(a._alignedByteOffset) & 0xffull) << 8ull)
				|	((uint64_t(a._semanticIndex) & 0xfull) << 16ull)
				|	((uint64_t(a._inputSlot) & 0xfull) << 20ull)
				|	((uint64_t(a._inputSlotClass) & 0xfull) << 24ull)
				|	((uint64_t(a._instanceDataStepRate) & 0xfull) << 28ull)
				;
			result = HashCombine(paramHash, result);
		}

		return result;
	}

	std::shared_ptr<PipelineAccelerator> PipelineAcceleratorPool::CreatePipelineAccelerator(
		const std::shared_ptr<CompiledShaderPatchCollection>& shaderPatches,
		const ParameterBox& materialSelectors,
		IteratorRange<const InputElementDesc*> inputAssembly,
		RenderCore::Topology topology,
		const RenderCore::Assets::RenderStateSet& stateSet)
	{
		uint64_t hash = HashCombine(materialSelectors.GetHash(), materialSelectors.GetParameterNamesHash());
		hash = HashCombine(Hash(inputAssembly), hash);
		hash = HashCombine((unsigned)topology, hash);
		hash = HashCombine(stateSet.GetHash(), hash);

		// If it already exists in the cache, just return it now
		auto i = LowerBound(_pimpl->_pipelineAccelerators, hash);
		if (i != _pimpl->_pipelineAccelerators.end() && i->first == hash) {
			auto l = i->second.lock();
			if (l)
				return l;
		}

		auto newAccelerator = std::make_shared<PipelineAccelerator>(
			_guid,
			shaderPatches, materialSelectors,
			inputAssembly, topology,
			stateSet);

		if (i != _pimpl->_pipelineAccelerators.end() && i->first == hash) {
			i->second = newAccelerator;		// (we replaced one that expired)
		} else {
			_pimpl->_pipelineAccelerators.insert(i, std::make_pair(hash, newAccelerator));
		}

		_pimpl->RebuildAllPipelines(_guid, *newAccelerator);

		return newAccelerator;
	}

	auto PipelineAcceleratorPool::CreateSequencerConfig(
		const std::shared_ptr<ITechniqueDelegate>& delegate,
		const ParameterBox& sequencerSelectors,
		const FrameBufferProperties& fbProps,
		const FrameBufferDesc& fbDesc,
		unsigned subpassIndex) -> std::shared_ptr<SequencerConfig>
	{
		uint64_t hash = 0;
		auto cfg = _pimpl->MakeSequencerConfig(hash, delegate, sequencerSelectors, fbProps, fbDesc, subpassIndex);

		// Look for an existing configuration with the same settings
		//	-- todo, not checking the delegate here!
		for (auto i=_pimpl->_sequencerConfigById.begin(); i!=_pimpl->_sequencerConfigById.end(); ++i) {
			if (i->first == hash) {
				auto cfgId = SequencerConfigId(i - _pimpl->_sequencerConfigById.begin()) | (SequencerConfigId(_guid) << 32ull);
				
				auto result = i->second.lock();

				// The configuration may have expired. In this case, we should just create it again, and reset
				// our pointer. Note that we only even hold a weak pointer, so if the caller doesn't hold
				// onto the result, it's just going to expire once more
				if (!result) {
					result = std::make_shared<SequencerConfig>(std::move(cfg));
					result->_cfgId = cfgId;
					i->second = result;
				}
				
				return result;
			}
		}

		auto cfgId = SequencerConfigId(_pimpl->_sequencerConfigById.size()) | (SequencerConfigId(_guid) << 32ull);
		auto result = std::make_shared<SequencerConfig>(std::move(cfg));
		result->_cfgId = cfgId;

		_pimpl->_sequencerConfigById.emplace_back(std::make_pair(hash, result));		// (note; only holding onto a weak pointer here)

		// trigger creation of pipeline states for all accelerators
		for (auto& accelerator:_pimpl->_pipelineAccelerators) {
			auto a = accelerator.second.lock();
			if (a)
				a->PipelineForCfgId(cfgId) = a->CreatePipelineForSequencerState(*result, _pimpl->_globalSelectors);
		}

		return result;
	}

	void PipelineAcceleratorPool::Pimpl::RebuildAllPipelines(unsigned poolGuid, PipelineAccelerator& pipeline)
	{
		for (unsigned c=0; c<_sequencerConfigById.size(); ++c) {
			auto cfgId = SequencerConfigId(c) | (SequencerConfigId(poolGuid) << 32ull);
			auto l = _sequencerConfigById[c].second.lock();
			if (l) 
				pipeline.PipelineForCfgId(cfgId) = pipeline.CreatePipelineForSequencerState(*l, _globalSelectors);
		}
	}

	void PipelineAcceleratorPool::Pimpl::RebuildAllPipelines(unsigned poolGuid)
	{
		for (auto& accelerator:_pipelineAccelerators) {
			auto a = accelerator.second.lock();
			if (a)
				RebuildAllPipelines(poolGuid, *a);
		}
	}

	void PipelineAcceleratorPool::RebuildAllOutOfDatePipelines()
	{
		// Look through every pipeline registered in this pool, and 
		// trigger a rebuild of any that appear to be out of date.
		// This allows us to support hotreloading when files change, etc
		std::vector<std::shared_ptr<SequencerConfig>> lockedSequencerConfigs;
		lockedSequencerConfigs.reserve(_pimpl->_sequencerConfigById.size());
		for (unsigned c=0; c<_pimpl->_sequencerConfigById.size(); ++c) {
			auto l = _pimpl->_sequencerConfigById[c].second.lock();
			lockedSequencerConfigs.emplace_back(std::move(l));
		}
					
		for (auto& accelerator:_pimpl->_pipelineAccelerators) {
			auto a = accelerator.second.lock();
			if (a) {
				for (unsigned c=0; c<std::min(_pimpl->_sequencerConfigById.size(), a->_finalPipelines.size()); ++c) {
					if (!lockedSequencerConfigs[c])
						continue;

					auto& p = a->_finalPipelines[c];
					if (p._future->GetAssetState() != ::Assets::AssetState::Pending && p._future->GetDependencyValidation()->GetValidationIndex() != 0) {
						// It's out of date -- let's rebuild and reassign it
						p = a->CreatePipelineForSequencerState(*lockedSequencerConfigs[c], _pimpl->_globalSelectors);
					}
				}
			}
		}
	}

	void PipelineAcceleratorPool::SetGlobalSelector(StringSection<> name, IteratorRange<const void*> data, const ImpliedTyping::TypeDesc& type)
	{
		_pimpl->_globalSelectors.SetParameter(name.Cast<utf8>(), data, type);
		_pimpl->RebuildAllPipelines(_guid);
	}

	void PipelineAcceleratorPool::RemoveGlobalSelector(StringSection<> name)
	{
		_pimpl->_globalSelectors.RemoveParameter(name.Cast<utf8>());
		_pimpl->RebuildAllPipelines(_guid);
	}

	static unsigned s_nextPipelineAcceleratorPoolGUID = 1;

	PipelineAcceleratorPool::PipelineAcceleratorPool()
	: _guid(s_nextPipelineAcceleratorPoolGUID++)
	{
		_pimpl = std::make_unique<Pimpl>();
	}

	PipelineAcceleratorPool::~PipelineAcceleratorPool() {}


	std::shared_ptr<PipelineAcceleratorPool> CreatePipelineAcceleratorPool()
	{
		return std::make_shared<PipelineAcceleratorPool>();
	}

}}
