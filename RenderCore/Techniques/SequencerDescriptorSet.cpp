// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SequencerDescriptorSet.h"
#include "PipelineAccelerator.h"
#include "../../OSServices/Log.h"

namespace RenderCore { namespace Techniques
{

	void SequencerUniformsHelper::Prepare(IShaderResourceDelegate& del, ParsingContext& parsingContext)
	{
		ShaderResourceDelegateBinding newBinding;
		newBinding._delegate = &del;

		auto& usi = del.GetInterface();
		newBinding._resourceInterfaceToUSI.reserve(usi._resourceViewBindings.size());
		for (auto b:usi._resourceViewBindings) {
			auto existing = std::find(_finalUSI._resourceViewBindings.begin(), _finalUSI._resourceViewBindings.end(), b);
			if (existing != _finalUSI._resourceViewBindings.end()) {
				newBinding._resourceInterfaceToUSI.push_back(~0u);
			} else {
				auto finalUSISlot = (unsigned)_finalUSI._resourceViewBindings.size();
				newBinding._resourceInterfaceToUSI.push_back(finalUSISlot);
				_finalUSI._resourceViewBindings.push_back(b);
				assert(finalUSISlot < 64);
				newBinding._usiSlotsFilled_ResourceViews |= 1ull << uint64_t(finalUSISlot);
			}
		}

		newBinding._samplerInterfaceToUSI.reserve(usi._samplerBindings.size());
		for (auto b:usi._samplerBindings) {
			auto existing = std::find(_finalUSI._samplerBindings.begin(), _finalUSI._samplerBindings.end(), b);
			if (existing != _finalUSI._samplerBindings.end()) {
				newBinding._samplerInterfaceToUSI.push_back(~0u);
			} else {
				auto finalUSISlot = (unsigned)_finalUSI._samplerBindings.size();
				newBinding._samplerInterfaceToUSI.push_back(finalUSISlot);
				_finalUSI._samplerBindings.push_back(b);
				assert(finalUSISlot < 64);
				newBinding._usiSlotsFilled_Samplers |= 1ull << uint64_t(finalUSISlot);
			}
		}

		newBinding._immediateDataInterfaceToUSI.reserve(usi._immediateDataBindings.size());
		unsigned idx=0;
		for (auto b:usi._immediateDataBindings) {
			auto existing = std::find(_finalUSI._immediateDataBindings.begin(), _finalUSI._immediateDataBindings.end(), b);
			if (existing != _finalUSI._immediateDataBindings.end()) {
				newBinding._immediateDataInterfaceToUSI.push_back(~0u);
				newBinding._immediateDataBeginAndEnd.push_back({});
			} else {
				auto finalUSISlot = (unsigned)_finalUSI._immediateDataBindings.size();
				newBinding._immediateDataInterfaceToUSI.push_back(finalUSISlot);
				_finalUSI._immediateDataBindings.push_back(b);
				assert(finalUSISlot < 64);
				newBinding._usiSlotsFilled_ImmediateDatas |= 1ull << uint64_t(finalUSISlot);

				auto size = del.GetImmediateDataSize(parsingContext, nullptr, idx);
				std::pair<size_t, size_t> beginAndEnd;
				beginAndEnd.first = _workingTempBufferSize;
				beginAndEnd.second = _workingTempBufferSize + size;
				newBinding._immediateDataBeginAndEnd.push_back(beginAndEnd);
				_workingTempBufferSize += CeilToMultiplePow2(size, s_immediateDataAlignment);
			}
			++idx;
		}

		_srBindings.push_back(newBinding);
	}

	void SequencerUniformsHelper::QueryResources(ParsingContext& parsingContext, uint64_t resourcesToQuery, ShaderResourceDelegateBinding& del)
	{
		auto toLoad = resourcesToQuery & del._usiSlotsFilled_ResourceViews;
		if (!toLoad) return;

		uint64_t toLoadDelegate = 0;
		for (unsigned c=0; c<del._resourceInterfaceToUSI.size(); ++c)
			if (del._resourceInterfaceToUSI[c] != ~0u && (resourcesToQuery & (1 << uint64_t(del._resourceInterfaceToUSI[c]))))
				toLoadDelegate |= 1ull << uint64_t(c);

		assert(toLoadDelegate);
		auto minToCheck = xl_ctz8(toLoad);
		auto maxPlusOneToCheck = 64 - xl_clz8(toLoad);
		IResourceView* rvDst[maxPlusOneToCheck];

		del._delegate->WriteResourceViews(parsingContext, nullptr, toLoadDelegate, MakeIteratorRange(rvDst, &rvDst[maxPlusOneToCheck]));
		
		for (unsigned c=minToCheck; c<maxPlusOneToCheck; ++c)
			if (del._resourceInterfaceToUSI[c] != ~0u && (resourcesToQuery & (1 << uint64_t(del._resourceInterfaceToUSI[c]))))
				_queriedResources[del._resourceInterfaceToUSI[c]] = rvDst[c];

		_slotsQueried_ResourceViews |= toLoad;
	}

	void SequencerUniformsHelper::QuerySamplers(ParsingContext& parsingContext, uint64_t samplersToQuery, ShaderResourceDelegateBinding& del)
	{
		auto toLoad = samplersToQuery & del._usiSlotsFilled_Samplers;
		if (!toLoad) return;

		uint64_t toLoadDelegate = 0;
		for (unsigned c=0; c<del._samplerInterfaceToUSI.size(); ++c)
			if (del._samplerInterfaceToUSI[c] != ~0u && (samplersToQuery & (1 << uint64_t(del._samplerInterfaceToUSI[c]))))
				toLoadDelegate |= 1ull << uint64_t(c);

		assert(toLoadDelegate);
		auto minToCheck = xl_ctz8(toLoad);
		auto maxPlusOneToCheck = 64 - xl_clz8(toLoad);
		ISampler* samplerDst[maxPlusOneToCheck];

		del._delegate->WriteSamplers(parsingContext, nullptr, toLoadDelegate, MakeIteratorRange(samplerDst, &samplerDst[maxPlusOneToCheck]));
		
		for (unsigned c=minToCheck; c<maxPlusOneToCheck; ++c)
			if (del._samplerInterfaceToUSI[c] != ~0u && (samplersToQuery & (1 << uint64_t(del._samplerInterfaceToUSI[c]))))
				_queriedSamplers[del._samplerInterfaceToUSI[c]] = samplerDst[c];

		_slotsQueried_Samplers |= toLoad;
	}

	void SequencerUniformsHelper::QueryImmediateDatas(ParsingContext& parsingContext, uint64_t immediateDatasToQuery, ShaderResourceDelegateBinding& del)
	{
		auto toLoad = immediateDatasToQuery & del._usiSlotsFilled_ImmediateDatas;
		if (!toLoad) return;

		uint64_t toLoadDelegate = 0;
		for (unsigned c=0; c<del._immediateDataInterfaceToUSI.size(); ++c)
			if (del._immediateDataInterfaceToUSI[c] != ~0u && (toLoad & (1 << uint64_t(del._immediateDataInterfaceToUSI[c]))))
				toLoadDelegate |= 1ull << uint64_t(c);

		assert(toLoadDelegate);
		auto minToCheck = xl_ctz8(toLoadDelegate);
		auto maxPlusOneToCheck = 64 - xl_clz8(toLoadDelegate);

		for (unsigned c=minToCheck; c<maxPlusOneToCheck; ++c) {
			if (!(toLoadDelegate & (1ull << uint64_t(c)))) continue;
			auto beginAndEnd = del._immediateDataBeginAndEnd[c];
			auto dstRange = MakeIteratorRange(_tempDataBuffer.begin() + beginAndEnd.first, _tempDataBuffer.begin() + beginAndEnd.second); 
			del._delegate->WriteImmediateData(parsingContext, nullptr, c, dstRange);
			_queriedImmediateDatas[del._immediateDataInterfaceToUSI[c]] = dstRange;
		}

		_slotsQueried_ImmediateDatas |= toLoad;
	}

	void SequencerUniformsHelper::Prepare(IUniformBufferDelegate& del, uint64_t delBinding)
	{
		auto existing = std::find(_finalUSI._immediateDataBindings.begin(), _finalUSI._immediateDataBindings.end(), delBinding);
		if (existing != _finalUSI._immediateDataBindings.end())
			return;
			
		UniformBufferDelegateBinding newBinding;
		newBinding._delegate = &del;
		newBinding._usiSlotFilled = (unsigned)_finalUSI._immediateDataBindings.size();
		_finalUSI._immediateDataBindings.push_back(delBinding);
		newBinding._size = del.GetSize();
		newBinding._tempBufferOffset = _workingTempBufferSize;
		_workingTempBufferSize += CeilToMultiplePow2(newBinding._size, s_immediateDataAlignment);

		_uBindings.push_back(newBinding);
	}

	void SequencerUniformsHelper::QueryImmediateDatas(ParsingContext& parsingContext, uint64_t immediateDatasToQuery, UniformBufferDelegateBinding& del)
	{
		auto mask = 1ull << uint64_t(del._usiSlotFilled);
		if (!(immediateDatasToQuery & mask)) return;

		auto dstRange = MakeIteratorRange(_tempDataBuffer.begin() + del._tempBufferOffset, _tempDataBuffer.begin() + del._tempBufferOffset + del._size); 
		del._delegate->WriteImmediateData(
			parsingContext, nullptr,
			dstRange);
		
		_queriedImmediateDatas[del._usiSlotFilled] = dstRange;
		_slotsQueried_ImmediateDatas |= mask;
	}

	void SequencerUniformsHelper::QueryResources(ParsingContext& parsingContext, uint64_t resourcesToQuery)
	{
		resourcesToQuery &= ~_slotsQueried_ResourceViews;
		if (!resourcesToQuery) return;
		for (auto& del:_srBindings) QueryResources(parsingContext, resourcesToQuery, del);
	}

	void SequencerUniformsHelper::QuerySamplers(ParsingContext& parsingContext, uint64_t samplersToQuery)
	{
		samplersToQuery &= ~_slotsQueried_Samplers;
		if (!samplersToQuery) return;
		for (auto& del:_srBindings) QuerySamplers(parsingContext, samplersToQuery, del);
	}

	void SequencerUniformsHelper::QueryImmediateDatas(ParsingContext& parsingContext, uint64_t immediateDatasToQuery)
	{
		immediateDatasToQuery &= ~_slotsQueried_ImmediateDatas;
		if (!immediateDatasToQuery) return;
		for (auto& del:_srBindings) QueryImmediateDatas(parsingContext, immediateDatasToQuery, del);
		for (auto& del:_uBindings) QueryImmediateDatas(parsingContext, immediateDatasToQuery, del);
	}

	SequencerUniformsHelper::SequencerUniformsHelper(ParsingContext& parsingContext, const SequencerContext& sequencerTechnique)
	{
		_finalUSI._resourceViewBindings.reserve(64);
		_finalUSI._immediateDataBindings.reserve(64);
		_finalUSI._samplerBindings.reserve(64);

		// Delegates we visit first will be preferred over subsequent delegates (if they bind the same thing)
		// So, we should go through in reverse order
		for (signed c=sequencerTechnique._sequencerResources.size()-1; c>=0; c--)
			Prepare(*sequencerTechnique._sequencerResources[c], parsingContext);

		for (signed c=sequencerTechnique._sequencerUniforms.size()-1; c>=0; c--)
			Prepare(*sequencerTechnique._sequencerUniforms[c].second, sequencerTechnique._sequencerUniforms[c].first);

		auto parserDelegates = parsingContext.GetShaderResourceDelegates();
		for (signed c=parserDelegates.size()-1; c>=0; c--)
			Prepare(*parserDelegates[c], parsingContext);

		auto parserUDelegates = parsingContext.GetUniformDelegates();
		for (signed c=parserUDelegates.size()-1; c>=0; c--)
			Prepare(*parserUDelegates[c].second, parserUDelegates[c].first);

		_queriedResources.resize(_finalUSI._resourceViewBindings.size(), nullptr);
		_queriedSamplers.resize(_finalUSI._samplerBindings.size(), nullptr);
		_queriedImmediateDatas.resize(_finalUSI._immediateDataBindings.size(), {});
		_tempDataBuffer.resize(_workingTempBufferSize, 0);
	}
	
	std::pair<std::shared_ptr<IDescriptorSet>, DescriptorSetSignature> CreateSequencerDescriptorSet(
		IDevice& device,
		ParsingContext& parsingContext,
		SequencerUniformsHelper& uniformHelper,
		const RenderCore::Assets::PredefinedDescriptorSetLayout& descSetLayout)
	{
		// Create a temporary descriptor set, with per-sequencer bindings
		// We need to look for something providing data for this:
		// * parsingContext uniform buffer delegate
		// * sequencer technique uniform buffer delegate
		// * sequencer technique shader resource delegate
		// Unfortunately we have to do a make a lot of small temporary allocations in order to
		// calculate how the various delegates map onto the descriptor set layout. It might be
		// worth considering caching this result, because there should actually only be a finite
		// number of different configurations in most use cases
		
		std::vector<DescriptorSetInitializer::BindTypeAndIdx> bindTypesAndIdx;
		bindTypesAndIdx.reserve(descSetLayout._slots.size());
		uint64_t resourcesWeNeed = 0ull;
		uint64_t samplersWeNeed = 0ull;
        uint64_t immediateDatasWeNeed = 0ull;

		for (unsigned slotIdx=0; slotIdx<descSetLayout._slots.size(); ++slotIdx) {
			auto hashName = Hash64(descSetLayout._slots[slotIdx]._name);

			if (descSetLayout._slots[slotIdx]._type == DescriptorType::Sampler) {
				auto samplerBinding = std::find(uniformHelper._finalUSI._samplerBindings.begin(), uniformHelper._finalUSI._samplerBindings.end(), hashName);
				if (samplerBinding != uniformHelper._finalUSI._samplerBindings.end()) {
					auto samplerIdx = (unsigned)std::distance(uniformHelper._finalUSI._samplerBindings.begin(), samplerBinding);
					bindTypesAndIdx.push_back({DescriptorSetInitializer::BindType::Sampler, samplerIdx});
					samplersWeNeed |= 1ull << uint64_t(samplerIdx);
					continue;
				}
				#if defined(_DEBUG)		// just check to make sure we're not attempting to bind some incorrect type here 
					auto resourceBinding = std::find(uniformHelper._finalUSI._resourceViewBindings.begin(), uniformHelper._finalUSI._resourceViewBindings.end(), hashName);
					if (resourceBinding != uniformHelper._finalUSI._resourceViewBindings.end())
						Log(Warning) << "Resource view provided for descriptor set slot (" << descSetLayout._slots[slotIdx]._name << "), however, this lot is 'sampler' type in the descriptor set layout." << std::endl;
					auto immediateDataBinding = std::find(uniformHelper._finalUSI._immediateDataBindings.begin(), uniformHelper._finalUSI._immediateDataBindings.end(), hashName);
					if (immediateDataBinding != uniformHelper._finalUSI._immediateDataBindings.end())
						Log(Warning) << "Immediate data provided for descriptor set slot (" << descSetLayout._slots[slotIdx]._name << "), however, this lot is 'sampler' type in the descriptor set layout." << std::endl;
				#endif
			} else {
				auto resourceBinding = std::find(uniformHelper._finalUSI._resourceViewBindings.begin(), uniformHelper._finalUSI._resourceViewBindings.end(), hashName);
				if (resourceBinding != uniformHelper._finalUSI._resourceViewBindings.end()) {
					auto resourceIdx = (unsigned)std::distance(uniformHelper._finalUSI._resourceViewBindings.begin(), resourceBinding);
					bindTypesAndIdx.push_back({DescriptorSetInitializer::BindType::ResourceView, resourceIdx});
					resourcesWeNeed |= 1ull << uint64_t(resourceIdx);
					continue;
				}

                auto immediateDataBinding = std::find(uniformHelper._finalUSI._immediateDataBindings.begin(), uniformHelper._finalUSI._immediateDataBindings.end(), hashName);
                if (immediateDataBinding != uniformHelper._finalUSI._immediateDataBindings.end()) {
					auto resourceIdx = (unsigned)std::distance(uniformHelper._finalUSI._immediateDataBindings.begin(), immediateDataBinding);
					bindTypesAndIdx.push_back({DescriptorSetInitializer::BindType::ImmediateData, resourceIdx});
					immediateDatasWeNeed |= 1ull << uint64_t(resourceIdx);
					continue;
				}

				#if defined(_DEBUG)		// just check to make sure we're not attempting to bind some incorrect type here 
					auto samplerBinding = std::find(uniformHelper._finalUSI._samplerBindings.begin(), uniformHelper._finalUSI._samplerBindings.end(), hashName);
					if (samplerBinding != uniformHelper._finalUSI._samplerBindings.end())
						Log(Warning) << "Sampler provided for descriptor set slot (" << descSetLayout._slots[slotIdx]._name << "), however, this lot is not a sampler type in the descriptor set layout." << std::endl;
				#endif
			}

			bindTypesAndIdx.push_back({});		// didn't find any binding
		}

		// Now that we know what we need, we should query the delegates to get the associated data
		uniformHelper.QueryResources(parsingContext, resourcesWeNeed);
		uniformHelper.QuerySamplers(parsingContext, samplersWeNeed);
        uniformHelper.QueryImmediateDatas(parsingContext, immediateDatasWeNeed);

		DescriptorSetInitializer initializer;
		initializer._slotBindings = MakeIteratorRange(bindTypesAndIdx);
		initializer._bindItems._resourceViews = MakeIteratorRange(uniformHelper._queriedResources);
		initializer._bindItems._samplers = MakeIteratorRange(uniformHelper._queriedSamplers);
		initializer._bindItems._immediateData = MakeIteratorRange(uniformHelper._queriedImmediateDatas);
		auto sig = descSetLayout.MakeDescriptorSetSignature();		// todo -- we probably have this stored somewhere else, it might not be a great idea to keep rebuilding it
		initializer._signature = &sig;
		auto set = device.CreateDescriptorSet(initializer);
		return std::make_pair(std::move(set), std::move(sig));
	}
}}

