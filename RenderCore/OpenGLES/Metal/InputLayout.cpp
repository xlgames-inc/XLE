// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "InputLayout.h"
#include "Shader.h"
#include "ShaderIntrospection.h"
#include "Format.h"
#include "ShaderResource.h"
#include "../../Types.h"
#include "../../Format.h"
#include "../../BufferView.h"
#include "../../../Utility/StringUtils.h"
#include "../../../Utility/StringFormat.h"
#include "../../../Utility/PtrUtils.h"
#include "../../../Utility/ArithmeticUtils.h"
#include "IncludeGLES.h"

#if defined(XLE_HAS_CONSOLE_RIG)
    #include "../../../ConsoleRig/Log.h"
#endif

namespace RenderCore { namespace Metal_OpenGLES
{
    BoundInputLayout::BoundInputLayout(IteratorRange<const InputElementDesc*> layout, const ShaderProgram& program)
    {
            //
            //      For each entry in "layout", we need to compare it's name
            //      with the names of the attributes in "shader".
            //
            //      When we find a match, write the details into a binding object.
            //      The binding object will be used to call glVertexAttribPointer.
            //
        const InputElementDesc* elements = layout.begin();
        size_t elementsCount = layout.size();
        _bindings.reserve(elementsCount);
        _attributeState = 0;

        size_t vertexStride = 0;
        {
            unsigned lastElementEnd = 0;
            for (size_t c=0; c<elementsCount; ++c) {
                const unsigned elementSize  = BitsPerPixel(elements[c]._nativeFormat) / 8;
                const unsigned elementStart = 
                    (elements[c]._alignedByteOffset != ~unsigned(0x0))?elements[c]._alignedByteOffset:lastElementEnd;
                vertexStride = std::max(vertexStride, size_t(elementStart + elementSize));
                lastElementEnd = elementStart + elementSize;
            }
        }

        auto programIndex = program.GetUnderlying();

        int maxVertexAttributes;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttributes);

        unsigned lastElementEnd = 0;
        for (size_t c=0; c<elementsCount; ++c) {
            char buffer[64];
            XlCopyString(buffer, elements[c]._semanticName.c_str());
            XlCatString(buffer, dimof(buffer), char('0' + elements[c]._semanticIndex));
            GLint attribute = glGetAttribLocation(programIndex->AsRawGLHandle(), buffer);

            const unsigned elementSize      = BitsPerPixel(elements[c]._nativeFormat) / 8;
            const unsigned elementStart     = (elements[c]._alignedByteOffset != ~unsigned(0x0)) ? elements[c]._alignedByteOffset : lastElementEnd;

                //
                //  The index can be left off the string for the first
                //  one
                //
            if (attribute == -1 && elements[c]._semanticIndex == 0) {
                attribute = glGetAttribLocation(programIndex->AsRawGLHandle(), elements[c]._semanticName.c_str());
            }

            if (attribute < 0 || attribute >= maxVertexAttributes) {
                    //  Binding failure! Write a warning, but ignore it. The binding is 
                    //  still valid even if one or more attributes fail
                #if defined(XLE_HAS_CONSOLE_RIG)
                    LogWarning << "Failure during vertex attribute binding. Attribute (" << buffer << ") cannot be found in the program. Ignoring" << std::endl;
                #endif
            } else {
                const auto componentType = GetComponentType(elements[c]._nativeFormat);
                _bindings.push_back({
                        unsigned(attribute),
                        GetComponentCount(GetComponents(elements[c]._nativeFormat)), AsGLVertexComponentType(elements[c]._nativeFormat),
                        (componentType == FormatComponentType::UNorm) || (componentType == FormatComponentType::SNorm) || (componentType == FormatComponentType::UNorm_SRGB),
                        unsigned(vertexStride),
                        elementStart
                    });

                assert(!(_attributeState & 1<<unsigned(attribute)));
                _attributeState |= 1<<unsigned(attribute);
            }

            lastElementEnd = elementStart + elementSize;
        }
    }

    BoundInputLayout::BoundInputLayout(IteratorRange<const MiniInputElementDesc*> layout, const ShaderProgram& program)
    {
        _bindings.reserve(layout.size());
        _attributeState = 0;

        auto vertexStride = CalculateVertexStride(layout, false);
        auto programHandle = program.GetUnderlying()->AsRawGLHandle();

        int activeAttributeCount = 0, activeAttributeMaxLength = 0;
        glGetProgramiv(programHandle, GL_ACTIVE_ATTRIBUTES, &activeAttributeCount);
        glGetProgramiv(programHandle, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &activeAttributeMaxLength);

        GLchar buffer[activeAttributeMaxLength];

        for (int a=0; a<activeAttributeCount; ++a) {
            GLint size; GLenum type;
            GLsizei nameLen;
            glGetActiveAttrib(programHandle, a, activeAttributeMaxLength, &nameLen, &size, &type, buffer);
            if (!nameLen) continue;

            auto semanticIdx = nameLen;
            while (std::isdigit(buffer[semanticIdx-1])) --semanticIdx;
            uint64_t hash = Hash64(buffer, &buffer[semanticIdx]);
            hash += std::atoi(&buffer[semanticIdx]);
            auto i = std::find_if(layout.begin(), layout.end(), [hash](const MiniInputElementDesc&l) { return l._semanticHash == hash; });
            if (i == layout.end()) {
                #if defined(XLE_HAS_CONSOLE_RIG)
                    LogWarning << "Failure during vertex attribute binding. Attribute (" << buffer << ") cannot be found in the input binding. Ignoring" << std::endl;
                #endif
                continue;
            }

            const auto elementStart = CalculateVertexStride(MakeIteratorRange(layout.begin(), i), false);
            // auto fmt = VertexAttributePointerAsFormat(size, type, true);
            // assert(fmt == i->_nativeFormat);

            const auto componentType = GetComponentType(i->_nativeFormat);
            _bindings.push_back({
                    unsigned(a),
                    GetComponentCount(GetComponents(i->_nativeFormat)), AsGLVertexComponentType(i->_nativeFormat),
                    (componentType == FormatComponentType::UNorm) || (componentType == FormatComponentType::SNorm) || (componentType == FormatComponentType::UNorm_SRGB),
                    unsigned(vertexStride),
                    elementStart
                });

            assert(!(_attributeState & 1<<unsigned(a)));
            _attributeState |= 1<<unsigned(a);
        }
    }

    void BoundInputLayout::Apply(const void* vertexBufferStart) const never_throws
    {
        for(auto i=_bindings.begin(); i!=_bindings.end(); ++i)
            glVertexAttribPointer( 
                i->_attributeIndex, i->_size, i->_type, i->_isNormalized, 
                i->_stride,
                PtrAdd(vertexBufferStart, i->_offset));

        // set enable/disable flags --
        // Note that this method cannot support more than 32 vertex attributes
        int maxVertexAttributes;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttributes);

        int firstActive = xl_ctz4(_attributeState);
        int lastActive = 32u - xl_clz4(_attributeState);

        int c=0;
        for (; c<firstActive; ++c)
            glDisableVertexAttribArray(c);
        for (; c<lastActive; ++c)
            if (_attributeState & (1<<c)) {
                glEnableVertexAttribArray(c);
            } else {
                glDisableVertexAttribArray(c);
            }
        for (; c<maxVertexAttributes; ++c)
            glDisableVertexAttribArray(c);
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    void BoundUniforms::Apply(
        DeviceContext& context,
        const UniformsStream& stream0, const UniformsStream& stream1, const UniformsStream& stream2) const
    {
        const UniformsStream* inputStreams[] = { &stream0, &stream1, &stream2 };
        for (const auto&cb:_cbs) {
            assert(cb._stream < dimof(inputStreams));
            assert(cb._slot < inputStreams[cb._stream]->_constantBuffers.size());
            const auto& pkt = inputStreams[cb._stream]->_constantBuffers[cb._slot]._packet;
            if (!cb._commandGroup._commands.empty() && pkt.size() != 0)
                Bind(context, cb._commandGroup, MakeIteratorRange(pkt.begin(), pkt.end()));
        }

        for (const auto&srv:_srvs) {
            assert(srv._stream < dimof(inputStreams));
            assert(srv._slot < inputStreams[srv._stream]->_resources.size());
            const auto& res = *inputStreams[srv._stream]->_resources[srv._slot];
            glActiveTexture(GL_TEXTURE0 + srv._textureUnit);
            glBindTexture(srv._dimensionality, res.GetUnderlying()->AsRawGLHandle());
        }

        // Commit changes to texture uniforms
        // This must be done separately to the texture binding, because when using array uniforms,
        // a single uniform set operation can be used for multiple texture bindings
        if (!_textureAssignmentCommands._commands.empty())
            Bind(context, _textureAssignmentCommands, MakeIteratorRange(_textureAssignmentByteData));
    }

    static GLenum DimensionalityForUniformType(GLenum uniformType)
    {
        switch (uniformType) {
        case GL_SAMPLER_2D:
        case GL_SAMPLER_2D_SHADOW:
        case GL_INT_SAMPLER_2D:
        case GL_UNSIGNED_INT_SAMPLER_2D:
            return GL_TEXTURE_2D;

        case GL_SAMPLER_CUBE:
            return GL_TEXTURE_CUBE_MAP;

        default:
            return GL_NONE;
        }
    }

    BoundUniforms::BoundUniforms(
        const ShaderProgram& shader,
        const IPipelineLayout& pipelineLayout,
        const UniformsStreamInterface& interface0,
        const UniformsStreamInterface& interface1,
        const UniformsStreamInterface& interface2)
    {
        auto introspection = ShaderIntrospection(shader);

        unsigned textureUnitAccumulator = 0;

        struct UniformSet { int _location; unsigned _index, _value; GLenum _type; int _elementCount; };
        std::vector<UniformSet> uniformSets;

        const UniformsStreamInterface* inputInterface[] = { &interface0, &interface1, &interface2 };
        auto streamCount = (unsigned)dimof(inputInterface);
        for (unsigned s=0; s<streamCount; ++s) {
            const UniformsStreamInterface& interf = *inputInterface[s];
            for (unsigned b=0; b<interf._cbBindings.size(); ++b) {
                const auto& binding = interf._cbBindings[b];
                auto cmdGroup = introspection.MakeBinding(binding.second._hashName, MakeIteratorRange(binding.second._elements));
                _cbs.emplace_back(CB{s, binding.first, std::move(cmdGroup)});
            }

            for (unsigned b=0; b<interf._srvBindings.size(); ++b) {
                const auto& binding = interf._srvBindings[b];
                auto uniform = introspection.FindUniform(binding.second);
                if (uniform._elementCount != 0) {
                    // assign a texture unit for this binding
                    auto textureUnit = textureUnitAccumulator++;
                    auto dim = DimensionalityForUniformType(uniform._type);
                    assert(dim != GL_NONE);
                    _srvs.emplace_back(SRV{s, binding.first, textureUnit, dim});

                    // Record the command to set the uniform. Note that this
                    // is made a little more complicated due to array uniforms.
                    assert((binding.second - uniform._bindingName) < uniform._elementCount);
                    uniformSets.push_back({uniform._location, unsigned(binding.second - uniform._bindingName), textureUnit, uniform._type, uniform._elementCount});
                }
            }
        }

        // sort the uniform sets to collect up sequential sets on the same uniforms
        std::sort(
            uniformSets.begin(), uniformSets.end(),
            [](const UniformSet& lhs, const UniformSet& rhs) {
                if (lhs._location < rhs._location) return true;
                if (lhs._location > rhs._location) return false;
                return lhs._index < rhs._index;
            });

        // Now generate the set commands that will assign the uniforms as required
        auto i = uniformSets.begin();
        while (i!=uniformSets.end()) {
            auto i2 = i+1;
            while (i2 != uniformSets.end() && i2->_location == i->_location) { ++i2; };

            // unsigned maxIndex = (i2-1)->_index;
            unsigned elementCount = i->_elementCount;
            auto dataOffsetStart = _textureAssignmentByteData.size();
            _textureAssignmentByteData.resize(dataOffsetStart+sizeof(GLuint)*(elementCount+1), 0u);
            GLuint* dst = (GLuint*)&_textureAssignmentByteData[dataOffsetStart];
            for (auto q=i; q<i2; ++q)
                dst[q->_index] = q->_value;
            _textureAssignmentCommands._commands.push_back(
                SetUniformCommandGroup::SetCommand{i->_location, i->_type, elementCount, dataOffsetStart});

            i = i2;
        }
    }

    BoundUniforms::~BoundUniforms() {}

    IPipelineLayout::~IPipelineLayout() {}

}}

