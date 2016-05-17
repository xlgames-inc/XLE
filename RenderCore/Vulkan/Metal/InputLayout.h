// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "VulkanCore.h"
#include "ShaderReflection.h"
#include "IncludeVulkan.h"
#include "../../ShaderService.h"
#include "../../RenderUtils.h"
#include <memory>
#include <vector>

namespace RenderCore 
{
	class InputElementDesc;
	using InputLayout = std::pair<const InputElementDesc*, size_t>;
	enum class Format; 
}

namespace RenderCore { namespace Metal_Vulkan
{
	class DeviceContext;
	class ConstantBuffer;
	class ShaderResourceView;

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class ShaderProgram;

    class BoundInputLayout
    {
    public:
        BoundInputLayout(const InputLayout& layout, const CompiledShaderByteCode& shader);
        BoundInputLayout(const InputLayout& layout, const ShaderProgram& shader);
        BoundInputLayout();
        ~BoundInputLayout();

		BoundInputLayout(BoundInputLayout&& moveFrom) never_throws;
		BoundInputLayout& operator=(BoundInputLayout&& moveFrom) never_throws;

        const IteratorRange<const VkVertexInputAttributeDescription*> GetAttributes() const { return MakeIteratorRange(_attributes); }
    private:
        std::vector<VkVertexInputAttributeDescription> _attributes;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class ConstantBufferLayoutElement
    {
    public:
        const char*     _name;
        Format			_format;
        unsigned        _offset;
        unsigned        _arrayCount;
    };

    class ConstantBufferLayoutElementHash
    {
    public:
        uint64          _name;
		Format			_format;
        unsigned        _offset;
        unsigned        _arrayCount;
    };

    class ConstantBufferLayout
    {
    public:
        size_t _size;
        std::unique_ptr<ConstantBufferLayoutElementHash[]> _elements;
        unsigned _elementCount;

        ConstantBufferLayout() {}
        ConstantBufferLayout(ConstantBufferLayout&& moveFrom) {}
        ConstantBufferLayout& operator=(ConstantBufferLayout&& moveFrom) {}
        ConstantBufferLayout(const ConstantBufferLayout&) = delete;
        ConstantBufferLayout& operator=(const ConstantBufferLayout&) = delete;
    };

    class DeviceContext;
    class ShaderResourceView;
    class ShaderProgram;
    class DeepShaderProgram;
    class ObjectFactory;
    class DescriptorPool;

    typedef SharedPkt ConstantBufferPacket;

    class UniformsStream
    {
    public:
        UniformsStream();
        UniformsStream( const ConstantBufferPacket packets[], const ConstantBuffer* prebuiltBuffers[], size_t packetCount,
                        const ShaderResourceView* resources[] = nullptr, size_t resourceCount = 0);

        template <int Count0>
            UniformsStream( ConstantBufferPacket (&packets)[Count0]);
        template <int Count0, int Count1>
            UniformsStream( ConstantBufferPacket (&packets)[Count0],
                            const ConstantBuffer* (&prebuiltBuffers)[Count1]);
        template <int Count0, int Count1>
            UniformsStream( ConstantBufferPacket (&packets)[Count0],
                            const ShaderResourceView* (&resources)[Count1]);
        template <int Count0, int Count1, int Count2>
            UniformsStream( ConstantBufferPacket (&packets)[Count0],
                            const ConstantBuffer* (&prebuiltBuffers)[Count1],
                            const ShaderResourceView* (&resources)[Count2]);

        UniformsStream(
            std::initializer_list<const ConstantBufferPacket> cbs,
            std::initializer_list<const ShaderResourceView*> srvs);
    protected:
        const ConstantBufferPacket*     _packets;
        const ConstantBuffer*const*     _prebuiltBuffers;
        size_t                          _packetCount;
        const ShaderResourceView*const* _resources;
        size_t                          _resourceCount;

        friend class BoundUniforms;
    };

    class BoundUniforms
    {
    public:
        BoundUniforms(const ShaderProgram& shader);
        BoundUniforms(const DeepShaderProgram& shader);
        BoundUniforms(const CompiledShaderByteCode& shader);
        BoundUniforms(const BoundUniforms& copyFrom);
        BoundUniforms();
        ~BoundUniforms();
        BoundUniforms& operator=(const BoundUniforms& copyFrom);
        BoundUniforms(BoundUniforms&& moveFrom) never_throws;
        BoundUniforms& operator=(BoundUniforms&& moveFrom) never_throws;

        bool BindConstantBuffer(    uint64 hashName, unsigned slot, unsigned uniformsStream,
                                    const ConstantBufferLayoutElement elements[] = nullptr, 
                                    size_t elementCount = 0);
        bool BindShaderResource(    uint64 hashName, unsigned slot, unsigned uniformsStream);

        bool BindConstantBuffers(unsigned uniformsStream, std::initializer_list<const char*> cbs);
        bool BindConstantBuffers(unsigned uniformsStream, std::initializer_list<uint64> cbs);

        bool BindShaderResources(unsigned uniformsStream, std::initializer_list<const char*> res);
        bool BindShaderResources(unsigned uniformsStream, std::initializer_list<uint64> res);


        // todo -- the following should be moved into another object. Something that is created
        //      from the completed BoundUniforms
        void Apply( DeviceContext& context, 
                    const UniformsStream& stream0, const UniformsStream& stream1) const;
        void UnbindShaderResources(DeviceContext& context, unsigned streamIndex) const;

    private:
        SPIRVReflection _reflection[ShaderStage::Max];

        static const unsigned s_descriptorSetCount = 1;
        static const unsigned s_streamCount = 2;
        std::vector<uint32> _cbBindingIndices[s_streamCount];
        std::vector<uint32> _srvBindingIndices[s_streamCount];
        uint64  _shaderBindingMask[s_descriptorSetCount];
        bool _isComputeShader;

        mutable VulkanUniquePtr<VkDescriptorSet> _descriptorSets[s_descriptorSetCount];

        void BuildShaderBindingMask();
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class BoundClassInterfaces
    {
    public:
        void Bind(uint64 hashName, unsigned bindingArrayIndex, const char instance[]) {}

        BoundClassInterfaces(const ShaderProgram& shader) {}
        BoundClassInterfaces(const DeepShaderProgram& shader) {}
        BoundClassInterfaces() {}
        ~BoundClassInterfaces() {}

        BoundClassInterfaces(BoundClassInterfaces&& moveFrom) {}
        BoundClassInterfaces& operator=(BoundClassInterfaces&& moveFrom) { return *this; }
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    inline UniformsStream::UniformsStream()
    {
        _packets = nullptr;
        _prebuiltBuffers = nullptr;
        _packetCount = 0;
        _resources = nullptr;
        _resourceCount = 0;
    }

    inline UniformsStream::UniformsStream(  const ConstantBufferPacket packets[], const ConstantBuffer* prebuiltBuffers[], size_t packetCount,
                                            const ShaderResourceView* resources[], size_t resourceCount)
    {
        _packets = packets;
        _prebuiltBuffers = prebuiltBuffers;
        _packetCount = packetCount;
        _resources = resources;
        _resourceCount = resourceCount;
    }

    template <int Count0>
        UniformsStream::UniformsStream(ConstantBufferPacket (&packets)[Count0])
        {
            _packets = packets;
            _prebuiltBuffers = nullptr;
            _packetCount = Count0;
            _resources = nullptr;
            _resourceCount = 0;
        }
        
    template <int Count0, int Count1>
        UniformsStream::UniformsStream( ConstantBufferPacket (&packets)[Count0],
                                        const ConstantBuffer* (&prebuildBuffers)[Count1])
        {
            static_assert(Count0 == Count1, "Expecting equal length arrays in UniformsStream constructor");
            _packets = packets;
            _prebuiltBuffers = prebuildBuffers;
            _packetCount = Count0;
            _resources = nullptr;
            _resourceCount = 0;
        }

    template <int Count0, int Count1>
        UniformsStream::UniformsStream( ConstantBufferPacket (&packets)[Count0],
                                        const ShaderResourceView* (&resources)[Count1])
        {
            _packets = packets;
            _prebuiltBuffers = nullptr;
            _packetCount = Count0;
            _resources = resources;
            _resourceCount = Count1;
        }

    template <int Count0, int Count1, int Count2>
        UniformsStream::UniformsStream( ConstantBufferPacket (&packets)[Count0],
                                        const ConstantBuffer* (&prebuiltBuffers)[Count1],
                                        const ShaderResourceView* (&resources)[Count2])
    {
            static_assert(Count0 == Count1, "Expecting equal length arrays in UniformsStream constructor");
            _packets = packets;
            _prebuiltBuffers = prebuiltBuffers;
            _packetCount = Count0;
            _resources = resources;
            _resourceCount = Count2;
    }

    inline UniformsStream::UniformsStream(
        std::initializer_list<const ConstantBufferPacket> cbs,
        std::initializer_list<const ShaderResourceView*> srvs)
    {
            // note -- this is really dangerous!
            //      we're taking pointers into the initializer_lists. This is fine
            //      if the lifetime of UniformsStream is longer than the initializer_list
            //      (which is common in many use cases of this class).
            //      But there is no protection to make sure that the memory here is valid
            //      when it is used!
            // Use at own risk!
        _packets = cbs.begin();
        _prebuiltBuffers = nullptr;
        _packetCount = cbs.size();
        _resources = srvs.begin();
        _resourceCount = srvs.size();
    }

}}

