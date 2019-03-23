// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "InputLayout.h"
#include "PipelineLayout.h"
#include "TextureView.h"
#include "ObjectFactory.h"
#include "Pools.h"
#include "../../../ConsoleRig/Log.h"
#include "../../../Utility/MemoryUtils.h"
#include "../../../Utility/ArithmeticUtils.h"

#include "IncludeVulkan.h"

namespace RenderCore { namespace Metal_Vulkan
{

	class NumericUniformsInterface::Pimpl
    {
    public:
        static const unsigned s_pendingBufferLength = 32;
        static const unsigned s_descriptorSetCount = 1;
        static const unsigned s_maxBindings = 64u;

        VkDescriptorBufferInfo  _bufferInfo[s_pendingBufferLength];
        VkDescriptorImageInfo   _imageInfo[s_pendingBufferLength];
        VkWriteDescriptorSet    _writes[s_pendingBufferLength];

        unsigned _pendingWrites = 0;
        unsigned _pendingImageInfos = 0;
        unsigned _pendingBufferInfos = 0;

        VkDescriptorSetLayout               _layouts[s_descriptorSetCount];
        VulkanUniquePtr<VkDescriptorSet>    _activeDescSets[s_descriptorSetCount];
        VulkanUniquePtr<VkDescriptorSet>    _defaultDescSets[s_descriptorSetCount];

        uint64              _sinceLastFlush[s_descriptorSetCount];
        uint64              _slotsFilled[s_descriptorSetCount];

        DescriptorPool*     _descriptorPool = nullptr;

		#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
			DescriptorSetVerboseDescription _verboseDescription[s_descriptorSetCount];
		#endif

		struct Binding
		{
			DescriptorSetBindingSignature::Type _type = DescriptorSetBindingSignature::Type::Unknown;
			unsigned _descriptorSetBindIndex = ~0u;
		};

        Binding		_srvMapping[s_maxBindings];
        Binding		_uavMapping[s_maxBindings];
        Binding		_cbMapping[s_maxBindings];
        Binding		_samplerMapping[s_maxBindings];

        template<typename BindingInfo> void WriteBinding(unsigned bindingPoint, unsigned descriptorSet, VkDescriptorType type, const BindingInfo& bindingInfo VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, const std::string& description));
        template<typename BindingInfo> BindingInfo& AllocateInfo(const BindingInfo& init);

		Pimpl()
		{
			for (unsigned c=0; c<s_descriptorSetCount; ++c) {
				_layouts[c] = nullptr;
				_sinceLastFlush[c] = _slotsFilled[c] = 0;
			}
			XlZeroMemory(_bufferInfo);
			XlZeroMemory(_imageInfo);
			XlZeroMemory(_writes);
		}
    };

    template<typename BindingInfo> static BindingInfo*& InfoPtr(VkWriteDescriptorSet& writeDesc);
    template<> VkDescriptorImageInfo*& InfoPtr(VkWriteDescriptorSet& writeDesc)
    {
        return *const_cast<VkDescriptorImageInfo**>(&writeDesc.pImageInfo);
    }

    template<> VkDescriptorBufferInfo*& InfoPtr(VkWriteDescriptorSet& writeDesc)
    {
        return *const_cast<VkDescriptorBufferInfo**>(&writeDesc.pBufferInfo);
    }

    template<> 
        VkDescriptorImageInfo& NumericUniformsInterface::Pimpl::AllocateInfo(const VkDescriptorImageInfo& init)
    {
        assert(_pendingImageInfos < s_pendingBufferLength);
        auto& i = _imageInfo[_pendingImageInfos++];
        i = init;
        return i;
    }

    template<> 
        VkDescriptorBufferInfo& NumericUniformsInterface::Pimpl::AllocateInfo(const VkDescriptorBufferInfo& init)
    {
        assert(_pendingBufferInfos < s_pendingBufferLength);
        auto& i = _bufferInfo[_pendingBufferInfos++];
        i = init;
        return i;
    }

    template<typename BindingInfo>
        void    NumericUniformsInterface::Pimpl::WriteBinding(
			unsigned bindingPoint, unsigned descriptorSet, VkDescriptorType type, const BindingInfo& bindingInfo
			VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, const std::string& description))
    {
            // (we're limited by the number of bits in _sinceLastFlush)
        if (bindingPoint >= 64u) {
            Log(Warning) << "Cannot bind to binding point " << bindingPoint << std::endl;
            return;
        }

        if (_sinceLastFlush[descriptorSet] & (1ull<<bindingPoint)) {
            // we already have a pending write to this slot. Let's find it, and just
            // update the details with the new view.
            bool foundExisting = false; (void)foundExisting;
            for (unsigned p=0; p<_pendingWrites; ++p) {
                auto& w = _writes[p];
                if (w.descriptorType == type && w.dstBinding == bindingPoint) {
                    *InfoPtr<BindingInfo>(w) = bindingInfo;
                    foundExisting = true;
                    break;
                }
            }
            assert(foundExisting);
        } else {
            _sinceLastFlush[descriptorSet] |= 1ull<<bindingPoint;

            assert(_pendingWrites < Pimpl::s_pendingBufferLength);
            auto& w = _writes[_pendingWrites++];
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.pNext = nullptr;
            w.dstSet = nullptr;
            w.dstBinding = bindingPoint;
            w.dstArrayElement = 0;
            w.descriptorCount = 1;
            w.descriptorType = type;

            InfoPtr<BindingInfo>(w) = &AllocateInfo(bindingInfo);

			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				if (_verboseDescription[descriptorSet]._bindingDescriptions.size() <= bindingPoint)
					_verboseDescription[descriptorSet]._bindingDescriptions.resize(bindingPoint+1);
				_verboseDescription[descriptorSet]._bindingDescriptions[bindingPoint] = { type, description };
			#endif
        }
    }

    void    NumericUniformsInterface::BindSRV(unsigned startingPoint, IteratorRange<const TextureView*const*> resources)
    {
        const auto descriptorSet = 0u;
        for (unsigned c=0; c<unsigned(resources.size()); ++c) {
            if (!resources[c] || (!resources[c]->GetImageView() && !resources[c]->GetResource())) continue;
            assert((startingPoint + c) < Pimpl::s_maxBindings);
			const auto& binding = _pimpl->_srvMapping[startingPoint + c];
			if (binding._descriptorSetBindIndex == ~0u) {
				Log(Warning) << "SRV numeric binding (" << (startingPoint + c) << " is off root signature" << std::endl;
				continue;
			}

			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				std::string description;
				if (resources[c]->GetResource()) {
					description = std::string{"SRV: "} + resources[c]->GetResource()->GetDesc()._name;
				} else {
					description = std::string{"SRV"};
				}
			#endif

            if (resources[c]->GetImageView()) {
				assert(binding._type == DescriptorSetBindingSignature::Type::Texture);

                _pimpl->WriteBinding(
                    binding._descriptorSetBindIndex, descriptorSet, 
                    VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,        // (could be a COMBINED_IMAGE_SAMPLER or just a SAMPLED_IMAGE)
                    VkDescriptorImageInfo { nullptr, resources[c]->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
					VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, description));
            } else {
                assert(binding._type == DescriptorSetBindingSignature::Type::TextureAsBuffer);
            
                // This is a "structured buffer" in the DirectX terminology
                auto buffer = resources[c]->GetResource()->GetBuffer();
                assert(buffer);
                _pimpl->WriteBinding(
                    binding._descriptorSetBindIndex, descriptorSet, 
                    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    VkDescriptorBufferInfo { buffer, 0, VK_WHOLE_SIZE }
					VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, description));
            }
        }
    }

    void    NumericUniformsInterface::BindUAV(unsigned startingPoint, IteratorRange<const TextureView*const*> resources)
    {
        const auto descriptorSet = 0u;
        for (unsigned c=0; c<unsigned(resources.size()); ++c) {
            if (!resources[c] || (!resources[c]->GetImageView() && !resources[c]->GetResource())) continue;
            const auto& binding = _pimpl->_uavMapping[startingPoint + c];
			if (binding._descriptorSetBindIndex == ~0u) {
				Log(Warning) << "UAV numeric binding (" << (startingPoint + c) << " is off root signature" << std::endl;
				continue;
			}

			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				std::string description;
				if (resources[c]->GetResource()) {
					description = std::string{"UAV: "} + resources[c]->GetResource()->GetDesc()._name;
				} else {
					description = std::string{"UAV"};
				}
			#endif

            // We could be binding either a "gimageXX" type, or a "buffer" type. Check the texture 
            // to see if there is an image view attached.
            // The type must correspond to what the layout is expecting.

            if (resources[c]->GetImageView()) {
				assert(binding._type == DescriptorSetBindingSignature::Type::UnorderedAccess);
                // note --  load and store operations can only be performed in VK_IMAGE_LAYOUT_GENERAL
                _pimpl->WriteBinding(
                    binding._descriptorSetBindIndex, descriptorSet, 
                    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    VkDescriptorImageInfo { nullptr, resources[c]->GetImageView(), VK_IMAGE_LAYOUT_GENERAL }
					VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, description));
            } else {
				assert(binding._type == DescriptorSetBindingSignature::Type::UnorderedAccessAsBuffer);
                auto buffer = resources[c]->GetResource()->GetBuffer();
                assert(buffer);
                _pimpl->WriteBinding(
                    binding._descriptorSetBindIndex, descriptorSet, 
                    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    VkDescriptorBufferInfo { buffer, 0, VK_WHOLE_SIZE }
					VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, description));
            }
        }
    }

    void    NumericUniformsInterface::BindCB(unsigned startingPoint, IteratorRange<const VkBuffer*> uniformBuffers)
    {
        const auto descriptorSet = 0u;
        for (unsigned c=0; c<unsigned(uniformBuffers.size()); ++c) {
            if (!uniformBuffers[c]) continue;
			const auto& binding = _pimpl->_cbMapping[startingPoint + c];
			if (binding._descriptorSetBindIndex == ~0u) {
				Log(Warning) << "Uniform buffer numeric binding (" << (startingPoint + c) << " is off root signature" << std::endl;
				continue;
			}

			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				std::string description = std::string{"CB"};
			#endif

			assert(binding._type == DescriptorSetBindingSignature::Type::ConstantBuffer);
            _pimpl->WriteBinding(
                binding._descriptorSetBindIndex, descriptorSet, 
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VkDescriptorBufferInfo { uniformBuffers[c], 0, VK_WHOLE_SIZE }
				VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, description));
        }
    }

    void    NumericUniformsInterface::BindSampler(unsigned startingPoint, IteratorRange<const VkSampler*> samplers)
    {
        const auto descriptorSet = 0u;
        for (unsigned c=0; c<unsigned(samplers.size()); ++c) {
            if (!samplers[c]) continue;
			const auto& binding = _pimpl->_samplerMapping[startingPoint + c];
			if (binding._descriptorSetBindIndex == ~0u) {
				Log(Warning) << "Sampler numeric binding (" << (startingPoint + c) << ") is off root signature" << std::endl;
				continue;
			}

			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				std::string description = std::string{"Sampler"};
			#endif

            assert(binding._type == DescriptorSetBindingSignature::Type::Sampler);
			_pimpl->WriteBinding(
                binding._descriptorSetBindIndex, descriptorSet, 
                VK_DESCRIPTOR_TYPE_SAMPLER,
                VkDescriptorImageInfo { samplers[c] }
				VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, description));
        }
    }

    void    NumericUniformsInterface::GetDescriptorSets(IteratorRange<VkDescriptorSet*> dst)
    {
        // If we've had any changes this last time, we must create new
        // descriptor sets. We will use vkUpdateDescriptorSets to fill in these
        // sets with the latest changes. Note that this will require copy across the
        // bindings that haven't changed.
        // It turns out that copying using VkCopyDescriptorSet is probably going to be
        // slow. We should try a different approach.
        if (_pimpl->_pendingWrites || !_pimpl->_activeDescSets[0]) {
            VulkanUniquePtr<VkDescriptorSet> newSets[Pimpl::s_descriptorSetCount];
            _pimpl->_descriptorPool->Allocate(MakeIteratorRange(newSets), MakeIteratorRange(_pimpl->_layouts));

            for (unsigned c=0; c<_pimpl->_pendingWrites; ++c)
                _pimpl->_writes[c].dstSet = newSets[0].get();

            VkCopyDescriptorSet copies[Pimpl::s_maxBindings * Pimpl::s_descriptorSetCount];
            unsigned copyCount = 0;
            for (unsigned s=0; s<Pimpl::s_descriptorSetCount; ++s) {
                auto set = _pimpl->_activeDescSets[s].get();
                if (!set) set = _pimpl->_defaultDescSets[s].get();

                auto filledButNotWritten = _pimpl->_slotsFilled[s] & ~_pimpl->_sinceLastFlush[s];
                unsigned msbBit = 64u - xl_clz8(filledButNotWritten);
                unsigned lsbBit = xl_ctz8(filledButNotWritten);
                for (unsigned b=lsbBit; b<=msbBit; ++b) {
                    if (filledButNotWritten & (1ull<<b)) {
                        assert(copyCount < dimof(copies));
                        auto& cpy = copies[copyCount++];
                        cpy.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
                        cpy.pNext = nullptr;

                        cpy.srcSet = set;
                        cpy.srcBinding = b;
                        cpy.srcArrayElement = 0;

                        cpy.dstSet = newSets[s].get();
                        cpy.dstBinding = b;
                        cpy.dstArrayElement = 0;
                        cpy.descriptorCount = 1;        // (we can set this higher to set multiple sequential descriptors)
                    }
                }
            }

            vkUpdateDescriptorSets(
                _pimpl->_descriptorPool->GetDevice(), 
                _pimpl->_pendingWrites, _pimpl->_writes, 
                copyCount, copies);

            _pimpl->_pendingWrites = 0;
            _pimpl->_pendingImageInfos = 0;
            _pimpl->_pendingBufferInfos = 0;

            for (unsigned c=0; c<Pimpl::s_descriptorSetCount; ++c) {
                _pimpl->_slotsFilled[c] |= _pimpl->_sinceLastFlush[c];
                _pimpl->_sinceLastFlush[c] = 0ull;
                _pimpl->_activeDescSets[c] = std::move(newSets[c]);
            }
        }

        for (unsigned c=0; c<unsigned(dst.size()); ++c)
            dst[c] = (c < Pimpl::s_descriptorSetCount) ? _pimpl->_activeDescSets[c].get() : nullptr;
    }

    bool    NumericUniformsInterface::HasChanges() const
    {
        // note --  we have to bind some descriptor set for the first draw of the frame,
        //          even if nothing has been bound! So, when _activeDescSets is empty
        //          we must return true here.
        return _pimpl->_pendingWrites != 0 || !_pimpl->_activeDescSets[0];
    }

    void    NumericUniformsInterface::Reset()
    {
        _pimpl->_pendingWrites = 0u;
        _pimpl->_pendingImageInfos = 0u;
        _pimpl->_pendingBufferInfos = 0u;

        XlZeroMemory(_pimpl->_bufferInfo);
        XlZeroMemory(_pimpl->_imageInfo);
        XlZeroMemory(_pimpl->_writes);

        for (unsigned c=0; c<Pimpl::s_descriptorSetCount; ++c) {
            _pimpl->_sinceLastFlush[c] = 0x0u;
            _pimpl->_activeDescSets[c] = nullptr;
        }
    }

    NumericUniformsInterface::NumericUniformsInterface(
        const ObjectFactory& factory,
        DescriptorPool& descPool, 
        DummyResources& defResources,
        VkDescriptorSetLayout layout,
        const DescriptorSetSignature& signature)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_descriptorPool = &descPool;
        _pimpl->_layouts[0] = layout;

        for (unsigned c=0; c<Pimpl::s_descriptorSetCount; ++c) {
            _pimpl->_slotsFilled[c] = 0x0ull;
            _pimpl->_sinceLastFlush[c] = 0x0ull;

			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				_pimpl->_verboseDescription[c]._descriptorSetInfo = "NumericUniformsInterface";
			#endif
        }

        Reset();

        // We need to make a mapping between the HLSL binding numbers and types
        // and the binding index in our descriptor set
		unsigned srvBindingCount = 0u;
		unsigned uavBindingCount = 0u;
		unsigned cbBindingCount = 0u;
		unsigned samplerBindingCount = 0u;

        for (unsigned bIndex=0; bIndex<(unsigned)signature._bindings.size(); ++bIndex) {
            const auto& b = signature._bindings[bIndex];
            assert(b._hlslBindingIndex < Pimpl::s_maxBindings);
			switch (b._type) {
			case DescriptorSetBindingSignature::Type::Texture:
			case DescriptorSetBindingSignature::Type::TextureAsBuffer:
				assert(srvBindingCount < Pimpl::s_maxBindings);
                _pimpl->_srvMapping[srvBindingCount]._type = b._type;
				_pimpl->_srvMapping[srvBindingCount]._descriptorSetBindIndex = bIndex;
				++srvBindingCount;
				break;

			case DescriptorSetBindingSignature::Type::ConstantBuffer:
                assert(cbBindingCount < Pimpl::s_maxBindings);
                _pimpl->_cbMapping[cbBindingCount]._type = b._type;
				_pimpl->_cbMapping[cbBindingCount]._descriptorSetBindIndex = bIndex;
				++cbBindingCount;
				break;

			case DescriptorSetBindingSignature::Type::Sampler:
                assert(samplerBindingCount < Pimpl::s_maxBindings);
                _pimpl->_samplerMapping[samplerBindingCount]._type = b._type;
				_pimpl->_samplerMapping[samplerBindingCount]._descriptorSetBindIndex = bIndex;
				++samplerBindingCount;
				break;

			case DescriptorSetBindingSignature::Type::UnorderedAccess:
			case DescriptorSetBindingSignature::Type::UnorderedAccessAsBuffer:
                assert(uavBindingCount < Pimpl::s_maxBindings);
                _pimpl->_uavMapping[uavBindingCount]._type = b._type;
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

		const TextureView* blankSRVImage = &defResources._blankSrv;
		const TextureView* blankSRVBuffer = &defResources._blankUavBuffer;
		const TextureView* blankUAVImage = &defResources._blankUavImage;
		const TextureView* blankUAVBuffer = &defResources._blankUavBuffer;
		VkSampler blankSampler = defResources._blankSampler->GetUnderlying();
		VkBuffer blankBuffer = defResources._blankBuffer.GetUnderlying();

		for (unsigned bIndex=0; bIndex<(unsigned)signature._bindings.size(); ++bIndex) {
			switch (signature._bindings[bIndex]._type) {
			case DescriptorSetBindingSignature::Type::Texture:
				BindSRV(srvBindingCount++, MakeIteratorRange(&blankSRVImage, &blankSRVImage+1));
				break;
			case DescriptorSetBindingSignature::Type::TextureAsBuffer:
				BindSRV(srvBindingCount++, MakeIteratorRange(&blankSRVBuffer, &blankSRVBuffer+1));
				break;
			case DescriptorSetBindingSignature::Type::UnorderedAccess:
				BindUAV(uavBindingCount++, MakeIteratorRange(&blankUAVImage, &blankUAVImage+1));
				break;
			case DescriptorSetBindingSignature::Type::UnorderedAccessAsBuffer:
				BindUAV(uavBindingCount++, MakeIteratorRange(&blankUAVBuffer, &blankUAVBuffer+1));
				break;
			case DescriptorSetBindingSignature::Type::ConstantBuffer:
				BindCB(cbBindingCount++, MakeIteratorRange(&blankBuffer, &blankBuffer+1));
				break;
			case DescriptorSetBindingSignature::Type::Sampler:
				BindSampler(samplerBindingCount++, MakeIteratorRange(&blankSampler, &blankSampler+1));
				break;
			}
		}

        // Create the descriptor sets, and then move them into "_defaultDescSets"
        // Note that these sets must come from a non-resetable permanent pool
        GetDescriptorSets(IteratorRange<VkDescriptorSet*>());
        for (unsigned c=0; c<Pimpl::s_descriptorSetCount; ++c)
            _pimpl->_defaultDescSets[c] = std::move(_pimpl->_activeDescSets[c]);
    }

	#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
		DescriptorSetVerboseDescription NumericUniformsInterface::GetDescription() const
		{
			return _pimpl->_verboseDescription[0];
		}
	#endif

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

