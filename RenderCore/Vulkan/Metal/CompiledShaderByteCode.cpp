// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Shader.h"
#include "DeviceContext.h"
#include "IncludeVulkan.h"
#include "../../ShaderService.h"
#include "../../ShaderLangUtil.h"
#include "../../../Assets/AssetServices.h"
#include "../../../Assets/InvalidAssetManager.h"

#define EXCLUDE_PSTDINT
#include "hlslcc.hpp"
#include "ShaderLang.h"
#include "../../SPIRV/GlslangToSpv.h"

namespace RenderCore { namespace Metal_DX11
{
    std::shared_ptr<ShaderService::ILowLevelCompiler> CreateVulkanPrecompiler();
}}

namespace RenderCore { namespace Metal_Vulkan
{
    using ::Assets::ResChar;

    class HLSLToSPIRVCompiler : public ShaderService::ILowLevelCompiler
    {
    public:
        virtual void AdaptShaderModel(
            ResChar destination[], 
            const size_t destinationCount,
            const ResChar source[]) const;

        virtual bool DoLowLevelCompile(
            /*out*/ Payload& payload,
            /*out*/ Payload& errors,
            /*out*/ std::vector<::Assets::DependentFileState>& dependencies,
            const void* sourceCode, size_t sourceCodeLength,
            const ShaderService::ResId& shaderPath,
            const ::Assets::ResChar definesTable[]) const;

        virtual std::string MakeShaderMetricsString(
            const void* byteCode, size_t byteCodeSize) const;

        HLSLToSPIRVCompiler(std::shared_ptr<ShaderService::ILowLevelCompiler> hlslCompiler);
        ~HLSLToSPIRVCompiler();

    private:
        std::shared_ptr<ShaderService::ILowLevelCompiler> _hlslCompiler;

        static std::weak_ptr<HLSLToSPIRVCompiler> s_instance;
        friend std::shared_ptr<ShaderService::ILowLevelCompiler> CreateLowLevelShaderCompiler();
    };

        ////////////////////////////////////////////////////////////

    void HLSLToSPIRVCompiler::AdaptShaderModel(
        ResChar destination[], 
        const size_t destinationCount,
        const ResChar inputShaderModel[]) const
    {
        // _hlslCompiler->AdaptShaderModel(destination, destinationCount, inputShaderModel);

        assert(inputShaderModel);
        if (inputShaderModel[0] != '\0') {
            size_t length = XlStringLen(inputShaderModel);

            //  Some shaders end with vs_*, gs_*, etc..
            //  Change this to the default shader model version. We should use
            //  version 5.0 in this case. We haven't initialized a D3D device (so we
            //  don't have to care about compatibility with any physical device).
            //  As long as the installed shader compile dll supports 5.0, then we
            //  should be ok.
            if (inputShaderModel[length-1] == '*') {
                const char* bestShaderModel = "5_0";
                if (destination != inputShaderModel) 
                    XlCopyString(destination, destinationCount, inputShaderModel);
                destination[std::min(length-1, destinationCount-1)] = '\0';
                XlCatString(destination, destinationCount, bestShaderModel);
                return;
            }
        }

        if (destination != inputShaderModel) 
            XlCopyString(destination, destinationCount, inputShaderModel);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static TBuiltInResource CreateTBuiltInResource()
    {
        TBuiltInResource result;
        result.maxLights = 32;
        result.maxClipPlanes = 6;
        result.maxTextureUnits = 32;
        result.maxTextureCoords = 32;
        result.maxVertexAttribs = 64;
        result.maxVertexUniformComponents = 4096;
        result.maxVaryingFloats = 64;
        result.maxVertexTextureImageUnits = 32;
        result.maxCombinedTextureImageUnits = 80;
        result.maxTextureImageUnits = 32;
        result.maxFragmentUniformComponents = 4096;
        result.maxDrawBuffers = 32;
        result.maxVertexUniformVectors = 128;
        result.maxVaryingVectors = 8;
        result.maxFragmentUniformVectors = 16;
        result.maxVertexOutputVectors = 16;
        result.maxFragmentInputVectors = 15;
        result.minProgramTexelOffset = -8;
        result.maxProgramTexelOffset = 7;
        result.maxClipDistances = 8;
        result.maxComputeWorkGroupCountX = 65535;
        result.maxComputeWorkGroupCountY = 65535;
        result.maxComputeWorkGroupCountZ = 65535;
        result.maxComputeWorkGroupSizeX = 1024;
        result.maxComputeWorkGroupSizeY = 1024;
        result.maxComputeWorkGroupSizeZ = 64;
        result.maxComputeUniformComponents = 1024;
        result.maxComputeTextureImageUnits = 16;
        result.maxComputeImageUniforms = 8;
        result.maxComputeAtomicCounters = 8;
        result.maxComputeAtomicCounterBuffers = 1;
        result.maxVaryingComponents = 60;
        result.maxVertexOutputComponents = 64;
        result.maxGeometryInputComponents = 64;
        result.maxGeometryOutputComponents = 128;
        result.maxFragmentInputComponents = 128;
        result.maxImageUnits = 8;
        result.maxCombinedImageUnitsAndFragmentOutputs = 8;
        result.maxCombinedShaderOutputResources = 8;
        result.maxImageSamples = 0;
        result.maxVertexImageUniforms = 0;
        result.maxTessControlImageUniforms = 0;
        result.maxTessEvaluationImageUniforms = 0;
        result.maxGeometryImageUniforms = 0;
        result.maxFragmentImageUniforms = 8;
        result.maxCombinedImageUniforms = 8;
        result.maxGeometryTextureImageUnits = 16;
        result.maxGeometryOutputVertices = 256;
        result.maxGeometryTotalOutputComponents = 1024;
        result.maxGeometryUniformComponents = 1024;
        result.maxGeometryVaryingComponents = 64;
        result.maxTessControlInputComponents = 128;
        result.maxTessControlOutputComponents = 128;
        result.maxTessControlTextureImageUnits = 16;
        result.maxTessControlUniformComponents = 1024;
        result.maxTessControlTotalOutputComponents = 4096;
        result.maxTessEvaluationInputComponents = 128;
        result.maxTessEvaluationOutputComponents = 128;
        result.maxTessEvaluationTextureImageUnits = 16;
        result.maxTessEvaluationUniformComponents = 1024;
        result.maxTessPatchComponents = 120;
        result.maxPatchVertices = 32;
        result.maxTessGenLevel = 64;
        result.maxViewports = 16;
        result.maxVertexAtomicCounters = 0;
        result.maxTessControlAtomicCounters = 0;
        result.maxTessEvaluationAtomicCounters = 0;
        result.maxGeometryAtomicCounters = 0;
        result.maxFragmentAtomicCounters = 8;
        result.maxCombinedAtomicCounters = 8;
        result.maxAtomicCounterBindings = 1;
        result.maxVertexAtomicCounterBuffers = 0;
        result.maxTessControlAtomicCounterBuffers = 0;
        result.maxTessEvaluationAtomicCounterBuffers = 0;
        result.maxGeometryAtomicCounterBuffers = 0;
        result.maxFragmentAtomicCounterBuffers = 1;
        result.maxCombinedAtomicCounterBuffers = 1;
        result.maxAtomicCounterBufferSize = 16384;
        result.maxTransformFeedbackBuffers = 4;
        result.maxTransformFeedbackInterleavedComponents = 64;
        result.maxCullDistances = 8;
        result.maxCombinedClipAndCullDistances = 8;
        result.maxSamples = 4;
        result.limits.nonInductiveForLoops = 1;
        result.limits.whileLoops = 1;
        result.limits.doWhileLoops = 1;
        result.limits.generalUniformIndexing = 1;
        result.limits.generalAttributeMatrixVectorIndexing = 1;
        result.limits.generalVaryingIndexing = 1;
        result.limits.generalSamplerIndexing = 1;
        result.limits.generalVariableIndexing = 1;
        result.limits.generalConstantMatrixVectorIndexing = 1;
        return result;
    }

    #if 0
        static EShLanguage AsEShLanguage(const VkShaderStageFlagBits shader_type) 
        {
            switch (shader_type) {
            case VK_SHADER_STAGE_VERTEX_BIT:                    return EShLangVertex;
            case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:      return EShLangTessControl;
            case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:   return EShLangTessEvaluation;
            case VK_SHADER_STAGE_GEOMETRY_BIT:                  return EShLangGeometry;
            case VK_SHADER_STAGE_FRAGMENT_BIT:                  return EShLangFragment;
            case VK_SHADER_STAGE_COMPUTE_BIT:                   return EShLangCompute;
            default:                                            return EShLangVertex;
            }
        }
    #endif

    static EShLanguage GLSLShaderTypeToEShLanguage(unsigned glslShaderType)
    {
        switch (glslShaderType)
        {
        case 0x8B31: // GL_VERTEX_SHADER_ARB;
            return EShLangVertex;
        default:
        case 0x8B30: // GL_FRAGMENT_SHADER_ARB
            return EShLangFragment;
        case 0x8DD9: // GL_GEOMETRY_SHADER;
            return EShLangGeometry;
        case 0x8E87: // GL_TESS_EVALUATION_SHADER;
            return EShLangTessEvaluation;
        case 0x8E88: // GL_TESS_CONTROL_SHADER;
            return EShLangTessControl;
        case 0x91B9: // GL_COMPUTE_SHADER;
            return EShLangCompute;
        }
    }

    static void AppendErrors(
        std::shared_ptr<std::vector<uint8>>& errors,
        glslang::TShader& shader)
    {
        auto infoLog = shader.getInfoLog();
        auto infoDebugLog = shader.getInfoDebugLog();

        std::stringstream result;
        result << "--- Info log ---" << std::endl;
        result << infoLog << std::endl;
        result << "--- Info debug log ---" << std::endl;
        result << infoDebugLog << std::endl;

        auto str = result.str();
        if (!errors) errors = std::make_shared<std::vector<uint8>>();
        errors->insert(
            errors->end(),
            AsPointer(str.begin()), AsPointer(str.end()));
    }

    static bool GLSLtoSPV(
        /*out*/ std::shared_ptr<std::vector<uint8>>& payload,
        /*out*/ std::shared_ptr<std::vector<uint8>>& errors,
        EShLanguage shaderType,
        const char glslSource[]) 
    {
        // This function is derived from the Vulkan SDK samples
        auto builtInLimits = CreateTBuiltInResource();

        // Enable SPIR-V and Vulkan rules when parsing GLSL
        EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
        messages = (EShMessages)(messages|EShMsgAST);

        glslang::TShader shader(shaderType);

        const char *shaderStrings[1];
        shaderStrings[0] = glslSource;
        shader.setStrings(shaderStrings, 1);
        if (!shader.parse(&builtInLimits, 100, false, messages)) {
            AppendErrors(errors, shader);
            return false;
        }

        glslang::TProgram program;
        program.addShader(&shader);

        if (!program.link(messages)) {
            AppendErrors(errors, shader);
            return false;
        }

        // Awkwardly, GlslangToSpv only writes to a vector of "unsigned". 
        // But we're expecting to return a vector of "uint8". There's no way
        // to move the block from one vector type to another...!
        std::vector<unsigned> spirv;
        glslang::GlslangToSpv(*program.getIntermediate(shaderType), spirv);

        auto spirvBlockSize = spirv.size() * sizeof(unsigned);
        payload = std::make_shared<std::vector<uint8>>(spirvBlockSize + sizeof(ShaderService::ShaderHeader));

        *(ShaderService::ShaderHeader*)AsPointer(payload->begin())
            = ShaderService::ShaderHeader { ShaderService::ShaderHeader::Version, false };

        std::memcpy(
            PtrAdd(AsPointer(payload->begin()), sizeof(ShaderService::ShaderHeader)),
            AsPointer(spirv.begin()), spirvBlockSize);

        return true;
    }

    bool HLSLToSPIRVCompiler::DoLowLevelCompile(
        /*out*/ std::shared_ptr<std::vector<uint8>>& payload,
        /*out*/ std::shared_ptr<std::vector<uint8>>& errors,
        /*out*/ std::vector<::Assets::DependentFileState>& dependencies,
        const void* sourceCode, size_t sourceCodeLength,
        const ShaderService::ResId& shaderPath,
        const ::Assets::ResChar definesTable[]) const
    {
        // So, this is a complex process for converting from HLSL source code into SPIR-V.
        // In time, hopefully there will be better solutions for this problem.
        // But, let's go HLSL -> HLSL bytecode -> GLSL -> SPIR-V

        // First, attempt to compile the HLSL code into byte code (using D3D compilers)
        std::shared_ptr<std::vector<uint8>> hlslBytecode;
        bool hlslGood = _hlslCompiler->DoLowLevelCompile(
            hlslBytecode, errors, dependencies,
            sourceCode, sourceCodeLength, shaderPath, definesTable);
        if (!hlslGood) return false;

        // Second, HLSL bytecode -> GLSL source
        // We're going to be using James-Jones' cross compiler: 
        //      https://github.com/James-Jones/HLSLCrossCompiler
        GlExtensions ext;
        ext.ARB_explicit_attrib_location = 0;
        ext.ARB_explicit_uniform_location = 0;
        ext.ARB_shading_language_420pack = 0;
        GLSLCrossDependencyData depData = {};
        GLSLShader glslShader;
        auto* bytecodeStart = (const char*)PtrAdd(AsPointer(hlslBytecode->begin()), sizeof(ShaderService::ShaderHeader));
        auto translateResult = TranslateHLSLFromMem(
            bytecodeStart,
            HLSLCC_FLAG_UNIFORM_BUFFER_OBJECT,
            LANG_330, &ext, &depData, 
            &glslShader);
        if (!translateResult) return false;
        
        // Third, GLSL source -> glslang::TShader -> SPIR-V bytecode
        auto spvRes = GLSLtoSPV(
            payload, errors, 
            GLSLShaderTypeToEShLanguage(glslShader.shaderType),
            glslShader.sourceCode);

        return spvRes;
    }

    std::string HLSLToSPIRVCompiler::MakeShaderMetricsString(const void* data, size_t dataSize) const
    {
        return "No metrics for SPIR-V shaders currently";
    }
    
    std::weak_ptr<HLSLToSPIRVCompiler> HLSLToSPIRVCompiler::s_instance;

    HLSLToSPIRVCompiler::HLSLToSPIRVCompiler(std::shared_ptr<ShaderService::ILowLevelCompiler> hlslCompiler) 
    : _hlslCompiler(std::move(hlslCompiler))
    {
        bool initResult = glslang::InitializeProcess();
        if (!initResult)
            Throw(::Exceptions::BasicLabel("Failed while initializing glsl to spirv shader compiler"));
    }

    HLSLToSPIRVCompiler::~HLSLToSPIRVCompiler()
    {
        glslang::FinalizeProcess();
    }

    std::shared_ptr<ShaderService::ILowLevelCompiler> CreateLowLevelShaderCompiler()
    {
        auto result = HLSLToSPIRVCompiler::s_instance.lock();
        if (result) return std::move(result);

        auto hlslCompiler = Metal_DX11::CreateVulkanPrecompiler();

        result = std::make_shared<HLSLToSPIRVCompiler>(hlslCompiler);
        HLSLToSPIRVCompiler::s_instance = result;
        return std::move(result);
    }

}}


