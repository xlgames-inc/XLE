// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DX11.h"
#include "Format.h"
#include "Buffer.h"
#include "../../RenderUtils.h"
#include "../../ShaderService.h"     // (just for ShaderStage enum)
#include "../../../Utility/IntrusivePtr.h"
#include "../../../Utility/MiniHeap.h"
#include <memory>
#include <vector>

namespace RenderCore { namespace Metal_DX11
{

        ////////////////////////////////////////////////////////////////////////////////////////////////

    /// Container for InputClassification::Enum
    namespace InputClassification
    {
        enum Enum { PerVertex, PerInstance };
    }

    class InputElementDesc
    {
    public:
        std::string                 _semanticName;
        unsigned                    _semanticIndex;
        NativeFormat::Enum          _nativeFormat;
        unsigned                    _inputSlot;
        unsigned                    _alignedByteOffset;
        InputClassification::Enum   _inputSlotClass;
        unsigned                    _instanceDataStepRate;

        InputElementDesc();
        InputElementDesc(   const std::string& name, unsigned semanticIndex, 
                            NativeFormat::Enum nativeFormat, unsigned inputSlot = 0, 
                            unsigned alignedByteOffset = ~unsigned(0x0), 
                            InputClassification::Enum inputSlotClass = InputClassification::PerVertex,
                            unsigned instanceDataStepRate = 0);
    };

    typedef std::pair<const InputElementDesc*, size_t>   InputLayout;

    unsigned CalculateVertexStride(
        const InputElementDesc* start, const InputElementDesc* end,
        unsigned slot);

    unsigned HasElement(const InputElementDesc* begin, const InputElementDesc* end, const char elementSemantic[]);
    unsigned FindElement(const InputElementDesc* begin, const InputElementDesc* end, const char elementSemantic[], unsigned semanticIndex = 0);

    /// Contains some common reusable vertex input layouts
    namespace GlobalInputLayouts
    {
        extern InputLayout P;
        extern InputLayout PC;
        extern InputLayout P2C;
        extern InputLayout P2CT;
        extern InputLayout PCT;
        extern InputLayout PT;
        extern InputLayout PN;
        extern InputLayout PNT;
        extern InputLayout PNTT;
    }

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class ShaderProgram;

    class BoundInputLayout
    {
    public:
        BoundInputLayout(const InputLayout& layout, const CompiledShaderByteCode& shader);
        BoundInputLayout(const InputLayout& layout, const ShaderProgram& shader);
        explicit BoundInputLayout(DeviceContext& context);
        BoundInputLayout();
        ~BoundInputLayout();

        typedef ID3D::InputLayout*  UnderlyingType;
        UnderlyingType              GetUnderlying() const { return _underlying.get(); }

    private:
        intrusive_ptr<ID3D::InputLayout>   _underlying;
        static intrusive_ptr<ID3D::InputLayout>   BuildInputLayout(const InputLayout& layout, const CompiledShaderByteCode& shader);
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class ConstantBufferLayoutElement
    {
    public:
        const char*         _name;
        NativeFormat::Enum  _format;
        unsigned            _offset;
        unsigned            _arrayCount;
    };

    class ConstantBufferLayoutElementHash
    {
    public:
        uint64              _name;
        NativeFormat::Enum  _format;
        unsigned            _offset;
        unsigned            _arrayCount;
    };

    class ConstantBufferLayout
    {
    public:
        size_t _size;
        std::unique_ptr<ConstantBufferLayoutElementHash[]> _elements;
        unsigned _elementCount;

        ConstantBufferLayout();
        ConstantBufferLayout(ConstantBufferLayout&& moveFrom);
        ConstantBufferLayout& operator=(ConstantBufferLayout&& moveFrom);

    protected:
        ConstantBufferLayout(const ConstantBufferLayout&);
        ConstantBufferLayout& operator=(const ConstantBufferLayout&);
    };

    class DeviceContext;
    class ShaderResourceView;
    class ShaderProgram;
    class DeepShaderProgram;

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
        BoundUniforms(BoundUniforms&& moveFrom);
        BoundUniforms& operator=(BoundUniforms&& moveFrom);

        bool BindConstantBuffer(    uint64 hashName, unsigned slot, unsigned uniformsStream,
                                    const ConstantBufferLayoutElement elements[] = nullptr, 
                                    size_t elementCount = 0);
        bool BindShaderResource(    uint64 hashName, unsigned slot, unsigned uniformsStream);

        bool BindConstantBuffers(unsigned uniformsStream, std::initializer_list<const char*> cbs);
        bool BindConstantBuffers(unsigned uniformsStream, std::initializer_list<uint64> cbs);

        bool BindShaderResources(unsigned uniformsStream, std::initializer_list<const char*> res);
        bool BindShaderResources(unsigned uniformsStream, std::initializer_list<uint64> res);

        ConstantBufferLayout                                GetConstantBufferLayout(const char name[]);
        std::vector<std::pair<ShaderStage::Enum,unsigned>>  GetConstantBufferBinding(const char name[]);

        void Apply( DeviceContext& context, 
                    const UniformsStream& stream0, const UniformsStream& stream1) const;
        void UnbindShaderResources(DeviceContext& context, unsigned streamIndex) const;
    private:

        class StageBinding
        {
        public:
            intrusive_ptr<ID3D::ShaderReflection>  _reflection;
            class Binding
            {
            public:
                unsigned _shaderSlot;
                unsigned _inputInterfaceSlot;
                mutable ConstantBuffer _savedCB;
            };
            std::vector<Binding>    _shaderConstantBindings;
            std::vector<Binding>    _shaderResourceBindings;

            StageBinding();
            ~StageBinding();
            StageBinding(StageBinding&& moveFrom);
            StageBinding& operator=(StageBinding&& moveFrom);
        };

        StageBinding    _stageBindings[ShaderStage::Max];
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class BoundClassInterfaces
    {
    public:
        void Bind(uint64 hashName, unsigned bindingArrayIndex, const char instance[]);

        const std::vector<intrusive_ptr<ID3D::ClassInstance>>& GetClassInstances(ShaderStage::Enum stage) const;

        BoundClassInterfaces(const ShaderProgram& shader);
        BoundClassInterfaces(const DeepShaderProgram& shader);
        BoundClassInterfaces();
        ~BoundClassInterfaces();

        BoundClassInterfaces(BoundClassInterfaces&& moveFrom);
        BoundClassInterfaces& operator=(BoundClassInterfaces&& moveFrom);
    private:
        class StageBinding
        {
        public:
            intrusive_ptr<ID3D::ShaderReflection>   _reflection;
            intrusive_ptr<ID3D::ClassLinkage>       _linkage;
            std::vector<intrusive_ptr<ID3D::ClassInstance>> _classInstanceArray;

            StageBinding();
            ~StageBinding();
            StageBinding(StageBinding&& moveFrom);
            StageBinding& operator=(StageBinding&& moveFrom);
        };

        StageBinding    _stageBindings[ShaderStage::Max];
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

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

