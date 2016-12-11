// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Metal/Forward.h"
#include "../IThreadContext_Forward.h"
#include "../../Assets/AssetUtils.h"
#include "../../Core/Types.h"
#include <string>
#include <memory>

namespace RenderCore { namespace Techniques 
{ 
    class TechniqueContext; class ParsingContext; 
    class IStateSetResolver;
    class RenderStateSet;
    class PredefinedCBLayout;
}}
namespace RenderCore
{
	class InputElementDesc;
}
namespace Utility { class ParameterBox; }

namespace RenderCore { namespace Assets
{
    class ModelRendererContext
    {
    public:
        Metal::DeviceContext* _context;
        Techniques::ParsingContext* _parserContext;
        unsigned _techniqueIndex;

        ModelRendererContext(
            Metal::DeviceContext& context, 
            Techniques::ParsingContext& parserContext,
            unsigned techniqueIndex)
        : _context(&context), _parserContext(&parserContext), _techniqueIndex(techniqueIndex) {}

        ModelRendererContext(
            IThreadContext& context, 
            Techniques::ParsingContext& parserContext,
            unsigned techniqueIndex);
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

    using SharedTechniqueConfig = Internal::SharedStateIndex<0>;
    using SharedParameterBox = Internal::SharedStateIndex<1>;
    using SharedTechniqueInterface = Internal::SharedStateIndex<2>;
    using SharedRenderStateSet = Internal::SharedStateIndex<3>;

    class SharedStateSet
    {
    public:
        SharedTechniqueInterface InsertTechniqueInterface(
            const RenderCore::InputElementDesc vertexElements[], unsigned count,
            const uint64 textureBindPoints[], unsigned textureBindPointsCount);

        SharedTechniqueConfig InsertTechniqueConfig(StringSection<::Assets::ResChar> shaderName);
        SharedParameterBox InsertParameterBox(const Utility::ParameterBox& box);
        unsigned InsertRenderStateSet(const Techniques::RenderStateSet& states);

        Metal::BoundUniforms* BeginVariation(
            const ModelRendererContext& context, 
            SharedTechniqueConfig shaderName, SharedTechniqueInterface techniqueInterface,
            SharedParameterBox geoParamBox, SharedParameterBox materialParamBox) const;

        void BeginRenderState(
            const ModelRendererContext& context, 
            SharedRenderStateSet renderStateSetIndex) const;

        const Techniques::PredefinedCBLayout* GetCBLayout(SharedTechniqueConfig shaderName);

        class CaptureMarker
        {
        public:
            CaptureMarker();
            ~CaptureMarker();
            CaptureMarker(CaptureMarker&& moveFrom) never_throws;
            CaptureMarker& operator=(CaptureMarker&& moveFrom) never_throws;

        private:
            SharedStateSet* _state;
            Metal::DeviceContext* _metalContext;

            CaptureMarker(Metal::DeviceContext& metalContext, SharedStateSet& state);
            CaptureMarker(const CaptureMarker&) = delete;
            CaptureMarker& operator=(const CaptureMarker&) = delete;

            friend class SharedStateSet;
        };
        
        CaptureMarker CaptureState(
            IThreadContext& context,
            std::shared_ptr<Techniques::IStateSetResolver> stateResolver,
            std::shared_ptr<Utility::ParameterBox> environment);
        CaptureMarker CaptureState(
            Metal::DeviceContext& context,
            std::shared_ptr<Techniques::IStateSetResolver> stateResolver,
            std::shared_ptr<Utility::ParameterBox> environment);

        SharedStateSet(const ::Assets::DirectorySearchRules& shaderSearchDir);
        ~SharedStateSet();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        mutable SharedTechniqueConfig _currentShaderName;
        mutable SharedTechniqueInterface _currentTechniqueInterface;
        mutable SharedParameterBox _currentMaterialParamBox;
        mutable SharedParameterBox _currentGeoParamBox;
        mutable SharedRenderStateSet _currentRenderState;
        mutable Metal::BoundUniforms* _currentBoundUniforms;

        void ReleaseState(Metal::DeviceContext& context);
        friend class CaptureMarker;
    };
  
    
}}

