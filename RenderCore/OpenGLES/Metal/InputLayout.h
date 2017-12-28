// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ShaderIntrospection.h"            // (for SetUniformCommandGroup)
#include "../../UniformsStream.h"
#include "../../../Utility/IteratorUtils.h"
#include <vector>

namespace RenderCore { namespace Metal_OpenGLES
{
    class ShaderProgram;

    class BoundInputLayout
    {
    public:
        void Apply(const void* vertexBufferStart, unsigned vertexStride) const never_throws;

        BoundInputLayout() : _attributeState(0) {}
        BoundInputLayout(IteratorRange<const InputElementDesc*> layout, const ShaderProgram& program);
        BoundInputLayout(IteratorRange<const MiniInputElementDesc*> layout, const ShaderProgram& program);
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
        std::vector<Binding> _bindings;
        uint32_t _attributeState;
    };

    class IPipelineLayout
    {
    public:
        virtual ~IPipelineLayout();
    };

    class BoundUniforms
    {
    public:
        void Apply(
            DeviceContext& context,
            const UniformsStream& stream0, 
            const UniformsStream& stream1 = {},
            const UniformsStream& stream2 = {}) const;

        BoundUniforms(
            const ShaderProgram& shader,
            const IPipelineLayout& pipelineLayout,
            const UniformsStreamInterface& interface0 = {},
            const UniformsStreamInterface& interface1 = {},
            const UniformsStreamInterface& interface2 = {});
        ~BoundUniforms();
    private:
        struct CB { unsigned _stream, _slot; SetUniformCommandGroup _commandGroup; };
        std::vector<CB> _cbs;
        struct SRV { unsigned _stream, _slot; unsigned _textureUnit; GLenum _dimensionality; };
        std::vector<SRV> _srvs;

        SetUniformCommandGroup  _textureAssignmentCommands;
        std::vector<uint8_t>    _textureAssignmentByteData;
    };

}}

