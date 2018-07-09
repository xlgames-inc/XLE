// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderVariationSet.h"
#include "Services.h"
#include "../Techniques/ParsingContext.h"
#include "../Techniques/ResolvedTechniqueShaders.h"
#include "../Techniques/TechniqueUtils.h"
#include "../Types.h"
#include "../../Assets/Assets.h"
#include "../../Utility/StringFormat.h"

namespace RenderCore { namespace Assets
{
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
            &parsingContext.GetTechniqueContext()._runtimeState, 
            &_materialParameters
        };
		
		const auto& searchDirs = Services::GetTechniqueConfigDirs();

		assert(!XlFindStringI(techniqueConfig, ".tech"));
		::Assets::ResChar resName[MaxPath];
		XlCopyString(resName, techniqueConfig);
		XlCatString(resName, ".tech");
		searchDirs.ResolveFile(resName, dimof(resName), resName);

        auto& techConfig = ::Assets::GetAssetDep<Techniques::ResolvedTechniqueInterfaceShaders>(MakeStringSection(resName));

        Variation result;
        result._cbLayout = nullptr;
        result._shader = techConfig.FindVariation(techniqueIndex, state, _techniqueInterface);
        result._cbLayout = &techConfig.GetTechnique().TechniqueCBLayout();
        return result;
    }

    const Techniques::PredefinedCBLayout& ShaderVariationSet::GetCBLayout(StringSection<> techniqueConfigName)
    {
        auto& techConfig = ::Assets::GetAssetDep<Techniques::ResolvedTechniqueInterfaceShaders>(techniqueConfigName);
        return techConfig.GetTechnique().TechniqueCBLayout();
    }

}}

