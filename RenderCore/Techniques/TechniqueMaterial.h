// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Techniques.h"
#include "../../Assets/AssetsCore.h"
#include "../../Utility/ParameterBox.h"

namespace RenderCore { namespace Techniques
{
    class PredefinedCBLayout;

    class TechniqueMaterial
    {
    public:
        ParameterBox _materialParameters;
        ParameterBox _geometryParameters;
        TechniqueInterface _techniqueInterface;

        class Variation
        {
        public:
            ResolvedShader      _shader;
            const PredefinedCBLayout* _cbLayout;
        };

        Variation FindVariation(
            ParsingContext& parsingContext,
            unsigned techniqueIndex,
            const ::Assets::ResChar techniqueConfig[]) const;

        const PredefinedCBLayout& GetCBLayout(const ::Assets::ResChar techniqueConfig[]);

        TechniqueMaterial(
            const InputLayout& inputLayout,
            const std::initializer_list<uint64>& objectCBs,
            ParameterBox materialParameters);
        TechniqueMaterial();
        TechniqueMaterial(TechniqueMaterial&& moveFrom);
        const TechniqueMaterial& operator=(TechniqueMaterial&& moveFrom);
        ~TechniqueMaterial();
    };

    ParameterBox TechParams_SetGeo(const InputLayout& inputLayout);
}}

