// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Format.h"
#include "Shader.h"
#include "ShaderIntrospection.h"
#include "../../RenderUtils.h"
#include "../../../Core/Exceptions.h"
#include "../../../Core/Types.h"
#include <algorithm>
#include <vector>
#include <memory>

namespace RenderCore { namespace Metal_OpenGLES
{
    class ShaderProgram;

    class BoundInputLayout
    {
    public:
        BoundInputLayout() {}
        BoundInputLayout(const InputLayout& layout, const ShaderProgram& program);

        BoundInputLayout(const BoundInputLayout&& moveFrom);
        BoundInputLayout& operator=(const BoundInputLayout& copyFrom);
        BoundInputLayout& operator=(const BoundInputLayout&& moveFrom);

        void Apply(const void* vertexBufferStart, unsigned vertexStride) const never_throws;

    private:
        class Binding
        {
        public:
            unsigned    _attributeIndex;
            unsigned    _size;
            unsigned    _type;
            bool        _isNormalized;
            unsigned    _stride;
            unsigned    _offset;
        };
        std::vector<Binding>        _bindings;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////

    class DeviceContext;
    using ConstantBufferPacket = SharedPkt;
    class ConstantBuffer;
    class ShaderResourceView;

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

#if 0
    class ConstantBufferLayout
    {
    public:
        using HashType = ParameterBox::ParameterNameHash;
        class Element
        {
        public:
            HashType        _nameHash;
            Format          _format;
            unsigned        _offset;
            unsigned        _arrayCount;
        };

        std::vector<Element> _elements;
        size_t _size;
    };
#endif

    class BoundUniforms
    {
    public:
        bool BindConstantBuffer(
            uint64 hashName, unsigned slot, unsigned uniformsStream,
            IteratorRange<const MiniInputElementDesc*> elements = {});

        void Apply(
            DeviceContext& context,
            const UniformsStream& stream0, const UniformsStream& stream1 = {}) const;

        BoundUniforms(ShaderProgram& shader);
        ~BoundUniforms();
    private:
        ShaderIntrospection _introspection;
        std::vector<SetUniformCommandGroup> _streamCBs[2];
    };

////////////////////////////////////////////////////////////////////////////////////////////////////

    /*
    inline InputElementDesc::InputElementDesc() {}
    inline InputElementDesc::InputElementDesc(  const std::string& name, unsigned semanticIndex, 
                                                NativeFormat::Enum nativeFormat, unsigned inputSlot, 
                                                unsigned alignedByteOffset, 
                                                InputClassification::Enum inputSlotClass,
                                                unsigned instanceDataStepRate)
    {
        _semanticName = name; _semanticIndex = semanticIndex;
        _nativeFormat = nativeFormat; _inputSlot = inputSlot;
        _alignedByteOffset = alignedByteOffset; _inputSlotClass = inputSlotClass;
        _instanceDataStepRate = instanceDataStepRate;
    }
    */

////////////////////////////////////////////////////////////////////////////////////////////////////

    inline UniformsStream::UniformsStream()
    {
        _packets = nullptr;
        _prebuiltBuffers = nullptr;
        _packetCount = 0;
        _resources = nullptr;
        _resourceCount = 0;
    }

    inline UniformsStream::UniformsStream(
        const ConstantBufferPacket packets[], const ConstantBuffer* prebuiltBuffers[], size_t packetCount,
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

