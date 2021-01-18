// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DescriptorSet.h"
#include "TextureView.h"
#include "Pools.h"
#include "PipelineLayout.h"
#include "PipelineLayoutSignatureFile.h"
#include "ShaderReflection.h"
#include "../../ShaderService.h"
#include "../../../OSServices/Log.h"
#include "../../../Utility/MemoryUtils.h"
#include "../../../Utility/ArithmeticUtils.h"
#include "../../../Utility/StreamUtils.h"
#include "../../../Core/Prefix.h"
#include <sstream>

namespace RenderCore { namespace Metal_Vulkan
{
	static const std::string s_dummyDescriptorString{"<DummyDescriptor>"};

	template<typename BindingInfo> static BindingInfo const*& InfoPtr(VkWriteDescriptorSet& writeDesc);
    template<> VkDescriptorImageInfo const*& InfoPtr(VkWriteDescriptorSet& writeDesc) { return writeDesc.pImageInfo; }
    template<> VkDescriptorBufferInfo const*& InfoPtr(VkWriteDescriptorSet& writeDesc) { return writeDesc.pBufferInfo; }

    template<> 
        VkDescriptorImageInfo& DescriptorSetBuilder::AllocateInfo(const VkDescriptorImageInfo& init)
    {
        assert(_pendingImageInfos < s_pendingBufferLength);
        auto& i = _imageInfo[_pendingImageInfos++];
        i = init;
        return i;
    }

    template<> 
        VkDescriptorBufferInfo& DescriptorSetBuilder::AllocateInfo(const VkDescriptorBufferInfo& init)
    {
        assert(_pendingBufferInfos < s_pendingBufferLength);
        auto& i = _bufferInfo[_pendingBufferInfos++];
        i = init;
        return i;
    }

    template<typename BindingInfo>
        void    DescriptorSetBuilder::WriteBinding(
			unsigned bindingPoint, VkDescriptorType type, const BindingInfo& bindingInfo, bool reallocateBufferInfo
			VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, const std::string& description))
    {
            // (we're limited by the number of bits in _sinceLastFlush)
        if (bindingPoint >= 64u) {
            Log(Warning) << "Cannot bind to binding point " << bindingPoint << std::endl;
            return;
        }

        if (_sinceLastFlush & (1ull<<bindingPoint)) {
            // we already have a pending write to this slot. Let's find it, and just
            // update the details with the new view.
            bool foundExisting = false; (void)foundExisting;
            for (unsigned p=0; p<_pendingWrites; ++p) {
                auto& w = _writes[p];
                if (w.descriptorType == type && w.dstBinding == bindingPoint) {
					InfoPtr<BindingInfo>(w) = (reallocateBufferInfo) ? &AllocateInfo(bindingInfo) : &bindingInfo;
                    foundExisting = true;
                    break;
                }
            }
            assert(foundExisting);
        } else {
            _sinceLastFlush |= 1ull<<bindingPoint;

            assert(_pendingWrites < s_pendingBufferLength);
            auto& w = _writes[_pendingWrites++];
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.pNext = nullptr;
            w.dstSet = nullptr;
            w.dstBinding = bindingPoint;
            w.dstArrayElement = 0;
            w.descriptorCount = 1;
            w.descriptorType = type;

			InfoPtr<BindingInfo>(w) = (reallocateBufferInfo) ? &AllocateInfo(bindingInfo) : &bindingInfo;

			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				if (_verboseDescription._bindingDescriptions.size() <= bindingPoint)
					_verboseDescription._bindingDescriptions.resize(bindingPoint+1);
				_verboseDescription._bindingDescriptions[bindingPoint] = { type, description };
			#endif
        }
    }

    void    DescriptorSetBuilder::BindSRV(unsigned descriptorSetBindPoint, const TextureView* resource)
    {
		// Our "StructuredBuffer" objects are being mapped onto uniform buffers in SPIR-V
		// So sometimes a SRV will end up writing to a VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
		// descriptor.
		bool wroteSomething = false;
		if (expect_evaluation(resource, 1)) {
			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				std::string description;
				if (resource->GetResource()) {
					description = std::string{"SRV: "} + resource->GetResource()->GetDesc()._name;
				} else {
					description = std::string{"SRV"};
				}
			#endif

			if (resource->GetImageView()) {
				WriteBinding(
					descriptorSetBindPoint,
					VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,        // (could be a COMBINED_IMAGE_SAMPLER or just a SAMPLED_IMAGE)
					VkDescriptorImageInfo { nullptr, resource->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, true
					VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, description));
				wroteSomething = true;
			} else if (resource->GetResource()) {
				// This is a "structured buffer" in the DirectX terminology
				auto buffer = resource->GetResource()->GetBuffer();
				assert(buffer);
				WriteBinding(
					descriptorSetBindPoint,
					VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					VkDescriptorBufferInfo { buffer, 0, VK_WHOLE_SIZE }, true
					VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, description));
				wroteSomething = true;
			}
		}

		if (!wroteSomething) {
			// given a null pointer -- have to substitute a dummy image
			WriteBinding(
				descriptorSetBindPoint,
				VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				VkDescriptorImageInfo {
					nullptr,
					_globalPools->_dummyResources._blankSrv.GetImageView(),
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, true
				VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, s_dummyDescriptorString));
		}
    }

    void    DescriptorSetBuilder::BindUAV(unsigned descriptorSetBindPoint, const TextureView* resource)
    {
		bool wroteSomething = false;
		if (expect_evaluation(resource, 1)) {
			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				std::string description;
				if (resource->GetResource()) {
					description = std::string{"UAV: "} + resource->GetResource()->GetDesc()._name;
				} else {
					description = std::string{"UAV"};
				}
			#endif

			// We could be binding either a "gimageXX" type, or a "buffer" type. Check the texture 
			// to see if there is an image view attached.
			// The type must correspond to what the layout is expecting.

			if (resource->GetImageView()) {
				// note --  load and store operations can only be performed in VK_IMAGE_LAYOUT_GENERAL
				WriteBinding(
					descriptorSetBindPoint,
					VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
					VkDescriptorImageInfo { _globalPools->_dummyResources._blankSampler->GetUnderlying(), resource->GetImageView(), VK_IMAGE_LAYOUT_GENERAL }, true
					VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, description));
				wroteSomething = true;
			} else if (resource->GetResource()) {
				auto buffer = resource->GetResource()->GetBuffer();
				assert(buffer);
				WriteBinding(
					descriptorSetBindPoint,
					VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					VkDescriptorBufferInfo { buffer, 0, VK_WHOLE_SIZE }, true
					VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, description));
				wroteSomething = true;
			}
		}

		if (!wroteSomething) {
			// given a null pointer -- have to substitute a dummy image
			WriteBinding(
				descriptorSetBindPoint,
				VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				VkDescriptorImageInfo {
					_globalPools->_dummyResources._blankSampler->GetUnderlying(),
					_globalPools->_dummyResources._blankSrv.GetImageView(),
					VK_IMAGE_LAYOUT_GENERAL }, true
				VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, s_dummyDescriptorString));
		}
    }

    void    DescriptorSetBuilder::BindCB(unsigned descriptorSetBindPoint, VkDescriptorBufferInfo uniformBuffer, StringSection<> description)
    {
		#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
			std::string descString;
			if (description.IsEmpty()) {
				descString = std::string{"CB"};
			} else {
				descString = std::string{"CB: "} + description.AsString();
			}
		#endif

        WriteBinding(
            descriptorSetBindPoint,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			uniformBuffer, true
			VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, descString));
    }

    void    DescriptorSetBuilder::BindSampler(unsigned descriptorSetBindPoint, VkSampler sampler, StringSection<> description)
    {
		#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
			std::string descString;
			if (description.IsEmpty()) {
				descString = std::string{"Sampler"};
			} else {
				descString = std::string{"Sampler: "} + description.AsString();
			}
		#endif

		WriteBinding(
            descriptorSetBindPoint,
            VK_DESCRIPTOR_TYPE_SAMPLER,
            VkDescriptorImageInfo { sampler }, true
			VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, descString));
    }

	uint64_t	DescriptorSetBuilder::BindDummyDescriptors(const DescriptorSetSignature& sig, uint64_t dummyDescWriteMask)
	{
		uint64_t bindingsWrittenTo = 0u;

		auto& blankBuffer = AllocateInfo(
			VkDescriptorBufferInfo { 
				_globalPools->_dummyResources._blankBuffer.GetUnderlying(),
				0, VK_WHOLE_SIZE });
		auto& blankImage = AllocateInfo(
			VkDescriptorImageInfo {
				_globalPools->_dummyResources._blankSampler->GetUnderlying(),
				_globalPools->_dummyResources._blankSrv.GetImageView(),
				VK_IMAGE_LAYOUT_GENERAL });
		auto& blankSampler = AllocateInfo(
			VkDescriptorImageInfo {
				_globalPools->_dummyResources._blankSampler->GetUnderlying(),
				nullptr,
				VK_IMAGE_LAYOUT_UNDEFINED });
		auto& blankStorageImage = AllocateInfo(
			VkDescriptorImageInfo {
				_globalPools->_dummyResources._blankSampler->GetUnderlying(),
				_globalPools->_dummyResources._blankUavImage.GetImageView(),
				VK_IMAGE_LAYOUT_GENERAL });
		auto& blankStorageBuffer = AllocateInfo(
			VkDescriptorBufferInfo {
				_globalPools->_dummyResources._blankUavBuffer.GetResource()->GetBuffer(),
				0, VK_WHOLE_SIZE });

        unsigned minBit = xl_ctz8(dummyDescWriteMask);
        unsigned maxBit = std::min(64u - xl_clz8(dummyDescWriteMask), (unsigned)sig._bindings.size()-1);

        for (unsigned bIndex=minBit; bIndex<=maxBit; ++bIndex) {
            if (!(dummyDescWriteMask & (1ull<<uint64(bIndex)))) continue;

			const auto& b = sig._bindings[bIndex];
            if (b == DescriptorType::ConstantBuffer) {
				WriteBinding(
					bIndex,
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					blankBuffer, false
					VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, s_dummyDescriptorString));
            } else if (b == DescriptorType::Texture) {
				WriteBinding(
					bIndex,
					VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
					blankImage, false
					VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, s_dummyDescriptorString));
            } else if (b == DescriptorType::Sampler) {
				WriteBinding(
					bIndex,
					VK_DESCRIPTOR_TYPE_SAMPLER,
					blankSampler, false
					VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, s_dummyDescriptorString));
			} else if (b == DescriptorType::UnorderedAccessTexture) {
				WriteBinding(
					bIndex,
					VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
					blankStorageImage, false
					VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, s_dummyDescriptorString));
			} else if (b == DescriptorType::UnorderedAccessBuffer) {
				WriteBinding(
					bIndex,
					VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					blankStorageBuffer, false
					VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, s_dummyDescriptorString));
			} else {
                assert(0);
				continue;
            }
			bindingsWrittenTo |= 1ull << uint64(bIndex);
        }

		return bindingsWrittenTo;
	}

	uint64_t	DescriptorSetBuilder::FlushChanges(
		VkDevice device,
		VkDescriptorSet destination,
		VkDescriptorSet copyPrevDescriptors, uint64_t prevDescriptorMask
		VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, DescriptorSetVerboseDescription& description))
	{
		// Flush out changes to the given descriptor set.
		// Copy unwritten descriptors from copyPrevDescriptors
		// Return a mask of the writes that we actually committed

        VkCopyDescriptorSet copies[64];
        unsigned copyCount = 0;

		if (copyPrevDescriptors && prevDescriptorMask) {
            auto filledButNotWritten = prevDescriptorMask & ~_sinceLastFlush;
            unsigned msbBit = 64u - xl_clz8(filledButNotWritten);
            unsigned lsbBit = xl_ctz8(filledButNotWritten);
            for (unsigned b=lsbBit; b<=msbBit; ++b) {
                if (filledButNotWritten & (1ull<<b)) {
                    assert(copyCount < dimof(copies));
                    auto& cpy = copies[copyCount++];
                    cpy.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
                    cpy.pNext = nullptr;

                    cpy.srcSet = copyPrevDescriptors;
                    cpy.srcBinding = b;
                    cpy.srcArrayElement = 0;

                    cpy.dstSet = destination;
                    cpy.dstBinding = b;
                    cpy.dstArrayElement = 0;
                    cpy.descriptorCount = 1;        // (we can set this higher to set multiple sequential descriptors)
                }
            }
        }

        for (unsigned c=0; c<_pendingWrites; ++c)
            _writes[c].dstSet = destination;
		vkUpdateDescriptorSets(
            device, 
            _pendingWrites, _writes, 
            copyCount, copies);

        _pendingWrites = 0;
        _pendingImageInfos = 0;
        _pendingBufferInfos = 0;
		auto result = _sinceLastFlush;
		_sinceLastFlush = 0;

		#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
			if (description._bindingDescriptions.size() < _verboseDescription._bindingDescriptions.size())
				description._bindingDescriptions.resize(_verboseDescription._bindingDescriptions.size());
			for (unsigned b=0; b<_verboseDescription._bindingDescriptions.size(); ++b) {
				if (!(result && 1ull<<uint64_t(b))) continue;
				description._bindingDescriptions[b] = _verboseDescription._bindingDescriptions[b];
			}
		#endif

		return result;
	}

	void	DescriptorSetBuilder::ValidatePendingWrites(const DescriptorSetSignature& sig)
	{
		for (unsigned w=0; w<_pendingWrites; ++w) {
			auto binding = _writes[w].dstBinding;
			assert(binding < sig._bindings.size()); (void)binding;
			assert(AsVkDescriptorType(sig._bindings[binding]) == _writes[w].descriptorType);
		}
	}

	bool    DescriptorSetBuilder::HasChanges() const
    {
        // note --  we have to bind some descriptor set for the first draw of the frame,
        //          even if nothing has been bound! So, when _activeDescSets is empty
        //          we must return true here.
        return _sinceLastFlush != 0;
    }

    void    DescriptorSetBuilder::Reset()
    {
        _pendingWrites = 0u;
        _pendingImageInfos = 0u;
        _pendingBufferInfos = 0u;

        XlZeroMemory(_bufferInfo);
        XlZeroMemory(_imageInfo);
        XlZeroMemory(_writes);

        _sinceLastFlush = 0x0u;
    }

	DescriptorSetBuilder::DescriptorSetBuilder(GlobalPools& globalPools)
	{
		_globalPools = &globalPools;
		_sinceLastFlush = 0;
		XlZeroMemory(_bufferInfo);
		XlZeroMemory(_imageInfo);
		XlZeroMemory(_writes);
	}

	DescriptorSetBuilder::~DescriptorSetBuilder()
	{
	}

	#if defined(VULKAN_VERBOSE_DESCRIPTIONS)

		static const std::string s_columnHeader0 = "Root Signature";
		static const std::string s_columnHeader2 = "Binding";
		static const std::string s_columnHeader3 = "Legacy Binding";

		std::ostream& WriteDescriptorSet(
			std::ostream& stream,
			const DescriptorSetVerboseDescription& bindingDescription,
			const DescriptorSetSignature& signature,
			const LegacyRegisterBinding& legacyBinding,
			IteratorRange<const CompiledShaderByteCode**> compiledShaderByteCode,
			unsigned descriptorSetIndex, bool isBound)
		{
			std::vector<std::string> signatureColumn;
			std::vector<std::string> shaderColumns[(unsigned)ShaderStage::Max];
			std::vector<std::string> legacyBindingColumn;

			size_t signatureColumnMax = 0, bindingColumnMax = 0, legacyBindingColumnMax = 0;
			size_t shaderColumnMax[(unsigned)ShaderStage::Max] = {};

			signatureColumn.resize(signature._bindings.size());
			for (unsigned c=0; c<signature._bindings.size(); ++c) {
				signatureColumn[c] = std::string{AsString(signature._bindings[c])};
				signatureColumnMax = std::max(signatureColumnMax, signatureColumn[c].size());
			}
			signatureColumnMax = std::max(signatureColumnMax, s_columnHeader0.size());

			for (unsigned stage=0; stage<std::min((unsigned)ShaderStage::Max, (unsigned)compiledShaderByteCode.size()); ++stage) {
				if (!compiledShaderByteCode[stage] || compiledShaderByteCode[stage]->GetByteCode().empty())
					continue;

				shaderColumns[stage].reserve(signature._bindings.size());
				SPIRVReflection reflection{compiledShaderByteCode[stage]->GetByteCode()};
				for (const auto& v:reflection._bindings) {
					if (v.second._descriptorSet != descriptorSetIndex || v.second._bindingPoint == ~0u)
						continue;
					if (shaderColumns[stage].size() <= v.second._bindingPoint)
						shaderColumns[stage].resize(v.second._bindingPoint+1);
					
					shaderColumns[stage][v.second._bindingPoint] = reflection.GetName(v.first).AsString();
					shaderColumnMax[stage] = std::max(shaderColumnMax[stage], shaderColumns[stage][v.second._bindingPoint].size());
				}

				if (shaderColumnMax[stage] != 0)
					shaderColumnMax[stage] = std::max(shaderColumnMax[stage], std::strlen(AsString((ShaderStage)stage)));
			}

			for (const auto&b:bindingDescription._bindingDescriptions)
				bindingColumnMax = std::max(bindingColumnMax, b._description.size());
			bindingColumnMax = std::max(bindingColumnMax, s_columnHeader2.size());

			auto rowCount = (unsigned)std::max(signatureColumn.size(), bindingDescription._bindingDescriptions.size());
			for (unsigned stage=0; stage<(unsigned)ShaderStage::Max; ++stage)
				rowCount = std::max(rowCount, (unsigned)shaderColumns[stage].size());

			legacyBindingColumn.resize(rowCount);
			for (unsigned regType=0; regType<(unsigned)LegacyRegisterBinding::RegisterType::Unknown; ++regType) {
				auto prefix = GetRegisterPrefix((LegacyRegisterBinding::RegisterType)regType);
				auto entries = legacyBinding.GetEntries((LegacyRegisterBinding::RegisterType)regType, LegacyRegisterBinding::RegisterQualifier::None);
				for (const auto&e:entries)
					if (e._targetDescriptorSet == descriptorSetIndex && e._targetBegin < rowCount)
						for (unsigned t=e._targetBegin; t<std::min(e._targetEnd, rowCount); ++t) {
							if (!legacyBindingColumn[t].empty())
								legacyBindingColumn[t] += ", ";
							legacyBindingColumn[t] += prefix + std::to_string(t-e._targetBegin+e._begin);
						}
			}
			for (const auto&e:legacyBindingColumn)
				legacyBindingColumnMax = std::max(legacyBindingColumnMax, e.size());
			if (legacyBindingColumnMax)
				legacyBindingColumnMax = std::max(legacyBindingColumnMax, s_columnHeader3.size());

			stream << "[" << descriptorSetIndex << "] Descriptor Set: " << signature._name;
			if (isBound) {
				stream << " (bound with UniformsStream: " << bindingDescription._descriptorSetInfo << ")" << std::endl;
			} else {
				stream << " (not bound to any UniformsStream)" << std::endl;
			}
			stream << " " << s_columnHeader0 << StreamIndent(unsigned(signatureColumnMax - s_columnHeader0.size())) << " | ";
			size_t accumulatedShaderColumns = 0;
			for (unsigned stage=0; stage<(unsigned)ShaderStage::Max; ++stage) {
				if (!shaderColumnMax[stage]) continue;
				auto* title = AsString((ShaderStage)stage);
				stream << title << StreamIndent(unsigned(shaderColumnMax[stage] - std::strlen(title))) << " | ";
				accumulatedShaderColumns += shaderColumnMax[stage] + 3;
			}
			stream << s_columnHeader2 << StreamIndent(unsigned(bindingColumnMax - s_columnHeader2.size()));
			if (legacyBindingColumnMax) {
				stream << " | " << s_columnHeader3 << StreamIndent(unsigned(legacyBindingColumnMax - s_columnHeader3.size()));
			}
			stream << std::endl;
			auto totalWidth = signatureColumnMax + bindingColumnMax + accumulatedShaderColumns + 5;
			if (legacyBindingColumnMax) totalWidth += 3 + legacyBindingColumnMax;
			stream << StreamIndent{unsigned(totalWidth), '-'} << std::endl;

			for (unsigned row=0; row<rowCount; ++row) {
				stream << " ";
				if (row < signatureColumn.size()) {
					stream << signatureColumn[row] << StreamIndent(unsigned(signatureColumnMax - signatureColumn[row].size()));
				} else {
					stream << StreamIndent(unsigned(signatureColumnMax));
				}
				stream << " | ";

				for (unsigned stage=0; stage<(unsigned)ShaderStage::Max; ++stage) {
					if (!shaderColumnMax[stage]) continue;
					if (row < shaderColumns[stage].size()) {
						stream << shaderColumns[stage][row] << StreamIndent(unsigned(shaderColumnMax[stage] - shaderColumns[stage][row].size()));
					} else {
						stream << StreamIndent(unsigned(shaderColumnMax[stage]));
					}
					stream << " | ";
				}

				if (row < bindingDescription._bindingDescriptions.size()) {
					stream << bindingDescription._bindingDescriptions[row]._description << StreamIndent(unsigned(bindingColumnMax - bindingDescription._bindingDescriptions[row]._description.size()));
				} else {
					stream << StreamIndent(unsigned(bindingColumnMax));
				}

				if (legacyBindingColumnMax) {
					stream << " | ";
					if (row < legacyBindingColumn.size()) {
						stream << legacyBindingColumn[row] << StreamIndent(unsigned(legacyBindingColumnMax - legacyBindingColumn[row].size()));
					} else {
						stream << StreamIndent(unsigned(legacyBindingColumnMax));
					}
				}

				stream << std::endl;
			}
			stream << StreamIndent{unsigned(totalWidth), '-'} << std::endl;

			return stream;
		}

	#endif

}}

