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

		struct Binding
		{
			unsigned _descriptorSetBindIndex = ~0u;
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
			DescriptorSetBuilder				_builder;
			VulkanUniquePtr<VkDescriptorSet>    _activeDescSet;
			uint64_t							_slotsFilled = 0;
			VulkanUniquePtr<VkDescriptorSetLayout>	_layout;

			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				DescriptorSetVerboseDescription _description;
			#endif

			std::shared_ptr<DescriptorSetSignature> _signature;

			DescSet(VulkanUniquePtr<VkDescriptorSetLayout>&& layout, GlobalPools& globalPools) : _builder(globalPools), _layout(std::move(layout))
			{
				#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
					_description._descriptorSetInfo = "NumericUniformsInterface";
				#endif
			}

			void Reset()
			{
				_builder.Reset();
				_activeDescSet.reset();
				_slotsFilled = 0;
				#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
					_description = DescriptorSetVerboseDescription{};
					_description._descriptorSetInfo = "NumericUniformsInterface";
				#endif

				// bind dummies in every slot
				_builder.BindDummyDescriptors(*_signature, (1ull<<uint64_t(_signature->_bindings.size()))-1ull);
			}
		};
		DescSet _descSet;

		LegacyRegisterBinding _legacyRegisterBindings;

		Pimpl(VulkanUniquePtr<VkDescriptorSetLayout>&& layout, GlobalPools& globalPools) : _descSet(std::move(layout), globalPools) {}
    };

    void    NumericUniformsInterface::BindSRV(unsigned startingPoint, IteratorRange<const TextureView*const*> resources)
    {
		assert(_pimpl);
        for (unsigned c=0; c<unsigned(resources.size()); ++c) {
            assert((startingPoint + c) < Pimpl::s_maxBindings);
			if  (!resources[c]) continue;

			if (!resources[c]->GetImageView()) {
				if (!resources[c]->GetResource()) continue;

				const auto& binding = _pimpl->_srvRegisters_boundToBuffer[startingPoint + c];
				if (binding._descriptorSetBindIndex == ~0u) {
					Log(Debug) << "SRV numeric binding (" << (startingPoint + c) << ") is off root signature" << std::endl;
					continue;
				}
				_pimpl->_descSet._builder.BindSRV(binding._descriptorSetBindIndex, resources[c]);
			} else {
				const auto& binding = _pimpl->_srvRegisters[startingPoint + c];
				if (binding._descriptorSetBindIndex == ~0u) {
					Log(Debug) << "SRV numeric binding (" << (startingPoint + c) << ") is off root signature" << std::endl;
					continue;
				}
				_pimpl->_descSet._builder.BindSRV(binding._descriptorSetBindIndex, resources[c]);
			}
        }
    }

    void    NumericUniformsInterface::BindUAV(unsigned startingPoint, IteratorRange<const TextureView*const*> resources)
    {
		assert(_pimpl);
        for (unsigned c=0; c<unsigned(resources.size()); ++c) {
			assert((startingPoint + c) < Pimpl::s_maxBindings);
			if  (!resources[c]) continue;
            
			if (!resources[c]->GetImageView()) {
				if (!resources[c]->GetResource()) continue;

				const auto& binding = _pimpl->_uavRegisters_boundToBuffer[startingPoint + c];
				if (binding._descriptorSetBindIndex == ~0u) {
					Log(Debug) << "UAV numeric binding (" << (startingPoint + c) << ") is off root signature" << std::endl;
					continue;
				}

				_pimpl->_descSet._builder.BindUAV(binding._descriptorSetBindIndex, resources[c]);
			} else {
				const auto& binding = _pimpl->_uavRegisters[startingPoint + c];
				if (binding._descriptorSetBindIndex == ~0u) {
					Log(Debug) << "UAV numeric binding (" << (startingPoint + c) << ") is off root signature" << std::endl;
					continue;
				}

				_pimpl->_descSet._builder.BindUAV(binding._descriptorSetBindIndex, resources[c]);
			}
        }
    }

    void    NumericUniformsInterface::BindCB(unsigned startingPoint, IteratorRange<const VkBuffer*> uniformBuffers)
    {
		assert(_pimpl);
        for (unsigned c=0; c<unsigned(uniformBuffers.size()); ++c) {
            if (!uniformBuffers[c]) continue;

			const auto& binding = _pimpl->_constantBufferRegisters[startingPoint + c];
			if (binding._descriptorSetBindIndex == ~0u) {
				Log(Debug) << "Uniform buffer numeric binding (" << (startingPoint + c) << ") is off root signature" << std::endl;
				continue;
			}

			_pimpl->_descSet._builder.BindCB(binding._descriptorSetBindIndex, { uniformBuffers[c], 0, VK_WHOLE_SIZE});
        }
    }

    void    NumericUniformsInterface::BindSampler(unsigned startingPoint, IteratorRange<const VkSampler*> samplers)
    {
		assert(_pimpl);
        for (unsigned c=0; c<unsigned(samplers.size()); ++c) {
            if (!samplers[c]) continue;

			const auto& binding = _pimpl->_samplerRegisters[startingPoint + c];
			if (binding._descriptorSetBindIndex == ~0u) {
				Log(Debug) << "Sampler numeric binding (" << (startingPoint + c) << ") is off root signature" << std::endl;
				continue;
			}

			_pimpl->_descSet._builder.BindSampler(binding._descriptorSetBindIndex, samplers[c]);
        }
    }

    void    NumericUniformsInterface::GetDescriptorSets(
		IteratorRange<VkDescriptorSet*> dst
		VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, IteratorRange<DescriptorSetVerboseDescription**> descriptions))
    {
		assert(_pimpl);
        // If we've had any changes this last time, we must create new
        // descriptor sets. We will use vkUpdateDescriptorSets to fill in these
        // sets with the latest changes. Note that this will require copy across the
        // bindings that haven't changed.
        // It turns out that copying using VkCopyDescriptorSet is probably going to be
        // slow. We should try a different approach.
        if (_pimpl->_descSet._builder.HasChanges()) {
            VulkanUniquePtr<VkDescriptorSet> newSets[1];
			VkDescriptorSetLayout layouts[1] = { _pimpl->_descSet._layout.get() };
            _pimpl->_descriptorPool->Allocate(MakeIteratorRange(newSets), MakeIteratorRange(layouts));

			auto written = _pimpl->_descSet._builder.FlushChanges(
				_pimpl->_descriptorPool->GetDevice(),
				newSets[0].get(),
				_pimpl->_descSet._activeDescSet.get(),
				_pimpl->_descSet._slotsFilled
				VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, _pimpl->_descSet._description));

            _pimpl->_descSet._slotsFilled |= written;
            _pimpl->_descSet._activeDescSet = std::move(newSets[0]);
        }

        for (unsigned c=0; c<unsigned(dst.size()); ++c) {
            dst[c] = (c < 1) ? _pimpl->_descSet._activeDescSet.get() : nullptr;
			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				descriptions[c] = &_pimpl->_descSet._description;
			#endif
		}
    }

	void    NumericUniformsInterface::Reset()
	{
		if (_pimpl)
			_pimpl->_descSet.Reset();
	}

	bool	NumericUniformsInterface::HasChanges() const
	{
		if (!_pimpl) return false;
		return _pimpl->_descSet._builder.HasChanges();
	}

	const DescriptorSetSignature& NumericUniformsInterface::GetSignature() const
	{
		assert(_pimpl);
		return *_pimpl->_descSet._signature;
	}

	const LegacyRegisterBinding& NumericUniformsInterface::GetLegacyRegisterBindings() const
	{
		assert(_pimpl);
		return _pimpl->_legacyRegisterBindings;
	}

    NumericUniformsInterface::NumericUniformsInterface(
        const ObjectFactory& factory,
        GlobalPools& globalPools, 
		const std::shared_ptr<DescriptorSetSignature>& signature,
        const LegacyRegisterBinding& bindings,
		VkShaderStageFlags stageFlags,
		unsigned descriptorSetIndex)
    {
		auto layout = CreateDescriptorSetLayout(factory, *signature, stageFlags);
        _pimpl = std::make_unique<Pimpl>(std::move(layout), globalPools);
        _pimpl->_descriptorPool = &globalPools._mainDescriptorPool;
		_pimpl->_descSet._signature = signature;
		_pimpl->_legacyRegisterBindings = bindings;		// we store this only so we can return it from the GetLegacyRegisterBindings() query
        
        Reset();

		for (const auto&e:bindings._samplerRegisters) {
			if (e._targetDescriptorSet != descriptorSetIndex) continue;
			assert(e._end <= Pimpl::s_maxBindings);
			for (unsigned b=e._begin; b!=e._end; ++b)
				_pimpl->_samplerRegisters[b]._descriptorSetBindIndex = b-e._begin+e._targetBegin;
		}

		for (const auto&e:bindings._constantBufferRegisters) {
			if (e._targetDescriptorSet != descriptorSetIndex) continue;
			assert(e._end <= Pimpl::s_maxBindings);
			for (unsigned b=e._begin; b!=e._end; ++b)
				_pimpl->_constantBufferRegisters[b]._descriptorSetBindIndex = b-e._begin+e._targetBegin;
		}

		for (const auto&e:bindings._srvRegisters) {
			if (e._targetDescriptorSet != descriptorSetIndex) continue;
			assert(e._end <= Pimpl::s_maxBindings);
			for (unsigned b=e._begin; b!=e._end; ++b)
				_pimpl->_srvRegisters[b]._descriptorSetBindIndex = b-e._begin+e._targetBegin;
		}

		for (const auto&e:bindings._uavRegisters) {
			if (e._targetDescriptorSet != descriptorSetIndex) continue;
			assert(e._end <= Pimpl::s_maxBindings);
			for (unsigned b=e._begin; b!=e._end; ++b)
				_pimpl->_uavRegisters[b]._descriptorSetBindIndex = b-e._begin+e._targetBegin;
		}

		for (const auto&e:bindings._srvRegisters_boundToBuffer) {
			if (e._targetDescriptorSet != descriptorSetIndex) continue;
			assert(e._end <= Pimpl::s_maxBindings);
			for (unsigned b=e._begin; b!=e._end; ++b)
				_pimpl->_srvRegisters_boundToBuffer[b]._descriptorSetBindIndex = b-e._begin+e._targetBegin;
		}

		for (const auto&e:bindings._uavRegisters_boundToBuffer) {
			if (e._targetDescriptorSet != descriptorSetIndex) continue;
			assert(e._end <= Pimpl::s_maxBindings);
			for (unsigned b=e._begin; b!=e._end; ++b)
				_pimpl->_uavRegisters_boundToBuffer[b]._descriptorSetBindIndex = b-e._begin+e._targetBegin;
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

