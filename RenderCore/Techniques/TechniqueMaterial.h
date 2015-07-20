// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Techniques.h"
#include "../Metal/Forward.h"
#include "../../Utility/ParameterBox.h"

namespace RenderCore { namespace Techniques
{
    class TechniqueMaterial
    {
    public:
        ParameterBox _materialParameters;
        ParameterBox _geometryParameters;
        TechniqueInterface _techniqueInterface;

        ResolvedShader FindVariation(
            ParsingContext& parsingContext,
            unsigned techniqueIndex,
            const char shaderName[]) const;

        TechniqueMaterial(
            const Metal::InputLayout& inputLayout,
            const std::initializer_list<uint64>& objectCBs,
            ParameterBox materialParameters);
        TechniqueMaterial();
        TechniqueMaterial(TechniqueMaterial&& moveFrom);
        const TechniqueMaterial& operator=(TechniqueMaterial&& moveFrom);
        ~TechniqueMaterial();
    };
}}

