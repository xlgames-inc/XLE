// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "InputLayout.h"
#include "Shader.h"
#include "Buffer.h"
#include "ShaderResource.h"
#include "State.h"
#include "ObjectFactory.h"
#include "DeviceContext.h"
#include "Pools.h"
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

    namespace GlobalInputLayouts
    {
        namespace Detail
        {
            static const unsigned AppendAlignedElement = ~unsigned(0x0);
            InputElementDesc P2CT_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, NativeFormat::R32G32_FLOAT   ),
                InputElementDesc( "COLOR",      0, NativeFormat::R8G8B8A8_UNORM ),
                InputElementDesc( "TEXCOORD",   0, NativeFormat::R32G32_FLOAT   )
            };

            InputElementDesc P2C_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, NativeFormat::R32G32_FLOAT   ),
                InputElementDesc( "COLOR",      0, NativeFormat::R8G8B8A8_UNORM )
            };

            InputElementDesc PCT_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, NativeFormat::R32G32B32_FLOAT),
                InputElementDesc( "COLOR",      0, NativeFormat::R8G8B8A8_UNORM ),
                InputElementDesc( "TEXCOORD",   0, NativeFormat::R32G32_FLOAT   )
            };

            InputElementDesc P_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, NativeFormat::R32G32B32_FLOAT)
            };

            InputElementDesc PC_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, NativeFormat::R32G32B32_FLOAT),
                InputElementDesc( "COLOR",      0, NativeFormat::R8G8B8A8_UNORM )
            };

            InputElementDesc PT_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, NativeFormat::R32G32B32_FLOAT),
                InputElementDesc( "TEXCOORD",   0, NativeFormat::R32G32_FLOAT   )
            };

            InputElementDesc PN_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, NativeFormat::R32G32B32_FLOAT),
                InputElementDesc( "NORMAL",   0, NativeFormat::R32G32B32_FLOAT )
            };

            InputElementDesc PNT_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, NativeFormat::R32G32B32_FLOAT),
                InputElementDesc( "NORMAL",   0, NativeFormat::R32G32B32_FLOAT ),
                InputElementDesc( "TEXCOORD",   0, NativeFormat::R32G32_FLOAT )
            };

            InputElementDesc PNTT_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, NativeFormat::R32G32B32_FLOAT),
                InputElementDesc( "NORMAL",   0, NativeFormat::R32G32B32_FLOAT ),
                InputElementDesc( "TEXCOORD",   0, NativeFormat::R32G32_FLOAT ),
                InputElementDesc( "TEXTANGENT",   0, NativeFormat::R32G32B32_FLOAT ),
                InputElementDesc( "TEXBITANGENT",   0, NativeFormat::R32G32B32_FLOAT )
            };
        }

        InputLayout P2CT = std::make_pair(Detail::P2CT_Elements, dimof(Detail::P2CT_Elements));
        InputLayout P2C = std::make_pair(Detail::P2C_Elements, dimof(Detail::P2C_Elements));
        InputLayout PCT = std::make_pair(Detail::PCT_Elements, dimof(Detail::PCT_Elements));
        InputLayout P = std::make_pair(Detail::P_Elements, dimof(Detail::P_Elements));
        InputLayout PC = std::make_pair(Detail::PC_Elements, dimof(Detail::PC_Elements));
        InputLayout PT = std::make_pair(Detail::PT_Elements, dimof(Detail::PT_Elements));
        InputLayout PN = std::make_pair(Detail::PN_Elements, dimof(Detail::PN_Elements));
        InputLayout PNT = std::make_pair(Detail::PNT_Elements, dimof(Detail::PNT_Elements));
        InputLayout PNTT = std::make_pair(Detail::PNTT_Elements, dimof(Detail::PNTT_Elements));
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
        bool gotBinding = false;

        for (unsigned s=0; s<dimof(_reflection); ++s) {
            auto i = LowerBound(_reflection[s]._uniformQuickLookup, hashName);
            if (i == _reflection[s]._uniformQuickLookup.end() || i->first != hashName) continue;

            assert(i->second._descriptorSet == stream);
            assert(stream < s_descriptorSetCount);

			// note --  expecting this object to be a uniform block. We should
            //          do another lookup to check the type of the object.
            VkDescriptorSetLayoutBinding binding;
            binding.binding = i->second._bindingPoint;
            binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;     // (note, see also VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
            binding.descriptorCount = 1;
            binding.stageFlags = AsShaderStageFlag(ShaderStage::Enum(s));   // note, can we combine multiple bindings into one by using multiple bits here?
            binding.pImmutableSamplers = nullptr;
            _bindings[stream].push_back(binding);
            gotBinding = true;
        }

        return gotBinding;
    }

    bool BoundUniforms::BindShaderResource(uint64 hashName, unsigned slot, unsigned stream)
    {
        bool gotBinding = false;

        for (unsigned s=0; s<dimof(_reflection); ++s) {
            auto i = LowerBound(_reflection[s]._uniformQuickLookup, hashName);
            if (i == _reflection[s]._uniformQuickLookup.end() || i->first != hashName) continue;

            assert(i->second._descriptorSet == stream);
            assert(stream < s_descriptorSetCount);

			// note --  We should validate the type of this object!
            VkDescriptorSetLayoutBinding binding;
            binding.binding = (i->second._bindingPoint == ~0x0) ? unsigned(_bindings[stream].size()) : i->second._bindingPoint;
            binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount = 1;
            binding.stageFlags = AsShaderStageFlag(ShaderStage::Enum(s));   // note, can we combine multiple bindings into one by using multiple bits here?
            binding.pImmutableSamplers = nullptr;
            _bindings[stream].push_back(binding);
            gotBinding = true;
        }

        return gotBinding;
    }

    static bool IsCBType(VkDescriptorType type)
    {
        return type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
            || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
            || type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
            || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
            ;
    }

    static bool IsShaderResType(VkDescriptorType type)
    {
        return type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
            || type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
            || type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
            ;
    }

    bool BoundUniforms::BindConstantBuffers(unsigned uniformsStream, std::initializer_list<const char*> cbs)
    {
            // expecting this method to be called before any other BindConstantBuffers 
            // operations for this uniformsStream (because we start from a zero index)
        assert(uniformsStream < s_descriptorSetCount);
        #if defined(_DEBUG)
            for (const auto& i:_bindings[uniformsStream])
                assert(!IsCBType(i.descriptorType));
        #endif

        bool result = true;
        for (auto c=cbs.begin(); c<cbs.end(); ++c)
            result &= BindConstantBuffer(Hash64(*c), unsigned(c-cbs.begin()), uniformsStream);
        return result;
    }

    bool BoundUniforms::BindConstantBuffers(unsigned uniformsStream, std::initializer_list<uint64> cbs)
    {
            // expecting this method to be called before any other BindConstantBuffers 
            // operations for this uniformsStream (because we start from a zero index)
        assert(uniformsStream < s_descriptorSetCount);
        #if defined(_DEBUG)
            for (const auto& i:_bindings[uniformsStream])
                assert(!IsCBType(i.descriptorType));
        #endif

        bool result = true;
        for (auto c=cbs.begin(); c<cbs.end(); ++c)
            result &= BindConstantBuffer(*c, unsigned(c-cbs.begin()), uniformsStream);
        return result;
    }

    bool BoundUniforms::BindShaderResources(unsigned uniformsStream, std::initializer_list<const char*> res)
    {
        assert(uniformsStream < s_descriptorSetCount);
        #if defined(_DEBUG)
            for (const auto& i:_bindings[uniformsStream])
                assert(!IsShaderResType(i.descriptorType));
        #endif

        bool result = true;
        for (auto c=res.begin(); c<res.end(); ++c)
            result &= BindShaderResource(Hash64(*c), unsigned(c-res.begin()), uniformsStream);
        return result;
    }

    bool BoundUniforms::BindShaderResources(unsigned uniformsStream, std::initializer_list<uint64> res)
    {
        assert(uniformsStream < s_descriptorSetCount);
        #if defined(_DEBUG)
            for (const auto& i:_bindings[uniformsStream])
                assert(!IsShaderResType(i.descriptorType));
        #endif

        bool result = true;
        for (auto c=res.begin(); c<res.end(); ++c)
            result &= BindShaderResource(*c, unsigned(c-res.begin()), uniformsStream);
        return result;
    }

    VulkanUniquePtr<VkDescriptorSetLayout> 
        BoundUniforms::CreateLayout(const ObjectFactory& factory, unsigned streamIndex) const
    {
        return factory.CreateDescriptorSetLayout(MakeIteratorRange(_bindings[streamIndex]));
    }

    const VulkanSharedPtr<VkPipelineLayout>& BoundUniforms::SharePipelineLayout(
        const ObjectFactory& factory,
        DescriptorPool& descriptorPool) const
    {
        if (!_pipelineLayout) {
            for (unsigned c=0; c<s_descriptorSetCount; ++c)
                _layouts[c] = CreateLayout(factory, c);

            VkDescriptorSetLayout rawLayouts[s_descriptorSetCount];
            for (unsigned c=0; c<s_descriptorSetCount; ++c) rawLayouts[c] = _layouts[c].get();
            _pipelineLayout = factory.CreatePipelineLayout(MakeIteratorRange(rawLayouts));

            descriptorPool.Allocate(
                MakeIteratorRange(_descriptorSets),
                MakeIteratorRange(rawLayouts));
        }
        return _pipelineLayout;
    }

    void BoundUniforms::Apply(  DeviceContext& context, 
                                const UniformsStream& stream0, 
                                const UniformsStream& stream1) const
    {
        auto pipelineLayout = SharePipelineLayout(GetDefaultObjectFactory(), context.GetGlobalPools()._mainDescriptorPool).get();

        // -------- write descriptor set --------
        const unsigned maxBindings = 16;
        VkDescriptorBufferInfo bufferInfo[maxBindings];
        VkDescriptorImageInfo imageInfo[maxBindings];
        VkWriteDescriptorSet writes[maxBindings];

        unsigned writeCount = 0, bufferCount = 0, imageCount = 0;

        const UniformsStream* streams[] = { &stream0, &stream1 };

        for (unsigned stri=0; stri<dimof(streams); ++stri) {
            const auto& s = *streams[stri];

            for (unsigned p=0; p<s._packetCount; ++p) {
                if (s._prebuiltBuffers[p]) {

                    assert(bufferCount < dimof(bufferInfo));
                    assert(writeCount < dimof(writes));

                    bufferInfo[bufferCount] = VkDescriptorBufferInfo{s._prebuiltBuffers[p]->GetUnderlying(), 0, VK_WHOLE_SIZE};

                    writes[writeCount] = {};
                    writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    writes[writeCount].dstSet = _descriptorSets[stri*2+0].get();
                    writes[writeCount].dstBinding = p;
                    writes[writeCount].descriptorCount = 1;
                    writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    writes[writeCount].pBufferInfo = &bufferInfo[bufferCount];
                    writes[writeCount].dstArrayElement = 0;

                    ++writeCount;
                    ++bufferCount;
                } else if (s._packets[p]) {
                    // todo -- append these onto a large buffer, or use push constant updates
                }
            }

            for (unsigned r=0; r<s._resourceCount; ++r) {
                if (s._resources[r]) {

                    assert(imageCount < dimof(imageInfo));
                    assert(writeCount < dimof(writes));

                    imageInfo[imageCount] = VkDescriptorImageInfo {
                        s._resources[r]->GetSampler().GetUnderlying(),
                        s._resources[r]->GetUnderlying(),
                        s._resources[r]->_layout };

                    writes[writeCount] = {};
                    writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    writes[writeCount].dstSet = _descriptorSets[stri*2+1].get();
                    writes[writeCount].dstBinding = r;
                    writes[writeCount].descriptorCount = 1;
                    writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    writes[writeCount].pImageInfo = &imageInfo[imageCount];
                    writes[writeCount].dstArrayElement = 0;

                    ++writeCount;
                    ++imageCount;
                }
            }
        }

        if (!writeCount) return;

        vkUpdateDescriptorSets(context.GetUnderlyingDevice(), writeCount, writes, 0, nullptr);

        auto cmdBuffer = context.GetPrimaryCommandList().get();

        VkDescriptorSet rawDescriptorSets[s_descriptorSetCount];
        for (unsigned c=0; c<s_descriptorSetCount; ++c) rawDescriptorSets[c] = _descriptorSets[c].get();

        vkCmdBindDescriptorSets(
            cmdBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout, 0, 
            dimof(rawDescriptorSets), rawDescriptorSets, 
            0, nullptr);
    }

    void BoundUniforms::UnbindShaderResources(DeviceContext& context, unsigned streamIndex) const
    {
    }

}}

