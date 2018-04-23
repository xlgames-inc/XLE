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
#include <unordered_map>

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
            StringSection<::Assets::ResChar> definesTable,
            IteratorRange<const ShaderService::SourceLineMarker*> sourceLineMarkers) const;

        virtual std::string MakeShaderMetricsString(
            const void* byteCode, size_t byteCodeSize) const;

        ShaderCompiler() = delete;
        ShaderCompiler(id<MTLDevice> device);
        ~ShaderCompiler();

        TBC::OCPtr<id> _device; // MTLDevice
        static std::unordered_map<uint64_t, TBC::OCPtr<id>> s_compiledShaders;
    };

    std::unordered_map<uint64_t, TBC::OCPtr<id>> ShaderCompiler::s_compiledShaders;

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
        StringSection<::Assets::ResChar> definesTable,
        IteratorRange<const ShaderService::SourceLineMarker*> sourceLineMarkers) const
    {
        std::stringstream definesPreamble;
        NSMutableDictionary* preprocessorMacros = [[NSMutableDictionary alloc] init];
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
                    NSString* key = [NSString stringWithCString:MakeStringSection(p, endOfName).AsString().c_str() encoding:NSUTF8StringEncoding];
                    NSString* value = [NSString stringWithCString:MakeStringSection(e, defineEnd).AsString().c_str() encoding:NSUTF8StringEncoding];
                    preprocessorMacros[key] = value;
                    definesPreamble << "#define " << MakeStringSection(p, endOfName).AsString() << " " << MakeStringSection(e, defineEnd).AsString() << std::endl;
                } else {
                    NSString* key = [NSString stringWithCString:MakeStringSection(p, endOfName).AsString().c_str() encoding:NSUTF8StringEncoding];
                    preprocessorMacros[key] = @(1);
                    definesPreamble << "#define " << MakeStringSection(p, endOfName).AsString() << std::endl;
                }

                p = (defineEnd == definesTable.end()) ? defineEnd : (defineEnd+1);
            }
        }

        bool isFragmentShader = shaderPath._shaderModel[0] == 'p';

        NSString *s = [NSString stringWithCString:(const char*)sourceCode encoding:NSUTF8StringEncoding];
        // #include <metal_stdlib>
        // using namespace metal;
        // #define METAL_INSTEAD_OF_OPENGL
        s = [@"#include <metal_stdlib>\nusing namespace metal;\n#define METAL_INSTEAD_OF_OPENGL\n" stringByAppendingString:s];

#if PLATFORMOS_TARGET == PLATFORMOS_OSX
        // hack for version string for OSX
        const char* versionDecl = isFragmentShader ? "#define FRAGMENT_SHADER 1\n#define NEW_UNIFORM_API 1\n" : "#define NEW_UNIFORM_API 1\n#define CC3_PLATFORM_WINDOWS 0\n";
#else
        const char* versionDecl = isFragmentShader ? "#define FRAGMENT_SHADER 1\n#define NEW_UNIFORM_API 1\n" : "#define NEW_UNIFORM_API 1\n#define CC3_PLATFORM_WINDOWS 0\n";
#endif
        s = [[NSString stringWithCString:versionDecl encoding:NSUTF8StringEncoding] stringByAppendingString:s];

        s = [[NSString stringWithCString:definesPreamble.str().c_str() encoding:NSUTF8StringEncoding] stringByAppendingString:s];

        s = [s stringByReplacingOccurrencesOfString:@"__VERSION__" withString:@"110"];

        auto definesPreambleStr = definesPreamble.str();
        const char* shaderSourcePointers[3] { versionDecl, definesPreambleStr.data(), (const char*)sourceCode };
        unsigned shaderSourceLengths[3] = { (unsigned)std::strlen(versionDecl), (unsigned)definesPreambleStr.size(), (unsigned)sourceCodeLength };

        MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
        options.languageVersion = MTLLanguageVersion1_1;
        [preprocessorMacros release];
        NSError* error = NULL;
        id<MTLLibrary> newLibrary = [_device.get() newLibraryWithSource:s
                                                                options:options
                                                                  error:&error];
        [options release];
        if (!newLibrary) {
#if defined(_DEBUG)
            // Failure in shader
            std::cout << "Failed to create library from source:" << std::endl << definesPreambleStr << [s UTF8String] << std::endl;
            std::cout << "Errors:" << std::endl << [[error description] UTF8String] << std::endl;
#endif
            return false;
        }
        assert(newLibrary);

        uint64_t hashCode = DefaultSeed64;
        /* TODO: improve hash for shader library */
        for (unsigned c=0; c<dimof(shaderSourcePointers); ++c)
            hashCode = Hash64(shaderSourcePointers[c], PtrAdd(shaderSourcePointers[c], shaderSourceLengths[c]), hashCode);
        s_compiledShaders.emplace(std::make_pair(hashCode, newLibrary));

        struct OutputBlob
        {
            ShaderService::ShaderHeader _hdr;
            uint64_t _hashCode;
        };
        payload = std::make_shared<std::vector<uint8>>(sizeof(OutputBlob));
        OutputBlob& output = *(OutputBlob*)payload->data();
        StringMeld<dimof(ShaderService::ShaderHeader::_identifier)> identifier;
        identifier << shaderPath._filename << "-" << shaderPath._entryPoint << "-" << std::hex << hashCode;
        output._hdr = ShaderService::ShaderHeader { identifier.AsStringSection(), shaderPath._shaderModel, false };
        output._hashCode = hashCode;
        return true;
    }

    std::string ShaderCompiler::MakeShaderMetricsString(const void* byteCode, size_t byteCodeSize) const { return std::string(); }

    ShaderCompiler::ShaderCompiler(id<MTLDevice> device)
    : _device(device)
    {}
    ShaderCompiler::~ShaderCompiler()
    {}

    std::shared_ptr<ShaderService::ILowLevelCompiler> CreateLowLevelShaderCompiler(IDevice& device)
    {
        // KenD -- Metal HACK -- holding on to a static Metal library for now.  We should cache it, but we might consider a different approach to construction
        auto* dev = (ImplAppleMetal::Device*)device.QueryInterface(typeid(ImplAppleMetal::Device).hash_code());
        id<MTLDevice> metalDevice = dev->GetUnderlying();
        assert(metalDevice);
        /* default library may be useful for some things still... quick testing of ad-hoc functions, perhaps */
        s_defaultLibrary = [metalDevice newDefaultLibrary];

        return std::make_shared<ShaderCompiler>(metalDevice);
    }

    static uint32_t g_nextShaderProgramGUID = 0;

    ShaderProgram::ShaderProgram(   ObjectFactory&,
                                    const CompiledShaderByteCode& vertexShader,
                                    const CompiledShaderByteCode& fragmentShader)
    {
        /* Get the vertex and fragment function from the MTLLibraries */

        auto vsByteCode = vertexShader.GetByteCode();
        auto fsByteCode = fragmentShader.GetByteCode();

        assert(vsByteCode.size() == sizeof(uint64_t) && fsByteCode.size() == sizeof(uint64_t));
        const auto& vs = ShaderCompiler::s_compiledShaders[*(uint64_t*)vsByteCode.first];
        const auto& fs = ShaderCompiler::s_compiledShaders[*(uint64_t*)fsByteCode.first];

        assert(vs && fs);

        _depVal = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_depVal, vertexShader.GetDependencyValidation());
        Assets::RegisterAssetDependency(_depVal, fragmentShader.GetDependencyValidation());

        id<MTLLibrary> vertexLibrary = vs.get();
        id<MTLLibrary> fragmentLibrary = fs.get();

        assert([vertexLibrary functionNames].count == 1);
        assert([fragmentLibrary functionNames].count == 1);
        /* as for what function to call, there should only be one function in the library */
        _vf = [vertexLibrary newFunctionWithName:[[vertexLibrary functionNames] firstObject]];
        _ff = [fragmentLibrary newFunctionWithName:[[fragmentLibrary functionNames] firstObject]];

        _guid = g_nextShaderProgramGUID++;
    }

    ShaderProgram::~ShaderProgram()
    {
    }

    ShaderProgram::ShaderProgram(const std::string& vertexFunctionName, const std::string& fragmentFunctionName)
    {
        /* this function can be useful on an ad-hoc basis, but otherwise, could remove it */
        assert(s_defaultLibrary);
        _vf = [s_defaultLibrary newFunctionWithName:[NSString stringWithCString:vertexFunctionName.c_str() encoding:NSUTF8StringEncoding]];
        _ff = [s_defaultLibrary newFunctionWithName:[NSString stringWithCString:fragmentFunctionName.c_str() encoding:NSUTF8StringEncoding]];
        assert(_vf);
        assert(_ff);

        _depVal = std::make_shared<Assets::DependencyValidation>();

        _guid = g_nextShaderProgramGUID++;
    }
}}
