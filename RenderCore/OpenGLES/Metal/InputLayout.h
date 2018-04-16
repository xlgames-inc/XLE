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

namespace RenderCore { class VertexBufferView; class SharedPkt; }

namespace RenderCore { namespace Metal_OpenGLES
{
    class ShaderProgram;
    class PipelineLayoutConfig;
	class DeviceContext;

    class BoundInputLayout
    {
    public:
        void Apply(DeviceContext& context, IteratorRange<const VertexBufferView*> vertexBuffers) const never_throws;
        void CreateVAO(DeviceContext& context, IteratorRange<const VertexBufferView*> vertexBuffers);

        BoundInputLayout() : _attributeState(0) {}
        BoundInputLayout(IteratorRange<const InputElementDesc*> layout, const ShaderProgram& program);

        struct SlotBinding
        {
            IteratorRange<const MiniInputElementDesc*> _elements;
            unsigned _instanceStepDataRate;     // set to 0 for per vertex, otherwise a per-instance rate
        };
        BoundInputLayout(
            IteratorRange<const SlotBinding*> layouts,
            const ShaderProgram& program);

    private:
        class Binding
        {
        public:
            unsigned    _attributeLocation;
            unsigned    _size;
            unsigned    _type;
            bool        _isNormalized;
            unsigned    _stride;
            unsigned    _offset;
            unsigned    _instanceDataRate;
        };
        std::vector<Binding> _bindings;
        std::vector<unsigned> _bindingsByVertexBuffer;
        uint32_t _attributeState;

        uint32_t _maxVertexAttributes;

        intrusive_ptr<OpenGL::VAO> _vao;
        uint64_t _vaoBindingHash;

        void UnderlyingApply(DeviceContext& devContext, IteratorRange<const VertexBufferView*> vertexBuffers, bool useCache = true) const never_throws;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////

    class BoundUniforms
    {
    public:
        void Apply(
            DeviceContext& context,
            unsigned streamIdx,
            const UniformsStream& stream) const;

        uint64_t _boundUniformBufferSlots[4];
        uint64_t _boundResourceSlots[4];

        BoundUniforms(
            const ShaderProgram& shader,
            const PipelineLayoutConfig& pipelineLayout,
            const UniformsStreamInterface& interface0 = {},
            const UniformsStreamInterface& interface1 = {},
            const UniformsStreamInterface& interface2 = {},
            const UniformsStreamInterface& interface3 = {});
        BoundUniforms();
        ~BoundUniforms();

        BoundUniforms(BoundUniforms&& moveFrom) never_throws;
        BoundUniforms& operator=(BoundUniforms&& moveFrom) never_throws;
    private:
        struct CB { unsigned _stream, _slot; SetUniformCommandGroup _commandGroup; DEBUG_ONLY(std::string _name;) };
        std::vector<CB> _cbs;
        struct SRV { unsigned _stream, _slot; unsigned _textureUnit; GLenum _dimensionality; DEBUG_ONLY(std::string _name;) };
        std::vector<SRV> _srvs;

        SetUniformCommandGroup  _textureAssignmentCommands;
        std::vector<uint8_t>    _textureAssignmentByteData;
    };

}}

