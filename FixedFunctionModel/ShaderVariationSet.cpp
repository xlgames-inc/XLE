// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderVariationSet.h"
#include "../RenderCore/Assets/Services.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Techniques/ResolvedTechniqueShaders.h"
#include "../RenderCore/Techniques/TechniqueUtils.h"
#include "../RenderCore/Types.h"
#include "../Assets/Assets.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/StringFormat.h"

namespace FixedFunctionModel
{
	using namespace RenderCore;

    static Techniques::TechniqueInterface MakeTechInterface(
        IteratorRange<const InputElementDesc*> inputLayout,
        const std::initializer_list<uint64_t>& objectCBs)
    {
		UniformsStreamInterface interf;
		unsigned index = 0;
		for (auto o : objectCBs)
			interf.BindConstantBuffer(index++, { o });

        Techniques::TechniqueInterface techniqueInterface(inputLayout);
        techniqueInterface.BindGlobalUniforms();
		techniqueInterface.BindUniformsStream(1, interf);
        return std::move(techniqueInterface);
    }

    ParameterBox TechParams_SetGeo(IteratorRange<const InputElementDesc*> inputLayout)
    {
        ParameterBox result;
		Techniques::SetGeoSelectors(result, inputLayout);
        return result;
    }

    ShaderVariationSet::ShaderVariationSet(
        IteratorRange<const InputElementDesc*> inputLayout,
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
            &parsingContext.GetSubframeShaderSelectors(), 
            &_materialParameters
        };
		
		const auto& searchDirs = RenderCore::Assets::Services::GetTechniqueConfigDirs();

		assert(XlEqStringI(MakeFileNameSplitter(techniqueConfig).Extension(), "tech"));
		::Assets::ResChar resName[MaxPath];
		searchDirs.ResolveFile(resName, dimof(resName), techniqueConfig);

        auto& techConfig = ::Assets::GetAssetDep<Techniques::ResolvedTechniqueShaders>(MakeStringSection(resName));

        Variation result;
        result._cbLayout = nullptr;
        result._shader = techConfig.FindVariation(techniqueIndex, state, _techniqueInterface);
        result._cbLayout = &techConfig.GetTechnique().TechniqueCBLayout();
        return result;
    }

    const Techniques::PredefinedCBLayout& ShaderVariationSet::GetCBLayout(StringSection<> techniqueConfigName)
    {
        auto& techConfig = ::Assets::GetAssetDep<Techniques::ResolvedTechniqueShaders>(techniqueConfigName);
        return techConfig.GetTechnique().TechniqueCBLayout();
    }

}

