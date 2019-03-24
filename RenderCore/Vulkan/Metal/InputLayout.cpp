// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "InputLayout.h"
#include "ShaderReflection.h"
#include "Shader.h"
#include "Format.h"
#include "PipelineLayout.h"
#include "DeviceContext.h"
#include "Pools.h"
#include "../../Format.h"
#include "../../Types.h"
#include "../../BufferView.h"
#include "../../UniformsStream.h"
#include "../../../ConsoleRig/Log.h"
#include "../../../Utility/MemoryUtils.h"
#include "../../../Utility/ArithmeticUtils.h"
#include "../../../Utility/StringFormat.h"
#include <sstream>

#include "IncludeVulkan.h"

namespace RenderCore { namespace Metal_Vulkan
{
	void BoundInputLayout::Apply(DeviceContext& context, IteratorRange<const VertexBufferView*> vertexBuffers) const never_throws
	{
        VkBuffer buffers[s_maxBoundVBs];
		VkDeviceSize offsets[s_maxBoundVBs];
		auto count = (unsigned)std::min(std::min(vertexBuffers.size(), dimof(buffers)), _vbBindingDescriptions.size());
		for (unsigned c=0; c<count; ++c) {
			offsets[c] = vertexBuffers[c]._offset;
			assert(const_cast<IResource*>(vertexBuffers[c]._resource)->QueryInterface(typeid(Resource).hash_code()));
			buffers[c] = ((Resource*)vertexBuffers[c]._resource)->GetBuffer();
		}
        context.GetActiveCommandList().BindVertexBuffers(0, count, buffers, offsets);
		context.SetBoundInputLayout(*this);
	}

    BoundInputLayout::BoundInputLayout(IteratorRange<const InputElementDesc*> layout, const CompiledShaderByteCode& shader)
    {
        // find the vertex inputs into the shader, and match them against the input layout
        unsigned trackingOffset = 0;

        SPIRVReflection reflection(shader.GetByteCode());
        _attributes.reserve(layout.size());
        for (unsigned c=0; c<layout.size(); ++c) {
            const auto& e = layout[c];
            auto hash = Hash64(e._semanticName, DefaultSeed64 + e._semanticIndex);

            auto offset = e._alignedByteOffset == ~0x0u ? trackingOffset : e._alignedByteOffset;
            trackingOffset = offset + BitsPerPixel(e._nativeFormat) / 8;

            auto i = LowerBound(reflection._inputInterfaceQuickLookup, hash);
            if (i == reflection._inputInterfaceQuickLookup.end() || i->first != hash)
                continue;   // Could not be bound

            VkVertexInputAttributeDescription desc;
            desc.location = i->second._location;
            desc.binding = e._inputSlot;
            desc.format = AsVkFormat(e._nativeFormat);
            desc.offset = offset;
            _attributes.push_back(desc);
        }

		auto vertexStrides = CalculateVertexStrides(layout);
		_vbBindingDescriptions.reserve(vertexStrides.size());
		for (unsigned b=0; b<(unsigned)vertexStrides.size(); ++b)
			_vbBindingDescriptions.push_back({b, vertexStrides[b], VK_VERTEX_INPUT_RATE_VERTEX});

        // todo -- check if any input slots are not bound to anything
    }

	BoundInputLayout::BoundInputLayout(IteratorRange<const InputElementDesc*> layout, const ShaderProgram& shader)
    : BoundInputLayout(layout, shader.GetCompiledCode(ShaderStage::Vertex))
    {
    }

	BoundInputLayout::BoundInputLayout(
        IteratorRange<const SlotBinding*> layouts,
        const CompiledShaderByteCode& shader)
	{
		SPIRVReflection reflection(shader.GetByteCode());
		_vbBindingDescriptions.reserve(layouts.size());

		for (unsigned slot=0; slot<layouts.size(); ++slot) {
			bool boundAtLeastOne = false;
			UINT accumulatingOffset = 0;
			for (unsigned ei=0; ei<layouts[slot]._elements.size(); ++ei) {
				const auto& e = layouts[slot]._elements[ei];
				auto hash = e._semanticHash;

				auto i = LowerBound(reflection._inputInterfaceQuickLookup, hash);
				if (i == reflection._inputInterfaceQuickLookup.end() || i->first != hash) {
					accumulatingOffset += BitsPerPixel(e._nativeFormat) / 8;
					continue;
				}

				VkVertexInputAttributeDescription desc;
				desc.location = i->second._location;
				desc.binding = slot;
				desc.format = AsVkFormat(e._nativeFormat);
				desc.offset = accumulatingOffset;
				_attributes.push_back(desc);

				accumulatingOffset += BitsPerPixel(e._nativeFormat) / 8;
				boundAtLeastOne = true;
			}

			if (boundAtLeastOne)
				_vbBindingDescriptions.push_back({slot, accumulatingOffset, VK_VERTEX_INPUT_RATE_VERTEX});
		}
	}

	BoundInputLayout::BoundInputLayout(
        IteratorRange<const SlotBinding*> layouts,
        const ShaderProgram& shader)
	: BoundInputLayout(layouts, shader.GetCompiledCode(ShaderStage::Vertex))
	{
	}

    BoundInputLayout::BoundInputLayout() {}
    BoundInputLayout::~BoundInputLayout() {}

        ////////////////////////////////////////////////////////////////////////////////////////////////

	class BoundUniformsHelper
     {
     public:
        BoundUniformsHelper(const ShaderProgram& shader);
		BoundUniformsHelper(const CompiledShaderByteCode& shader);

		bool BindConstantBuffer(uint64 hashName, unsigned uniformsStream, unsigned slot);
		bool BindShaderResource(uint64 hashName, unsigned uniformsStream, unsigned slot);

		SPIRVReflection			_reflection[(unsigned)ShaderStage::Max];

        static const unsigned s_streamCount = 4;
        std::vector<uint32_t>	_cbBindingIndices[s_streamCount];
        std::vector<uint32_t>	_srvBindingIndices[s_streamCount];
		unsigned				_vsPushConstantSlot[s_streamCount];
		unsigned				_psPushConstantSlot[s_streamCount];
		bool					_isComputeShader;

        uint64_t				BuildShaderBindingMask(unsigned descriptorSet);
     };

    BoundUniformsHelper::BoundUniformsHelper(const ShaderProgram& shader)
    {
        _isComputeShader = false;
		for (unsigned c=0; c<(unsigned)ShaderProgram::s_maxShaderStages; ++c) {
			const auto& compiledCode = shader.GetCompiledCode((ShaderStage)c);
			if (compiledCode.GetByteCode().size())
				_reflection[c] = SPIRVReflection(compiledCode.GetByteCode());
		}
		for (unsigned c=0; c<s_streamCount; c++)
			_vsPushConstantSlot[c] = _psPushConstantSlot[c] = ~0u;
    }

	BoundUniformsHelper::BoundUniformsHelper(const CompiledShaderByteCode& shader)
    {
        auto stage = shader.GetStage();
        if ((unsigned)stage < dimof(_reflection)) {
            _reflection[(unsigned)stage] = SPIRVReflection(shader.GetByteCode());
        }
        _isComputeShader = stage == ShaderStage::Compute;
		for (unsigned c=0; c<s_streamCount; c++)
			_vsPushConstantSlot[c] = _psPushConstantSlot[c] = ~0u;
    }

    uint64_t BoundUniformsHelper::BuildShaderBindingMask(unsigned descriptorSet)
    {
        uint64_t shaderBindingMask = 0x0ull;
        for (unsigned r=0; r<(unsigned)ShaderStage::Max; ++r) {
            // Look for all of the bindings in this descriptor set that are referenced by the shader
            for(const auto&b:_reflection[r]._bindings) {
                if (b.second._descriptorSet == descriptorSet && b.second._bindingPoint != ~0x0u)
                    shaderBindingMask |= 1ull << uint64(b.second._bindingPoint);
            }
        }
		return shaderBindingMask;
    }

    bool BoundUniformsHelper::BindConstantBuffer(uint64 hashName, unsigned stream, unsigned slot)
    {
        // assert(!_pipelineLayout);
        bool gotBinding = false;

        for (unsigned s=0; s<dimof(_reflection); ++s) {
            auto i = LowerBound(_reflection[s]._uniformQuickLookup, hashName);
            if (i == _reflection[s]._uniformQuickLookup.end() || i->first != hashName) {
                // Could not match. This happens sometimes in normal usage.
                // It just means the provided interface is a little too broad for this shader
                continue;
            }

            if (i->second._descriptorSet != stream) {
                Log(Warning) << "Constant buffer binding appears to be in the wrong descriptor set." << std::endl;
                continue;
            }

            // Sometimes the binding isn't set. This occurs if constant buffer is actually push constants
            // In this case, we can't bind like this -- we need to use the push constants interface.
            if (i->second._bindingPoint == ~0x0u)
                continue;

            if (_cbBindingIndices[stream].size() <= slot) _cbBindingIndices[stream].resize(slot+1, ~0u);

            auto descSetBindingPoint = i->second._bindingPoint;
            assert(_cbBindingIndices[stream][slot] == ~0u || _cbBindingIndices[stream][slot] == descSetBindingPoint);
            _cbBindingIndices[stream][slot] = descSetBindingPoint;

            gotBinding = true;
        }

		{
			auto i = LowerBound(_reflection[(unsigned)ShaderStage::Vertex]._pushConstantsQuickLookup, hashName);
			if (i != _reflection[(unsigned)ShaderStage::Vertex]._pushConstantsQuickLookup.end() && i->first == hashName) {
				if (_vsPushConstantSlot[stream] == ~0u) {
					_vsPushConstantSlot[stream] = slot;
					gotBinding = true;
				} else {
					// We can't support multiple separate constant buffers as push constants in the same shader, because the SPIRV
					// reflection code doesn't extract offset information for subsequent buffers
					Log(Warning) << "Got mutltiple constant buffers assigned as push constants in the same shader." << std::endl;
				}
			}

			i = LowerBound(_reflection[(unsigned)ShaderStage::Pixel]._pushConstantsQuickLookup, hashName);
			if (i != _reflection[(unsigned)ShaderStage::Pixel]._pushConstantsQuickLookup.end() && i->first == hashName) {
				if (_psPushConstantSlot[stream] == ~0u) {
					_psPushConstantSlot[stream] = slot;
					gotBinding = true;
				} else {
					// We can't support multiple separate constant buffers as push constants in the same shader, because the SPIRV
					// reflection code doesn't extract offset information for subsequent buffers
					Log(Warning) << "Got mutltiple constant buffers assigned as push constants in the same shader." << std::endl;
				}
			}
		}

        return gotBinding;
    }

    bool BoundUniformsHelper::BindShaderResource(uint64 hashName, unsigned stream, unsigned slot)
    {
        // assert(!_pipelineLayout);
        bool gotBinding = false;

        for (unsigned s=0; s<dimof(_reflection); ++s) {
            auto i = LowerBound(_reflection[s]._uniformQuickLookup, hashName);
            if (i == _reflection[s]._uniformQuickLookup.end() || i->first != hashName) {
                // Could not match. This happens sometimes in normal usage.
                // It just means the provided interface is a little too broad for this shader
                continue;
            }

            if (i->second._descriptorSet != stream) {
                Log(Warning) << "Shader resource binding appears to be in the wrong descriptor set." << std::endl;
                continue;
            }

            if (_srvBindingIndices[stream].size() <= slot) _srvBindingIndices[stream].resize(slot+1, ~0u);

            assert(i->second._bindingPoint != ~0x0u);
            auto descSetBindingPoint = i->second._bindingPoint;
            assert(_srvBindingIndices[stream][slot] == ~0u || _srvBindingIndices[stream][slot] == descSetBindingPoint);
            _srvBindingIndices[stream][slot] = descSetBindingPoint;
            gotBinding = true;
        }

        return gotBinding;
    }

	void BoundUniforms::UnbindShaderResources(DeviceContext& context, unsigned streamIdx)
	{
		assert(0);		// todo -- unimplemented
	}

	BoundUniforms::BoundUniforms(
        const ShaderProgram& shader,
        const PipelineLayoutConfig& pipelineLayout,
        const UniformsStreamInterface& interface0,
        const UniformsStreamInterface& interface1,
        const UniformsStreamInterface& interface2,
        const UniformsStreamInterface& interface3)
	{
		for (unsigned c=0; c<s_streamCount; c++) {
			_descriptorSetBindingMask[c] = 0;
			_vsPushConstantSlot[c] = _psPushConstantSlot[c] = ~0u;
		}

		BoundUniformsHelper helper(shader);
		const UniformsStreamInterface* interfaces[] = { &interface0, &interface1, &interface2, &interface3 };
		SetupBindings(helper, interfaces, dimof(interfaces));
		assert(!helper._isComputeShader);
		_isComputeShader = false;
	}

	BoundUniforms::BoundUniforms(
        const ComputeShader& shader,
        const PipelineLayoutConfig& pipelineLayout,
        const UniformsStreamInterface& interface0,
        const UniformsStreamInterface& interface1,
        const UniformsStreamInterface& interface2,
        const UniformsStreamInterface& interface3)
	{
		for (unsigned c=0; c<s_streamCount; c++) {
			_descriptorSetBindingMask[c] = 0;
			_vsPushConstantSlot[c] = _psPushConstantSlot[c] = ~0u;
		}

		BoundUniformsHelper helper(shader.GetCompiledShaderByteCode());
		const UniformsStreamInterface* interfaces[] = { &interface0, &interface1, &interface2, &interface3 };
		SetupBindings(helper, interfaces, dimof(interfaces));
		assert(helper._isComputeShader);
		_isComputeShader = true;
	}

	void BoundUniforms::SetupBindings(
		BoundUniformsHelper& helper,
		const UniformsStreamInterface* interfaces[],
		size_t interfaceCount)
	{
		for (unsigned stream=0; stream<interfaceCount; ++stream) {
			_boundUniformBufferSlots[stream] = 0;
			_boundResourceSlots[stream] = 0;
			for (unsigned slot=0; slot<interfaces[stream]->_cbBindings.size(); ++slot) {
				bool bindSuccess = helper.BindConstantBuffer(interfaces[stream]->_cbBindings[slot]._hashName, stream, slot);
				if (bindSuccess)
					_boundUniformBufferSlots[stream] |= 1ull<<uint64_t(slot);
			}
			for (unsigned slot=0; slot<interfaces[stream]->_srvBindings.size(); ++slot) {
				bool bindSuccess = helper.BindShaderResource(interfaces[stream]->_srvBindings[slot], stream, slot);
				if (bindSuccess)
					_boundResourceSlots[stream] |= 1ull<<uint64_t(slot);
			}
			_cbBindingIndices[stream] = std::move(helper._cbBindingIndices[stream]);
			_srvBindingIndices[stream] = std::move(helper._srvBindingIndices[stream]);
			_descriptorSetBindingMask[stream] = helper.BuildShaderBindingMask(stream);
			_vsPushConstantSlot[stream] = helper._vsPushConstantSlot[stream];
			_psPushConstantSlot[stream] = helper._psPushConstantSlot[stream];
		}

		for (unsigned c=(unsigned)interfaceCount; c<s_streamCount; ++c)
			_descriptorSetBindingMask[c] = 0;

		#if defined(_DEBUG)
			{
				std::stringstream str;
				for (unsigned stream=0; stream<interfaceCount; ++stream) {
					str << "---------- Stream [" << stream << "] ----------" << std::endl;
					const auto& interf = *interfaces[stream];
					for (unsigned slot=0; slot<interf._cbBindings.size(); ++slot) {
						if (slot < _cbBindingIndices[stream].size() && _cbBindingIndices[stream][slot] != ~0u) {
							auto bindingPoint = _cbBindingIndices[stream][slot];
							str << "[" << slot << "] binding point: " << bindingPoint;
							for (unsigned stage=0; stage<(unsigned)dimof(helper._reflection); ++stage) {
								auto& refl = helper._reflection[stage];
								auto b = std::find_if(
									refl._bindings.begin(), refl._bindings.end(),
									[bindingPoint, stream](const std::pair<SPIRVReflection::ObjectId, SPIRVReflection::Binding>& t) {
										return t.second._bindingPoint == bindingPoint && t.second._descriptorSet == stream;
									});
								if (b != refl._bindings.end()) {
									str << " (Stage: " << AsString((ShaderStage)stage) << ", variable: ";
									auto n = LowerBound(refl._names, b->first);
									if (n != refl._names.end() && n->first == b->first) {
										str << n->second << ")";
									} else {
										str << "<<unnamed>>)";
									}
								}
							}
							str << std::endl;
						}
						if (_vsPushConstantSlot[stream] == slot)
							str << "[" << slot << "] Vertex push constants " << std::endl;
						if (_psPushConstantSlot[stream] == slot)
							str << "[" << slot << "] Pixel push constants " << std::endl;
					}
				}

				str << std::endl;
				for (unsigned stage=0; stage<(unsigned)dimof(helper._reflection); ++stage) {
					if (helper._reflection[stage]._entryPoint._name.IsEmpty()) continue;
					str << "---------- Reflection [" << AsString((ShaderStage)stage) << "] ----------" << std::endl;
					str << helper._reflection[stage];
				}

				_debuggingDescription = str.str();
			}
		#endif
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	struct NascentDescriptorWrite
	{
	public:
		static const unsigned s_maxBindings = 64u;
        VkDescriptorBufferInfo	_bufferInfo[s_maxBindings];
        VkDescriptorImageInfo	_imageInfo[s_maxBindings];
        VkWriteDescriptorSet	_writes[s_maxBindings];

        unsigned _writeCount = 0, _bufferCount = 0, _imageCount = 0;
		bool _requiresTemporaryBufferBarrier = false;
	};

	static const std::string s_dummyDescriptorString{"<DummyDescriptor>"};

	static uint64_t WriteCBBindings(
		NascentDescriptorWrite& result,
		TemporaryBufferSpace& temporaryBufferSpace,
		ObjectFactory& factory,
		DescriptorSet& dstSet,
		IteratorRange<const ConstantBufferView*> cbvs,
		IteratorRange<const uint32_t*> bindingIndicies,
		uint64_t shaderBindingMask)
	{
		auto writeCount = result._writeCount;
		auto bufferCount = result._bufferCount;
		auto bindingsWrittenTo = 0u;

		auto count = std::min(cbvs.size(), bindingIndicies.size());
		for (unsigned c=0; c<count; ++c) {
			auto dstBinding = bindingIndicies[c];
            if (dstBinding == ~0u) continue;

			assert(shaderBindingMask & (1ull << uint64_t(dstBinding)));

			#if defined(_DEBUG) // check for duplicate descriptor writes (ie, writing to the same binding twice as part of the same operation)
				for (unsigned w=0; w<writeCount; ++w)
					assert( result._writes[w].dstBinding != dstBinding
						||  result._writes[w].dstSet != dstSet._underlying.get());
			#endif

			assert(writeCount < dimof(result._writes));
			auto& write = result._writes[writeCount++];
			write = {};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.dstSet = dstSet._underlying.get();
			write.dstBinding = dstBinding;
			write.descriptorCount = 1;
			write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			write.dstArrayElement = 0;

			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				if (dstSet._description._bindingDescriptions.size() <= (write.dstBinding + write.descriptorCount))
					dstSet._description._bindingDescriptions.resize(write.dstBinding + write.descriptorCount);
				for (unsigned q=0; q<write.descriptorCount; ++q)
					dstSet._description._bindingDescriptions[write.dstBinding+q]._descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			#endif

			if (cbvs[c]._prebuiltBuffer) {
				assert(const_cast<IResource*>(cbvs[c]._prebuiltBuffer)->QueryInterface(typeid(Resource).hash_code()));
				auto& res = *(Resource*)cbvs[c]._prebuiltBuffer;
				assert(res.GetBuffer());
				result._bufferInfo[bufferCount] = VkDescriptorBufferInfo{res.GetBuffer(), 0, VK_WHOLE_SIZE};
				write.pBufferInfo = &result._bufferInfo[bufferCount];
				++bufferCount;

				#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
					for (unsigned q=0; q<write.descriptorCount; ++q)
						dstSet._description._bindingDescriptions[write.dstBinding+q]._description = std::string{"Prebuild CB: "} + res.GetDesc()._name;
				#endif
			} else {
				auto& pkt = cbvs[c]._packet;
				assert(bufferCount < dimof(result._bufferInfo));
				// We must either allocate some memory from a temporary pool, or 
				// (or we could use push constants)
				result._bufferInfo[bufferCount] = temporaryBufferSpace.AllocateBuffer(pkt.AsIteratorRange());
				if (!result._bufferInfo[bufferCount].buffer) {
					Log(Warning) << "Failed to allocate temporary buffer space. Falling back to new buffer." << std::endl;
					auto cb = MakeConstantBuffer(factory, pkt.AsIteratorRange());
					result._bufferInfo[bufferCount] = VkDescriptorBufferInfo{ cb.GetUnderlying(), 0, VK_WHOLE_SIZE };
				} else {
					result._requiresTemporaryBufferBarrier |= true;
				}
				write.pBufferInfo = &result._bufferInfo[bufferCount];
				++bufferCount;

				#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
					for (unsigned q=0; q<write.descriptorCount; ++q)
						dstSet._description._bindingDescriptions[write.dstBinding+q]._description = std::string{"CB pkt slot: "} + std::to_string(c);
				#endif
			}

			bindingsWrittenTo |= 1ull << uint64(dstBinding);
		}

		result._writeCount = writeCount;
		result._bufferCount = bufferCount;
		return bindingsWrittenTo;
	}

	static uint64_t WriteSRVBindings(
		NascentDescriptorWrite& result,
		GlobalPools& globalPools,
		DescriptorSet& dstSet,
		IteratorRange<const ShaderResourceView*const*> srvs,
		IteratorRange<const uint32_t*> bindingIndicies,
		uint64_t shaderBindingMask)
	{
		auto writeCount = result._writeCount;
		auto imageCount = result._imageCount;
		auto bufferCount = result._bufferCount;
		auto bindingsWrittenTo = 0u;

		unsigned dummyImage = ~0u;

		auto count = std::min(srvs.size(), bindingIndicies.size());
		for (unsigned c=0; c<count; ++c) {
			auto dstBinding = bindingIndicies[c];
            if (dstBinding == ~0u) continue;

			assert(shaderBindingMask & (1ull << uint64_t(dstBinding)));

			#if defined(_DEBUG) // check for duplicate descriptor writes
                for (unsigned w=0; w<writeCount; ++w)
                    assert( result._writes[w].dstBinding != dstBinding
						||  result._writes[w].dstSet != dstSet._underlying.get());
			#endif

			assert(writeCount < dimof(result._writes));
			auto& write = result._writes[writeCount++];
			write = {};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = dstSet._underlying.get();
            write.dstBinding = dstBinding;
            write.descriptorCount = 1;
            write.dstArrayElement = 0;

			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				if (dstSet._description._bindingDescriptions.size() <= (write.dstBinding + write.descriptorCount))
					dstSet._description._bindingDescriptions.resize(write.dstBinding + write.descriptorCount);
			#endif

			// Our "StructuredBuffer" objects are being mapped onto uniform buffers in SPIR-V
			// So sometimes a SRV will end up writing to a VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
			// descriptor.
			bool wroteSomething = false;
			if (expect(srvs[c], 1)) {
				if (srvs[c]->GetImageView()) {
					assert(imageCount < dimof(result._imageInfo));
					result._imageInfo[imageCount] = VkDescriptorImageInfo {
						globalPools._dummyResources._blankSampler->GetUnderlying(),
						srvs[c]->GetImageView(),
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
					write.pImageInfo = &result._imageInfo[imageCount];
					write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
					++imageCount;
					wroteSomething = true;
				} else if (srvs[c]->GetResource()) {
					auto buffer = srvs[c]->GetResource()->GetBuffer();
					assert(bufferCount < dimof(result._bufferInfo));
					result._bufferInfo[bufferCount] = VkDescriptorBufferInfo{buffer, 0, VK_WHOLE_SIZE};
					write.pBufferInfo = &result._bufferInfo[bufferCount];
					write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					++bufferCount;
					wroteSomething = true;
				}

				#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
					if (wroteSomething)
						for (unsigned q=0; q<write.descriptorCount; ++q) {
							dstSet._description._bindingDescriptions[write.dstBinding+q]._descriptorType = write.descriptorType;
							dstSet._description._bindingDescriptions[write.dstBinding+q]._description = std::string{"SRV resource: "} + srvs[c]->GetResource()->GetDesc()._name;
						}
				#endif
			}

			if (!wroteSomething) {
				// given a null pointer -- have to substitute a dummy image
				assert(imageCount < dimof(result._imageInfo));
				if (dummyImage == ~0u) {
					result._imageInfo[imageCount] = VkDescriptorImageInfo {
						globalPools._dummyResources._blankSampler->GetUnderlying(),
						globalPools._dummyResources._blankSrv.GetImageView(),
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
					dummyImage = imageCount;
					++imageCount;
				}
				write.pImageInfo = &result._imageInfo[dummyImage];
				write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

				#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
					for (unsigned q=0; q<write.descriptorCount; ++q) {
						dstSet._description._bindingDescriptions[write.dstBinding+q]._descriptorType = write.descriptorType;
						dstSet._description._bindingDescriptions[write.dstBinding+q]._description = s_dummyDescriptorString;
					}
				#endif
			}

			bindingsWrittenTo |= 1ull << uint64(dstBinding);
		}

		result._writeCount = writeCount;
		result._imageCount = imageCount;
		result._bufferCount = bufferCount;
		return bindingsWrittenTo;
	}

	static uint64_t WriteDummyDescriptors(
		NascentDescriptorWrite& result,
		GlobalPools& globalPools,
		DescriptorSet& dstSet,
		const DescriptorSetSignature& sig,
		uint64_t dummyDescWriteMask)
	{
		auto bindingsWrittenTo = 0u;

		assert(result._bufferCount < dimof(result._bufferInfo));
        assert(result._imageCount < dimof(result._imageInfo));
        auto blankBuffer = result._bufferCount;
        auto blankImage = result._imageCount;
        result._bufferInfo[result._bufferCount++] = VkDescriptorBufferInfo { 
            globalPools._dummyResources._blankBuffer.GetUnderlying(),
            0, VK_WHOLE_SIZE };
        result._imageInfo[result._imageCount++] = VkDescriptorImageInfo {
            globalPools._dummyResources._blankSampler->GetUnderlying(),
            globalPools._dummyResources._blankSrv.GetImageView(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		auto blankSampler = result._imageCount;
		result._imageInfo[result._imageCount++] = VkDescriptorImageInfo {
            globalPools._dummyResources._blankSampler->GetUnderlying(),
            nullptr,
            VK_IMAGE_LAYOUT_UNDEFINED };

        unsigned minBit = xl_ctz8(dummyDescWriteMask);
        unsigned maxBit = std::min(64u - xl_clz8(dummyDescWriteMask), (unsigned)sig._bindings.size()-1);

        for (unsigned bIndex=minBit; bIndex<=maxBit; ++bIndex) {
            if (!(dummyDescWriteMask & (1ull<<uint64(bIndex)))) continue;

            assert(result._writeCount < dimof(result._writes));
			auto& write = result._writes[result._writeCount];
            write = {};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = dstSet._underlying.get();
            write.dstBinding = bIndex;
            write.descriptorCount = 1;
            write.dstArrayElement = 0;

            const auto& b = sig._bindings[bIndex];
            if (b._type == DescriptorSetBindingSignature::Type::ConstantBuffer) {
				Log(Warning) << "No data provided for bound CB (" << bIndex << "). Using dummy resource." << std::endl;
                write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                write.pBufferInfo = &result._bufferInfo[blankBuffer];
            } else if (b._type == DescriptorSetBindingSignature::Type::Texture) {
				Log(Warning) << "No data provided for bound SRV (" << bIndex << "). Using dummy resource." << std::endl;
                write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                write.pImageInfo = &result._imageInfo[blankImage];
            } else if (b._type == DescriptorSetBindingSignature::Type::Sampler) {
				Log(Warning) << "No data provided for bound sampler (" << bIndex << "). Using dummy resource." << std::endl;
                write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                write.pImageInfo = &result._imageInfo[blankSampler];
			} else  {
                assert(0);      // (other types, such as UAVs and structured buffers not supported)
				continue;
            }

            bindingsWrittenTo |= 1ull << uint64(bIndex);
            ++result._writeCount;

			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				if (dstSet._description._bindingDescriptions.size() <= (write.dstBinding + write.descriptorCount))
					dstSet._description._bindingDescriptions.resize(write.dstBinding + write.descriptorCount);
				for (unsigned q=0; q<write.descriptorCount; ++q) {
					dstSet._description._bindingDescriptions[write.dstBinding+q]._descriptorType = write.descriptorType;
					dstSet._description._bindingDescriptions[write.dstBinding+q]._description = s_dummyDescriptorString;
				}
			#endif
        }

		return bindingsWrittenTo;
	}

	void WriteDummyDescriptors(
		DeviceContext& context,
		DescriptorSet& dstSet,
		const DescriptorSetSignature& sig)
	{
		NascentDescriptorWrite descWrite;
		uint64_t mask = sig._bindings.size()-1;
		WriteDummyDescriptors(
			descWrite, context.GetGlobalPools(), dstSet,
			sig, mask);
		if (descWrite._writeCount)
			vkUpdateDescriptorSets(context.GetUnderlyingDevice(), descWrite._writeCount, descWrite._writes, 0, nullptr);
	}

	static std::string s_boundUniformsNames[4] = {
		std::string{"BoundUniforms0"},
		std::string{"BoundUniforms1"},
		std::string{"BoundUniforms2"},
		std::string{"BoundUniforms3"}
	};

    void BoundUniforms::Apply(  
		DeviceContext& context,
        unsigned streamIdx,
        const UniformsStream& stream) const
    {
		assert(streamIdx < s_streamCount);
		if (_descriptorSetBindingMask[streamIdx]) {
			// Descriptor sets can't be written to again after they've been bound to a command buffer (unless we're
			// sure that all of the commands have already been completed).
			//
			// So, in effect writing a new descriptor set will always be a allocate operation. We may have a pool
			// of prebuild sets that we can reuse; or we can just allocate and free every time.
			//
			// Because each uniform stream can be set independantly, and at different rates, we'll use a separate
			// descriptor set for each uniform stream. 
			//
			// In this call, we could attempt to reuse another descriptor set that was created from exactly the same
			// inputs and already used earlier this frame...? But that may not be worth it. It seems like it will
			// make more sense to just create and set a full descriptor set for every call to this function.

			auto pipelineType = _isComputeShader 
				? DeviceContext::PipelineType::Compute 
				: DeviceContext::PipelineType::Graphics;

			auto& globalPools = context.GetGlobalPools();
			auto descriptorSet = context.AllocateDescriptorSet(pipelineType, streamIdx);

			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				assert(streamIdx < dimof(s_boundUniformsNames));
				descriptorSet._description._descriptorSetInfo = s_boundUniformsNames[streamIdx];
			#endif

			// -------- write descriptor set --------
			NascentDescriptorWrite descWrite;
			auto cbBindingFlag = WriteCBBindings(
				descWrite,
				context.GetTemporaryBufferSpace(),
				context.GetFactory(),
				descriptorSet,
				stream._constantBuffers,
				MakeIteratorRange(_cbBindingIndices[streamIdx]),
				_descriptorSetBindingMask[streamIdx]);

			auto svBindingFlag = WriteSRVBindings(
				descWrite,
				globalPools,
				descriptorSet,
				MakeIteratorRange((const ShaderResourceView*const*)stream._resources.begin(), (const ShaderResourceView*const*)stream._resources.end()),
				MakeIteratorRange(_srvBindingIndices[streamIdx]),
				_descriptorSetBindingMask[streamIdx]);		

			auto& pipelineLayout = *context.GetPipelineLayout(pipelineType);
			auto& rootSig = *pipelineLayout.GetRootSignature();
			const auto& sig = rootSig._descriptorSets[streamIdx];

			#if defined(_DEBUG)
				// Check to make sure the descriptor type matches the write operation we're performing
				for (unsigned w=0; w<descWrite._writeCount; w++) {
					auto& write = descWrite._writes[w];
					assert(write.dstBinding < sig._bindings.size());
					assert(AsDescriptorType(sig._bindings[write.dstBinding]._type) == write.descriptorType);
				}
			#endif

			// Any locations referenced by the descriptor layout, by not written by the values in
			// the streams must now be filled in with the defaults.
			// Vulkan doesn't seem to have well defined behaviour for descriptor set entries that
			// are part of the layout, but never written.
			// We can do this with "write" operations, or with "copy" operations. It seems like copy
			// might be inefficient on many platforms, so we'll prefer "write"
			//
			// In the most common case, there should be no dummy descriptors to fill in here... So we'll 
			// optimise for that case.
			uint64 dummyDescWriteMask = (~(cbBindingFlag|svBindingFlag)) & _descriptorSetBindingMask[streamIdx];
			if (dummyDescWriteMask != 0)
				WriteDummyDescriptors(descWrite, globalPools, descriptorSet, sig, dummyDescWriteMask);

			// note --  vkUpdateDescriptorSets happens immediately, regardless of command list progress.
			//          Ideally we don't really want to have to update these constantly... Once they are 
			//          set, maybe we can just reuse them?
			if (descWrite._writeCount)
				vkUpdateDescriptorSets(context.GetUnderlyingDevice(), descWrite._writeCount, descWrite._writes, 0, nullptr);
        
			context.BindDescriptorSet(
				pipelineType, streamIdx, descriptorSet._underlying.get()
				VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, std::move(descriptorSet._description)));

			if (descWrite._requiresTemporaryBufferBarrier)
				context.GetTemporaryBufferSpace().WriteBarrier(context);
		}

		if (_vsPushConstantSlot[streamIdx] < stream._constantBuffers.size()) {
			auto& cb = stream._constantBuffers[_vsPushConstantSlot[streamIdx]];
			assert(!cb._prebuiltBuffer);	// it doesn't make sense to bind push constants using a prebuild buffer -- so discourage this
			context.PushConstants(VK_SHADER_STAGE_VERTEX_BIT, cb._packet.AsIteratorRange());
		}

		if (_psPushConstantSlot[streamIdx] < stream._constantBuffers.size()) {
			auto& cb = stream._constantBuffers[_vsPushConstantSlot[streamIdx]];
			assert(!cb._prebuiltBuffer);	// it doesn't make sense to bind push constants using a prebuild buffer -- so discourage this
			context.PushConstants(VK_SHADER_STAGE_FRAGMENT_BIT, cb._packet.AsIteratorRange());
		}
    }

	BoundUniforms::BoundUniforms() 
	{
		for (unsigned c=0; c<s_streamCount; c++) {
			_descriptorSetBindingMask[c] = 0;
			_vsPushConstantSlot[c] = _psPushConstantSlot[c] = ~0u;
		}
	}
	BoundUniforms::~BoundUniforms() {}

}}

