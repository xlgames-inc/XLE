// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Format.h"
#include "Shader.h"
#include "../../../Core/Exceptions.h"
#include "../../../Core/Types.h"
#include <algorithm>
#include <vector>
#include <memory>

namespace RenderCore { namespace Metal_OpenGLES
{

        ////////////////////////////////////////////////////////////////////////////////////////////////

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

    namespace GlobalInputLayouts
    {
        extern InputLayout P2CT;
        extern InputLayout PCT;
    }

        ////////////////////////////////////////////////////////////////////////////////////////////////

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

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class ConstantBufferLayoutElement
    {
    public:
        const char*         _name;
        NativeFormat::Enum  _format;
        unsigned            _offset;
    };

    class DeviceContext;

    class BoundUniforms
    {
    public:
        BoundUniforms(ShaderProgram& shader);
        ~BoundUniforms();

        void BindConstantBuffer(const char name[], unsigned slot, const ConstantBufferLayoutElement elements[], size_t elementCount);
        void Apply(const DeviceContext& context, std::shared_ptr<std::vector<uint8>> packets[], size_t packetCount);
    private:
        intrusive_ptr<OpenGL::ShaderProgram>     _shader;
        class Binding
        {
        public:
            unsigned    _cbSlot;
            unsigned    _cbOffset;
            unsigned    _shaderLoc;
            unsigned    _setterType;
        };
        std::vector<Binding> _bindings;
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

}}

