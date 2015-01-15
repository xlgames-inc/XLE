// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "InputLayout.h"
#include "Shader.h"
#include "Format.h"
#include "../../../Utility/StringUtils.h"
#include "../../../Utility/StringFormat.h"
#include "../../../Utility/PtrUtils.h"
#include <GLES2/gl2.h>

#if PLATFORMOS_ACTIVE == PLATFORMOS_WINDOWS
    extern "C" dll_import void __stdcall OutputDebugStringA(const char lpOutputString[]);
#endif

namespace RenderCore { namespace Metal_OpenGLES
{
    static unsigned     ComponentCount      (FormatComponents::Enum components)     
    { 
        switch (components) {
        default:
        case FormatComponents::Alpha:
        case FormatComponents::Luminance:
        case FormatComponents::Depth:             return 1;
        case FormatComponents::LuminanceAlpha:
        case FormatComponents::RG:                return 2;
        case FormatComponents::RGB:               return 3;
        case FormatComponents::RGBAlpha:
        case FormatComponents::RGBE:              return 4;
        }
    }

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
            GLint attribute = glGetAttribLocation((GLuint)programIndex, buffer);

            const unsigned elementSize      = BitsPerPixel(elements[c]._nativeFormat) / 8;
            const unsigned elementStart     = (elements[c]._alignedByteOffset != ~unsigned(0x0)) ? elements[c]._alignedByteOffset : lastElementEnd;

                //
                //  The index can be left off the string for the first
                //  one
                //
            if (attribute == -1 && elements[c]._semanticIndex == 0) {
                attribute = glGetAttribLocation((GLuint)programIndex, elements[c]._semanticName.c_str());
            }

            if (attribute < 0 || attribute >= maxVertexAttributes) {
                    //  Binding failure! Write a warning, but ignore it. The binding is 
                    //  still valid even if one or more attributes fail
                #if PLATFORMOS_ACTIVE == PLATFORMOS_WINDOWS
                    OutputDebugStringA(XlDynFormatString("Warning -- failure during vertex attribute binding. Attribute (%s) cannot be found in the program. Ignoring.\n", buffer).c_str());
                #endif
            } else {
                    // (easier with C++11 {} initializer and emplace_back)
                const FormatComponentType::Enum componentType = GetComponentType(elements[c]._nativeFormat);
                Binding b = {
                    unsigned(attribute), 
                    ComponentCount(GetComponents(elements[c]._nativeFormat)), AsGLVertexComponentType(elements[c]._nativeFormat), 
                    (componentType == FormatComponentType::UNorm) || (componentType == FormatComponentType::SNorm) || (componentType == FormatComponentType::UNorm_SRGB),
                    unsigned(vertexStride),
                    elementStart
                    };
                _bindings.push_back(b);
            }

            lastElementEnd = elementStart + elementSize;
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

    namespace GlobalInputLayouts
    {
        namespace Detail
        {
            InputElementDesc P2CT_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, NativeFormat::R32G32_FLOAT   ),
                InputElementDesc( "COLOR",      0, NativeFormat::R8G8B8A8_UNORM ),
                InputElementDesc( "TEXCOORD",   0, NativeFormat::R32G32_FLOAT   )
            };
        }

        InputLayout P2CT = std::make_pair(Detail::P2CT_Elements, dimof(Detail::P2CT_Elements));
    }


    BoundUniforms::BoundUniforms(ShaderProgram& shader)
    : _shader(shader.GetUnderlying())
    {
    }

    BoundUniforms::~BoundUniforms() {}

    void BoundUniforms::BindConstantBuffer(const char name[], unsigned slot, const ConstantBufferLayoutElement elements[], size_t elementCount)
    {
            //
            //      For each element, find the binding location within the shader
            //
        _bindings.reserve(_bindings.size() + elementCount);
        for (unsigned c=0; c<elementCount; ++c) {
            Binding newBinding;
            newBinding._cbSlot       = slot;
            newBinding._shaderLoc    = glGetUniformLocation((GLuint)_shader.get(), elements[c]._name);
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

}}

