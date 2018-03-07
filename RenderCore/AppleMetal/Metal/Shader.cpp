// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Shader.h"
#include "Device.h"
#include "../../ShaderService.h"
#include "../../RenderUtils.h"
#include "../../Types.h"
#include "../../../Assets/Assets.h"
#include "../../../Assets/DepVal.h"
#include "../../../ConsoleRig/Log.h"
#include "../../../Utility/Streams/FileUtils.h"
#include "../../../Utility/StringUtils.h"
#include "../../../Utility/MemoryUtils.h"
#include <iostream>

#include "IncludeAppleMetal.h"
#import <Metal/MTLLibrary.h>
#import <Foundation/NSObject.h>

static id<MTLLibrary> s_defaultLibrary = nil;

namespace RenderCore { namespace Metal_AppleMetal
{
    using ::Assets::ResChar;

    class ShaderCompiler : public ShaderService::ILowLevelCompiler
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

        ShaderCompiler();
        ~ShaderCompiler();
    };

    void ShaderCompiler::AdaptShaderModel(
        ResChar destination[],
        const size_t destinationCount,
        StringSection<ResChar> inputShaderModel) const
    {
        if (destination != inputShaderModel.begin())
            XlCopyString(destination, destinationCount, inputShaderModel);
    }

    bool ShaderCompiler::DoLowLevelCompile(
        /*out*/ Payload& payload,
        /*out*/ Payload& errors,
        /*out*/ std::vector<::Assets::DependentFileState>& dependencies,
        const void* sourceCode, size_t sourceCodeLength,
        const ShaderService::ResId& shaderPath,
        StringSection<::Assets::ResChar> definesTable) const
    {
        // KenD -- Metal TODO -- consider compiling Metal shaders from source code; for now, just using Metal library
        return true;
    }

    std::string ShaderCompiler::MakeShaderMetricsString(const void* byteCode, size_t byteCodeSize) const { return std::string(); }

    ShaderCompiler::ShaderCompiler()
    {}
    ShaderCompiler::~ShaderCompiler()
    {}

    std::shared_ptr<ShaderService::ILowLevelCompiler> CreateLowLevelShaderCompiler(IDevice& device)
    {
        // KenD -- Metal HACK -- holding on to a static Metal library for now.  We should cache it, but we might consider a different approach to construction
        auto* dev = (ImplAppleMetal::Device*)device.QueryInterface(typeid(ImplAppleMetal::Device).hash_code());
        id<MTLDevice> mtlDev = dev->GetUnderlying();
        s_defaultLibrary = [mtlDev newDefaultLibrary];

        return std::make_shared<ShaderCompiler>();
    }

    static uint32_t g_nextShaderProgramGUID = 0;

    ShaderProgram::ShaderProgram(   const CompiledShaderByteCode& vertexShader,
                                    const CompiledShaderByteCode& fragmentShader)
    {
        // KenD -- Metal TODO -- architecture should support CompiledShaderByteCode
        assert(0);
        _guid = g_nextShaderProgramGUID++;
    }

    ShaderProgram::~ShaderProgram()
    {
    }

    ShaderProgram::ShaderProgram(const std::string& vertexFunctionName, const std::string& fragmentFunctionName)
    {
        _vf = [s_defaultLibrary newFunctionWithName:[NSString stringWithCString:vertexFunctionName.c_str() encoding:NSUTF8StringEncoding]];
        _ff = [s_defaultLibrary newFunctionWithName:[NSString stringWithCString:fragmentFunctionName.c_str() encoding:NSUTF8StringEncoding]];

        _depVal = std::make_shared<Assets::DependencyValidation>();

        _guid = g_nextShaderProgramGUID++;
    }
}}
