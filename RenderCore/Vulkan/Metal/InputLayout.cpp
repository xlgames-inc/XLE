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
            auto hash = Hash64(e._semanticName) + e._semanticIndex;

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
		const PipelineLayoutConfig& pipelineLayoutConfig,
        const UniformsStreamInterface& interface0,
        const UniformsStreamInterface& interface1,
        const UniformsStreamInterface& interface2,
        const UniformsStreamInterface& interface3)
	{
		for (unsigned c=0; c<s_streamCount; c++) {
			_descriptorSetBindingMask[c] = 0;
			_vsPushConstantSlot[c] = _psPushConstantSlot[c] = ~0u;
		}

		SetupDescriptorSets(*shader._pipelineLayoutHelper);

		BoundUniformsHelper helper(shader);
		const UniformsStreamInterface* interfaces[] = { &interface0, &interface1, &interface2, &interface3 };
		SetupBindings(helper, interfaces, dimof(interfaces));
		assert(!helper._isComputeShader);
		_isComputeShader = false;
	}

	BoundUniforms::BoundUniforms(
        const ComputeShader& shader,
		const PipelineLayoutConfig& pipelineLayoutConfig,
        const UniformsStreamInterface& interface0,
        const UniformsStreamInterface& interface1,
        const UniformsStreamInterface& interface2,
        const UniformsStreamInterface& interface3)
	{
		for (unsigned c=0; c<s_streamCount; c++) {
			_descriptorSetBindingMask[c] = 0;
			_vsPushConstantSlot[c] = _psPushConstantSlot[c] = ~0u;
		}

		SetupDescriptorSets(*shader._pipelineLayoutHelper);

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

	void BoundUniforms::SetupDescriptorSets(const PipelineLayoutShaderConfig& pipelineLayoutHelper)
	{
		for (const auto&desc:pipelineLayoutHelper._descriptorSets) {
			if (desc._type != RootSignature::DescriptorSetType::Adaptive)
				continue;

			if (desc._uniformStream >= s_streamCount)
				continue;

			_descriptorSetSig[desc._uniformStream] = desc._signature;
			_underlyingLayouts[desc._uniformStream] = desc._bound._layout;
			_descriptorSetBindingPoint[desc._uniformStream] = desc._pipelineLayoutBindingIndex;
		}
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	static uint64_t WriteCBBindings(
		DescriptorSetBuilder& builder,
		TemporaryBufferSpace& temporaryBufferSpace,
		bool& requiresTemporaryBufferBarrier,
		ObjectFactory& factory,
		IteratorRange<const ConstantBufferView*> cbvs,
		IteratorRange<const uint32_t*> bindingIndicies,
		uint64_t shaderBindingMask)
	{
		uint64_t bindingsWrittenTo = 0u;

		auto count = std::min(cbvs.size(), bindingIndicies.size());
		for (unsigned c=0; c<count; ++c) {
			auto dstBinding = bindingIndicies[c];
            if (dstBinding == ~0u) continue;

			assert(shaderBindingMask & (1ull << uint64_t(dstBinding)));
			assert(!(builder.PendingWriteMask() & (1ull<<uint64(dstBinding))));

			if (cbvs[c]._prebuiltBuffer) {
				assert(const_cast<IResource*>(cbvs[c]._prebuiltBuffer)->QueryInterface(typeid(Resource).hash_code()));
				auto& res = *(Resource*)cbvs[c]._prebuiltBuffer;
				assert(res.GetBuffer());
				builder.BindCB(dstBinding, { res.GetBuffer(), 0, VK_WHOLE_SIZE } VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, "prebuilt"));
			} else {
				auto& pkt = cbvs[c]._packet;
				// assert(bufferCount < dimof(result._bufferInfo));
				// We must either allocate some memory from a temporary pool, or 
				// (or we could use push constants)
				auto tempSpace = temporaryBufferSpace.AllocateBuffer(pkt.AsIteratorRange());
				if (!tempSpace.buffer) {
					Log(Warning) << "Failed to allocate temporary buffer space. Falling back to new buffer." << std::endl;
					auto cb = MakeConstantBuffer(factory, pkt.AsIteratorRange());
					builder.BindCB(dstBinding, { cb.GetUnderlying(), 0, VK_WHOLE_SIZE } VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, "temporary buffer"));
				} else {
					builder.BindCB(dstBinding, tempSpace VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, "temporary buffer"));
					requiresTemporaryBufferBarrier |= true;
				}
			}

			bindingsWrittenTo |= 1ull << uint64(dstBinding);
		}

		return bindingsWrittenTo;
	}

	static uint64_t WriteSRVBindings(
		DescriptorSetBuilder& builder,
		IteratorRange<const ShaderResourceView*const*> srvs,
		IteratorRange<const uint32_t*> bindingIndicies,
		uint64_t shaderBindingMask)
	{
		uint64_t bindingsWrittenTo = 0u;

		auto count = std::min(srvs.size(), bindingIndicies.size());
		for (unsigned c=0; c<count; ++c) {
			auto dstBinding = bindingIndicies[c];
            if (dstBinding == ~0u) continue;

			assert(shaderBindingMask & (1ull << uint64_t(dstBinding)));
			assert(!(builder.PendingWriteMask() & (1ull<<uint64(dstBinding))));

			builder.BindSRV(dstBinding, srvs[c]);

			bindingsWrittenTo |= 1ull << uint64(dstBinding);
		}

		return bindingsWrittenTo;
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
		if (_descriptorSetBindingMask[streamIdx] && _underlyingLayouts[streamIdx]) {
			assert(_descriptorSetSig[streamIdx]);

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

			auto pipelineType = _isComputeShader ? PipelineType::Compute : PipelineType::Graphics;

			auto& globalPools = context.GetGlobalPools();
			DescriptorSet descriptorSet { globalPools._mainDescriptorPool.Allocate(_underlyingLayouts[streamIdx].get()) };
			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				assert(streamIdx < dimof(s_boundUniformsNames));
				descriptorSet._description._descriptorSetInfo = s_boundUniformsNames[streamIdx];
			#endif

			bool requiresTemporaryBufferBarrier = false;

			// -------- write descriptor set --------
			DescriptorSetBuilder builder { context.GetGlobalPools() };
			auto cbBindingFlag = WriteCBBindings(
				builder,
				context.GetTemporaryBufferSpace(),
				requiresTemporaryBufferBarrier,
				context.GetFactory(),
				stream._constantBuffers,
				MakeIteratorRange(_cbBindingIndices[streamIdx]),
				_descriptorSetBindingMask[streamIdx]);

			auto srvBindingFlag = WriteSRVBindings(
				builder,
				MakeIteratorRange((const ShaderResourceView*const*)stream._resources.begin(), (const ShaderResourceView*const*)stream._resources.end()),
				MakeIteratorRange(_srvBindingIndices[streamIdx]),
				_descriptorSetBindingMask[streamIdx]);		

			const auto& sig = *_descriptorSetSig[streamIdx];

			// Any locations referenced by the descriptor layout, by not written by the values in
			// the streams must now be filled in with the defaults.
			// Vulkan doesn't seem to have well defined behaviour for descriptor set entries that
			// are part of the layout, but never written.
			// We can do this with "write" operations, or with "copy" operations. It seems like copy
			// might be inefficient on many platforms, so we'll prefer "write"
			//
			// In the most common case, there should be no dummy descriptors to fill in here... So we'll 
			// optimise for that case.
			uint64_t dummyDescWriteMask = (~(cbBindingFlag|srvBindingFlag)) & _descriptorSetBindingMask[streamIdx];
			uint64_t dummyDescWritten = 0;
			if (dummyDescWriteMask != 0)
				dummyDescWritten = builder.BindDummyDescriptors(sig, dummyDescWriteMask);

			// note --  vkUpdateDescriptorSets happens immediately, regardless of command list progress.
			//          Ideally we don't really want to have to update these constantly... Once they are 
			//          set, maybe we can just reuse them?
			if (cbBindingFlag | srvBindingFlag | dummyDescWriteMask) {
				#if defined(_DEBUG)
					// Check to make sure the descriptor type matches the write operation we're performing
					builder.ValidatePendingWrites(sig);
				#endif

				builder.FlushChanges(context.GetUnderlyingDevice(), descriptorSet._underlying.get(), nullptr, 0 VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, descriptorSet._description));
			}
        
			context.BindDescriptorSet(
				pipelineType, streamIdx, descriptorSet._underlying.get()
				VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, std::move(descriptorSet._description)));

			if (requiresTemporaryBufferBarrier)
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

