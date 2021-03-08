// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PipelineAccelerator.h"
#include "../FrameBufferDesc.h"
#include "../Metal/DeviceContext.h"
#include "../Metal/InputLayout.h"
#include "../Metal/ObjectFactory.h"
#include "../Assets/MaterialScaffold.h"
#include "../../Assets/AssetFuture.h"
#include "../../Assets/AssetFutureContinuation.h"
#include "../../Assets/Assets.h"
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

		FrameBufferDesc _fbDesc;
		uint64_t _fbRelevanceValue = 0;
	};

	class SharedPools
	{
	public:
		Threading::Mutex _lock;
		UniqueShaderVariationSet _selectorVariationsSet;
	};

	class PipelineAccelerator : public std::enable_shared_from_this<PipelineAccelerator>
	{
	public:
		PipelineAccelerator(
			unsigned ownerPoolId,
			const std::shared_ptr<CompiledShaderPatchCollection>& shaderPatches,
			const ParameterBox& materialSelectors,
			IteratorRange<const InputElementDesc*> inputAssembly,
			Topology topology,
			const RenderCore::Assets::RenderStateSet& stateSet);
		~PipelineAccelerator();
	
		struct Pipeline
		{
			::Assets::FuturePtr<Metal::GraphicsPipeline> _future;
		};
		std::vector<Pipeline> _finalPipelines;

		Pipeline CreatePipelineForSequencerState(
			const SequencerConfig& cfg,
			const ParameterBox& globalSelectors,
			const std::shared_ptr<ICompiledPipelineLayout>& pipelineLayout,
			const std::shared_ptr<SharedPools>& sharedPools);

		Pipeline& PipelineForCfgId(SequencerConfigId cfgId);

		std::shared_ptr<CompiledShaderPatchCollection> _shaderPatches;
		ParameterBox _materialSelectors;
		ParameterBox _geoSelectors;

		std::vector<InputElementDesc> _inputAssembly;
		Topology _topology;
		RenderCore::Assets::RenderStateSet _stateSet;

		unsigned _ownerPoolId;

		std::shared_ptr<Metal::GraphicsPipeline> InternalCreatePipeline(
			const Metal::ShaderProgram& shader,
			const DepthStencilDesc& depthStencil,
			const AttachmentBlendDesc& blend,
			const RasterizationDesc& rasterization,
			const SequencerConfig& sequencerCfg)
		{
			Metal::GraphicsPipelineBuilder builder;
			builder.Bind(shader);
			builder.Bind(MakeIteratorRange(&blend, &blend+1));
			builder.Bind(depthStencil);
			builder.Bind(rasterization);

			Metal::BoundInputLayout ia(MakeIteratorRange(_inputAssembly), shader);
			builder.Bind(ia, _topology);

			builder.SetRenderPassConfiguration(sequencerCfg._fbDesc, 0);

			return builder.CreatePipeline(Metal::GetObjectFactory());
		}
	};

	auto PipelineAccelerator::CreatePipelineForSequencerState(
		const SequencerConfig& cfg,
		const ParameterBox& globalSelectors,
		const std::shared_ptr<ICompiledPipelineLayout>& pipelineLayout,
		const std::shared_ptr<SharedPools>& sharedPools) -> Pipeline
	{
		Pipeline result;
		result._future = std::make_shared<::Assets::AssetFuture<Metal::GraphicsPipeline>>("PipelineAccelerator Pipeline");

		ParameterBox copyGlobalSelectors = globalSelectors;

		auto resolvedTechnique = cfg._delegate->Resolve(_shaderPatches->GetInterface(), _stateSet);
		std::weak_ptr<PipelineAccelerator> weakThis = shared_from_this();
		::Assets::WhenAll(resolvedTechnique).ThenConstructToFuture<Metal::GraphicsPipeline>(
			*result._future,
			[sharedPools, pipelineLayout, copyGlobalSelectors, cfg, weakThis](
				::Assets::AssetFuture<Metal::GraphicsPipeline>& resultFuture,
				const std::shared_ptr<ITechniqueDelegate::GraphicsPipelineDesc>& pipelineDesc) {

				UniqueShaderVariationSet::FilteredSelectorSet filteredSelectors[dimof(ITechniqueDelegate::GraphicsPipelineDesc::_shaders)];

				auto containingPipelineAccelerator = weakThis.lock();
				if (!containingPipelineAccelerator)
					Throw(std::runtime_error("Containing GraphicsPipeline builder has been destroyed"));
				
				{
					// The list here defines the override order. Note that the global settings are last
					// because they can actually override everything
					const ParameterBox* paramBoxes[] = {
						&cfg._sequencerSelectors,
						&containingPipelineAccelerator->_geoSelectors,
						&containingPipelineAccelerator->_materialSelectors,
						&copyGlobalSelectors
					};

					ScopedLock(sharedPools->_lock);
					for (unsigned c=0; c<dimof(ITechniqueDelegate::GraphicsPipelineDesc::_shaders); ++c)
						if (!pipelineDesc->_shaders[c]._initializer.empty())
							filteredSelectors[c] = sharedPools->_selectorVariationsSet.FilterSelectors(
								MakeIteratorRange(paramBoxes),
								pipelineDesc->_manualSelectorFiltering,
								*pipelineDesc->_shaders[c]._automaticFiltering);
				}

				::Assets::FuturePtr<Metal::ShaderProgram> shaderProgram;
				if (pipelineDesc->_shaders[(unsigned)ShaderStage::Geometry]._initializer.empty()) {
					shaderProgram = ::Assets::MakeAsset<Metal::ShaderProgram>(
						pipelineLayout,
						pipelineDesc->_shaders[(unsigned)ShaderStage::Vertex]._initializer,
						pipelineDesc->_shaders[(unsigned)ShaderStage::Pixel]._initializer);
				} else {
					shaderProgram = ::Assets::MakeAsset<Metal::ShaderProgram>(
						pipelineLayout,
						pipelineDesc->_shaders[(unsigned)ShaderStage::Vertex]._initializer,
						pipelineDesc->_shaders[(unsigned)ShaderStage::Geometry]._initializer,
						pipelineDesc->_shaders[(unsigned)ShaderStage::Pixel]._initializer);
				}

				::Assets::WhenAll(shaderProgram).ThenConstructToFuture<Metal::GraphicsPipeline>(
					resultFuture,
					[cfg, pipelineDesc, weakThis](const std::shared_ptr<Metal::ShaderProgram>& shaderProgram) {
						auto containingPipelineAccelerator = weakThis.lock();
						if (!containingPipelineAccelerator)
							Throw(std::runtime_error("Containing GraphicsPipeline builder has been destroyed"));

						return containingPipelineAccelerator->InternalCreatePipeline(
							*shaderProgram, 
							pipelineDesc->_depthStencil, 
							pipelineDesc->_blend[0], 
							pipelineDesc->_rasterization, 
							cfg);
					});
			});

	return result;

#if 0
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
					::Assets::AssetPtr<Metal::ShaderProgram> shaderActual;
					::Assets::DepValPtr depVal;
					::Assets::Blob actualizationLog;
					auto shaderState = shader._shaderProgram->CheckStatusBkgrnd(shaderActual, depVal, actualizationLog);
					if (!shaderActual) {
						if (shaderState == ::Assets::AssetState::Invalid) {
							thatFuture.SetInvalidAsset(depVal, actualizationLog);
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

					auto pipeline = containingPipelineAccelerator->InternalCreatePipeline(*shaderActual, shader._depthStencil, shader._blend, shader._rasterization, cfg);
					thatFuture.SetAsset(std::move(pipeline), actualizationLog);
					return false;
				});
		}
#endif
		
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
		Topology topology,
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

		bool foundPosition = false;

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

			foundPosition |= XlEqStringI(i->_semanticName, "POSITION");
		}

		// If we have no IA elements at all, force on GEO_HAS_VERTEX_ID. Shaders will almost always
		// require it in this case, because there's no other way to distinquish one vertex from
		// the next.
		if (sortedIA.empty()) {
			_geoSelectors.SetParameter("GEO_HAS_VERTEX_ID", 1);
		}
		if (!foundPosition) {
			_geoSelectors.SetParameter("GEO_NO_POSITION", 1);
		}
	}

	PipelineAccelerator::~PipelineAccelerator()
	{}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//		P   O   O   L
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class PipelineAcceleratorPool : public IPipelineAcceleratorPool
	{
	public:
		std::shared_ptr<PipelineAccelerator> CreatePipelineAccelerator(
			const std::shared_ptr<CompiledShaderPatchCollection>& shaderPatches,
			const ParameterBox& materialSelectors,
			IteratorRange<const InputElementDesc*> inputAssembly,
			Topology topology,
			const RenderCore::Assets::RenderStateSet& stateSet) override;

		std::shared_ptr<SequencerConfig> CreateSequencerConfig(
			const std::shared_ptr<ITechniqueDelegate>& delegate,
			const ParameterBox& sequencerSelectors,
			const FrameBufferDesc& fbDesc,
			unsigned subpassIndex = 0) override;

		const ::Assets::FuturePtr<Metal::GraphicsPipeline>& GetPipeline(PipelineAccelerator& pipelineAccelerator, const SequencerConfig& sequencerConfig) const override;
		const Metal::GraphicsPipeline* TryGetPipeline(PipelineAccelerator& pipelineAccelerator, const SequencerConfig& sequencerConfig) const override;

		void			SetGlobalSelector(StringSection<> name, IteratorRange<const void*> data, const ImpliedTyping::TypeDesc& type) override;
		T1(Type) void   SetGlobalSelector(StringSection<> name, Type value);
		void			RemoveGlobalSelector(StringSection<> name) override;

		void			RebuildAllOutOfDatePipelines() override;

		const std::shared_ptr<IDevice>& GetDevice() const override;

		PipelineAcceleratorPool(const std::shared_ptr<IDevice>& device, const std::shared_ptr<ICompiledPipelineLayout>& pipelineLayout);
		~PipelineAcceleratorPool();
		PipelineAcceleratorPool(const PipelineAcceleratorPool&) = delete;
		PipelineAcceleratorPool& operator=(const PipelineAcceleratorPool&) = delete;

	protected:
		ParameterBox _globalSelectors;
		std::vector<std::pair<uint64_t, std::weak_ptr<SequencerConfig>>> _sequencerConfigById;
		std::vector<std::pair<uint64_t, std::weak_ptr<PipelineAccelerator>>> _pipelineAccelerators;

		SequencerConfig MakeSequencerConfig(
			/*out*/ uint64_t& hash,
			const std::shared_ptr<ITechniqueDelegate>& delegate,
			const ParameterBox& sequencerSelectors,
			const FrameBufferDesc& fbDesc,
			unsigned subpassIndex);

		void RebuildAllPipelines(unsigned poolGuid);
		void RebuildAllPipelines(unsigned poolGuid, PipelineAccelerator& pipeline);

		std::shared_ptr<ICompiledPipelineLayout> _pipelineLayout;
		std::shared_ptr<SharedPools> _sharedPools;
	};

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

	SequencerConfig PipelineAcceleratorPool::MakeSequencerConfig(
		/*out*/ uint64_t& hash,
		const std::shared_ptr<ITechniqueDelegate>& delegate,
		const ParameterBox& sequencerSelectors,
		const FrameBufferDesc& fbDesc,
		unsigned subpassIndex)
	{
		// Search for an identical sequencer config already registered, and return it
		// if it's here already. Other create it and return the result
		assert(!fbDesc.GetSubpasses().empty());

		SequencerConfig cfg;
		cfg._delegate = delegate;
		cfg._sequencerSelectors = sequencerSelectors;

		cfg._fbDesc = fbDesc;
		if (subpassIndex != 0 || fbDesc.GetSubpasses().size() > 1)
			cfg._fbDesc = SeparateSingleSubpass(fbDesc, subpassIndex);

		cfg._fbRelevanceValue = Metal::GraphicsPipelineBuilder::CalculateFrameBufferRelevance(cfg._fbDesc);

		hash = HashCombine(sequencerSelectors.GetHash(), sequencerSelectors.GetParameterNamesHash());
		hash = HashCombine(cfg._fbRelevanceValue, hash);

		// todo -- we must take into account the delegate itself; it must impact the hash
		hash = HashCombine(uint64_t(delegate.get()), hash);

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
		Topology topology,
		const RenderCore::Assets::RenderStateSet& stateSet)
	{
		uint64_t hash = HashCombine(materialSelectors.GetHash(), materialSelectors.GetParameterNamesHash());
		hash = HashCombine(Hash(inputAssembly), hash);
		hash = HashCombine((unsigned)topology, hash);
		hash = HashCombine(stateSet.GetHash(), hash);

		// If it already exists in the cache, just return it now
		auto i = LowerBound(_pipelineAccelerators, hash);
		if (i != _pipelineAccelerators.end() && i->first == hash) {
			auto l = i->second.lock();
			if (l)
				return l;
		}

		auto newAccelerator = std::make_shared<PipelineAccelerator>(
			_guid,
			shaderPatches, materialSelectors,
			inputAssembly, topology,
			stateSet);

		if (i != _pipelineAccelerators.end() && i->first == hash) {
			i->second = newAccelerator;		// (we replaced one that expired)
		} else {
			_pipelineAccelerators.insert(i, std::make_pair(hash, newAccelerator));
		}

		RebuildAllPipelines(_guid, *newAccelerator);

		return newAccelerator;
	}

	auto PipelineAcceleratorPool::CreateSequencerConfig(
		const std::shared_ptr<ITechniqueDelegate>& delegate,
		const ParameterBox& sequencerSelectors,
		const FrameBufferDesc& fbDesc,
		unsigned subpassIndex) -> std::shared_ptr<SequencerConfig>
	{
		uint64_t hash = 0;
		auto cfg = MakeSequencerConfig(hash, delegate, sequencerSelectors, fbDesc, subpassIndex);

		// Look for an existing configuration with the same settings
		//	-- todo, not checking the delegate here!
		for (auto i=_sequencerConfigById.begin(); i!=_sequencerConfigById.end(); ++i) {
			if (i->first == hash) {
				auto cfgId = SequencerConfigId(i - _sequencerConfigById.begin()) | (SequencerConfigId(_guid) << 32ull);
				
				auto result = i->second.lock();

				// The configuration may have expired. In this case, we should just create it again, and reset
				// our pointer. Note that we only even hold a weak pointer, so if the caller doesn't hold
				// onto the result, it's just going to expire once more
				if (!result) {
					result = std::make_shared<SequencerConfig>(std::move(cfg));
					result->_cfgId = cfgId;
					i->second = result;
				}

				// If a pipeline accelerator was added while this sequencer config was expired, the pipeline
				// accelerator would not have been configured. We have to check for this case and construct
				// as necessary -- 
				for (auto& accelerator:_pipelineAccelerators) {
					auto a = accelerator.second.lock();
					if (a) {
						auto& pipeline = a->PipelineForCfgId(cfgId);
						if (!pipeline._future)
							pipeline = a->CreatePipelineForSequencerState(*result, _globalSelectors, _pipelineLayout, _sharedPools);
					}
				}
				
				return result;
			}
		}

		auto cfgId = SequencerConfigId(_sequencerConfigById.size()) | (SequencerConfigId(_guid) << 32ull);
		auto result = std::make_shared<SequencerConfig>(std::move(cfg));
		result->_cfgId = cfgId;

		_sequencerConfigById.emplace_back(std::make_pair(hash, result));		// (note; only holding onto a weak pointer here)

		// trigger creation of pipeline states for all accelerators
		for (auto& accelerator:_pipelineAccelerators) {
			auto a = accelerator.second.lock();
			if (a)
				a->PipelineForCfgId(cfgId) = a->CreatePipelineForSequencerState(*result, _globalSelectors, _pipelineLayout, _sharedPools);
		}

		return result;
	}

	void PipelineAcceleratorPool::RebuildAllPipelines(unsigned poolGuid, PipelineAccelerator& pipeline)
	{
		for (unsigned c=0; c<_sequencerConfigById.size(); ++c) {
			auto cfgId = SequencerConfigId(c) | (SequencerConfigId(poolGuid) << 32ull);
			auto l = _sequencerConfigById[c].second.lock();
			if (l) 
				pipeline.PipelineForCfgId(cfgId) = pipeline.CreatePipelineForSequencerState(*l, _globalSelectors, _pipelineLayout, _sharedPools);
		}
	}

	void PipelineAcceleratorPool::RebuildAllPipelines(unsigned poolGuid)
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
		lockedSequencerConfigs.reserve(_sequencerConfigById.size());
		for (unsigned c=0; c<_sequencerConfigById.size(); ++c) {
			auto l = _sequencerConfigById[c].second.lock();
			lockedSequencerConfigs.emplace_back(std::move(l));
		}
					
		for (auto& accelerator:_pipelineAccelerators) {
			auto a = accelerator.second.lock();
			if (a) {
				for (unsigned c=0; c<std::min(_sequencerConfigById.size(), a->_finalPipelines.size()); ++c) {
					if (!lockedSequencerConfigs[c])
						continue;

					auto& p = a->_finalPipelines[c];
					if (p._future->GetAssetState() != ::Assets::AssetState::Pending && p._future->GetDependencyValidation()->GetValidationIndex() != 0) {
						// It's out of date -- let's rebuild and reassign it
						p = a->CreatePipelineForSequencerState(*lockedSequencerConfigs[c], _globalSelectors, _pipelineLayout, _sharedPools);
					}
				}
			}
		}
	}

	void PipelineAcceleratorPool::SetGlobalSelector(StringSection<> name, IteratorRange<const void*> data, const ImpliedTyping::TypeDesc& type)
	{
		_globalSelectors.SetParameter(name.Cast<utf8>(), data, type);
		RebuildAllPipelines(_guid);
	}

	void PipelineAcceleratorPool::RemoveGlobalSelector(StringSection<> name)
	{
		_globalSelectors.RemoveParameter(name.Cast<utf8>());
		RebuildAllPipelines(_guid);
	}

	const std::shared_ptr<IDevice>& PipelineAcceleratorPool::GetDevice() const { return _device; }

	static unsigned s_nextPipelineAcceleratorPoolGUID = 1;

	PipelineAcceleratorPool::PipelineAcceleratorPool(const std::shared_ptr<IDevice>& device, const std::shared_ptr<ICompiledPipelineLayout>& pipelineLayout)
	{
		_guid = s_nextPipelineAcceleratorPoolGUID++;
		_device = device;
		_pipelineLayout = pipelineLayout;
		_sharedPools = std::make_shared<SharedPools>();
	}

	PipelineAcceleratorPool::~PipelineAcceleratorPool() {}


	IPipelineAcceleratorPool::~IPipelineAcceleratorPool() {}

	std::shared_ptr<IPipelineAcceleratorPool> CreatePipelineAcceleratorPool(const std::shared_ptr<IDevice>& device, const std::shared_ptr<ICompiledPipelineLayout>& pipelineLayout)
	{
		return std::make_shared<PipelineAcceleratorPool>(device, pipelineLayout);
	}

}}
