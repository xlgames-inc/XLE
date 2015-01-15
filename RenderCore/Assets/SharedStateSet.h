// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Metal/Forward.h"
#include "../../Core/Types.h"
#include <string>
#include <memory>

namespace SceneEngine { class ParameterBox; class LightingParserContext; }

namespace RenderCore { namespace Assets
{
    class SharedStateSet
    {
    public:
        unsigned InsertTechniqueInterface(
            const RenderCore::Metal::InputElementDesc vertexElements[], unsigned count,
            const uint64 textureBindPoints[], unsigned textureBindPointsCount);

        unsigned InsertShaderName(const std::string& shaderName);
        unsigned InsertParameterBox(const SceneEngine::ParameterBox& box);

        RenderCore::Metal::BoundUniforms* BeginVariation(
            Metal::DeviceContext* context, 
            SceneEngine::LightingParserContext& parserContext,
            unsigned techniqueIndex,
            unsigned shaderName, unsigned techniqueInterface, 
            unsigned geoParamBox, unsigned materialParamBox) const;

        void Reset();

        SharedStateSet();
        ~SharedStateSet();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        mutable unsigned _currentShaderName;
        mutable unsigned _currentTechniqueInterface;
        mutable unsigned _currentMaterialParamBox;
        mutable RenderCore::Metal::BoundUniforms* _currentBoundUniforms;
    };
}}

