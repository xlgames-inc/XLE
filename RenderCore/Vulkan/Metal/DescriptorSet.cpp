// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DescriptorSet.h"
#include "DescriptorSetSignatureFile.h"
#include "TextureView.h"
#include "Pools.h"
#include "PipelineLayout.h"
#include "DescriptorSetSignatureFile.h"
#include "ShaderReflection.h"
#include "../../ShaderService.h"
#include "../../UniformsStream.h"
#include "../../BufferView.h"
#include "../../../OSServices/Log.h"
#include "../../../Utility/MemoryUtils.h"
#include "../../../Utility/ArithmeticUtils.h"
#include "../../../Utility/StreamUtils.h"
#include "../../../Core/Prefix.h"
#include <sstream>

namespace RenderCore { namespace Metal_Vulkan
{
	static const std::string s_dummyDescriptorString{"<DummyDescriptor>"};

	VkDescriptorType_ AsVkDescriptorType(DescriptorType type);

	template<typename BindingInfo> static BindingInfo const*& InfoPtr(VkWriteDescriptorSet& writeDesc);
	template<> VkDescriptorImageInfo const*& InfoPtr(VkWriteDescriptorSet& writeDesc) { return writeDesc.pImageInfo; }
	template<> VkDescriptorBufferInfo const*& InfoPtr(VkWriteDescriptorSet& writeDesc) { return writeDesc.pBufferInfo; }

	template<> 
		VkDescriptorImageInfo& ProgressiveDescriptorSetBuilder::AllocateInfo(const VkDescriptorImageInfo& init)
	{
		assert(_pendingImageInfos < s_pendingBufferLength);
		auto& i = _imageInfo[_pendingImageInfos++];
		i = init;
		return i;
	}

	template<> 
		VkDescriptorBufferInfo& ProgressiveDescriptorSetBuilder::AllocateInfo(const VkDescriptorBufferInfo& init)
	{
		assert(_pendingBufferInfos < s_pendingBufferLength);
		auto& i = _bufferInfo[_pendingBufferInfos++];
		i = init;
		return i;
	}

	template<typename BindingInfo>
		void    ProgressiveDescriptorSetBuilder::WriteBinding(
			unsigned bindingPoint, VkDescriptorType_ type, const BindingInfo& bindingInfo, bool reallocateBufferInfo
			VULKAN_VERBOSE_DEBUG_ONLY(, const std::string& description))
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
			w.descriptorType = (VkDescriptorType)type;

			InfoPtr<BindingInfo>(w) = (reallocateBufferInfo) ? &AllocateInfo(bindingInfo) : &bindingInfo;

			#if defined(VULKAN_VERBOSE_DEBUG)
				if (_verboseDescription._bindingDescriptions.size() <= bindingPoint)
					_verboseDescription._bindingDescriptions.resize(bindingPoint+1);
				_verboseDescription._bindingDescriptions[bindingPoint] = { type, description };
			#endif
		}
	}

	void    ProgressiveDescriptorSetBuilder::Bind(unsigned descriptorSetBindPoint, const TextureView& resource)
	{
		// Our "StructuredBuffer" objects are being mapped onto uniform buffers in SPIR-V
		// So sometimes a SRV will end up writing to a VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
		// descriptor.
		#if defined(VULKAN_VERBOSE_DEBUG)
			std::string description;
			if (resource.GetResource()) {
				description = resource.GetResource()->GetDesc()._name;
			} else {
				description = std::string{"TextureView"};
			}
		#endif

		assert(descriptorSetBindPoint < _signature.size());
		auto slotType = _signature[descriptorSetBindPoint]._type;
		assert(_signature[descriptorSetBindPoint]._count == 1);

		switch (slotType) {
		case DescriptorType::Texture:
		case DescriptorType::UnorderedAccessTexture:
			assert(resource.GetResource());
			WriteBinding(
				descriptorSetBindPoint,
				AsVkDescriptorType(slotType),
				VkDescriptorImageInfo { nullptr, resource.GetImageView(), (VkImageLayout)Internal::AsVkImageLayout(resource.GetResource()->_steadyStateLayout) }, true
				VULKAN_VERBOSE_DEBUG_ONLY(, description));
			break;

		case DescriptorType::ConstantBuffer:
		case DescriptorType::UnorderedAccessBuffer:
			// This is a "structured buffer" in the DirectX terminology
			assert(resource.GetResource() && resource.GetResource()->GetBuffer());
			WriteBinding(
				descriptorSetBindPoint,
				AsVkDescriptorType(slotType),
				VkDescriptorBufferInfo { resource.GetResource()->GetBuffer(), 0, VK_WHOLE_SIZE }, true
				VULKAN_VERBOSE_DEBUG_ONLY(, description));
			break;

		default:
			assert(0);
		}
	}

	void    ProgressiveDescriptorSetBuilder::Bind(unsigned descriptorSetBindPoint, const ConstantBufferView& resource)
	{
		#if defined(VULKAN_VERBOSE_DEBUG)
			std::string description = checked_cast<const Resource*>(resource._prebuiltBuffer)->GetDesc()._name;
		#endif

		assert(descriptorSetBindPoint < _signature.size());
		auto slotType = _signature[descriptorSetBindPoint]._type;
		assert(_signature[descriptorSetBindPoint]._count == 1);

		switch (slotType) {
		case DescriptorType::ConstantBuffer:
		case DescriptorType::UnorderedAccessBuffer:
			{
				assert(resource._prebuiltBuffer);
				VkDescriptorBufferInfo bufferInfo { checked_cast<const Resource*>(resource._prebuiltBuffer)->GetBuffer(), 0, VK_WHOLE_SIZE };
				if (resource._prebuiltRangeBegin != 0 || resource._prebuiltRangeEnd != 0) {
					bufferInfo.offset = resource._prebuiltRangeBegin;
					bufferInfo.range = resource._prebuiltRangeEnd - resource._prebuiltRangeBegin;
				}
				WriteBinding(
					descriptorSetBindPoint,
					AsVkDescriptorType(slotType),
					bufferInfo, true
					VULKAN_VERBOSE_DEBUG_ONLY(, description));
			}
			break;

		default:
			assert(0);
		}
	}

	void    ProgressiveDescriptorSetBuilder::Bind(unsigned descriptorSetBindPoint, VkDescriptorBufferInfo uniformBuffer, StringSection<> description)
	{
		assert(descriptorSetBindPoint < _signature.size());
		auto slotType = _signature[descriptorSetBindPoint]._type;
		assert(_signature[descriptorSetBindPoint]._count == 1);

		switch (slotType) {
		case DescriptorType::ConstantBuffer:
		case DescriptorType::UnorderedAccessBuffer:
			WriteBinding(
				descriptorSetBindPoint,
				AsVkDescriptorType(slotType),
				uniformBuffer, true
				VULKAN_VERBOSE_DEBUG_ONLY(, description.AsString()));
			break;

		default:
			assert(0);
		}
	}

	void    ProgressiveDescriptorSetBuilder::Bind(unsigned descriptorSetBindPoint, VkSampler sampler, StringSection<> description)
	{
		assert(descriptorSetBindPoint < _signature.size());
		auto slotType = _signature[descriptorSetBindPoint]._type;
		assert(_signature[descriptorSetBindPoint]._count == 1);

		switch (slotType) {
		case DescriptorType::Sampler:
			WriteBinding(
				descriptorSetBindPoint,
				AsVkDescriptorType(slotType),
				VkDescriptorImageInfo { sampler }, true
				VULKAN_VERBOSE_DEBUG_ONLY(, description.AsString()));
			break;

		default:
			assert(0);
		}
	}

	uint64_t	ProgressiveDescriptorSetBuilder::BindDummyDescriptors(GlobalPools& globalPools, uint64_t dummyDescWriteMask)
	{
		uint64_t bindingsWrittenTo = 0u;

		auto& blankBuffer = AllocateInfo(
			VkDescriptorBufferInfo { 
				globalPools._dummyResources._blankBuffer.GetUnderlying(),
				0, VK_WHOLE_SIZE });
		auto& blankImage = AllocateInfo(
			VkDescriptorImageInfo {
				globalPools._dummyResources._blankSampler->GetUnderlying(),
				globalPools._dummyResources._blankSrv.GetImageView(),
				VK_IMAGE_LAYOUT_GENERAL });
		auto& blankSampler = AllocateInfo(
			VkDescriptorImageInfo {
				globalPools._dummyResources._blankSampler->GetUnderlying(),
				nullptr,
				VK_IMAGE_LAYOUT_UNDEFINED });
		auto& blankStorageImage = AllocateInfo(
			VkDescriptorImageInfo {
				globalPools._dummyResources._blankSampler->GetUnderlying(),
				globalPools._dummyResources._blankUavImage.GetImageView(),
				VK_IMAGE_LAYOUT_GENERAL });
		auto& blankStorageBuffer = AllocateInfo(
			VkDescriptorBufferInfo {
				globalPools._dummyResources._blankUavBuffer.GetResource()->GetBuffer(),
				0, VK_WHOLE_SIZE });

		unsigned minBit = xl_ctz8(dummyDescWriteMask);
		unsigned maxBit = std::min(64u - xl_clz8(dummyDescWriteMask), (unsigned)_signature.size()-1);

		for (unsigned bIndex=minBit; bIndex<=maxBit; ++bIndex) {
			if (!(dummyDescWriteMask & (1ull<<uint64(bIndex)))) continue;

			auto b = _signature[bIndex]._type;
			assert(_signature[bIndex]._count == 1);
			if (b == DescriptorType::ConstantBuffer) {
				WriteBinding(
					bIndex,
					AsVkDescriptorType(b),
					blankBuffer, false
					VULKAN_VERBOSE_DEBUG_ONLY(, s_dummyDescriptorString));
			} else if (b == DescriptorType::Texture) {
				WriteBinding(
					bIndex,
					AsVkDescriptorType(b),
					blankImage, false
					VULKAN_VERBOSE_DEBUG_ONLY(, s_dummyDescriptorString));
			} else if (b == DescriptorType::Sampler) {
				WriteBinding(
					bIndex,
					AsVkDescriptorType(b),
					blankSampler, false
					VULKAN_VERBOSE_DEBUG_ONLY(, s_dummyDescriptorString));
			} else if (b == DescriptorType::UnorderedAccessTexture) {
				WriteBinding(
					bIndex,
					AsVkDescriptorType(b),
					blankStorageImage, false
					VULKAN_VERBOSE_DEBUG_ONLY(, s_dummyDescriptorString));
			} else if (b == DescriptorType::UnorderedAccessBuffer) {
				WriteBinding(
					bIndex,
					AsVkDescriptorType(b),
					blankStorageBuffer, false
					VULKAN_VERBOSE_DEBUG_ONLY(, s_dummyDescriptorString));
			} else {
				assert(0);
				continue;
			}
			bindingsWrittenTo |= 1ull << uint64(bIndex);
		}

		return bindingsWrittenTo;
	}

	uint64_t	ProgressiveDescriptorSetBuilder::FlushChanges(
		VkDevice device,
		VkDescriptorSet destination,
		VkDescriptorSet copyPrevDescriptors, uint64_t prevDescriptorMask
		VULKAN_VERBOSE_DEBUG_ONLY(, DescriptorSetDebugInfo& description))
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

		#if defined(VULKAN_VERBOSE_DEBUG)
			if (description._bindingDescriptions.size() < _verboseDescription._bindingDescriptions.size())
				description._bindingDescriptions.resize(_verboseDescription._bindingDescriptions.size());
			for (unsigned b=0; b<_verboseDescription._bindingDescriptions.size(); ++b) {
				if (!(result && 1ull<<uint64_t(b))) continue;
				description._bindingDescriptions[b] = _verboseDescription._bindingDescriptions[b];
			}
		#endif

		return result;
	}

	bool    ProgressiveDescriptorSetBuilder::HasChanges() const
	{
		// note --  we have to bind some descriptor set for the first draw of the frame,
		//          even if nothing has been bound! So, when _activeDescSets is empty
		//          we must return true here.
		return _sinceLastFlush != 0;
	}

	void    ProgressiveDescriptorSetBuilder::Reset()
	{
		_pendingWrites = 0u;
		_pendingImageInfos = 0u;
		_pendingBufferInfos = 0u;

		XlZeroMemory(_bufferInfo);
		XlZeroMemory(_imageInfo);
		XlZeroMemory(_writes);

		_sinceLastFlush = 0x0u;
	}

	ProgressiveDescriptorSetBuilder::ProgressiveDescriptorSetBuilder(
		IteratorRange<const DescriptorSlot*> signature)
	: _signature(signature.begin(), signature.end())
	{
		_sinceLastFlush = 0;
		XlZeroMemory(_bufferInfo);
		XlZeroMemory(_imageInfo);
		XlZeroMemory(_writes);
	}

	ProgressiveDescriptorSetBuilder::~ProgressiveDescriptorSetBuilder()
	{
	}

	ProgressiveDescriptorSetBuilder::ProgressiveDescriptorSetBuilder(ProgressiveDescriptorSetBuilder&& moveFrom)
	: _signature(std::move(moveFrom._signature))
	#if defined(VULKAN_VERBOSE_DEBUG)
		, _verboseDescription(std::move(moveFrom._verboseDescription))
	#endif
	{
		_pendingWrites = moveFrom._pendingWrites; moveFrom._pendingWrites = 0;
        _pendingImageInfos = moveFrom._pendingImageInfos; moveFrom._pendingImageInfos = 0;
        _pendingBufferInfos = moveFrom._pendingBufferInfos; moveFrom._pendingBufferInfos = 0;
        _sinceLastFlush = moveFrom._sinceLastFlush; moveFrom._sinceLastFlush = 0;

		std::memcpy(_bufferInfo, moveFrom._bufferInfo, sizeof(_bufferInfo));
		std::memcpy(_imageInfo, moveFrom._imageInfo, sizeof(_imageInfo));
		std::memcpy(_writes, moveFrom._writes, sizeof(_writes));
		XlZeroMemory(moveFrom._bufferInfo);
		XlZeroMemory(moveFrom._imageInfo);
		XlZeroMemory(moveFrom._writes);
	}

	ProgressiveDescriptorSetBuilder& ProgressiveDescriptorSetBuilder::operator=(ProgressiveDescriptorSetBuilder&& moveFrom)
	{
		_signature = std::move(moveFrom._signature);
		#if defined(VULKAN_VERBOSE_DEBUG)
			_verboseDescription = std::move(moveFrom._verboseDescription);
		#endif

		_pendingWrites = moveFrom._pendingWrites; moveFrom._pendingWrites = 0;
        _pendingImageInfos = moveFrom._pendingImageInfos; moveFrom._pendingImageInfos = 0;
        _pendingBufferInfos = moveFrom._pendingBufferInfos; moveFrom._pendingBufferInfos = 0;
        _sinceLastFlush = moveFrom._sinceLastFlush; moveFrom._sinceLastFlush = 0;

		std::memcpy(_bufferInfo, moveFrom._bufferInfo, sizeof(_bufferInfo));
		std::memcpy(_imageInfo, moveFrom._imageInfo, sizeof(_imageInfo));
		std::memcpy(_writes, moveFrom._writes, sizeof(_writes));
		XlZeroMemory(moveFrom._bufferInfo);
		XlZeroMemory(moveFrom._imageInfo);
		XlZeroMemory(moveFrom._writes);
		return *this;
	}

	#if defined(VULKAN_VERBOSE_DEBUG)

		static const std::string s_columnHeader0 = "Root Signature";
		static const std::string s_columnHeader2 = "Binding";
		static const std::string s_columnHeader3 = "Legacy Binding";

		std::ostream& WriteDescriptorSet(
			std::ostream&& stream,
			const DescriptorSetDebugInfo& bindingDescription,
			IteratorRange<const DescriptorSlot*> signature,
			const std::string& descriptorSetName,
			const LegacyRegisterBinding& legacyBinding,
			IteratorRange<const CompiledShaderByteCode**> compiledShaderByteCode,
			unsigned descriptorSetIndex, bool isBound)
		{
			std::vector<std::string> signatureColumn;
			std::vector<std::string> shaderColumns[(unsigned)ShaderStage::Max];
			std::vector<std::string> legacyBindingColumn;

			size_t signatureColumnMax = 0, bindingColumnMax = 0, legacyBindingColumnMax = 0;
			size_t shaderColumnMax[(unsigned)ShaderStage::Max] = {};

			signatureColumn.resize(signature.size());
			for (unsigned c=0; c<signature.size(); ++c) {
				signatureColumn[c] = std::string{AsString(signature[c]._type)};
				signatureColumnMax = std::max(signatureColumnMax, signatureColumn[c].size());
			}
			signatureColumnMax = std::max(signatureColumnMax, s_columnHeader0.size());

			for (unsigned stage=0; stage<std::min((unsigned)ShaderStage::Max, (unsigned)compiledShaderByteCode.size()); ++stage) {
				if (!compiledShaderByteCode[stage] || compiledShaderByteCode[stage]->GetByteCode().empty())
					continue;

				shaderColumns[stage].reserve(signature.size());
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

			stream << "[" << descriptorSetIndex << "] Descriptor Set: " << descriptorSetName;
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////

	CompiledDescriptorSetLayout::CompiledDescriptorSetLayout(
		const ObjectFactory& factory, 
		IteratorRange<const DescriptorSlot*> srcLayout,
		VkShaderStageFlags stageFlags)
	: _shaderStageFlags(stageFlags)
	, _descriptorSlots(srcLayout.begin(), srcLayout.end())
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings;
		bindings.reserve(srcLayout.size());
		for (unsigned bIndex=0; bIndex<(unsigned)srcLayout.size(); ++bIndex) {
			VkDescriptorSetLayoutBinding dstBinding = {};
			dstBinding.binding = bIndex;
			dstBinding.descriptorType = (VkDescriptorType)AsVkDescriptorType(srcLayout[bIndex]._type);
			dstBinding.descriptorCount = srcLayout[bIndex]._count;
			dstBinding.stageFlags = stageFlags;
			dstBinding.pImmutableSamplers = nullptr;
			bindings.push_back(dstBinding);
		}
		_layout = factory.CreateDescriptorSetLayout(MakeIteratorRange(bindings));
	}

	CompiledDescriptorSetLayout::~CompiledDescriptorSetLayout()
	{
	}

	/*VkShaderStageFlags shaderStageFlags = 0;
		if (pipelineType == PipelineType::Compute) {
			shaderStageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		} else {
			assert(pipelineType == PipelineType::Graphics);
			shaderStageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
		}*/

	CompiledDescriptorSet::CompiledDescriptorSet(
		ObjectFactory& factory,
		GlobalPools& globalPools,
		const std::shared_ptr<CompiledDescriptorSetLayout>& layout,
		VkShaderStageFlags shaderStageFlags,
		IteratorRange<const BindTypeAndIdx*> binds,
		const UniformsStream& uniforms)
	: _layout(layout)
	{
		_underlying = globalPools._longTermDescriptorPool.Allocate(GetUnderlyingLayout());

		ProgressiveDescriptorSetBuilder builder { _layout->GetDescriptorSlots() };
		for (unsigned c=0; c<binds.size(); ++c) {
			if (binds[c]._type == BindType::BufferView) {
				builder.Bind(c, uniforms._constantBuffers[binds[c]._idx]);
			} else if (binds[c]._type == BindType::TextureView) {
				builder.Bind(c, *(TextureView*)uniforms._resources[binds[c]._idx]);
			} else {
				assert(binds[c]._type == BindType::Sampler);
				builder.Bind(c, ((SamplerState*)uniforms._samplers[binds[c]._idx])->GetUnderlying());
			}
		}

		uint64_t writtenMask = uint64_t(binds.size())-1;
		uint64_t signatureMask = uint64_t(layout->GetDescriptorSlots().size())-1;
		builder.BindDummyDescriptors(globalPools, signatureMask & ~writtenMask);

		builder.FlushChanges(
			factory.GetDevice().get(),
			_underlying.get(),
			nullptr, 0
			VULKAN_VERBOSE_DEBUG_ONLY(, _description));
	}

	CompiledDescriptorSet::~CompiledDescriptorSet()
	{}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

	VkDescriptorType_ AsVkDescriptorType(DescriptorType type)
	{
		switch (type) {
		case DescriptorType::Sampler:					return VK_DESCRIPTOR_TYPE_SAMPLER;
		case DescriptorType::Texture:					return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		case DescriptorType::ConstantBuffer:			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		case DescriptorType::UnorderedAccessTexture:	return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		case DescriptorType::UnorderedAccessBuffer:		return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		default:										return VK_DESCRIPTOR_TYPE_SAMPLER;
		}
	}
}}

