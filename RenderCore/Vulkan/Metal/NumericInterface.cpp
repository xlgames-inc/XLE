// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "InputLayout.h"
#include "PipelineLayout.h"
#include "TextureView.h"
#include "ObjectFactory.h"
#include "Pools.h"
#include "DescriptorSet.h"
#include "../../../ConsoleRig/Log.h"
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
			DescriptorType _type = DescriptorType::Unknown;
			unsigned _descriptorSetBindIndex = ~0u;
		};

        Binding		_srvMapping[s_maxBindings];
        Binding		_uavMapping[s_maxBindings];
        Binding		_cbMapping[s_maxBindings];
        Binding		_samplerMapping[s_maxBindings];

		class DescSet
		{
		public:
			DescriptorSetBuilder				_builder;
			VulkanUniquePtr<VkDescriptorSet>    _activeDescSet;
			uint64_t							_slotsFilled = 0;
			VkDescriptorSetLayout				_layout;

			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				DescriptorSetVerboseDescription _description;
			#endif

			DescSet(VkDescriptorSetLayout layout, GlobalPools& globalPools) : _builder(globalPools), _layout(layout)
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
			}
		};
		DescSet _descSet;

		Pimpl(VkDescriptorSetLayout layout, GlobalPools& globalPools) : _descSet(layout, globalPools) {}
    };

    void    NumericUniformsInterface::BindSRV(unsigned startingPoint, IteratorRange<const TextureView*const*> resources)
    {
        for (unsigned c=0; c<unsigned(resources.size()); ++c) {
            if (!resources[c] || (!resources[c]->GetImageView() && !resources[c]->GetResource())) continue;
            assert((startingPoint + c) < Pimpl::s_maxBindings);
			const auto& binding = _pimpl->_srvMapping[startingPoint + c];
			if (binding._descriptorSetBindIndex == ~0u) {
				Log(Warning) << "SRV numeric binding (" << (startingPoint + c) << ") is off root signature" << std::endl;
				continue;
			}

            if (resources[c]->GetImageView()) {
				assert(binding._type == DescriptorType::Texture);
            } else {
                assert(binding._type == DescriptorType::UnorderedAccessBuffer);
            }

			_pimpl->_descSet._builder.BindSRV(binding._descriptorSetBindIndex, resources[c]);
        }
    }

    void    NumericUniformsInterface::BindUAV(unsigned startingPoint, IteratorRange<const TextureView*const*> resources)
    {
        for (unsigned c=0; c<unsigned(resources.size()); ++c) {
            if (!resources[c] || (!resources[c]->GetImageView() && !resources[c]->GetResource())) continue;
            const auto& binding = _pimpl->_uavMapping[startingPoint + c];
			if (binding._descriptorSetBindIndex == ~0u) {
				Log(Warning) << "UAV numeric binding (" << (startingPoint + c) << ") is off root signature" << std::endl;
				continue;
			}

            if (resources[c]->GetImageView()) {
				assert(binding._type == DescriptorType::UnorderedAccessTexture);
            } else {
				assert(binding._type == DescriptorType::UnorderedAccessBuffer);
            }

			_pimpl->_descSet._builder.BindUAV(binding._descriptorSetBindIndex, resources[c]);
        }
    }

    void    NumericUniformsInterface::BindCB(unsigned startingPoint, IteratorRange<const VkBuffer*> uniformBuffers)
    {
        for (unsigned c=0; c<unsigned(uniformBuffers.size()); ++c) {
            if (!uniformBuffers[c]) continue;
			const auto& binding = _pimpl->_cbMapping[startingPoint + c];
			if (binding._descriptorSetBindIndex == ~0u) {
				Log(Warning) << "Uniform buffer numeric binding (" << (startingPoint + c) << ") is off root signature" << std::endl;
				continue;
			}

			assert(binding._type == DescriptorType::ConstantBuffer);
			_pimpl->_descSet._builder.BindCB(binding._descriptorSetBindIndex, { uniformBuffers[c], 0, VK_WHOLE_SIZE});
        }
    }

    void    NumericUniformsInterface::BindSampler(unsigned startingPoint, IteratorRange<const VkSampler*> samplers)
    {
        for (unsigned c=0; c<unsigned(samplers.size()); ++c) {
            if (!samplers[c]) continue;
			const auto& binding = _pimpl->_samplerMapping[startingPoint + c];
			if (binding._descriptorSetBindIndex == ~0u) {
				Log(Warning) << "Sampler numeric binding (" << (startingPoint + c) << ") is off root signature" << std::endl;
				continue;
			}

            assert(binding._type == DescriptorType::Sampler);
			_pimpl->_descSet._builder.BindSampler(binding._descriptorSetBindIndex, samplers[c]);
        }
    }

    void    NumericUniformsInterface::GetDescriptorSets(
		IteratorRange<VkDescriptorSet*> dst
		VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, IteratorRange<DescriptorSetVerboseDescription**> descriptions))
    {
        // If we've had any changes this last time, we must create new
        // descriptor sets. We will use vkUpdateDescriptorSets to fill in these
        // sets with the latest changes. Note that this will require copy across the
        // bindings that haven't changed.
        // It turns out that copying using VkCopyDescriptorSet is probably going to be
        // slow. We should try a different approach.
        if (_pimpl->_descSet._builder.HasChanges()) {
            VulkanUniquePtr<VkDescriptorSet> newSets[1];
			VkDescriptorSetLayout layouts[1] = { _pimpl->_descSet._layout };
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
		_pimpl->_descSet.Reset();
	}

	bool	NumericUniformsInterface::HasChanges() const
	{
		return _pimpl->_descSet._builder.HasChanges();
	}

    NumericUniformsInterface::NumericUniformsInterface(
        const ObjectFactory& factory,
        GlobalPools& globalPools, 
        VkDescriptorSetLayout layout,
        const DescriptorSetSignature& signature)
    {
        _pimpl = std::make_unique<Pimpl>(layout, globalPools);
        _pimpl->_descriptorPool = &globalPools._mainDescriptorPool;
        
        Reset();

        // We need to make a mapping between the HLSL binding numbers and types
        // and the binding index in our descriptor set
		unsigned srvBindingCount = 0u;
		unsigned uavBindingCount = 0u;
		unsigned cbBindingCount = 0u;
		unsigned samplerBindingCount = 0u;

        for (unsigned bIndex=0; bIndex<(unsigned)signature._bindings.size(); ++bIndex) {
            const auto& b = signature._bindings[bIndex];
			switch (b) {
			case DescriptorType::Texture:
				assert(srvBindingCount < Pimpl::s_maxBindings);
                _pimpl->_srvMapping[srvBindingCount]._type = b;
				_pimpl->_srvMapping[srvBindingCount]._descriptorSetBindIndex = bIndex;
				++srvBindingCount;
				break;

			case DescriptorType::ConstantBuffer:
                assert(cbBindingCount < Pimpl::s_maxBindings);
                _pimpl->_cbMapping[cbBindingCount]._type = b;
				_pimpl->_cbMapping[cbBindingCount]._descriptorSetBindIndex = bIndex;
				++cbBindingCount;
				break;

			case DescriptorType::Sampler:
                assert(samplerBindingCount < Pimpl::s_maxBindings);
                _pimpl->_samplerMapping[samplerBindingCount]._type = b;
				_pimpl->_samplerMapping[samplerBindingCount]._descriptorSetBindIndex = bIndex;
				++samplerBindingCount;
				break;

			case DescriptorType::UnorderedAccessTexture:
			case DescriptorType::UnorderedAccessBuffer:
                assert(uavBindingCount < Pimpl::s_maxBindings);
                _pimpl->_uavMapping[uavBindingCount]._type = b;
				_pimpl->_uavMapping[uavBindingCount]._descriptorSetBindIndex = bIndex;
				++uavBindingCount;
				break;

			default:
				assert(0);
			}
        }

        // Create the default resources binding sets by binding "blank" default resources to all
		// descriptor set slots
		srvBindingCount = 0u;
		uavBindingCount = 0u;
		cbBindingCount = 0u;
		samplerBindingCount = 0u;

		const auto& defResources = globalPools._dummyResources;
		const TextureView* blankSRVImage = &defResources._blankSrv;
		const TextureView* blankUAVImage = &defResources._blankUavImage;
		const TextureView* blankUAVBuffer = &defResources._blankUavBuffer;
		VkSampler blankSampler = defResources._blankSampler->GetUnderlying();
		VkBuffer blankBuffer = defResources._blankBuffer.GetUnderlying();

		for (unsigned bIndex=0; bIndex<(unsigned)signature._bindings.size(); ++bIndex) {
			switch (signature._bindings[bIndex]) {
			case DescriptorType::Texture:
				BindSRV(srvBindingCount++, MakeIteratorRange(&blankSRVImage, &blankSRVImage+1));
				break;
			case DescriptorType::UnorderedAccessTexture:
				BindUAV(uavBindingCount++, MakeIteratorRange(&blankUAVImage, &blankUAVImage+1));
				break;
			case DescriptorType::UnorderedAccessBuffer:
				BindUAV(uavBindingCount++, MakeIteratorRange(&blankUAVBuffer, &blankUAVBuffer+1));
				break;
			case DescriptorType::ConstantBuffer:
				BindCB(cbBindingCount++, MakeIteratorRange(&blankBuffer, &blankBuffer+1));
				break;
			case DescriptorType::Sampler:
				BindSampler(samplerBindingCount++, MakeIteratorRange(&blankSampler, &blankSampler+1));
				break;
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

