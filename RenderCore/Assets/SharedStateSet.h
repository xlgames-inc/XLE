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

namespace RenderCore { namespace Techniques { class TechniqueContext; class ParsingContext; } }
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

    class ModelRendererContext
    {
    public:
        Metal::DeviceContext* _context;
        Techniques::ParsingContext* _parserContext;
        unsigned _techniqueIndex;

        ModelRendererContext(
            Metal::DeviceContext* context, 
            Techniques::ParsingContext& parserContext,
            unsigned techniqueIndex)
            : _context(context), _parserContext(&parserContext), _techniqueIndex(techniqueIndex) {}
    };

    namespace Internal
    {
        template<int UniqueIndex>
            class SharedStateIndex
            {
            public:
                static const unsigned Invalid = ~0u;
                
                unsigned Value() const { return _value; }
                SharedStateIndex(unsigned value) : _value(value) {}
                SharedStateIndex() : _value(Invalid) {}

                friend bool operator==(const SharedStateIndex& lhs, const SharedStateIndex& rhs)    { return lhs._value == rhs._value; }
                friend bool operator!=(const SharedStateIndex& lhs, const SharedStateIndex& rhs)    { return lhs._value != rhs._value; }
                friend bool operator<(const SharedStateIndex& lhs, const SharedStateIndex& rhs)     { return lhs._value < rhs._value; }
            private:
                unsigned _value;
            };
    }

    using SharedShaderName = Internal::SharedStateIndex<0>;
    using SharedParameterBox = Internal::SharedStateIndex<1>;
    using SharedTechniqueInterface = Internal::SharedStateIndex<2>;
    using SharedRenderStateSet = Internal::SharedStateIndex<3>;

    class SharedStateSet
    {
    public:
        SharedTechniqueInterface InsertTechniqueInterface(
            const RenderCore::Metal::InputElementDesc vertexElements[], unsigned count,
            const uint64 textureBindPoints[], unsigned textureBindPointsCount);

        SharedShaderName InsertShaderName(const std::string& shaderName);
        SharedParameterBox InsertParameterBox(const Utility::ParameterBox& box);
        unsigned InsertRenderStateSet(const RenderStateSet& states);

        Metal::BoundUniforms* BeginVariation(
            const ModelRendererContext& context, 
            SharedShaderName shaderName, SharedTechniqueInterface techniqueInterface,
            SharedParameterBox geoParamBox, SharedParameterBox materialParamBox) const;

        void BeginRenderState(
            const ModelRendererContext& context, 
            const Utility::ParameterBox& globalStates,
            SharedRenderStateSet renderStateSetIndex) const;

        void CaptureState(Metal::DeviceContext* context);
        void ReleaseState(Metal::DeviceContext* context);

        SharedStateSet();
        ~SharedStateSet();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        mutable SharedShaderName _currentShaderName;
        mutable SharedTechniqueInterface _currentTechniqueInterface;
        mutable SharedParameterBox _currentMaterialParamBox;
        mutable SharedParameterBox _currentGeoParamBox;
        mutable SharedRenderStateSet _currentRenderState;
        mutable uint64 _currentGlobalRenderState;
        mutable Metal::BoundUniforms* _currentBoundUniforms;
    };
}}

