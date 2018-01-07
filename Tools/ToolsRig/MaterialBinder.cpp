// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialBinder.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/State.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Assets/DeferredShaderResource.h"
#include "../../RenderCore/Techniques/TechniqueMaterial.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/CommonBindings.h"
#include "../../RenderCore/Techniques/PredefinedCBLayout.h"
#include "../../RenderCore/Assets/MaterialScaffold.h"
#include "../../RenderCore/Assets/AssetUtils.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/IFileSystem.h"
#include "../../Assets/Assets.h"
#include "../../Math/Transformations.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/StringUtils.h"

#include "../../RenderCore/DX11/Metal/IncludeDX11.h"
#include "../../RenderCore/DX11/Metal/DX11Utils.h"
#include <d3d11shader.h>        // D3D11_SHADER_TYPE_DESC

#pragma warning(disable:4505)		// unreferenced local function has been removed

namespace ToolsRig
{

    IMaterialBinder::~IMaterialBinder() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    bool MaterialBinder::Apply(
        RenderCore::Metal::DeviceContext& metalContext,
        RenderCore::Techniques::ParsingContext& parserContext,
        unsigned techniqueIndex,
        const RenderCore::Techniques::Material& mat,
        const SystemConstants& sysConstants,
        const ::Assets::DirectorySearchRules& searchRules,
        const RenderCore::InputLayout& geoInputLayout)
    {
        using namespace RenderCore;
        using namespace RenderCore::Techniques;

        ParameterBox materialParameters = RenderCore::Assets::TechParams_SetResHas(mat._matParams, mat._bindings, searchRules);
        ShaderVariationSet material(geoInputLayout, {}, materialParameters);

        auto variation = material.FindVariation(parserContext, techniqueIndex, _shaderTypeName.c_str());
        if (variation._shader._shaderProgram == nullptr) {
            return false; // we can't render because we couldn't resolve a good shader variation
        }

            // we must bind the shader program & the bound layout
            // but we're not using the BoundUniforms in the ResolvedShader object
        metalContext.Bind(*variation._shader._shaderProgram);
        metalContext.Bind(*variation._shader._boundLayout);

            // Instead of using ResolvedShader::_boundUniforms, let's
            // look at the reflection information for the shader program
            // and assign each shader input to some reasonable value
        BindConstantsAndResources(
            metalContext, parserContext, mat, 
            sysConstants, searchRules, 
			*variation._shader._boundUniforms,
            *variation._cbLayout);
        return true;
    }
        
    MaterialBinder::MaterialBinder(StringSection<::Assets::ResChar> shaderTypeName)
    : _shaderTypeName(shaderTypeName.AsString())
    {}

    MaterialBinder::~MaterialBinder() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    static size_t WriteSystemVariable(
        const char name[], 
        const IMaterialBinder::SystemConstants& constants, 
        UInt2 viewportDims,
        void* destination, void* destinationEnd)
    {
        size_t size = size_t(destinationEnd) - size_t(destination);
        if (!_stricmp(name, "SI_OutputDimensions") && size >= (sizeof(unsigned)*2)) {
            ((unsigned*)destination)[0] = viewportDims[0];
            ((unsigned*)destination)[1] = viewportDims[1];
            return sizeof(unsigned)*2;
        } else if (!_stricmp(name, "SI_NegativeLightDirection") && size >= sizeof(Float3)) {
            *((Float3*)destination) = constants._lightNegativeDirection;
            return sizeof(Float3);
        } else if (!_stricmp(name, "SI_LightColor") && size >= sizeof(Float3)) {
            *((Float3*)destination) = constants._lightColour;
            return sizeof(Float3);
        }
        return 0;
    }

#if GFXAPI_GFXAPI_DX11
    static void WriteParameter(
        RenderCore::SharedPkt& result,
        const ParameterBox& constants,
        ParameterBox::ParameterNameHash nameHash,
        ID3D11ShaderReflectionVariable& reflectionVariable,
        const D3D11_SHADER_VARIABLE_DESC& variableDesc,
        unsigned bufferSize)
    {
        auto type = reflectionVariable.GetType();
        D3D11_SHADER_TYPE_DESC typeDesc;
        auto hresult = type->GetDesc(&typeDesc);
        if (SUCCEEDED(hresult)) {

                //
                //      Finally, copy whatever the material object
                //      is, into the destination position in the 
                //      constant buffer;
                //

            auto impliedType = RenderCore::Metal::GetType(typeDesc);
            assert((variableDesc.StartOffset + impliedType.GetSize()) <= bufferSize);
            if ((variableDesc.StartOffset + impliedType.GetSize()) <= bufferSize) {

                if (!result.size()) {
                    result = RenderCore::MakeSharedPktSize(bufferSize);
                    std::fill((uint8*)result.begin(), (uint8*)result.end(), 0);
                }

                constants.GetParameter(
                    nameHash,
                    PtrAdd(result.begin(), variableDesc.StartOffset),
                    impliedType);
            }
        }
    }



    static std::vector<std::pair<uint64, RenderCore::Metal::ConstantBufferPacket>> 
        BuildMaterialConstants(
            const RenderCore::Techniques::PredefinedCBLayout& cbLayout,
            RenderCore::Metal::BoundUniforms& boundUniforms, 
            const ParameterBox& constants,
            const IMaterialBinder::SystemConstants& systemConstantsContext,
            UInt2 viewportDims)
    {

            //
            //      Find the cbuffers, and look for the variables
            //      within. Attempt to fill those values with the appropriate values
            //      from the current previewing material state
            //
        std::vector<std::pair<uint64, RenderCore::Metal::ConstantBufferPacket>> finalResult;

		auto reflection = boundUniforms.GetReflection(RenderCore::ShaderStage::Pixel);
		if (!reflection) return finalResult;

        D3D11_SHADER_DESC shaderDesc;
        reflection->GetDesc(&shaderDesc);
        for (unsigned c=0; c<shaderDesc.BoundResources; ++c) {

            D3D11_SHADER_INPUT_BIND_DESC bindDesc;
            reflection->GetResourceBindingDesc(c, &bindDesc);

            if (bindDesc.Type == D3D10_SIT_CBUFFER) {
                auto cbuffer = reflection->GetConstantBufferByName(bindDesc.Name);
                if (cbuffer) {
                    D3D11_SHADER_BUFFER_DESC bufferDesc;
                    HRESULT hresult = cbuffer->GetDesc(&bufferDesc);
                    if (SUCCEEDED(hresult)) {
                        RenderCore::SharedPkt result;

                        for (unsigned c=0; c<bufferDesc.Variables; ++c) {
                            auto reflectionVariable = cbuffer->GetVariableByIndex(c);
                            D3D11_SHADER_VARIABLE_DESC variableDesc;
                            hresult = reflectionVariable->GetDesc(&variableDesc);
                            if (SUCCEEDED(hresult)) {

                                    //
                                    //      If the variable is within our table of 
                                    //      material parameter values, then copy that
                                    //      value into the appropriate place in the cbuffer.
                                    //
                                    //      However, note that this may require a cast sometimes
                                    //

                                auto nameHash = ParameterBox::MakeParameterNameHash(variableDesc.Name);
                                if (constants.HasParameter(nameHash)) {
                                    WriteParameter(
                                        result, constants, nameHash, *reflectionVariable, 
                                        variableDesc, bufferDesc.Size);
                                } else if (cbLayout._defaults.HasParameter(nameHash)) {
                                    WriteParameter(
                                        result, cbLayout._defaults, nameHash, *reflectionVariable, 
                                        variableDesc, bufferDesc.Size);
                                } else {
                                    if (!result.size()) {
                                        char buffer[4096];
                                        if (size_t size = WriteSystemVariable(
                                            variableDesc.Name, systemConstantsContext, viewportDims,
                                            buffer, PtrAdd(buffer, std::min(sizeof(buffer), (size_t)(bufferDesc.Size - variableDesc.StartOffset))))) {

                                            result = RenderCore::MakeSharedPktSize(bufferDesc.Size);
                                            std::fill((uint8*)result.begin(), (uint8*)result.end(), 0);
                                            XlCopyMemory(PtrAdd(result.begin(), variableDesc.StartOffset), buffer, size);
                                        }
                                    } else {
                                        WriteSystemVariable(
                                            variableDesc.Name, systemConstantsContext, viewportDims,
                                            PtrAdd(result.begin(), variableDesc.StartOffset), result.end());
                                    }
                                }

                            }
                        }

                        if (result.size()) {
                            finalResult.push_back(
                                std::make_pair(Hash64(bindDesc.Name), std::move(result)));
                        }   
                    }
                }
            }

        }

        return finalResult;
    }
#else
	static std::vector<std::pair<uint64, RenderCore::Metal::ConstantBufferPacket>>
	    BuildMaterialConstants(
		    const RenderCore::Techniques::PredefinedCBLayout& cbLayout,
		    RenderCore::Metal::BoundUniforms& boundUniforms,
		    const ParameterBox& constants,
		    const IMaterialBinder::SystemConstants& systemConstantsContext,
		    UInt2 viewportDims)
	{
        return std::vector<std::pair<uint64, RenderCore::Metal::ConstantBufferPacket>>();
	}
#endif

    static std::vector<const RenderCore::Metal::ShaderResourceView*>
        BuildBoundTextures(
            RenderCore::Metal::BoundUniforms& boundUniforms,
            const ParameterBox& bindings,
            const Assets::DirectorySearchRules& searchRules)
    {
        using namespace RenderCore;
        std::vector<const Metal::ShaderResourceView*> result;

#if GFXAPI_ACTIVE == GFXAPI_DX11
        std::vector<uint64> alreadyBound;

            //
            //      For each entry in our resource binding set, we're going
            //      to register a binding in the BoundUniforms, and find
            //      the associated shader resource view.
            //      For any shader resources that are used by the shader, but
            //      not bound to anything -- we need to assign them to the 
            //      default objects.
            //

        const ShaderStage stages[] = {
            ShaderStage::Vertex,
			ShaderStage::Pixel,
			ShaderStage::Geometry
        };

        for (unsigned s=0; s<dimof(stages); ++s) {
            auto reflection = boundUniforms.GetReflection(stages[s]);
			if (!reflection) continue;

            D3D11_SHADER_DESC shaderDesc;
            reflection->GetDesc(&shaderDesc);

            for (unsigned c=0; c<shaderDesc.BoundResources; ++c) {

                D3D11_SHADER_INPUT_BIND_DESC bindDesc;
                reflection->GetResourceBindingDesc(c, &bindDesc);
                if  (bindDesc.Type == D3D10_SIT_TEXTURE) {

                        // skip some textures that are only for system use...
                    if (!XlCompareString(bindDesc.Name, "NormalsFittingTexture")) continue;
                    if (!XlCompareString(bindDesc.Name, "SkyReflectionTexture[0]")) continue;
                    if (!XlCompareString(bindDesc.Name, "SkyReflectionTexture[1]")) continue;
                    if (!XlCompareString(bindDesc.Name, "SkyReflectionTexture[2]")) continue;
                    if (!XlCompareString(bindDesc.Name, "GGXTable")) continue;
                    if (!XlCompareString(bindDesc.Name, "GlossLUT")) continue;

                    auto str = bindings.GetString<::Assets::ResChar>(ParameterBox::MakeParameterNameHash(bindDesc.Name));
                    bool isDefaultRes = false;
                    if (str.empty()) {
                            //  It's not mentioned in the material resources. try to look
                            //  for a default resource for this bind point
                        str = ::Assets::rstring("xleres/DefaultResources/") + bindDesc.Name + ".dds";
                        isDefaultRes = true;
                    }

                    auto bindingHash = Hash64(bindDesc.Name, XlStringEnd(bindDesc.Name));
                    if (std::find(alreadyBound.cbegin(), alreadyBound.cend(), bindingHash) != alreadyBound.cend()) {
                        continue;
                    }
                    alreadyBound.push_back(bindingHash);

                    if (!str.empty()) {
                        TRY {
                            ::Assets::ResChar resolvedFile[MaxPath];
                            searchRules.ResolveFile(
                                resolvedFile, dimof(resolvedFile),
                                str.c_str());

                                // check DoesFileExist (but only for default resources)
                            if (!isDefaultRes || ::Assets::MainFileSystem::TryGetDesc(resolvedFile)._state == ::Assets::FileDesc::State::Normal) {
                                const RenderCore::Assets::DeferredShaderResource& texture = 
                                    ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>(resolvedFile);

                                result.push_back(&texture.GetShaderResource());
                                boundUniforms.BindShaderResource(bindingHash, unsigned(result.size()-1), 1);
                            }
                        }
                        CATCH (const ::Assets::Exceptions::InvalidAsset&) {}
                        CATCH_END
                    }
                
                } else if (bindDesc.Type == D3D10_SIT_SAMPLER) {

                        //  we should also bind samplers to something
                        //  reasonable, also...

                }
            }
        }
#endif

        return std::move(result);
    }

    void IMaterialBinder::BindConstantsAndResources(
        RenderCore::Metal::DeviceContext& metalContext,
        RenderCore::Techniques::ParsingContext& parsingContext,
        const RenderCore::Techniques::Material& mat,
        const SystemConstants& sysConstants,
        const ::Assets::DirectorySearchRules& searchRules,
		const RenderCore::Metal::BoundUniforms& srcLayout,
        const RenderCore::Techniques::PredefinedCBLayout& cbLayout)
    {
        using namespace RenderCore;

#if GFXAPI_ACTIVE == GFXAPI_DX11

                //
                //      Constants / Resources
                //

		Metal::BoundUniforms boundLayout;
		boundLayout.CopyReflection(srcLayout);
        Techniques::TechniqueContext::BindGlobalUniforms(boundLayout);

        Metal::ViewportDesc currentViewport(metalContext);
            
        auto materialConstants = BuildMaterialConstants(
            cbLayout, boundLayout, 
            mat._constants, sysConstants, 
            UInt2(unsigned(currentViewport.Width), unsigned(currentViewport.Height)));
            
        std::vector<RenderCore::Metal::ConstantBufferPacket> constantBufferPackets;
        constantBufferPackets.push_back(
            Techniques::MakeLocalTransformPacket(
                sysConstants._objectToWorld, ExtractTranslation(parsingContext.GetProjectionDesc()._cameraToWorld)));
        boundLayout.BindConstantBuffer(Techniques::ObjectCB::LocalTransform, 0, 1);
        for (auto i=materialConstants.cbegin(); i!=materialConstants.cend(); ++i) {
            boundLayout.BindConstantBuffer(i->first, unsigned(constantBufferPackets.size()), 1);
            constantBufferPackets.push_back(std::move(i->second));
        }

        auto boundTextures = BuildBoundTextures(
            boundLayout, mat._bindings, searchRules);
        boundLayout.Apply(
            metalContext,
            parsingContext.GetGlobalUniformsStream(),
            Metal::UniformsStream( 
                AsPointer(constantBufferPackets.begin()), nullptr, constantBufferPackets.size(), 
                AsPointer(boundTextures.begin()), boundTextures.size()));
#endif
    }

    IMaterialBinder::SystemConstants::SystemConstants()
    {
        _lightNegativeDirection = Float3(0.f, 0.f, 1.f);
        _lightColour = Float3(1.f, 1.f, 1.f);
        _objectToWorld = Identity<Float4x4>();
    }
}
