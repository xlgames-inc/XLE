// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Shader.h"
#include "../../ShaderService.h"
#include "../../RenderUtils.h"
#include "../../Types.h"
#include "../../../Assets/Assets.h"
#include "../../../Assets/DepVal.h"
#include "../../../Utility/Streams/FileUtils.h"
#include "../../../Utility/StringUtils.h"
#include "../../../Utility/MemoryUtils.h"
#include "IncludeGLES.h"

#if defined(XLE_HAS_CONSOLE_RIG)
    #include "../../../ConsoleRig/Log.h"
#endif

namespace RenderCore { namespace Metal_OpenGLES
{
    using ::Assets::ResChar;

    class OGLESShaderCompiler : public ShaderService::ILowLevelCompiler
    {
    public:
        virtual void AdaptShaderModel(
            ResChar destination[],
            const size_t destinationCount,
            StringSection<ResChar> inputShaderModel) const;

        virtual bool DoLowLevelCompile(
            /*out*/ Payload& payload,
            /*out*/ Payload& errors,
            /*out*/ std::vector<::Assets::DependentFileState>& dependencies,
            const void* sourceCode, size_t sourceCodeLength,
            const ShaderService::ResId& shaderPath,
            StringSection<::Assets::ResChar> definesTable) const;

        virtual std::string MakeShaderMetricsString(
            const void* byteCode, size_t byteCodeSize) const;

        OGLESShaderCompiler();
        ~OGLESShaderCompiler();

        static std::unordered_map<uint64_t, intrusive_ptr<OpenGL::Shader>> s_compiledShaders;
    };

    std::unordered_map<uint64_t, intrusive_ptr<OpenGL::Shader>> OGLESShaderCompiler::s_compiledShaders;

    void OGLESShaderCompiler::AdaptShaderModel(
        ResChar destination[],
        const size_t destinationCount,
        StringSection<ResChar> inputShaderModel) const
    {
        if (destination != inputShaderModel.begin())
            XlCopyString(destination, destinationCount, inputShaderModel);
    }

    bool OGLESShaderCompiler::DoLowLevelCompile(
        /*out*/ Payload& payload,
        /*out*/ Payload& errors,
        /*out*/ std::vector<::Assets::DependentFileState>& dependencies,
        const void* sourceCode, size_t sourceCodeLength,
        const ShaderService::ResId& shaderPath,
        StringSection<::Assets::ResChar> definesTable) const
    {
        ShaderType::Enum shaderType = ShaderType::FragmentShader;
        if (shaderPath._shaderModel[0] == 'v') {
            shaderType = ShaderType::VertexShader;
        }

        auto& objectFactory = GetObjectFactory();
        auto newShader = objectFactory.CreateShader(shaderType);
        if (newShader->AsRawGLHandle() == RawGLHandle_Invalid) {
            return false;
        }

        const GLchar* shaderSourcePointer = (const GLchar*)sourceCode;
        GLint shaderSourceLength = (GLint)sourceCodeLength;
        glShaderSource  (newShader->AsRawGLHandle(), 1, &shaderSourcePointer, &shaderSourceLength);
        glCompileShader (newShader->AsRawGLHandle());

        GLint compileStatus = 0;
        glGetShaderiv   (newShader->AsRawGLHandle(), GL_COMPILE_STATUS, &compileStatus);
        if (!compileStatus) {
            #if defined(_DEBUG)
                GLint infoLen = 0;
                glGetShaderiv(newShader->AsRawGLHandle(), GL_INFO_LOG_LENGTH, &infoLen);
                if ( infoLen > 1 ) {
                    errors = std::make_shared<std::vector<uint8>>(infoLen);
                    glGetShaderInfoLog(newShader->AsRawGLHandle(), infoLen, nullptr, (GLchar*)errors->data());
                }
            #endif

            return false;
        }

        uint64_t hashCode = Hash64(shaderSourcePointer, PtrAdd(shaderSourcePointer, shaderSourceLength));
        s_compiledShaders.emplace(std::make_pair(hashCode, std::move(newShader)));

        payload = std::make_shared<std::vector<uint8>>(sizeof(uint64_t));
        *(uint64_t*)payload->data() = hashCode;
        return true;
    }

    std::string OGLESShaderCompiler::MakeShaderMetricsString(const void* byteCode, size_t byteCodeSize) const { return std::string(); }

    OGLESShaderCompiler::OGLESShaderCompiler()
    {}
    OGLESShaderCompiler::~OGLESShaderCompiler()
    {}

    std::shared_ptr<ShaderService::ILowLevelCompiler> CreateLowLevelShaderCompiler(IDevice& device)
    {
        return std::make_shared<OGLESShaderCompiler>();
    }

    ShaderProgram::ShaderProgram(   const CompiledShaderByteCode& vertexShader,
                                    const CompiledShaderByteCode& fragmentShader)
    {
        auto vsByteCode = vertexShader.GetByteCode();
        auto fsByteCode = fragmentShader.GetByteCode();

        assert(vsByteCode.second == sizeof(uint64_t) && fsByteCode.second == sizeof(uint64_t));
        const auto& vs = OGLESShaderCompiler::s_compiledShaders[*(uint64_t*)vsByteCode.first];
        const auto& fs = OGLESShaderCompiler::s_compiledShaders[*(uint64_t*)fsByteCode.first];

        auto newProgramIndex = GetObjectFactory().CreateShaderProgram();
        glAttachShader  (newProgramIndex->AsRawGLHandle(), vs->AsRawGLHandle());
        glAttachShader  (newProgramIndex->AsRawGLHandle(), fs->AsRawGLHandle());
        glLinkProgram   (newProgramIndex->AsRawGLHandle());

        GLint linkStatus = 0;
        glGetProgramiv   (newProgramIndex->AsRawGLHandle(), GL_LINK_STATUS, &linkStatus);
        if (!linkStatus) {
            #if defined(_DEBUG)
                GLint infoLen = 0;
                glGetProgramiv(newProgramIndex->AsRawGLHandle(), GL_INFO_LOG_LENGTH, &infoLen);
                if ( infoLen > 1 ) {
                    auto infoLog = std::make_unique<GLchar[]>(sizeof(char) * infoLen);
                    glGetProgramInfoLog(newProgramIndex->AsRawGLHandle(), infoLen, nullptr, infoLog.get());

                    #if defined(XLE_HAS_CONSOLE_RIG)
                        LogWarning << (const char*)infoLog.get();
                    #endif
                }
            #endif

            Throw(Assets::Exceptions::InvalidAsset("", ""));
        }

        _underlying = std::move(newProgramIndex);

        _depVal = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_depVal, vertexShader.GetDependencyValidation());
        Assets::RegisterAssetDependency(_depVal, fragmentShader.GetDependencyValidation());
    }

    ShaderProgram::~ShaderProgram()
    {
    }

}}

