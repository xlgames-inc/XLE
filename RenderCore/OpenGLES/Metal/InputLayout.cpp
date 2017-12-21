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
    BoundInputLayout::BoundInputLayout(const InputLayout& layout, const ShaderProgram& program)
    {
            //
            //      For each entry in "layout", we need to compare it's name
            //      with the names of the attributes in "shader".
            //
            //      When we find a match, write the details into a binding object.
            //      The binding object will be used to call glVertexAttribPointer.
            //
        const InputElementDesc* elements = layout.first;
        size_t elementsCount = layout.second;
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
            auto fmt = VertexAttributePointerAsFormat(size, type, true);
            assert(fmt == i->_nativeFormat);

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

    BoundInputLayout::BoundInputLayout(const BoundInputLayout&& moveFrom)
    :   _bindings(std::move(moveFrom._bindings))
    {
    }

    BoundInputLayout& BoundInputLayout::operator=(const BoundInputLayout&& moveFrom)
    {
        _bindings = std::move(moveFrom._bindings);
        return *this;
    }

    BoundInputLayout& BoundInputLayout::operator=(const BoundInputLayout& copyFrom)
    {
        _bindings = copyFrom._bindings;
        return *this;
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

    bool BoundUniforms::BindConstantBuffer(
        uint64 hashName, unsigned slot, unsigned uniformsStream,
        IteratorRange<const MiniInputElementDesc*> elements)
    {
        assert(uniformsStream < dimof(_streamCBs));
        if (_streamCBs[uniformsStream].size() < (slot+1))
            _streamCBs[uniformsStream].resize(slot+1);
        _streamCBs[uniformsStream][slot] = _introspection.MakeBinding(hashName, elements);
        return true;
    }

    void BoundUniforms::Apply(
        DeviceContext& context,
        const UniformsStream& stream0, const UniformsStream& stream1) const
    {
        const UniformsStream* inputStreams[] = { &stream0, &stream1 };
        for (unsigned str=0; str<2; ++str) {
            for (unsigned c=0; c<inputStreams[str]->_packetCount; ++c) {
                const auto& pkt = inputStreams[str]->_packets[c];
                if (!_streamCBs[str][c]._commands.empty() && pkt.size() != 0) {
                    Bind(context, _streamCBs[str][c], MakeIteratorRange(pkt.begin(), pkt.end()));
                }
            }
        }
    }

    BoundUniforms::BoundUniforms(ShaderProgram& shader)
    : _introspection(shader)
    {}

    BoundUniforms::~BoundUniforms() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

#if 0
    void BindConstantBuffer(const char name[], unsigned slot, const ConstantBufferLayoutElement elements[], size_t elementCount)
    {
            //
            //      For each element, find the binding location within the shader
            //
        _bindings.reserve(_bindings.size() + elementCount);
        for (unsigned c=0; c<elementCount; ++c) {
            Binding newBinding;
            newBinding._cbSlot       = slot;
            newBinding._shaderLoc    = glGetUniformLocation(_shader->AsRawGLHandle(), elements[c]._name);
            newBinding._cbOffset     = elements[c]._offset;

            if (elements[c]._format == NativeFormat::Matrix4x4) {
                newBinding._setterType   = 8;
            } else if (elements[c]._format == NativeFormat::Matrix3x4) {
                newBinding._setterType   = 9;
            } else {
                const unsigned componentCount = ComponentCount(GetComponents(elements[c]._format));
                newBinding._setterType   = componentCount - 1 + ((GetComponentType(elements[c]._format) == FormatComponentType::Float) ? 4 : 0);
            }
            _bindings.push_back(newBinding);
        }
    }

    void BoundUniforms::Apply(const DeviceContext&, std::shared_ptr<std::vector<uint8>> packets[], size_t packetCount)
    {
            //
            //      We've already calculating the binding rules.
            //      Only thing to do now is to send the data from inside the constant buffers to
            //      the uniform registers. Note that the shader must be already bound!
            //
        for (std::vector<Binding>::iterator i=_bindings.begin(); i!=_bindings.end(); ++i) {
            if (i->_cbSlot < packetCount) {
                const void* packetStart = AsPointer(packets[i->_cbSlot]->begin());
                switch (i->_setterType) {
                case 0: glUniform1i(i->_shaderLoc, *(GLint*)PtrAdd(packetStart, i->_cbOffset)); break;
                case 1: glUniform2i(i->_shaderLoc, *(GLint*)PtrAdd(packetStart, i->_cbOffset), *(GLint*)PtrAdd(packetStart, i->_cbOffset+4)); break;
                case 2: glUniform3i(i->_shaderLoc, *(GLint*)PtrAdd(packetStart, i->_cbOffset), *(GLint*)PtrAdd(packetStart, i->_cbOffset+4), *(GLint*)PtrAdd(packetStart, i->_cbOffset+8)); break;
                case 3: glUniform4i(i->_shaderLoc, *(GLint*)PtrAdd(packetStart, i->_cbOffset), *(GLint*)PtrAdd(packetStart, i->_cbOffset+4), *(GLint*)PtrAdd(packetStart, i->_cbOffset+8), *(GLint*)PtrAdd(packetStart, i->_cbOffset+12)); break;
                case 4: glUniform1f(i->_shaderLoc, *(GLfloat*)PtrAdd(packetStart, i->_cbOffset)); break;
                case 5: glUniform2f(i->_shaderLoc, *(GLfloat*)PtrAdd(packetStart, i->_cbOffset), *(GLfloat*)PtrAdd(packetStart, i->_cbOffset+4)); break;
                case 6: glUniform3f(i->_shaderLoc, *(GLfloat*)PtrAdd(packetStart, i->_cbOffset), *(GLfloat*)PtrAdd(packetStart, i->_cbOffset+4), *(GLfloat*)PtrAdd(packetStart, i->_cbOffset+8)); break;
                case 7: glUniform4f(i->_shaderLoc, *(GLfloat*)PtrAdd(packetStart, i->_cbOffset), *(GLfloat*)PtrAdd(packetStart, i->_cbOffset+4), *(GLfloat*)PtrAdd(packetStart, i->_cbOffset+8), *(GLfloat*)PtrAdd(packetStart, i->_cbOffset+12)); break;

                case 8: glUniformMatrix4fv(i->_shaderLoc, 1, GL_FALSE, (GLfloat*)PtrAdd(packetStart, i->_cbOffset));
                        break;

                case 9: // glUniformMatrix3fv(i->_shaderLoc, 1, GL_FALSE, (GLfloat*)PtrAdd(packetStart, i->_cbOffset));
                        glUniform4fv(i->_shaderLoc, 3, (GLfloat*)PtrAdd(packetStart, i->_cbOffset));
                        break;
                }
            }
        }
    }
#endif

}}

