// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TechniqueMaterial.h"
#include "ParsingContext.h"
#include "PredefinedCBLayout.h"
#include "../Types.h"
#include "../../Assets/Assets.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/StringFormat.h"

namespace RenderCore { namespace Techniques
{
    static Techniques::TechniqueInterface MakeTechInterface(
        const InputLayout& inputLayout,
        const std::initializer_list<uint64>& objectCBs)
    {
        Techniques::TechniqueInterface techniqueInterface(inputLayout);
        Techniques::TechniqueContext::BindGlobalUniforms(techniqueInterface);
        unsigned index = 0;
        for (auto o:objectCBs)
            techniqueInterface.BindConstantBuffer(o, index++, 1);
        return std::move(techniqueInterface);
    }

    static bool HasElement(const InputLayout& inputLayout, const char elementSemantic[])
    {
        auto end = &inputLayout.first[inputLayout.second];
        return std::find_if
            (
                inputLayout.first, end,
                [=](const InputElementDesc& element)
                    { return !XlCompareStringI(element._semanticName.c_str(), elementSemantic); }
            ) != end;
    }

    ParameterBox TechParams_SetGeo(const InputLayout& inputLayout)
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
        const InputLayout& inputLayout,
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
        unsigned techniqueIndex, StringSection<::Assets::ResChar> techniqueConfig) const -> Variation
    {
        const ParameterBox* state[] = {
            &_geometryParameters, 
            &parsingContext.GetTechniqueContext()._globalEnvironmentState,
            &parsingContext.GetTechniqueContext()._runtimeState, 
            &_materialParameters
        };

        auto& techConfig = ::Assets::GetAssetDep<ShaderType>(techniqueConfig);

        Variation result;
        result._cbLayout = nullptr;
        result._shader = techConfig.FindVariation(techniqueIndex, state, _techniqueInterface);
        result._cbLayout = &techConfig.TechniqueCBLayout();
        return result;
    }

    const PredefinedCBLayout& TechniqueMaterial::GetCBLayout(StringSection<::Assets::ResChar> techniqueConfigName)
    {
        auto& techConfig = ::Assets::GetAssetDep<ShaderType>(techniqueConfigName);
        return techConfig.TechniqueCBLayout();
    }

}}

