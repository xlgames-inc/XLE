// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Techniques/Techniques.h"
#include "../../Utility/ParameterBox.h"

namespace RenderCore { namespace Techniques
{
	class PredefinedCBLayout;
}}

namespace RenderCore { namespace Assets
{
	/// <summary>Utility call for selecting a shader variation matching a given interface</summary>
	class ShaderVariationSet
    {
    public:
        ParameterBox _materialParameters;
        ParameterBox _geometryParameters;
        Techniques::TechniqueInterface _techniqueInterface;

        class Variation
        {
        public:
			Techniques::ResolvedShader      _shader;
            const Techniques::PredefinedCBLayout* _cbLayout;
        };

        Variation FindVariation(
			Techniques::ParsingContext& parsingContext,
            unsigned techniqueIndex,
            StringSection<> techniqueConfig) const;

        const Techniques::PredefinedCBLayout& GetCBLayout(StringSection<> techniqueConfig);

        ShaderVariationSet(
            const InputLayout& inputLayout,
            const std::initializer_list<uint64_t>& objectCBs,
            const ParameterBox& materialParameters);
        ShaderVariationSet();
        ShaderVariationSet(ShaderVariationSet&& moveFrom) never_throws = default;
        ShaderVariationSet& operator=(ShaderVariationSet&& moveFrom) never_throws = default;
        ~ShaderVariationSet();
    };

    ParameterBox TechParams_SetGeo(const InputLayout& inputLayout);

}}

