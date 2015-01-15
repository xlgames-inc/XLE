// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Shader.h"
#include "../../RenderUtils.h"
#include "../../../Utility/Streams/FileUtils.h"
#include <GLES2/gl2.h>

#if PLATFORMOS_ACTIVE == PLATFORMOS_WINDOWS
    extern "C" dll_import void __stdcall OutputDebugStringA(const char lpOutputString[]);
#endif

namespace RenderCore { namespace Metal_OpenGLES
{
    CompiledShaderByteCode::CompiledShaderByteCode(const ResChar initializer[])
    {
        ShaderResId shaderPath(initializer);

        std::unique_ptr<unsigned char[]> rawFile;
        size_t size = 0;
        {
            BasicFile file(shaderPath._filename, "rb");
            file.Seek(0, SEEK_END);
            size = file.TellP();
            file.Seek(0, SEEK_SET);

            rawFile = std::make_unique<unsigned char[]>(size);
            file.Read(rawFile.get(), 1, size);
        }

        ShaderType::Enum shaderType = ShaderType::FragmentShader;
        if (shaderPath._shaderModel[0] == 'v') {
            shaderType = ShaderType::VertexShader;
        }

        auto newShaderIndex = CreateShader(shaderType);
        if (newShaderIndex.get() == RawGLHandle_Invalid) {
            ThrowException(Exceptions::AllocationFailure("Shader allocation failure"));
        }
        
        const GLchar* shaderSourcePointer = (const GLchar*)rawFile.get();
        GLint sourceLength = GLint(size);
        glShaderSource  ((GLuint)newShaderIndex.get(), 1, &shaderSourcePointer, &sourceLength);
        glCompileShader ((GLuint)newShaderIndex.get());

        GLint compileStatus = 0;
        glGetShaderiv   ((GLuint)newShaderIndex.get(), GL_COMPILE_STATUS, &compileStatus);
        if (!compileStatus) {
            #if defined(_DEBUG)

                GLint infoLen = 0;
                glGetShaderiv((GLuint)newShaderIndex.get(), GL_INFO_LOG_LENGTH, &infoLen);
                if ( infoLen > 1 ) {
                    auto infoLog = std::make_unique<GLchar[]>(sizeof(char) * infoLen);
                    glGetShaderInfoLog((GLuint)newShaderIndex.get(), infoLen, nullptr, infoLog.get());

                    #if PLATFORMOS_ACTIVE == PLATFORMOS_WINDOWS
                        OutputDebugStringA(infoLog.get());
                    #endif
                }

            #endif

            ThrowException(Assets::Exceptions::InvalidResource("", ""));
        }

        _underlying = std::move(newShaderIndex);
    }

    CompiledShaderByteCode::~CompiledShaderByteCode()
    {

    }

    ShaderProgram::ShaderProgram(   const ResChar vertexShaderInitializer[], 
                                    const ResChar fragmentShaderInitializer[])
    {
            // Note -- an exception in the second will trash the first unnecessarily.
        CompiledShaderByteCode vertexShader(vertexShaderInitializer);
        CompiledShaderByteCode fragmentShader(fragmentShaderInitializer);

        auto newProgramIndex = CreateShaderProgram();
        glAttachShader  ((GLuint)newProgramIndex.get(), (GLuint)vertexShader.GetUnderlying());
        glAttachShader  ((GLuint)newProgramIndex.get(), (GLuint)fragmentShader.GetUnderlying());
        glLinkProgram   ((GLuint)newProgramIndex.get());

        GLint linkStatus = 0;
        glGetProgramiv   ((GLuint)newProgramIndex.get(), GL_LINK_STATUS, &linkStatus);
        if (!linkStatus) {
            #if defined(_DEBUG)

                GLint infoLen = 0;
                glGetProgramiv((GLuint)newProgramIndex.get(), GL_INFO_LOG_LENGTH, &infoLen);
                if ( infoLen > 1 ) {
                    auto infoLog = std::make_unique<GLchar[]>(sizeof(char) * infoLen);
                    glGetProgramInfoLog((GLuint)newProgramIndex.get(), infoLen, nullptr, infoLog.get());

                    #if PLATFORMOS_ACTIVE == PLATFORMOS_WINDOWS
                        OutputDebugStringA(infoLog.get());
                    #endif
                }

            #endif

            ThrowException(Assets::Exceptions::InvalidResource("", ""));
        }

        _underlying = std::move(newProgramIndex);
    }

    ShaderProgram::~ShaderProgram()
    {
    }

}}

