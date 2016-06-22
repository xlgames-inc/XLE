// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "InputLayout.h"
#include "Shader.h"
#include "Buffer.h"
#include "State.h"
#include "TextureView.h"
#include "ObjectFactory.h"
#include "DeviceContext.h"
#include "Pools.h"
#include "Format.h"
#include "PipelineLayout.h"
#include "../../Format.h"
#include "../../Types.h"
#include "../../ShaderService.h"
#include "../../../Utility/MemoryUtils.h"
#include "../../../Utility/ArithmeticUtils.h"
#include "../../../ConsoleRig/Log.h"

namespace RenderCore { namespace Metal_Vulkan
{
    BoundInputLayout::BoundInputLayout(const InputLayout& layout, const ShaderProgram& shader)
    : BoundInputLayout(layout, shader.GetCompiledVertexShader())
    {
    }

    BoundInputLayout::BoundInputLayout(const InputLayout& layout, const CompiledShaderByteCode& shader)
    {
        // find the vertex inputs into the shader, and match them against the input layout
        unsigned trackingOffset = 0;

        SPIRVReflection reflection(shader.GetByteCode());
        _attributes.reserve(layout.second);
        for (unsigned c=0; c<layout.second; ++c) {
            const auto& e = layout.first[c];
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

        // todo -- check if any input slots are not bound to anything
    }

    BoundInputLayout::BoundInputLayout() {}
    BoundInputLayout::~BoundInputLayout() {}

	BoundInputLayout::BoundInputLayout(BoundInputLayout&& moveFrom) never_throws
	: _attributes(std::move(moveFrom._attributes)) {}

	BoundInputLayout& BoundInputLayout::operator=(BoundInputLayout&& moveFrom) never_throws
	{
		_attributes = std::move(moveFrom._attributes);
		return *this;
	}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    BoundUniforms::BoundUniforms(const ShaderProgram& shader)
    {
        _isComputeShader = false;
        if (shader.GetCompiledVertexShader().GetStage() != ShaderStage::Null)
            _reflection[ShaderStage::Vertex] = SPIRVReflection(shader.GetCompiledVertexShader().GetByteCode());
        if (shader.GetCompiledPixelShader().GetStage() != ShaderStage::Null)
            _reflection[ShaderStage::Pixel] = SPIRVReflection(shader.GetCompiledPixelShader().GetByteCode());
        auto* geoShader = shader.GetCompiledGeometryShader();
        if (geoShader && geoShader->GetStage() != ShaderStage::Null)
            _reflection[ShaderStage::Geometry] = SPIRVReflection(geoShader->GetByteCode());
        BuildShaderBindingMask();
    }

    BoundUniforms::BoundUniforms(const DeepShaderProgram& shader)
    {
        _isComputeShader = false;
        if (shader.GetCompiledVertexShader().GetStage() != ShaderStage::Null)
            _reflection[ShaderStage::Vertex] = SPIRVReflection(shader.GetCompiledVertexShader().GetByteCode());
        if (shader.GetCompiledPixelShader().GetStage() != ShaderStage::Null)
            _reflection[ShaderStage::Pixel] = SPIRVReflection(shader.GetCompiledPixelShader().GetByteCode());
        auto* geoShader = shader.GetCompiledGeometryShader();
        if (geoShader && geoShader->GetStage() != ShaderStage::Null)
            _reflection[ShaderStage::Geometry] = SPIRVReflection(geoShader->GetByteCode());
        if (shader.GetCompiledHullShader().GetStage() != ShaderStage::Null)
            _reflection[ShaderStage::Hull] = SPIRVReflection(shader.GetCompiledHullShader().GetByteCode());
        if (shader.GetCompiledDomainShader().GetStage() != ShaderStage::Null)
            _reflection[ShaderStage::Domain] = SPIRVReflection(shader.GetCompiledDomainShader().GetByteCode());
        BuildShaderBindingMask();
    }

	BoundUniforms::BoundUniforms(const CompiledShaderByteCode& shader)
    {
        ShaderStage::Enum stage = shader.GetStage();
        if (stage < dimof(_reflection)) {
            _reflection[stage] = SPIRVReflection(shader.GetByteCode());
        }
        _isComputeShader = stage == ShaderStage::Compute;
        BuildShaderBindingMask();
    }

    BoundUniforms::BoundUniforms() {}
    BoundUniforms::~BoundUniforms() {}

    BoundUniforms::BoundUniforms(const BoundUniforms& copyFrom)
    {
        for (unsigned s=0; s<dimof(_reflection); ++s)
            _reflection[s] = copyFrom._reflection[s];

        for (unsigned s=0; s<s_streamCount; ++s) {
            _cbBindingIndices[s] = copyFrom._cbBindingIndices[s];
            _srvBindingIndices[s] = copyFrom._srvBindingIndices[s];
        }

        for (unsigned s=0; s<s_descriptorSetCount; ++s) {
            _shaderBindingMask[s] = copyFrom._shaderBindingMask[s];
            _descriptorSets[s] = nullptr;
        }
        _isComputeShader = copyFrom._isComputeShader;
    }

    BoundUniforms& BoundUniforms::operator=(const BoundUniforms& copyFrom)
    {
        for (unsigned s=0; s<dimof(_reflection); ++s)
            _reflection[s] = copyFrom._reflection[s];
        for (unsigned s=0; s<s_streamCount; ++s) {
            _cbBindingIndices[s] = copyFrom._cbBindingIndices[s];
            _srvBindingIndices[s] = copyFrom._srvBindingIndices[s];
        }

        for (unsigned s=0; s<s_descriptorSetCount; ++s) {
            _shaderBindingMask[s] = copyFrom._shaderBindingMask[s];
            _descriptorSets[s] = nullptr;
        }
        _isComputeShader = copyFrom._isComputeShader;
        return *this;
    }

    BoundUniforms::BoundUniforms(BoundUniforms&& moveFrom)
    {
        for (unsigned s=0; s<dimof(_reflection); ++s)
            _reflection[s] = std::move(moveFrom._reflection[s]);
        for (unsigned s=0; s<s_streamCount; ++s) {
            _cbBindingIndices[s] = std::move(moveFrom._cbBindingIndices[s]);
            _srvBindingIndices[s] = std::move(moveFrom._srvBindingIndices[s]);
        }
        for (unsigned s=0; s<s_descriptorSetCount; ++s) {
            _shaderBindingMask[s] = moveFrom._shaderBindingMask[s];
            _descriptorSets[s] = std::move(_descriptorSets[s]);
        }
        _isComputeShader = moveFrom._isComputeShader;
    }

    BoundUniforms& BoundUniforms::operator=(BoundUniforms&& moveFrom)
    {
        for (unsigned s=0; s<dimof(_reflection); ++s)
            _reflection[s] = std::move(moveFrom._reflection[s]);
        for (unsigned s=0; s<s_streamCount; ++s) {
            _cbBindingIndices[s] = std::move(moveFrom._cbBindingIndices[s]);
            _srvBindingIndices[s] = std::move(moveFrom._srvBindingIndices[s]);
        }
        for (unsigned s=0; s<s_descriptorSetCount; ++s) {
            _shaderBindingMask[s] = moveFrom._shaderBindingMask[s];
            _descriptorSets[s] = std::move(_descriptorSets[s]);
        }
        _isComputeShader = moveFrom._isComputeShader;
        return *this;
    }

    void BoundUniforms::BuildShaderBindingMask()
    {
        for (unsigned d=0; d<s_descriptorSetCount; ++d) {
            _shaderBindingMask[d] = 0x0ull;
            for (unsigned r=0; r<ShaderStage::Max; ++r) {
                // Look for all of the bindings in this descriptor set that are referenced by the shader
                for(const auto&b:_reflection[r]._bindings) {
                    if (b.second._descriptorSet == d && b.second._bindingPoint != ~0x0u)
                        _shaderBindingMask[d] |= 1ull << uint64(b.second._bindingPoint);
                }
            }
        }
    }

    bool BoundUniforms::BindConstantBuffer( uint64 hashName, unsigned slot, unsigned stream,
                                            const ConstantBufferLayoutElement elements[], size_t elementCount)
    {
        // assert(!_pipelineLayout);
		auto descSet = 0u;
        bool gotBinding = false;

        for (unsigned s=0; s<dimof(_reflection); ++s) {
            auto i = LowerBound(_reflection[s]._uniformQuickLookup, hashName);
            if (i == _reflection[s]._uniformQuickLookup.end() || i->first != hashName) {
                // Could not match. This happens sometimes in normal usage.
                // It just means the provided interface is a little too broad for this shader
                continue;
            }

            if (i->second._descriptorSet != descSet) {
                LogWarning << "Constant buffer binding appears to be in the wrong descriptor set.";
                continue;
            }

            // Sometimes the binding isn't set. This occurs if constant buffer is actually push constants
            // In this case, we can't bind like this -- we need to use the push constants interface.
            if (i->second._bindingPoint == ~0x0u)
                continue;

            assert(descSet < s_descriptorSetCount);

            if (_cbBindingIndices[stream].size() <= slot) _cbBindingIndices[stream].resize(slot+1, ~0u);

            auto descSetBindingPoint = (i->second._bindingPoint & 0xffff) | (descSet << 16);
            assert(_cbBindingIndices[stream][slot] == ~0u || _cbBindingIndices[stream][slot] == descSetBindingPoint);
            _cbBindingIndices[stream][slot] = descSetBindingPoint;

            gotBinding = true;
        }

        return gotBinding;
    }

    bool BoundUniforms::BindShaderResource(uint64 hashName, unsigned slot, unsigned stream)
    {
        // assert(!_pipelineLayout);
		auto descSet = 0u;
        bool gotBinding = false;

        for (unsigned s=0; s<dimof(_reflection); ++s) {
            auto i = LowerBound(_reflection[s]._uniformQuickLookup, hashName);
            if (i == _reflection[s]._uniformQuickLookup.end() || i->first != hashName) {
                // Could not match. This happens sometimes in normal usage.
                // It just means the provided interface is a little too broad for this shader
                continue;
            }

            if (i->second._descriptorSet != descSet) {
                LogWarning << "Shader resource binding appears to be in the wrong descriptor set.";
                continue;
            }
            assert(descSet < s_descriptorSetCount);

            if (_srvBindingIndices[stream].size() <= slot) _srvBindingIndices[stream].resize(slot+1, ~0u);

            assert(i->second._bindingPoint != ~0x0u);
            auto descSetBindingPoint = (i->second._bindingPoint & 0xffff) | (descSet << 16);
            assert(_srvBindingIndices[stream][slot] == ~0u || _srvBindingIndices[stream][slot] == descSetBindingPoint);
            _srvBindingIndices[stream][slot] = descSetBindingPoint;
            gotBinding = true;
        }

        return gotBinding;
    }

    bool BoundUniforms::BindConstantBuffers(unsigned uniformsStream, std::initializer_list<const char*> cbs)
    {
        // assert(!_pipelineLayout);
            // expecting this method to be called before any other BindConstantBuffers 
            // operations for this uniformsStream (because we start from a zero index)
        assert(uniformsStream < s_streamCount);

		if (_cbBindingIndices[uniformsStream].size() < cbs.size()) _cbBindingIndices[uniformsStream].resize(cbs.size(), ~0u);
        bool result = true;
        for (auto c=cbs.begin(); c<cbs.end(); ++c)
            result &= BindConstantBuffer(Hash64(*c), unsigned(c-cbs.begin()), uniformsStream);
        return result;
    }

    bool BoundUniforms::BindConstantBuffers(unsigned uniformsStream, std::initializer_list<uint64> cbs)
    {
        // assert(!_pipelineLayout);
            // expecting this method to be called before any other BindConstantBuffers 
            // operations for this uniformsStream (because we start from a zero index)
        assert(uniformsStream < s_streamCount);

		if (_cbBindingIndices[uniformsStream].size() < cbs.size()) _cbBindingIndices[uniformsStream].resize(cbs.size(), ~0u);
		bool result = true;
        for (auto c=cbs.begin(); c<cbs.end(); ++c)
            result &= BindConstantBuffer(*c, unsigned(c-cbs.begin()), uniformsStream);
        return result;
    }

    bool BoundUniforms::BindShaderResources(unsigned uniformsStream, std::initializer_list<const char*> res)
    {
        // assert(!_pipelineLayout);
        assert(uniformsStream < s_streamCount);

		if (_srvBindingIndices[uniformsStream].size() < res.size()) _srvBindingIndices[uniformsStream].resize(res.size(), ~0u);
		bool result = true;
        for (auto c=res.begin(); c<res.end(); ++c)
            result &= BindShaderResource(Hash64(*c), unsigned(c-res.begin()), uniformsStream);
        return result;
    }

    bool BoundUniforms::BindShaderResources(unsigned uniformsStream, std::initializer_list<uint64> res)
    {
        // assert(!_pipelineLayout);
        assert(uniformsStream < s_streamCount);

		if (_srvBindingIndices[uniformsStream].size() < res.size()) _srvBindingIndices[uniformsStream].resize(res.size(), ~0u);
		bool result = true;
        for (auto c=res.begin(); c<res.end(); ++c)
            result &= BindShaderResource(*c, unsigned(c-res.begin()), uniformsStream);
        return result;
    }

    static const bool s_reallocateDescriptorSets = true;

    void BoundUniforms::Apply(  DeviceContext& context, 
                                const UniformsStream& stream0, 
                                const UniformsStream& stream1) const
    {
        // BuildPipelineLayout(GetObjectFactory(), context.GetGlobalPools()._mainDescriptorPool);

        // There's currently a problem reusing descriptor sets from frame to frame. I'm not sure
        // what the issue is, but causes an error deep within Vulkan. The validation errors don't
        // report the issue, but just say that "You must call vkBeginCommandBuffer() before this call to ..."
        // Allocating a fresh descriptor set before every write appears to solve this problem.
        //
        // It seems like we could do with better management of the descriptor sets. Allocating new sets
        // frequently might be ok... But in that case, we should use a "pool" that can be reset once
        // per frame (as opposed to calling "free" for every separate descriptor.
        //
        // In some cases, we might want descriptor sets that are permanent. Ie, we allocate them while
        // loading a model, and then keep them around over multiple frames. For these cases, we could
        // have a separate pool with the VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT flag set.
        //
        // We have to be careful because vkUpdateDescriptorSets happens immediately -- like mapping
        // a texture. That makes the memory management more complicated.
        auto pipelineType = _isComputeShader 
            ? DeviceContext::PipelineType::Compute 
            : DeviceContext::PipelineType::Graphics;
        auto* pipelineLayout = context.GetPipelineLayout(pipelineType);

        if (constant_expression<s_reallocateDescriptorSets>::result()) {
            VkDescriptorSetLayout rawLayouts[s_descriptorSetCount];
            for (unsigned c=0; c<s_descriptorSetCount; ++c)
                rawLayouts[c] = pipelineLayout->GetDescriptorSetLayout(c);
            context.GetGlobalPools()._mainDescriptorPool.Allocate(
                MakeIteratorRange(_descriptorSets),
                MakeIteratorRange(rawLayouts));
        }

        // -------- write descriptor set --------
        const unsigned maxBindings = 16;
        VkDescriptorBufferInfo bufferInfo[maxBindings];
        VkDescriptorImageInfo imageInfo[maxBindings];
        VkWriteDescriptorSet writes[maxBindings];

        unsigned writeCount = 0, bufferCount = 0, imageCount = 0;

		const UniformsStream* streams[] = { &stream0, &stream1 };

        uint64 descSetWrites[s_descriptorSetCount] = { 0x0ull };
        auto& globalPools = context.GetGlobalPools();

		bool temporaryBufferBarrier = false;

        for (unsigned stri=0; stri<dimof(streams); ++stri) {
            const auto& s = *streams[stri];

			auto maxCbs = _cbBindingIndices[stri].size();
            for (unsigned p=0; p<std::min(s._packetCount, maxCbs); ++p) {
                if (s._prebuiltBuffers && s._prebuiltBuffers[p] && s._prebuiltBuffers[p]->IsGood()) {

                    assert(bufferCount < dimof(bufferInfo));
                    assert(writeCount < dimof(writes));

                    auto dstBinding = _cbBindingIndices[stri][p];
                    if (dstBinding == ~0u) continue;
                    bufferInfo[bufferCount] = VkDescriptorBufferInfo{s._prebuiltBuffers[p]->GetUnderlying(), 0, VK_WHOLE_SIZE};
                    assert(_shaderBindingMask[0] & (1ull << uint64(dstBinding&0xffff)));

                    // Currently occuring when creating shadow depth textures, because of some of the shadow constant buffers
                    // #if defined(_DEBUG) // check for duplicate descriptor writes
                    //     for (unsigned w=0; w<writeCount; ++w)
                    //         assert( writes[w].dstBinding != (dstBinding&0xffff)
                    //             ||  writes[w].dstSet != _descriptorSets[dstBinding>>16].get());
                    // #endif

                    writes[writeCount] = {};
                    writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    writes[writeCount].dstSet = _descriptorSets[dstBinding>>16].get();
                    writes[writeCount].dstBinding = dstBinding&0xffff;
                    writes[writeCount].descriptorCount = 1;
                    writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    writes[writeCount].pBufferInfo = &bufferInfo[bufferCount];
                    writes[writeCount].dstArrayElement = 0;

                    descSetWrites[dstBinding>>16] |= 1ull << uint64(dstBinding&0xffff);
                    ++writeCount;
                    ++bufferCount;
                } else if (s._packets && s._packets[p]) {
                    assert(bufferCount < dimof(bufferInfo));
                    assert(writeCount < dimof(writes));

                    auto dstBinding = _cbBindingIndices[stri][p];
                    if (dstBinding == ~0u) continue;
					bufferInfo[bufferCount] = context.GetTemporaryBufferSpace().AllocateBuffer(
						s._packets[p].begin(), s._packets[p].size());

					temporaryBufferBarrier |= bufferInfo[bufferCount].buffer != nullptr;
					if (!bufferInfo[bufferCount].buffer) {
						LogWarning << "Failed to allocate temporary buffer space. Falling back to new buffer.";
						ConstantBuffer cb(context.GetFactory(), s._packets[p].begin(), s._packets[p].size());
						bufferInfo[bufferCount] = VkDescriptorBufferInfo{ cb.GetUnderlying(), 0, VK_WHOLE_SIZE };
					}

					assert(_shaderBindingMask[0] & (1ull << uint64(dstBinding&0xffff)));

                    #if defined(_DEBUG) // check for duplicate descriptor writes
                        for (unsigned w=0; w<writeCount; ++w)
                            assert( writes[w].dstBinding != (dstBinding&0xffff)
                                ||  writes[w].dstSet != _descriptorSets[dstBinding>>16].get());
                    #endif

                    writes[writeCount] = {};
                    writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    writes[writeCount].dstSet = _descriptorSets[dstBinding>>16].get();
                    writes[writeCount].dstBinding = dstBinding&0xffff;
                    writes[writeCount].descriptorCount = 1;
                    writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    writes[writeCount].pBufferInfo = &bufferInfo[bufferCount];
                    writes[writeCount].dstArrayElement = 0;

                    descSetWrites[dstBinding>>16] |= 1ull << uint64(dstBinding&0xffff);
                    ++writeCount;
                    ++bufferCount;

                }
            }

			auto maxSrvs = _srvBindingIndices[stri].size();
            for (unsigned r=0; r<std::min(s._resourceCount, maxSrvs); ++r) {
                if (s._resources && s._resources[r]) {

                    assert(imageCount < dimof(imageInfo));
                    assert(writeCount < dimof(writes));

                    auto dstBinding = _srvBindingIndices[stri][r];
                    if (dstBinding == ~0u) continue;

                    // Our "StructuredBuffer" objects are being mapped onto uniform buffers in SPIR-V
                    // So sometimes a SRV will end up writing to a VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                    // descriptor.
                    if (s._resources[r]->GetImageView()) {
                        imageInfo[imageCount] = VkDescriptorImageInfo {
                            globalPools._dummyResources._blankSampler->GetUnderlying(),
                            s._resources[r]->GetImageView(),
						    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
                        assert(_shaderBindingMask[0] & (1ull << uint64(dstBinding&0xffff)));

                        #if defined(_DEBUG) // check for duplicate descriptor writes
                            for (unsigned w=0; w<writeCount; ++w)
                                assert( writes[w].dstBinding != (dstBinding&0xffff)
                                    ||  writes[w].dstSet != _descriptorSets[dstBinding>>16].get());
                        #endif

                        writes[writeCount] = {};
                        writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        writes[writeCount].dstSet = _descriptorSets[dstBinding>>16].get();
                        writes[writeCount].dstBinding = dstBinding&0xffff;
                        writes[writeCount].descriptorCount = 1;
                        writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                        writes[writeCount].pImageInfo = &imageInfo[imageCount];
                        writes[writeCount].dstArrayElement = 0;
                        ++imageCount;
                    } else {
                        auto buffer = UnderlyingResourcePtr(s._resources[r]->GetResource()).get()->GetBuffer();
                        bufferInfo[bufferCount] = VkDescriptorBufferInfo{buffer, 0, VK_WHOLE_SIZE};
                        assert(_shaderBindingMask[0] & (1ull << uint64(dstBinding&0xffff)));

                        #if defined(_DEBUG) // check for duplicate descriptor writes
                            for (unsigned w=0; w<writeCount; ++w)
                                assert( writes[w].dstBinding != (dstBinding&0xffff)
                                    ||  writes[w].dstSet != _descriptorSets[dstBinding>>16].get());
                        #endif

                        writes[writeCount] = {};
                        writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        writes[writeCount].dstSet = _descriptorSets[dstBinding>>16].get();
                        writes[writeCount].dstBinding = dstBinding&0xffff;
                        writes[writeCount].descriptorCount = 1;
                        writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                        writes[writeCount].pBufferInfo = &bufferInfo[bufferCount];
                        writes[writeCount].dstArrayElement = 0;
                        ++bufferCount;
                    }

                    descSetWrites[dstBinding>>16] |= 1ull << uint64(dstBinding&0xffff);
                    ++writeCount;
                }
            }
        }

        static_assert(s_descriptorSetCount==1, "Expecting single descriptor set");
        auto rootSig = pipelineLayout->ShareRootSignature();
        const auto& sig = rootSig->_descriptorSets[0];

        #if defined(_DEBUG)
            // Check to make sure the descriptor type matches the write operation we're performing
            for (unsigned w=0; w<writeCount; w++) {
                auto& write = writes[w];
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
        uint64 dummyDescWriteMask = (~descSetWrites[0]) & _shaderBindingMask[0];
        if (dummyDescWriteMask != 0) {

            assert(bufferCount < dimof(bufferInfo));
            assert(imageCount < dimof(imageInfo));
            auto blankBuffer = bufferCount;
            auto blankImage = imageCount;
            bufferInfo[bufferCount++] = VkDescriptorBufferInfo { 
                globalPools._dummyResources._blankBuffer.GetUnderlying(),
                0, VK_WHOLE_SIZE };
            imageInfo[imageCount++] = VkDescriptorImageInfo {
                globalPools._dummyResources._blankSampler->GetUnderlying(),
                globalPools._dummyResources._blankSrv.GetImageView(),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

            unsigned minBit = xl_ctz8(dummyDescWriteMask);
            unsigned maxBit = std::min(64u - xl_clz8(dummyDescWriteMask), (unsigned)sig._bindings.size()-1);

            for (unsigned bIndex=minBit; bIndex<=maxBit; ++bIndex) {
                if (!(dummyDescWriteMask & (1ull<<uint64(bIndex)))) continue;

                LogWarning << "No data provided for bound uniform (" << bIndex << "). Using dummy resource.";

                assert(writeCount < dimof(writes));
                writes[writeCount] = {};
                writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[writeCount].dstSet = _descriptorSets[0].get();
                writes[writeCount].dstBinding = bIndex;
                writes[writeCount].descriptorCount = 1;
                writes[writeCount].dstArrayElement = 0;

                const auto& b = sig._bindings[bIndex];
                if (b._type == DescriptorSetBindingSignature::Type::ConstantBuffer) {
                    writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    writes[writeCount].pBufferInfo = &bufferInfo[blankBuffer];
                } else if (b._type == DescriptorSetBindingSignature::Type::Texture) {
                    writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                    writes[writeCount].pImageInfo = &imageInfo[blankImage];
                } else {
                    assert(0);      // (other types, such as UAVs and structured buffers not supported)
                }

                // descSetWrites[0] |= 1ull << uint64(bIndex);
                ++writeCount;
            }
        }

        // note --  vkUpdateDescriptorSets happens immediately, regardless of command list progress.
        //          Ideally we don't really want to have to update these constantly... Once they are 
        //          set, maybe we can just reuse them?
        if (writeCount)
            vkUpdateDescriptorSets(context.GetUnderlyingDevice(), writeCount, writes, 0, nullptr);
        
        VkDescriptorSet rawDescriptorSets[s_descriptorSetCount];
        for (unsigned c=0; c<s_descriptorSetCount; ++c)
            rawDescriptorSets[c] = _descriptorSets[c].get();
        
        static_assert(dimof(rawDescriptorSets) == 1, "Expecting just a single descriptor set");
        context.BindDescriptorSet(pipelineType, 0, rawDescriptorSets[0]);

		if (temporaryBufferBarrier)
			context.GetTemporaryBufferSpace().WriteBarrier(context);
    }

    void BoundUniforms::UnbindShaderResources(DeviceContext& context, unsigned streamIndex) const
    {
    }

}}

