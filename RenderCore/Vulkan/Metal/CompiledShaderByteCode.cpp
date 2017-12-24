// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Shader.h"
#include "DeviceContext.h"
#include "PipelineLayout.h"
#include "IncludeVulkan.h"
#include "../IDeviceVulkan.h"
#include "../../ShaderService.h"
#include "../../ShaderLangUtil.h"
#include "../../../Assets/AssetServices.h"
#include "../../../Assets/InvalidAssetManager.h"
#include "../../../Assets/AssetUtils.h"
#include "../../../Utility/StringUtils.h"

#include <sstream>

// HLSL cross compiler includes --
#define EXCLUDE_PSTDINT
#include "hlslcc.hpp"

// Vulkan SDK includes -- 
#pragma push_macro("new")
#undef new
#include <glslang/glslang/Public/ShaderLang.h>
#include <glslang/glslang/Include/InitializeGlobals.h>
#include <glslang/SPIRV/GlslangToSpv.h>

// #include <glslang/SPIRV/disassemble.h>
// #include <sstream>
#pragma pop_macro("new")

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
            StringSection<ResChar> source) const;

        virtual bool DoLowLevelCompile(
            /*out*/ Payload& payload,
            /*out*/ Payload& errors,
            /*out*/ std::vector<::Assets::DependentFileState>& dependencies,
            const void* sourceCode, size_t sourceCodeLength,
            const ShaderService::ResId& shaderPath,
            StringSection<::Assets::ResChar> definesTable) const;

        virtual std::string MakeShaderMetricsString(
            const void* byteCode, size_t byteCodeSize) const;

        HLSLToSPIRVCompiler(
            std::shared_ptr<ShaderService::ILowLevelCompiler> hlslCompiler, 
            const std::shared_ptr<PipelineLayout>& graphicsPipelineLayout,
            const std::shared_ptr<PipelineLayout>& computePipelineLayout);
        ~HLSLToSPIRVCompiler();

    private:
        std::shared_ptr<ShaderService::ILowLevelCompiler>   _hlslCompiler;
        std::shared_ptr<PipelineLayout>                     _graphicsPipelineLayout;
        std::shared_ptr<PipelineLayout>                     _computePipelineLayout;

        static std::weak_ptr<HLSLToSPIRVCompiler> s_instance;
        friend std::shared_ptr<ShaderService::ILowLevelCompiler> CreateLowLevelShaderCompiler(IDevice& device);
    };

        ////////////////////////////////////////////////////////////

    void HLSLToSPIRVCompiler::AdaptShaderModel(
        ResChar destination[], 
        const size_t destinationCount,
        StringSection<ResChar> inputShaderModel) const
    {
        // _hlslCompiler->AdaptShaderModel(destination, destinationCount, inputShaderModel);

        if (inputShaderModel[0] != '\0') {
            size_t length = inputShaderModel.size();

            //  Some shaders end with vs_*, gs_*, etc..
            //  Change this to the default shader model version. We should use
            //  version 5.0 in this case. We haven't initialized a D3D device (so we
            //  don't have to care about compatibility with any physical device).
            //  As long as the installed shader compile dll supports 5.0, then we
            //  should be ok.
            if (inputShaderModel[length-1] == '*') {
                const char* bestShaderModel = "5_0";
                if (destination != inputShaderModel.begin()) 
                    XlCopyString(destination, destinationCount, inputShaderModel);
                destination[std::min(length-1, destinationCount-1)] = '\0';
                XlCatString(destination, destinationCount, bestShaderModel);
                return;
            }
        }

        if (destination != inputShaderModel.begin()) 
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

        // std::stringstream disassem;
        // spv::Disassemble(disassem, spirv);
        // auto d = disassem.str();

        std::memcpy(
            PtrAdd(AsPointer(payload->begin()), sizeof(ShaderService::ShaderHeader)),
            AsPointer(spirv.begin()), spirvBlockSize);

        return true;
    }

    static ResourceGroup ResourceTypeToResourceGroup(ResourceType eType)
    {
	    switch(eType)
	    {
	    case RTYPE_CBUFFER:
		    return RGROUP_CBUFFER;

	    case RTYPE_SAMPLER:
		    return RGROUP_SAMPLER;

	    case RTYPE_TEXTURE:
	    case RTYPE_BYTEADDRESS:
	    case RTYPE_STRUCTURED:
		    return RGROUP_TEXTURE;

	    case RTYPE_UAV_RWTYPED:
	    case RTYPE_UAV_RWSTRUCTURED:
	    case RTYPE_UAV_RWBYTEADDRESS:
	    case RTYPE_UAV_APPEND_STRUCTURED:
	    case RTYPE_UAV_CONSUME_STRUCTURED:
	    case RTYPE_UAV_RWSTRUCTURED_WITH_COUNTER:
		    return RGROUP_UAV;

	    case RTYPE_TBUFFER:
		    assert(0); // Need to find out which group this belongs to
		    return RGROUP_TEXTURE;
	    }

	    assert(0);
	    return RGROUP_CBUFFER;
    }

    static DescriptorSetBindingSignature::Type AsBindingType(ResourceBinding* srcResBinding, ::ConstantBuffer* srcCBBinding)
    {
        if (!srcResBinding) {
            if (srcCBBinding) 
                return DescriptorSetBindingSignature::Type::ConstantBuffer;
            return DescriptorSetBindingSignature::Type::Unknown;
        }

        auto group = ResourceTypeToResourceGroup(srcResBinding->eType);
        switch (group) {
        case RGROUP_CBUFFER:    return DescriptorSetBindingSignature::Type::ConstantBuffer;
        case RGROUP_TEXTURE:    
            if (    srcResBinding->eType == RTYPE_STRUCTURED
                ||  srcResBinding->eType == RTYPE_BYTEADDRESS)
                return DescriptorSetBindingSignature::Type::TextureAsBuffer;
            return DescriptorSetBindingSignature::Type::Texture;
        case RGROUP_SAMPLER:    return DescriptorSetBindingSignature::Type::Sampler;
        case RGROUP_UAV:        
            {
                if (    srcResBinding->eType == RTYPE_UAV_RWSTRUCTURED
                    ||  srcResBinding->eType == RTYPE_UAV_RWBYTEADDRESS
                    ||  srcResBinding->eType == RTYPE_UAV_APPEND_STRUCTURED
                    ||  srcResBinding->eType == RTYPE_UAV_CONSUME_STRUCTURED
                    ||  srcResBinding->eType == RTYPE_UAV_RWSTRUCTURED_WITH_COUNTER)
                    return DescriptorSetBindingSignature::Type::UnorderedAccessAsBuffer;
                return DescriptorSetBindingSignature::Type::UnorderedAccess;
            }
        }
        return DescriptorSetBindingSignature::Type::Unknown;
    }

    uint32_t __cdecl EvaluateBinding(
        void* userData,
        GLSLResourceBinding* dstBinding, 
        ResourceBinding* srcResBinding,
        ::ConstantBuffer* srcCBBinding,
        uint32_t bindPoint, uint32_t shaderStage)
    {
        // Attempt to find this binding in our root signature, and return the binding
        // index and set associated with it!
        auto& rootSig = *(RootSignature*)userData;
        auto type = AsBindingType(srcResBinding, srcCBBinding);
        if (type == DescriptorSetBindingSignature::Type::Unknown) return 0;

        char* name = nullptr;
        if (srcResBinding) name = srcResBinding->Name;
        else if (srcCBBinding) name = srcCBBinding->Name;

        // First, check to see if it has been assigned as push constants
        if (type == DescriptorSetBindingSignature::Type::ConstantBuffer && name) {
            for (unsigned rangeIndex=0; rangeIndex<(unsigned)rootSig._pushConstantRanges.size(); ++rangeIndex) {
                if (XlEqString(rootSig._pushConstantRanges[rangeIndex]._name, name)) {
                    assert(srcCBBinding);
                    assert(srcCBBinding->ui32TotalSizeInBytes == rootSig._pushConstantRanges[rangeIndex]._rangeSize);
                    dstBinding->_locationIndex = ~0u;
                    dstBinding->_bindingIndex = ~0u;
                    dstBinding->_setIndex = ~0u;
                    dstBinding->_flags = GLSL_BINDING_TYPE_PUSHCONSTANTS;
                    return 1;
                }
            }
        }

        for (unsigned setIndex=0; setIndex<(unsigned)rootSig._descriptorSets.size(); ++setIndex) {
            auto& set = rootSig._descriptorSets[setIndex];
            for (unsigned finalBind=0; finalBind<(unsigned)set._bindings.size(); ++finalBind)
                if (    set._bindings[finalBind]._hlslBindingIndex == bindPoint
                    &&  set._bindings[finalBind]._type == type) {
                    // found it!
                    dstBinding->_locationIndex = ~0u;
                    dstBinding->_bindingIndex = finalBind;
                    dstBinding->_setIndex = setIndex;
                    dstBinding->_flags = 0;
                    return 1;
                }
        }

        return 0;
    }

    bool HLSLToSPIRVCompiler::DoLowLevelCompile(
        /*out*/ std::shared_ptr<std::vector<uint8>>& payload,
        /*out*/ std::shared_ptr<std::vector<uint8>>& errors,
        /*out*/ std::vector<::Assets::DependentFileState>& dependencies,
        const void* sourceCode, size_t sourceCodeLength,
        const ShaderService::ResId& shaderPath,
        StringSection<::Assets::ResChar> definesTable) const
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

        // We need to load the root signature and add it as a dependency
        std::shared_ptr<RootSignature> rootSig;
        if (shaderPath._shaderModel[0] == 'c' || shaderPath._shaderModel[0] == 'C') {
            rootSig = _computePipelineLayout->ShareRootSignature();
        } else {
            rootSig = _graphicsPipelineLayout->ShareRootSignature();
        }
        dependencies.push_back(rootSig->GetDependentFileState());

        // Second, HLSL bytecode -> GLSL source
        // We're going to be using James-Jones' cross compiler: 
        //      https://github.com/James-Jones/HLSLCrossCompiler
        GlExtensions ext;
        ext.ARB_explicit_attrib_location = 0;
        ext.ARB_explicit_uniform_location = 0;
        ext.ARB_shading_language_420pack = 1;
        ext.GL_KHR_vulkan_glsl = 1;
        GLSLCrossDependencyData depData = {};
        GLSLShader glslShader;
        unsigned hlslccFlags = HLSLCC_FLAG_UNIFORM_BUFFER_OBJECT | HLSLCC_FLAG_INOUT_SEMANTIC_NAMES;
        if (tolower(shaderPath._shaderModel[0]) == 'g')
            hlslccFlags &= ~HLSLCC_FLAG_INOUT_SEMANTIC_NAMES;
        auto* bytecodeStart = (const char*)PtrAdd(AsPointer(hlslBytecode->begin()), sizeof(ShaderService::ShaderHeader));
        auto translateResult = TranslateHLSLFromMem(
            bytecodeStart,
            hlslccFlags,
            LANG_440, &ext, &depData, 
            &EvaluateBinding, rootSig.get(),
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

    HLSLToSPIRVCompiler::HLSLToSPIRVCompiler(
        std::shared_ptr<ShaderService::ILowLevelCompiler> hlslCompiler, 
        const std::shared_ptr<PipelineLayout>& graphicsPipelineLayout,
        const std::shared_ptr<PipelineLayout>& computePipelineLayout) 
    : _hlslCompiler(std::move(hlslCompiler))
    , _graphicsPipelineLayout(graphicsPipelineLayout)
    , _computePipelineLayout(computePipelineLayout)
    {
        bool initResult = glslang::InitializeProcess();
        if (!initResult)
            Throw(::Exceptions::BasicLabel("Failed while initializing glsl to spirv shader compiler"));
    }

    HLSLToSPIRVCompiler::~HLSLToSPIRVCompiler()
    {
        // it feels like these are intended to be called during DLL detach -- 
        glslang::FreeGlobalPools();
        glslang::FreePoolIndex();
        glslang::FinalizeProcess();
    }

    std::shared_ptr<ShaderService::ILowLevelCompiler> CreateLowLevelShaderCompiler(IDevice& device)
    {
        auto result = HLSLToSPIRVCompiler::s_instance.lock();
        if (result) return std::move(result);

        auto* vulkanDevice = (IDeviceVulkan*)device.QueryInterface(typeid(IDeviceVulkan).hash_code());
        if (!vulkanDevice) return nullptr;
        
        auto hlslCompiler = Metal_DX11::CreateVulkanPrecompiler();

        result = std::make_shared<HLSLToSPIRVCompiler>(
            hlslCompiler, 
            vulkanDevice->ShareGraphicsPipelineLayout(),
            vulkanDevice->ShareComputePipelineLayout());
        HLSLToSPIRVCompiler::s_instance = result;
        return std::move(result);
    }

}}


