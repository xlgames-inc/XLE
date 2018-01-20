// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderVariationSet.h"
#include "Services.h"
#include "../Techniques/ParsingContext.h"
#include "../Types.h"
#include "../../Assets/Assets.h"
#include "../../Utility/StringFormat.h"

namespace RenderCore { namespace Assets
{
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
        const ParameterBox& materialParameters)
    : _materialParameters(std::move(materialParameters))
    , _techniqueInterface(MakeTechInterface(inputLayout, objectCBs))
    , _geometryParameters(TechParams_SetGeo(inputLayout))
    {}

    ShaderVariationSet::ShaderVariationSet() {}
    ShaderVariationSet::~ShaderVariationSet() {}

    auto ShaderVariationSet::FindVariation(
        Techniques::ParsingContext& parsingContext,
        unsigned techniqueIndex, StringSection<> techniqueConfig) const -> Variation
    {
        const ParameterBox* state[] = {
            &_geometryParameters, 
            &parsingContext.GetTechniqueContext()._globalEnvironmentState,
            &parsingContext.GetTechniqueContext()._runtimeState, 
            &_materialParameters
        };
		
		const auto& searchDirs = Services::GetTechniqueConfigDirs();

		assert(!XlFindStringI(techniqueConfig, ".tech"));
		::Assets::ResChar resName[MaxPath];
		XlCopyString(resName, techniqueConfig);
		XlCatString(resName, ".tech");
		searchDirs.ResolveFile(resName, dimof(resName), resName);

        auto& techConfig = ::Assets::GetAssetDep<Techniques::ShaderType>(MakeStringSection(resName));

        Variation result;
        result._cbLayout = nullptr;
        result._shader = techConfig.FindVariation(techniqueIndex, state, _techniqueInterface);
        result._cbLayout = &techConfig.TechniqueCBLayout();
        return result;
    }

    const Techniques::PredefinedCBLayout& ShaderVariationSet::GetCBLayout(StringSection<> techniqueConfigName)
    {
        auto& techConfig = ::Assets::GetAssetDep<Techniques::ShaderType>(techniqueConfigName);
        return techConfig.TechniqueCBLayout();
    }

}}

