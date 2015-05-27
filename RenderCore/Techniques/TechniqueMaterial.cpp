// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TechniqueMaterial.h"
#include "ParsingContext.h"
#include "../Metal/InputLayout.h"
#include "../../Assets/Assets.h"
#include "../../Utility/StringUtils.h"

namespace RenderCore { namespace Techniques
{
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

    TechniqueMaterial::TechniqueMaterial(
        const Metal::InputLayout& inputLayout,
        const std::initializer_list<uint64>& objectCBs,
        ParameterBox materialParameters)
    : _materialParameters(std::move(materialParameters))
    , _techniqueInterface(MakeTechInterface(inputLayout, objectCBs))
    {
        if (HasElement(inputLayout, "NORMAL"))      _geometryParameters.SetParameter((const utf8*)"GEO_HAS_NORMAL", 1);
        if (HasElement(inputLayout, "TEXCOORD"))    _geometryParameters.SetParameter((const utf8*)"GEO_HAS_TEXCOORD", 1);
        if (HasElement(inputLayout, "TANGENT"))     _geometryParameters.SetParameter((const utf8*)"GEO_HAS_TANGENT_FRAME", 1);
        if (HasElement(inputLayout, "BITANGENT"))   _geometryParameters.SetParameter((const utf8*)"GEO_HAS_BITANGENT", 1);
        if (HasElement(inputLayout, "COLOR"))       _geometryParameters.SetParameter((const utf8*)"GEO_HAS_COLOUR", 1);
    }

    TechniqueMaterial::~TechniqueMaterial() {}

    Techniques::ResolvedShader TechniqueMaterial::FindVariation(
        Techniques::ParsingContext& parsingContext,
        unsigned techniqueIndex, const char shaderName[])
    {
        const ParameterBox* state[] = {
            &_geometryParameters, 
            &parsingContext.GetTechniqueContext()._globalEnvironmentState,
            &parsingContext.GetTechniqueContext()._runtimeState, 
            &_materialParameters
        };

        auto& shaderType = ::Assets::GetAssetDep<Techniques::ShaderType>(shaderName);
        return shaderType.FindVariation(techniqueIndex, state, _techniqueInterface);
    }
}}

