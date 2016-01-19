// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TechniqueMaterial.h"
#include "ParsingContext.h"
#include "PredefinedCBLayout.h"
#include "../Metal/InputLayout.h"
#include "../../Assets/Assets.h"
#include "../../Utility/StringUtils.h"

namespace RenderCore { namespace Techniques
{
    const ::Assets::ResChar* DefaultPredefinedCBLayout = "game/xleres/BasicMaterialConstants.txt";

    static Techniques::TechniqueInterface MakeTechInterface(
        const Metal::InputLayout& inputLayout,
        const std::initializer_list<uint64>& objectCBs)
    {
        Techniques::TechniqueInterface techniqueInterface(inputLayout);
        Techniques::TechniqueContext::BindGlobalUniforms(techniqueInterface);
        unsigned index = 0;
        for (auto o:objectCBs)
            techniqueInterface.BindConstantBuffer(o, index++, 1);
        return std::move(techniqueInterface);
    }

    static bool HasElement(const Metal::InputLayout& inputLayout, const char elementSemantic[])
    {
        auto end = &inputLayout.first[inputLayout.second];
        return std::find_if
            (
                inputLayout.first, end,
                [=](const Metal::InputElementDesc& element)
                    { return !XlCompareStringI(element._semanticName.c_str(), elementSemantic); }
            ) != end;
    }

    ParameterBox TechParams_SetGeo(const Metal::InputLayout& inputLayout)
    {
        ParameterBox result;
        if (HasElement(inputLayout, "NORMAL"))          result.SetParameter((const utf8*)"GEO_HAS_NORMAL", 1);
        if (HasElement(inputLayout, "TEXCOORD"))        result.SetParameter((const utf8*)"GEO_HAS_TEXCOORD", 1);
        if (HasElement(inputLayout, "TEXTANGENT"))      result.SetParameter((const utf8*)"GEO_HAS_TANGENT_FRAME", 1);
        if (HasElement(inputLayout, "TEXBITANGENT"))    result.SetParameter((const utf8*)"GEO_HAS_BITANGENT", 1);
        if (HasElement(inputLayout, "COLOR"))           result.SetParameter((const utf8*)"GEO_HAS_COLOUR", 1);
        return std::move(result);
    }

    TechniqueMaterial::TechniqueMaterial(
        const Metal::InputLayout& inputLayout,
        const std::initializer_list<uint64>& objectCBs,
        ParameterBox materialParameters)
    : _materialParameters(std::move(materialParameters))
    , _techniqueInterface(MakeTechInterface(inputLayout, objectCBs))
    , _geometryParameters(TechParams_SetGeo(inputLayout))
    {}

    TechniqueMaterial::TechniqueMaterial() {}
    TechniqueMaterial::TechniqueMaterial(TechniqueMaterial&& moveFrom)
    : _materialParameters(std::move(moveFrom._materialParameters))
    , _geometryParameters(std::move(moveFrom._geometryParameters))
    , _techniqueInterface(std::move(moveFrom._techniqueInterface))
    {
    }

    const TechniqueMaterial& TechniqueMaterial::operator=(TechniqueMaterial&& moveFrom)
    {
        _materialParameters = std::move(moveFrom._materialParameters);
        _geometryParameters = std::move(moveFrom._geometryParameters);
        _techniqueInterface = std::move(moveFrom._techniqueInterface);
        return *this;
    }

    TechniqueMaterial::~TechniqueMaterial() {}

    auto TechniqueMaterial::FindVariation(
        ParsingContext& parsingContext,
        unsigned techniqueIndex, const char techniqueConfigName[]) const -> Variation
    {
        const ParameterBox* state[] = {
            &_geometryParameters, 
            &parsingContext.GetTechniqueContext()._globalEnvironmentState,
            &parsingContext.GetTechniqueContext()._runtimeState, 
            &_materialParameters
        };

        auto& techConfig = ::Assets::GetAssetDep<ShaderType>(techniqueConfigName);

        Variation result;
        result._cbLayout = nullptr;
        result._shader = techConfig.FindVariation(techniqueIndex, state, _techniqueInterface);
        if (result._shader._shaderProgram == nullptr) return result;

        // We need to know the layout the main materials constant buffer now...
        // We could  just use the reflection interface to get the layout from there...
        // But that's not exactly what we want.
        // This constant buffer is defined to be the same for every shader produced by 
        // the same technique config. And sometimes we want to know the layout before we
        // decide on the particular technique variation we are going to use... Therefore,
        // we must define the layout of this cb in some way that is independent from the compiled
        // shader code...
        if (techConfig.HasEmbeddedCBLayout()) {
            result._cbLayout = &::Assets::GetAssetDep<PredefinedCBLayout>(techniqueConfigName);
        } else {
            // This is the default CB layout where there isn't one explicitly referenced by the
            // technique config file
            result._cbLayout = &::Assets::GetAssetDep<PredefinedCBLayout>(DefaultPredefinedCBLayout);
        }
        return result;
    }

    const PredefinedCBLayout& TechniqueMaterial::GetCBLayout(const ::Assets::ResChar techniqueConfigName[])
    {
        auto& techConfig = ::Assets::GetAssetDep<ShaderType>(techniqueConfigName);
        if (techConfig.HasEmbeddedCBLayout()) {
            return ::Assets::GetAssetDep<PredefinedCBLayout>(techniqueConfigName);
        } else {
            return ::Assets::GetAssetDep<PredefinedCBLayout>(DefaultPredefinedCBLayout);
        }
    }

}}

