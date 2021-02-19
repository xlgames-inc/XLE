// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "InputLayout.h"
#include "PipelineLayout.h"
#include "DescriptorSetSignatureFile.h"
#include "TextureView.h"
#include "ObjectFactory.h"
#include "Pools.h"
#include "DescriptorSet.h"
#include "DeviceContext.h"
#include "../../../OSServices/Log.h"
#include "../../../Utility/MemoryUtils.h"
#include "../../../Utility/ArithmeticUtils.h"

#include "IncludeVulkan.h"

namespace RenderCore { namespace Metal_Vulkan
{

	class NumericUniformsInterface::Pimpl
    {
    public:
		static const unsigned s_maxBindings = 64u;

        DescriptorPool*     _descriptorPool = nullptr;
		GlobalPools*		_globalPools = nullptr;

		struct Binding
		{
			unsigned _descSetIndex = ~0u;
			unsigned _slotIndex = ~0u;
		};

        Binding		_constantBufferRegisters[s_maxBindings];
        Binding		_samplerRegisters[s_maxBindings];

		Binding		_srvRegisters[s_maxBindings];
		Binding		_uavRegisters[s_maxBindings];

		Binding		_srvRegisters_boundToBuffer[s_maxBindings];
		Binding		_uavRegisters_boundToBuffer[s_maxBindings];

		class DescSet
		{
		public:
			ProgressiveDescriptorSetBuilder		_builder;
			VulkanUniquePtr<VkDescriptorSet>    _activeDescSet;
			uint64_t							_slotsFilled = 0;
			std::shared_ptr<CompiledDescriptorSetLayout> _layout;

			#if defined(VULKAN_VERBOSE_DEBUG)
				DescriptorSetDebugInfo _description;
			#endif

			std::shared_ptr<DescriptorSetSignature> _signature;

			DescSet(const std::shared_ptr<CompiledDescriptorSetLayout>& layout)
			: _builder(layout->GetDescriptorSlots()), _layout(layout)
			{
				#if defined(VULKAN_VERBOSE_DEBUG)
					_description._descriptorSetInfo = "NumericUniformsInterface";
				#endif
			}

			void Reset(GlobalPools& globalPools)
			{
				_builder.Reset();
				_activeDescSet.reset();
				_slotsFilled = 0;
				#if defined(VULKAN_VERBOSE_DEBUG)
					_description = DescriptorSetDebugInfo{};
					_description._descriptorSetInfo = "NumericUniformsInterface";
				#endif

				// bind dummies in every slot
				_builder.BindDummyDescriptors(globalPools, (1ull<<uint64_t(_signature->_slots.size()))-1ull);
			}
		};
		std::vector<DescSet> _descSet;
		bool _hasChanges = false;

		LegacyRegisterBindingDesc _legacyRegisterBindings;

		Pimpl(const CompiledPipelineLayout& layout)
		{
			_descSet.reserve(layout.GetDescriptorSetCount());
			for (unsigned c=0; c<layout.GetDescriptorSetCount(); ++c)
				_descSet.emplace_back(layout.GetDescriptorSetLayout(c));
		}
    };

    void    NumericUniformsInterface::Bind(unsigned startingPoint, IteratorRange<const TextureView*const*> resources)
    {
		assert(_pimpl);
        for (unsigned c=0; c<unsigned(resources.size()); ++c) {
            assert((startingPoint + c) < Pimpl::s_maxBindings);
			if  (!resources[c]) continue;

			if (!resources[c]->GetImageView()) {
				if (!resources[c]->GetResource()) continue;

				const auto& binding = _pimpl->_srvRegisters_boundToBuffer[startingPoint + c];
				if (binding._slotIndex == ~0u) {
					Log(Debug) << "Texture view numeric binding (" << (startingPoint + c) << ") is off root signature" << std::endl;
					continue;
				}
				_pimpl->_descSet[binding._descSetIndex]._builder.Bind(binding._slotIndex, *resources[c]);
				_pimpl->_hasChanges |= _pimpl->_descSet[binding._descSetIndex]._builder.HasChanges();
			} else {
				const auto& binding = _pimpl->_srvRegisters[startingPoint + c];
				if (binding._slotIndex == ~0u) {
					Log(Debug) << "Texture view numeric binding (" << (startingPoint + c) << ") is off root signature" << std::endl;
					continue;
				}
				_pimpl->_descSet[binding._descSetIndex]._builder.Bind(binding._slotIndex, *resources[c]);
				_pimpl->_hasChanges |= _pimpl->_descSet[binding._descSetIndex]._builder.HasChanges();
			}
        }
    }

    void    NumericUniformsInterface::Bind(unsigned startingPoint, IteratorRange<const VkBuffer*> uniformBuffers)
    {
		assert(_pimpl);
        for (unsigned c=0; c<unsigned(uniformBuffers.size()); ++c) {
            if (!uniformBuffers[c]) continue;

			const auto& binding = _pimpl->_constantBufferRegisters[startingPoint + c];
			if (binding._slotIndex == ~0u) {
				Log(Debug) << "Uniform buffer numeric binding (" << (startingPoint + c) << ") is off root signature" << std::endl;
				continue;
			}

			_pimpl->_descSet[binding._descSetIndex]._builder.Bind(binding._slotIndex, { uniformBuffers[c], 0, VK_WHOLE_SIZE});
			_pimpl->_hasChanges |= _pimpl->_descSet[binding._descSetIndex]._builder.HasChanges();
        }
    }

    void    NumericUniformsInterface::Bind(unsigned startingPoint, IteratorRange<const VkSampler*> samplers)
    {
		assert(_pimpl);
        for (unsigned c=0; c<unsigned(samplers.size()); ++c) {
            if (!samplers[c]) continue;

			const auto& binding = _pimpl->_samplerRegisters[startingPoint + c];
			if (binding._slotIndex == ~0u) {
				Log(Debug) << "Sampler numeric binding (" << (startingPoint + c) << ") is off root signature" << std::endl;
				continue;
			}

			_pimpl->_descSet[binding._descSetIndex]._builder.Bind(binding._slotIndex, samplers[c]);
			_pimpl->_hasChanges |= _pimpl->_descSet[binding._descSetIndex]._builder.HasChanges();
        }
    }

	void NumericUniformsInterface::Apply(
		DeviceContext& context,
		SharedGraphicsEncoder& encoder) const
	{
		assert(_pimpl);
        // If we've had any changes this last time, we must create new
        // descriptor sets. We will use vkUpdateDescriptorSets to fill in these
        // sets with the latest changes. Note that this will require copy across the
        // bindings that haven't changed.
        // It turns out that copying using VkCopyDescriptorSet is probably going to be
        // slow. We should try a different approach.
		for(unsigned dIdx=0; dIdx<_pimpl->_descSet.size(); ++dIdx) {
			auto&d = _pimpl->_descSet[dIdx];
			if (d._builder.HasChanges()) {
				VulkanUniquePtr<VkDescriptorSet> newSets[1];
				VkDescriptorSetLayout layouts[1] = { d._layout->GetUnderlying() };
				_pimpl->_descriptorPool->Allocate(MakeIteratorRange(newSets), MakeIteratorRange(layouts));

				auto written = d._builder.FlushChanges(
					_pimpl->_descriptorPool->GetDevice(),
					newSets[0].get(),
					d._activeDescSet.get(),
					d._slotsFilled
					VULKAN_VERBOSE_DEBUG_ONLY(, d._description));

				d._slotsFilled |= written;
				d._activeDescSet = std::move(newSets[0]);

				encoder.BindDescriptorSet(
					dIdx, newSets[0].get()
					VULKAN_VERBOSE_DEBUG_ONLY(, DescriptorSetDebugInfo{d._description}));
			}
		}
    }

	void    NumericUniformsInterface::Reset()
	{
		if (_pimpl) {
			for (auto&d:_pimpl->_descSet)
				d.Reset(*_pimpl->_globalPools);
			_pimpl->_hasChanges = false;
		}
	}

	bool	NumericUniformsInterface::HasChanges() const
	{
		return _pimpl->_hasChanges;
	}

	static unsigned LookupDescriptorSet(const CompiledPipelineLayout& pipelineLayout, uint64_t bindingName)
	{
		auto bindingNames = pipelineLayout.GetDescriptorSetBindingNames();
		for (unsigned c=0; c<bindingNames.size(); ++c)
			if (bindingNames[c] == bindingName)
				return c;
		return ~0u;
	}

    NumericUniformsInterface::NumericUniformsInterface(
        const ObjectFactory& factory,
		const CompiledPipelineLayout& pipelineLayout,
        const LegacyRegisterBindingDesc& bindings)
    {
        _pimpl = std::make_unique<Pimpl>(pipelineLayout);
        _pimpl->_globalPools = Internal::VulkanGlobalsTemp::GetInstance()._globalPools;
		_pimpl->_descriptorPool = &_pimpl->_globalPools->_mainDescriptorPool;
		_pimpl->_legacyRegisterBindings = bindings;		// we store this only so we can return it from the GetLegacyRegisterBindings() query
		_pimpl->_hasChanges = false;
        
        Reset();

		for (const auto&e:bindings.GetEntries(LegacyRegisterBindingDesc::RegisterType::Sampler)) {
			assert(e._end <= Pimpl::s_maxBindings);
			for (unsigned b=e._begin; b!=e._end; ++b) {
				auto descSet = LookupDescriptorSet(pipelineLayout, e._targetDescriptorSetBindingName);
				if (descSet != ~0u) {
					assert(descSet == e._targetDescriptorSetIdx);
					_pimpl->_samplerRegisters[b]._descSetIndex = descSet;
					_pimpl->_samplerRegisters[b]._slotIndex = b-e._begin+e._targetBegin;
				}
			}
		}

		for (const auto&e:bindings.GetEntries(LegacyRegisterBindingDesc::RegisterType::ConstantBuffer)) {
			assert(e._end <= Pimpl::s_maxBindings);
			for (unsigned b=e._begin; b!=e._end; ++b) {
				auto descSet = LookupDescriptorSet(pipelineLayout, e._targetDescriptorSetBindingName);
				if (descSet != ~0u) {
					assert(descSet == e._targetDescriptorSetIdx);
					_pimpl->_constantBufferRegisters[b]._descSetIndex = descSet;
					_pimpl->_constantBufferRegisters[b]._slotIndex = b-e._begin+e._targetBegin;
				}
			}
		}

		for (const auto&e:bindings.GetEntries(LegacyRegisterBindingDesc::RegisterType::ShaderResource)) {
			assert(e._end <= Pimpl::s_maxBindings);
			for (unsigned b=e._begin; b!=e._end; ++b) {
				auto descSet = LookupDescriptorSet(pipelineLayout, e._targetDescriptorSetBindingName);
				if (descSet != ~0u) {
					assert(descSet == e._targetDescriptorSetIdx);
					_pimpl->_srvRegisters[b]._descSetIndex = descSet;
					_pimpl->_srvRegisters[b]._slotIndex = b-e._begin+e._targetBegin;
				}
			}
		}

		for (const auto&e:bindings.GetEntries(LegacyRegisterBindingDesc::RegisterType::UnorderedAccess)) {
			assert(e._end <= Pimpl::s_maxBindings);
			for (unsigned b=e._begin; b!=e._end; ++b) {
				auto descSet = LookupDescriptorSet(pipelineLayout, e._targetDescriptorSetBindingName);
				if (descSet != ~0u) {
					assert(descSet == e._targetDescriptorSetIdx);
					_pimpl->_uavRegisters[b]._descSetIndex = descSet;
					_pimpl->_uavRegisters[b]._slotIndex = b-e._begin+e._targetBegin;
				}
			}
		}

		for (const auto&e:bindings.GetEntries(LegacyRegisterBindingDesc::RegisterType::ShaderResource, LegacyRegisterBindingDesc::RegisterQualifier::Buffer)) {
			assert(e._end <= Pimpl::s_maxBindings);
			for (unsigned b=e._begin; b!=e._end; ++b) {
				auto descSet = LookupDescriptorSet(pipelineLayout, e._targetDescriptorSetBindingName);
				if (descSet != ~0u) {
					assert(descSet == e._targetDescriptorSetIdx);
					_pimpl->_srvRegisters_boundToBuffer[b]._descSetIndex = descSet;
					_pimpl->_srvRegisters_boundToBuffer[b]._slotIndex = b-e._begin+e._targetBegin;
				}
			}
		}

		for (const auto&e:bindings.GetEntries(LegacyRegisterBindingDesc::RegisterType::UnorderedAccess, LegacyRegisterBindingDesc::RegisterQualifier::Buffer)) {
			assert(e._end <= Pimpl::s_maxBindings);
			for (unsigned b=e._begin; b!=e._end; ++b) {
				auto descSet = LookupDescriptorSet(pipelineLayout, e._targetDescriptorSetBindingName);
				if (descSet != ~0u) {
					assert(descSet == e._targetDescriptorSetIdx);
					_pimpl->_uavRegisters_boundToBuffer[b]._descSetIndex = descSet;
					_pimpl->_uavRegisters_boundToBuffer[b]._slotIndex = b-e._begin+e._targetBegin;
				}
			}
		}
    }

	NumericUniformsInterface::NumericUniformsInterface() 
	{
	}

    NumericUniformsInterface::~NumericUniformsInterface()
    {
    }

	NumericUniformsInterface::NumericUniformsInterface(NumericUniformsInterface&& moveFrom)
	: _pimpl(std::move(moveFrom._pimpl))
	{
	}

	NumericUniformsInterface& NumericUniformsInterface::operator=(NumericUniformsInterface&& moveFrom)
	{
		_pimpl = std::move(moveFrom._pimpl);
		return *this;
	}

}}

