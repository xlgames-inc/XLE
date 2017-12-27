// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "InputLayout.h"
#include "Shader.h"
#include "ShaderIntrospection.h"
#include "Format.h"
#include "../../Types.h"
#include "../../Format.h"
#include "../../../Utility/StringUtils.h"
#include "../../../Utility/StringFormat.h"
#include "../../../Utility/PtrUtils.h"
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
            }

            lastElementEnd = elementStart + elementSize;
        }
    }

    BoundInputLayout::BoundInputLayout(IteratorRange<const MiniInputElementDesc*> layout, const ShaderProgram& program)
    {
        _bindings.reserve(layout.size());

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
        }
    }

    void BoundInputLayout::Apply(const void* vertexBufferStart, unsigned vertexStride) const never_throws
    {
        int maxVertexAttributes;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttributes);
        for (int c=0; c<maxVertexAttributes; ++c) {
            glDisableVertexAttribArray(c);
        }

        for(auto i=_bindings.begin(); i!=_bindings.end(); ++i) {
            glVertexAttribPointer( 
                i->_attributeIndex, i->_size, i->_type, i->_isNormalized, 
                vertexStride?vertexStride:i->_stride, 
                PtrAdd(vertexBufferStart, i->_offset));

            glEnableVertexAttribArray(i->_attributeIndex);
        }
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    void BoundUniforms::Apply(
        DeviceContext& context,
        const UniformsStream& stream0, const UniformsStream& stream1, const UniformsStream& stream2) const
    {
        const UniformsStream* inputStreams[] = { &stream0, &stream1, &stream2 };
        for (unsigned str=0; str<2; ++str) {
            for (unsigned c=0; c<inputStreams[str]->_constantBuffers.size(); ++c) {
                const auto& pkt = inputStreams[str]->_constantBuffers[c]._packet;
                if (!_streamCBs[str][c]._commands.empty() && pkt.size() != 0) {
                    Bind(context, _streamCBs[str][c], MakeIteratorRange(pkt.begin(), pkt.end()));
                }
            }
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

        const UniformsStreamInterface* inputInterface[] = { &interface0, &interface1, &interface2 };
        size_t streamCount = std::min(dimof(_streamCBs), dimof(inputInterface));
        for (size_t s=0; s<streamCount; ++s) {
            const UniformsStreamInterface& interf = *inputInterface[s];
            for (unsigned b=0; b<interf._cbBindings.size(); ++b) {
                const auto& binding = interf._cbBindings[b];
                auto slot = binding.first;
                if (_streamCBs[s].size() < (slot+1))
                    _streamCBs[s].resize(slot+1);

                _streamCBs[s][slot] = introspection.MakeBinding(
                    binding.second._hashName, MakeIteratorRange(binding.second._elements));
            }
        }
    }

    BoundUniforms::~BoundUniforms() {}


    IPipelineLayout::~IPipelineLayout() {}

}}

