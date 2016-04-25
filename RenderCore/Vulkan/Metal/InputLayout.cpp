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
#include "../../Format.h"
#include "../../Types.h"
#include "../../ShaderService.h"
#include "../../../Utility/MemoryUtils.h"

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
        _reflection[ShaderStage::Vertex] = SPIRVReflection(shader.GetCompiledVertexShader().GetByteCode());
        _reflection[ShaderStage::Pixel] = SPIRVReflection(shader.GetCompiledPixelShader().GetByteCode());
        auto* geoShader = shader.GetCompiledGeometryShader();
        if (geoShader)
            _reflection[ShaderStage::Geometry] = SPIRVReflection(geoShader->GetByteCode());
    }

    BoundUniforms::BoundUniforms(const DeepShaderProgram& shader)
    {
        _reflection[ShaderStage::Vertex] = SPIRVReflection(shader.GetCompiledVertexShader().GetByteCode());
        _reflection[ShaderStage::Pixel] = SPIRVReflection(shader.GetCompiledPixelShader().GetByteCode());
        auto* geoShader = shader.GetCompiledGeometryShader();
        if (geoShader)
            _reflection[ShaderStage::Geometry] = SPIRVReflection(geoShader->GetByteCode());
        _reflection[ShaderStage::Hull] = SPIRVReflection(shader.GetCompiledHullShader().GetByteCode());
        _reflection[ShaderStage::Domain] = SPIRVReflection(shader.GetCompiledDomainShader().GetByteCode());
    }

	BoundUniforms::BoundUniforms(const CompiledShaderByteCode& shader)
    {
        ShaderStage::Enum stage = shader.GetStage();
        if (stage < dimof(_reflection)) {
            _reflection[stage] = SPIRVReflection(shader.GetByteCode());
        }
    }

    BoundUniforms::BoundUniforms() {}

    BoundUniforms::~BoundUniforms() {}

    BoundUniforms::BoundUniforms(const BoundUniforms& copyFrom)
    {
        for (unsigned s=0; s<dimof(_reflection); ++s)
            _reflection[s] = copyFrom._reflection[s];
    }

    BoundUniforms& BoundUniforms::operator=(const BoundUniforms& copyFrom)
    {
        for (unsigned s=0; s<dimof(_reflection); ++s)
            _reflection[s] = copyFrom._reflection[s];
        return *this;
    }

    BoundUniforms::BoundUniforms(BoundUniforms&& moveFrom)
    {
        for (unsigned s=0; s<dimof(_reflection); ++s)
            _reflection[s] = std::move(moveFrom._reflection[s]);
    }

    BoundUniforms& BoundUniforms::operator=(BoundUniforms&& moveFrom)
    {
        for (unsigned s=0; s<dimof(_reflection); ++s)
            _reflection[s] = std::move(moveFrom._reflection[s]);
        return *this;
    }

    static VkShaderStageFlags AsShaderStageFlag(ShaderStage::Enum stage)
    {
        switch (stage) {
        case ShaderStage::Vertex: return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderStage::Pixel: return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderStage::Geometry: return VK_SHADER_STAGE_GEOMETRY_BIT;
        case ShaderStage::Hull: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case ShaderStage::Domain: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case ShaderStage::Compute: return VK_SHADER_STAGE_COMPUTE_BIT;
        default: return 0;
        }
    }

    bool BoundUniforms::BindConstantBuffer( uint64 hashName, unsigned slot, unsigned stream,
                                            const ConstantBufferLayoutElement elements[], size_t elementCount)
    {
        assert(!_pipelineLayout);
		auto descSet = 0u;
        bool gotBinding = false;

        for (unsigned s=0; s<dimof(_reflection); ++s) {
            auto i = LowerBound(_reflection[s]._uniformQuickLookup, hashName);
            if (i == _reflection[s]._uniformQuickLookup.end() || i->first != hashName) continue;

            assert(i->second._descriptorSet == descSet);
            assert(descSet < s_descriptorSetCount);

            auto shaderBindingPoint = i->second._bindingPoint;
            unsigned descSetBindingPoint;

            auto existing = std::find_if(
                _bindings[descSet].begin(), _bindings[descSet].end(), 
                [shaderBindingPoint](const VkDescriptorSetLayoutBinding& b) { return b.binding == shaderBindingPoint; });
            if (existing != _bindings[descSet].end()) {
                // normally this only happens when we want to add another shader stage to the same binding
                existing->stageFlags |= AsShaderStageFlag(ShaderStage::Enum(s));
            } else {
			    // note --  expecting this object to be a uniform block. We should
                //          do another lookup to check the type of the object.
			    VkDescriptorSetLayoutBinding binding = {};
                binding.binding = shaderBindingPoint;
                binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;     // (note, see also VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
                binding.descriptorCount = 1;
                binding.stageFlags = AsShaderStageFlag(ShaderStage::Enum(s));   // note, can we combine multiple bindings into one by using multiple bits here?
                binding.pImmutableSamplers = nullptr;
			
                _bindings[descSet].push_back(binding);
            }

            if (_cbBindingIndices[stream].size() <= slot) _cbBindingIndices[stream].resize(slot+1, ~0u);
            descSetBindingPoint = (shaderBindingPoint & 0xffff) | (descSet << 16);
            _cbBindingIndices[stream][slot] = descSetBindingPoint;
            gotBinding = true;
        }

        return gotBinding;
    }

    bool BoundUniforms::BindShaderResource(uint64 hashName, unsigned slot, unsigned stream)
    {
        assert(!_pipelineLayout);
		auto descSet = 0u;
        bool gotBinding = false;

        for (unsigned s=0; s<dimof(_reflection); ++s) {
            auto i = LowerBound(_reflection[s]._uniformQuickLookup, hashName);
            if (i == _reflection[s]._uniformQuickLookup.end() || i->first != hashName) continue;

            assert(i->second._descriptorSet == descSet);
            assert(descSet < s_descriptorSetCount);

            auto shaderBindingPoint = i->second._bindingPoint;
            unsigned descSetBindingPoint;

            auto existing = std::find_if(
                _bindings[descSet].begin(), _bindings[descSet].end(), 
                [shaderBindingPoint](const VkDescriptorSetLayoutBinding& b) { return b.binding == shaderBindingPoint; });
            if (existing != _bindings[descSet].end()) {
                // normally this only happens when we want to add another shader stage to the same binding
                existing->stageFlags |= AsShaderStageFlag(ShaderStage::Enum(s));
            } else {
			    // note --  We should validate the type of this object!
			    VkDescriptorSetLayoutBinding binding = {};
                binding.binding = i->second._bindingPoint;
                binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                binding.descriptorCount = 1;
                binding.stageFlags = AsShaderStageFlag(ShaderStage::Enum(s));   // note, can we combine multiple bindings into one by using multiple bits here?
                binding.pImmutableSamplers = nullptr;
                
                _bindings[descSet].push_back(binding);
            }

            if (_srBindingIndices[stream].size() <= slot) _srBindingIndices[stream].resize(slot+1, ~0u);
            descSetBindingPoint = (shaderBindingPoint & 0xffff) | (descSet << 16);
            _srBindingIndices[stream][slot] = descSetBindingPoint;
            gotBinding = true;
        }

        return gotBinding;
    }

    bool BoundUniforms::BindConstantBuffers(unsigned uniformsStream, std::initializer_list<const char*> cbs)
    {
        assert(!_pipelineLayout);
            // expecting this method to be called before any other BindConstantBuffers 
            // operations for this uniformsStream (because we start from a zero index)
        assert(uniformsStream < s_descriptorSetCount);

		if (_cbBindingIndices[uniformsStream].size() < cbs.size()) _cbBindingIndices[uniformsStream].resize(cbs.size(), ~0u);
        bool result = true;
        for (auto c=cbs.begin(); c<cbs.end(); ++c)
            result &= BindConstantBuffer(Hash64(*c), unsigned(c-cbs.begin()), uniformsStream);
        return result;
    }

    bool BoundUniforms::BindConstantBuffers(unsigned uniformsStream, std::initializer_list<uint64> cbs)
    {
        assert(!_pipelineLayout);
            // expecting this method to be called before any other BindConstantBuffers 
            // operations for this uniformsStream (because we start from a zero index)
        assert(uniformsStream < s_descriptorSetCount);

		if (_cbBindingIndices[uniformsStream].size() < cbs.size()) _cbBindingIndices[uniformsStream].resize(cbs.size(), ~0u);
		bool result = true;
        for (auto c=cbs.begin(); c<cbs.end(); ++c)
            result &= BindConstantBuffer(*c, unsigned(c-cbs.begin()), uniformsStream);
        return result;
    }

    bool BoundUniforms::BindShaderResources(unsigned uniformsStream, std::initializer_list<const char*> res)
    {
        assert(!_pipelineLayout);
        assert(uniformsStream < s_descriptorSetCount);

		if (_srBindingIndices[uniformsStream].size() < res.size()) _srBindingIndices[uniformsStream].resize(res.size(), ~0u);
		bool result = true;
        for (auto c=res.begin(); c<res.end(); ++c)
            result &= BindShaderResource(Hash64(*c), unsigned(c-res.begin()), uniformsStream);
        return result;
    }

    bool BoundUniforms::BindShaderResources(unsigned uniformsStream, std::initializer_list<uint64> res)
    {
        assert(!_pipelineLayout);
        assert(uniformsStream < s_descriptorSetCount);

		if (_srBindingIndices[uniformsStream].size() < res.size()) _srBindingIndices[uniformsStream].resize(res.size(), ~0u);
		bool result = true;
        for (auto c=res.begin(); c<res.end(); ++c)
            result &= BindShaderResource(*c, unsigned(c-res.begin()), uniformsStream);
        return result;
    }

    VulkanUniquePtr<VkDescriptorSetLayout> 
        BoundUniforms::CreateLayout(const ObjectFactory& factory, unsigned descriptorSet) const
    {
        return factory.CreateDescriptorSetLayout(MakeIteratorRange(_bindings[descriptorSet]));
    }

    static const bool s_reallocateDescriptorSets = true;

    void BoundUniforms::BuildPipelineLayout(
        const ObjectFactory& factory,
        DescriptorPool& descriptorPool) const
    {
        if (!_pipelineLayout) {
            VkDescriptorSetLayout rawLayouts[s_descriptorSetCount];
            for (unsigned c=0; c<s_descriptorSetCount; ++c) {
                _layouts[c] = CreateLayout(factory, c);
                rawLayouts[c] = _layouts[c].get();
            }
            _pipelineLayout = factory.CreatePipelineLayout(MakeIteratorRange(rawLayouts));

            if (constant_expression<!s_reallocateDescriptorSets>::result())
                descriptorPool.Allocate(
                    MakeIteratorRange(_descriptorSets),
                    MakeIteratorRange(rawLayouts));
        }
    }

    void BoundUniforms::Apply(  DeviceContext& context, 
                                const UniformsStream& stream0, 
                                const UniformsStream& stream1) const
    {
        BuildPipelineLayout(GetObjectFactory(), context.GetGlobalPools()._mainDescriptorPool);

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
        if (constant_expression<s_reallocateDescriptorSets>::result()) {
            VkDescriptorSetLayout rawLayouts[s_descriptorSetCount];
            for (unsigned c=0; c<s_descriptorSetCount; ++c)
                rawLayouts[c] = _layouts[c].get();
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

        for (unsigned stri=0; stri<dimof(streams); ++stri) {
            const auto& s = *streams[stri];

			auto maxCbs = _cbBindingIndices[stri].size();
            for (unsigned p=0; p<std::min(s._packetCount, maxCbs); ++p) {
                if (s._prebuiltBuffers && s._prebuiltBuffers[p]) {

                    assert(bufferCount < dimof(bufferInfo));
                    assert(writeCount < dimof(writes));

                    auto dstBinding = _cbBindingIndices[stri][p];
                    if (dstBinding == ~0u) continue;
                    bufferInfo[bufferCount] = VkDescriptorBufferInfo{s._prebuiltBuffers[p]->GetUnderlying(), 0, VK_WHOLE_SIZE};

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
                } else if (s._packets[p]) {
                    // todo -- append these onto a large buffer, or use push constant updates
                }
            }

			auto maxSrvs = _srBindingIndices[stri].size();
            for (unsigned r=0; r<std::min(s._resourceCount, maxSrvs); ++r) {
                if (s._resources && s._resources[r]) {

                    assert(imageCount < dimof(imageInfo));
                    assert(writeCount < dimof(writes));

                    auto dstBinding = _srBindingIndices[stri][r];
                    if (dstBinding == ~0u) continue;
                    imageInfo[imageCount] = VkDescriptorImageInfo {
                        globalPools._dummyResources._blankSampler->GetUnderlying(),
                        s._resources[r]->GetUnderlying(),
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

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
                    writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    writes[writeCount].pImageInfo = &imageInfo[imageCount];
                    writes[writeCount].dstArrayElement = 0;

                    descSetWrites[dstBinding>>16] |= 1ull << uint64(dstBinding&0xffff);
                    ++writeCount;
                    ++imageCount;
                }
            }
        }

        // Any locations referenced by the descriptor layout, by not written by the values in
        // the streams must now be filled in with the defaults.
        // Vulkan doesn't seem to have well defined behaviour for descriptor set entries that
        // are part of the layout, but never written.
        // We can do this with "write" operations, or with "copy" operations. I'm assuming that there's
        // no major performance difference, so I'll just use writes.
        assert(bufferCount < dimof(bufferInfo));
        assert(imageCount < dimof(imageInfo));
        auto blankBuffer = bufferCount;
        auto blankImage = imageCount;
        bufferInfo[bufferCount++] = VkDescriptorBufferInfo { 
            globalPools._dummyResources._blankBuffer.GetUnderlying(),
            0, VK_WHOLE_SIZE };
        imageInfo[imageCount++] = VkDescriptorImageInfo {
            globalPools._dummyResources._blankSampler->GetUnderlying(),
            globalPools._dummyResources._blankSrv.GetUnderlying(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

        for (unsigned s=0; s<s_streamCount; ++s) {
            for (auto b:_cbBindingIndices[s]) {
                if (b == ~0u || (descSetWrites[b>>16] & (1ull<<uint64(b&0xffff)))) continue;

                assert(writeCount < dimof(writes));
                writes[writeCount] = {};
                writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[writeCount].dstSet = _descriptorSets[b>>16].get();
                writes[writeCount].dstBinding = b&0xffff;
                writes[writeCount].descriptorCount = 1;
                writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                writes[writeCount].pBufferInfo = &bufferInfo[blankBuffer];
                writes[writeCount].dstArrayElement = 0;

                descSetWrites[b>>16] |= 1ull << uint64(b&0xffff);
                ++writeCount;
            }

            for (auto b:_srBindingIndices[s]) {
                if (b == ~0u || (descSetWrites[b>>16] & (1ull<<uint64(b&0xffff)))) continue;

                assert(writeCount < dimof(writes));
                writes[writeCount] = {};
                writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[writeCount].dstSet = _descriptorSets[b>>16].get();
                writes[writeCount].dstBinding = b&0xffff;
                writes[writeCount].descriptorCount = 1;
                writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                writes[writeCount].pImageInfo = &imageInfo[blankImage];
                writes[writeCount].dstArrayElement = 0;

                descSetWrites[b>>16] |= 1ull << uint64(b&0xffff);
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
        
        context.SetPipelineLayout(_pipelineLayout);
        context.SetHideDescriptorSetBuilder();
        context.CmdBindDescriptorSets(
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            _pipelineLayout.get(), 0, 
            dimof(rawDescriptorSets), rawDescriptorSets, 
            0, nullptr);
    }

    void BoundUniforms::UnbindShaderResources(DeviceContext& context, unsigned streamIndex) const
    {
    }

}}

