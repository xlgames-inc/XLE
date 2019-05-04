// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Techniques/ResolvedTechniqueShaders.h"
#include "../Utility/ParameterBox.h"

namespace RenderCore { class InputElementDesc; }
namespace RenderCore { namespace Assets
{
	class PredefinedCBLayout;
}}

namespace FixedFunctionModel
{
	/// <summary>Utility call for selecting a shader variation matching a given interface</summary>
	class ShaderVariationSet
    {
    public:
        ParameterBox _materialParameters;
        ParameterBox _geometryParameters;
        RenderCore::Techniques::TechniquePrebindingInterface _techniqueInterface;

        class Variation
        {
        public:
			RenderCore::Techniques::BoundShaderVariationSet::ResolvedShader      _shader;
            const RenderCore::Assets::PredefinedCBLayout* _cbLayout;
        };

        Variation FindVariation(
			RenderCore::Techniques::ParsingContext& parsingContext,
            unsigned techniqueIndex,
            StringSection<> techniqueConfig) const;

        const RenderCore::Assets::PredefinedCBLayout& GetCBLayout(StringSection<> techniqueConfig);

        ShaderVariationSet(
            IteratorRange<const RenderCore::InputElementDesc*> inputLayout,
            const std::initializer_list<uint64_t>& objectCBs,
            const ParameterBox& materialParameters);
        ShaderVariationSet();
        ShaderVariationSet(ShaderVariationSet&& moveFrom) never_throws = default;
        ShaderVariationSet& operator=(ShaderVariationSet&& moveFrom) never_throws = default;
        ~ShaderVariationSet();
    };

    ParameterBox TechParams_SetGeo(IteratorRange<const RenderCore::InputElementDesc*> inputLayout);

}

