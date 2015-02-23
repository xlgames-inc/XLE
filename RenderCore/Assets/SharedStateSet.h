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

namespace SceneEngine { class TechniqueContext; }
namespace Utility { class ParameterBox; }

namespace RenderCore { namespace Assets
{
    class RenderStateSet;
    class CompiledRenderStateSet;

    class IRenderStateSetResolver
    {
    public:
        /// <summary>Given the current global state settings and a technique, build the low-level states for draw call<summary>
        /// There are only 3 influences on render states while rendering models:
        /// <list>
        ///     <item>Local states set on the draw call object
        ///     <item>The global state settings (eg, perhaps set by the lighting parser)
        ///     <item>The technique index/guid (ie, the type of rendering being performed)
        /// </list>
        /// These should be combined together to generate the low level state objects.
        virtual const CompiledRenderStateSet* Compile(
            const RenderStateSet& states, 
            const Utility::ParameterBox& globalStates,
            unsigned techniqueIndex) = 0;
        virtual ~IRenderStateSetResolver();
    };

    class SharedStateSet
    {
    public:
        unsigned InsertTechniqueInterface(
            const RenderCore::Metal::InputElementDesc vertexElements[], unsigned count,
            const uint64 textureBindPoints[], unsigned textureBindPointsCount);

        unsigned InsertShaderName(const std::string& shaderName);
        unsigned InsertParameterBox(const Utility::ParameterBox& box);
        unsigned InsertRenderStateSet(const RenderStateSet& states);

        RenderCore::Metal::BoundUniforms* BeginVariation(
            Metal::DeviceContext* context,
            SceneEngine::TechniqueContext& parserContext,
            unsigned techniqueIndex,
            unsigned shaderName, unsigned techniqueInterface,
            unsigned geoParamBox, unsigned materialParamBox) const;

        void BeginRenderState(
            Metal::DeviceContext* context, 
            // IRenderStateSetResolver& resolver,
            const Utility::ParameterBox& globalStates,
            unsigned techniqueIndex, unsigned renderStateSetIndex) const;

        void CaptureState(Metal::DeviceContext* context);
        void ReleaseState(Metal::DeviceContext* context);

        SharedStateSet();
        ~SharedStateSet();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        mutable unsigned _currentShaderName;
        mutable unsigned _currentTechniqueInterface;
        mutable unsigned _currentMaterialParamBox;
        mutable unsigned _currentGeoParamBox;
        mutable unsigned _currentRenderState;
        mutable uint64 _currentGlobalRenderState;
        mutable RenderCore::Metal::BoundUniforms* _currentBoundUniforms;
    };
}}

