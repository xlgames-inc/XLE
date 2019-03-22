// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ShaderIntrospection.h"            // (for SetUniformCommandGroup)
#include "PipelineLayout.h"
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
        bool AllAttributesBound() const { return _allAttributesBound; }
        #if defined(EXTRA_INPUT_LAYOUT_PROPERTIES)
            IteratorRange<const std::string *> UnboundAttributesNames() const { return Utility::MakeIteratorRange(_unboundAttributesNames); }
        #endif

        static void SetWarnOnMissingVertexAttribute(bool warn) { _warnOnMissingVertexAttribute = warn; }

        BoundInputLayout() : _attributeState(0), _maxVertexAttributes(0), _vaoBindingHash(0), _allAttributesBound(true) {}
        BoundInputLayout(IteratorRange<const InputElementDesc*> layout, const ShaderProgram& program);

        struct SlotBinding
        {
            IteratorRange<const MiniInputElementDesc*> _elements;
            unsigned _instanceStepDataRate;     // set to 0 for per vertex, otherwise a per-instance rate
        };
        BoundInputLayout(
            IteratorRange<const SlotBinding*> layouts,
            const ShaderProgram& program);

        BoundInputLayout(BoundInputLayout&&) = default;
        BoundInputLayout& operator=(BoundInputLayout&&) = default;

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

        bool _allAttributesBound;
        #if defined(EXTRA_INPUT_LAYOUT_PROPERTIES)
            std::vector<std::string> _unboundAttributesNames;
        #endif

        static bool _warnOnMissingVertexAttribute;

        void UnderlyingApply(DeviceContext& devContext, IteratorRange<const VertexBufferView*> vertexBuffers) const never_throws;
        bool CalculateAllAttributesBound(const ShaderProgram& program);
    };

////////////////////////////////////////////////////////////////////////////////////////////////////

    class ShaderProgramCapturedState;

    class BoundUniforms
    {
    public:
        void Apply(
            DeviceContext& context,
            unsigned streamIdx,
            const UniformsStream& stream) const;

        uint64_t _boundUniformBufferSlots[4];
        uint64_t _boundResourceSlots[4];

        #if defined(_DEBUG)
        std::vector<ShaderIntrospection::Uniform> _unboundUniforms;
        #endif

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

        static unsigned s_uniformSetAccumulator;
        static unsigned s_redundantUniformSetAccumulator;
        static bool s_doRedundantUniformSetReduction;
    private:
        struct CB
        {
            unsigned _stream, _slot;
            SetUniformCommandGroup _commandGroup;
            unsigned _capturedStateIndex;
            DEBUG_ONLY(std::string _name;)
        };
        std::vector<CB> _cbs;
        struct SRV
        {
            unsigned _stream, _slot;
            unsigned _textureUnit;
            GLenum _dimensionality;
            DEBUG_ONLY(std::string _name;)
        };
        std::vector<SRV> _srvs;

        SetUniformCommandGroup  _textureAssignmentCommands;
        std::vector<uint8_t>    _textureAssignmentByteData;

        std::shared_ptr<ShaderProgramCapturedState> _capturedState;
    };

}}

