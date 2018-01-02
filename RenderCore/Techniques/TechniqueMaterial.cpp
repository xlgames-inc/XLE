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
	Material::Material() { _techniqueConfig[0] = '\0'; }

	Material::Material(Material&& moveFrom) never_throws
	: _bindings(std::move(moveFrom._bindings))
	, _matParams(std::move(moveFrom._matParams))
	, _stateSet(moveFrom._stateSet)
	, _constants(std::move(moveFrom._constants))
	{
		XlCopyString(_techniqueConfig, moveFrom._techniqueConfig);
	}

	Material& Material::operator=(Material&& moveFrom) never_throws
	{
		_bindings = std::move(moveFrom._bindings);
		_matParams = std::move(moveFrom._matParams);
		_stateSet = moveFrom._stateSet;
		_constants = std::move(moveFrom._constants);
		XlCopyString(_techniqueConfig, moveFrom._techniqueConfig);
		return *this;
	}


    static Techniques::TechniqueInterface MakeTechInterface(
        const InputLayout& inputLayout,
        const std::initializer_list<uint64_t>& objectCBs)
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

    ShaderVariationSet::ShaderVariationSet(
        const InputLayout& inputLayout,
        const std::initializer_list<uint64_t>& objectCBs,
        ParameterBox materialParameters)
    : _materialParameters(std::move(materialParameters))
    , _techniqueInterface(MakeTechInterface(inputLayout, objectCBs))
    , _geometryParameters(TechParams_SetGeo(inputLayout))
    {}

    ShaderVariationSet::ShaderVariationSet() {}
    ShaderVariationSet::~ShaderVariationSet() {}

    auto ShaderVariationSet::FindVariation(
        ParsingContext& parsingContext,
        unsigned techniqueIndex, StringSection<> techniqueConfig) const -> Variation
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

    const PredefinedCBLayout& ShaderVariationSet::GetCBLayout(StringSection<> techniqueConfigName)
    {
        auto& techConfig = ::Assets::GetAssetDep<ShaderType>(techniqueConfigName);
        return techConfig.TechniqueCBLayout();
    }

}}

