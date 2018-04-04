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
#include "../../../ConsoleRig/Log.h"
#include "../../../Utility/Streams/FileUtils.h"
#include "../../../Utility/StringUtils.h"
#include "../../../Utility/MemoryUtils.h"
#include "IncludeGLES.h"

#include <iostream>

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

        std::stringstream definesPreamble;
        {
            auto p = definesTable.begin();
            while (p != definesTable.end()) {
                while (p != definesTable.end() && std::isspace(*p)) ++p;

                auto definition = std::find(p, definesTable.end(), '=');
                auto defineEnd = std::find(p, definesTable.end(), ';');

                auto endOfName = std::min(defineEnd, definition);
                while ((endOfName-1) > p && std::isspace(*(endOfName-1))) ++endOfName;

                if (definition < defineEnd) {
                    auto e = definition+1;
                    while (e < defineEnd && std::isspace(*e)) ++e;
                    definesPreamble << "#define " << MakeStringSection(p, endOfName).AsString() << " " << MakeStringSection(e, defineEnd).AsString() << std::endl;
                } else {
                    definesPreamble << "#define " << MakeStringSection(p, endOfName).AsString() << std::endl;
                }

                p = (defineEnd == definesTable.end()) ? defineEnd : (defineEnd+1);
            }
        }
        auto definesPreambleStr = definesPreamble.str();

        bool isFragmentShader = shaderPath._shaderModel[0] == 'p';

        bool supportsGLES300 = strstr((const char*)sourceCode, "SUPPORT_GLSL300");

        #if PLATFORMOS_TARGET == PLATFORMOS_OSX
            // hack for version string for OSX
            const GLchar* versionDecl = isFragmentShader
                ? "#version 120\n#define FRAGMENT_SHADER 1\n#define NEW_UNIFORM_API 1\n"
                : "#version 120\n#define NEW_UNIFORM_API 1\n#define CC3_PLATFORM_WINDOWS 0\n"
                ;
            (void)supportsGLES300;
        #else
            const GLchar* versionDecl;
            if (supportsGLES300) {
                versionDecl = isFragmentShader
                    ? "#version 300 es\n#define FRAGMENT_SHADER 1\n#define NEW_UNIFORM_API 1\n"
                    : "#version 300 es\n#define NEW_UNIFORM_API 1\n";
            } else {
                versionDecl = isFragmentShader
                    ? "#define FRAGMENT_SHADER 1\n#define NEW_UNIFORM_API 1\n"
                    : "#define NEW_UNIFORM_API 1\n";
            }
        #endif

        // DavidJ -- hack -- make this platform preamble configurable, and set when constructing
        //      the ShaderCompiler
        const char* platformPreamble =
            #if APPORTABLE
                "#define CC3_PLATFORM_IOS 0\n"
                "#define CC3_PLATFORM_OSX 0\n"
                #ifdef _WIN32
                    "#define CC3_PLATFORM_ANDROID 0\n"
                    "#define CC3_PLATFORM_WINDOWS 1\n";
                #else
                    "#define CC3_PLATFORM_ANDROID 1\n"
                    "#define CC3_PLATFORM_WINDOWS 0\n";
                #endif
            #else
                "#define CC3_PLATFORM_IOS 1\n"
                "#define CC3_PLATFORM_OSX 0\n"
                "#define CC3_PLATFORM_ANDROID 0\n"
                "#define CC3_PLATFORM_WINDOWS 0\n";
            #endif    // APPORTABLE

        const GLchar* shaderSourcePointers[4] { versionDecl, platformPreamble, definesPreambleStr.data(), (const GLchar*)sourceCode };
        GLint shaderSourceLengths[4] = { (GLint)std::strlen(versionDecl), (GLint)std::strlen(platformPreamble), (GLint)definesPreambleStr.size(), (GLint)sourceCodeLength };

        glShaderSource  (newShader->AsRawGLHandle(), dimof(shaderSourcePointers), shaderSourcePointers, shaderSourceLengths);
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

                    std::cout << "Failure in shader:" << std::endl << versionDecl << definesPreambleStr << (const GLchar*)sourceCode << std::endl;
                    const char* e = (const char*)errors->data();
                    std::cout << "Errors:" << std::endl << e << std::endl;
                }
            #endif

            return false;
        }

        uint64_t hashCode = DefaultSeed64;
        for (unsigned c=0; c<dimof(shaderSourcePointers); ++c)
            hashCode = Hash64(shaderSourcePointers[c], PtrAdd(shaderSourcePointers[c], shaderSourceLengths[c]), hashCode);
        s_compiledShaders.emplace(std::make_pair(hashCode, std::move(newShader)));

        struct OutputBlob
        {
            ShaderService::ShaderHeader _hdr;
            uint64_t _hashCode;
        };
        payload = std::make_shared<std::vector<uint8>>(sizeof(OutputBlob));
        OutputBlob& output = *(OutputBlob*)payload->data();
        output._hdr = ShaderService::ShaderHeader { shaderPath._shaderModel, false };
        output._hashCode = hashCode;
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

    static uint32_t g_nextShaderProgramGUID = 0;

    ShaderProgram::ShaderProgram(   ObjectFactory& factory,
                                    const CompiledShaderByteCode& vertexShader,
                                    const CompiledShaderByteCode& fragmentShader)
    {
        auto vsByteCode = vertexShader.GetByteCode();
        auto fsByteCode = fragmentShader.GetByteCode();

        assert(vsByteCode.size() == sizeof(uint64_t) && fsByteCode.size() == sizeof(uint64_t));
        const auto& vs = OGLESShaderCompiler::s_compiledShaders[*(uint64_t*)vsByteCode.first];
        const auto& fs = OGLESShaderCompiler::s_compiledShaders[*(uint64_t*)fsByteCode.first];

        _depVal = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_depVal, vertexShader.GetDependencyValidation());
        Assets::RegisterAssetDependency(_depVal, fragmentShader.GetDependencyValidation());

        auto newProgramIndex = factory.CreateShaderProgram();
        glAttachShader  (newProgramIndex->AsRawGLHandle(), vs->AsRawGLHandle());
        glAttachShader  (newProgramIndex->AsRawGLHandle(), fs->AsRawGLHandle());
        glLinkProgram   (newProgramIndex->AsRawGLHandle());

        GLint linkStatus = 0;
        glGetProgramiv   (newProgramIndex->AsRawGLHandle(), GL_LINK_STATUS, &linkStatus);
        if (!linkStatus) {

            GLint infoLen = 0;
            ::Assets::Blob errorsLog;
            glGetProgramiv(newProgramIndex->AsRawGLHandle(), GL_INFO_LOG_LENGTH, &infoLen);
            if ( infoLen > 1 ) {
                errorsLog = std::make_shared<std::vector<uint8_t>>(sizeof(char) * infoLen);
                glGetProgramInfoLog(newProgramIndex->AsRawGLHandle(), infoLen, nullptr, (GLchar*)errorsLog->data());

                Log(Warning) << (const char*)errorsLog->data() << std::endl;
            }

            Throw(::Assets::Exceptions::ConstructionError(
                ::Assets::Exceptions::ConstructionError::Reason::FormatNotUnderstood,
                _depVal,
                errorsLog));
        }

        _underlying = std::move(newProgramIndex);

        _guid = g_nextShaderProgramGUID++;
    }

    ShaderProgram::~ShaderProgram()
    {
    }

}}

