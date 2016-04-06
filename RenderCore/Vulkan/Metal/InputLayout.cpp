// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "InputLayout.h"
#include "Shader.h"
#include "ObjectFactory.h"
#include "../../ShaderService.h"
#include "../../../Utility/MemoryUtils.h"

namespace RenderCore { namespace Metal_Vulkan
{

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
            auto i = LowerBound(_reflection[s]._quickLookup, hashName);
            if (i == _reflection[s]._quickLookup.end() || i->first != hashName) continue;

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
            auto i = LowerBound(_reflection[s]._quickLookup, hashName);
            if (i == _reflection[s]._quickLookup.end() || i->first != hashName) continue;

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

    VulkanSharedPtr<VkDescriptorSetLayout> 
        BoundUniforms::CreateLayout(const ObjectFactory& factory, unsigned streamIndex) const
    {
        if (!_bindings[streamIndex].empty())
            return factory.CreateDescriptorSetLayout(MakeIteratorRange(_bindings[streamIndex]));
        return nullptr;
    }

    void BoundUniforms::Apply(  DeviceContext& context, 
                                const UniformsStream& stream0, 
                                const UniformsStream& stream1) const
    {
    }

    void BoundUniforms::UnbindShaderResources(DeviceContext& context, unsigned streamIndex) const
    {
    }

}}

