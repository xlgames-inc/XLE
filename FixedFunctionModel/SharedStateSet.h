// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"
#include "../RenderCore/IThreadContext_Forward.h"
#include "../Assets/AssetUtils.h"
#include "../Core/Types.h"
#include <string>
#include <memory>

namespace RenderCore
{
	class InputElementDesc;
	namespace Assets { class RenderStateSet; }
	namespace Techniques 
	{ 
		class TechniqueContext; class ParsingContext; 
		class IRenderStateDelegate;
		class PredefinedCBLayout;
	}
}
namespace Utility { class ParameterBox; }

namespace FixedFunctionModel
{
    class ModelRendererContext
    {
    public:
        RenderCore::Metal::DeviceContext* _context;
        RenderCore::Techniques::ParsingContext* _parserContext;
        unsigned _techniqueIndex;

        ModelRendererContext(
            RenderCore::Metal::DeviceContext& context, 
            RenderCore::Techniques::ParsingContext& parserContext,
            unsigned techniqueIndex)
        : _context(&context), _parserContext(&parserContext), _techniqueIndex(techniqueIndex) {}

        ModelRendererContext(
            RenderCore::IThreadContext& context, 
            RenderCore::Techniques::ParsingContext& parserContext,
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
        unsigned InsertRenderStateSet(const RenderCore::Assets::RenderStateSet& states);

		struct BoundVariation
		{
			RenderCore::Metal::BoundUniforms* _uniforms = nullptr;
			RenderCore::Metal::BoundInputLayout* _inputLayout = nullptr;
		};
        BoundVariation BeginVariation(
            const ModelRendererContext& context, 
            SharedTechniqueConfig shaderName, SharedTechniqueInterface techniqueInterface,
            SharedParameterBox geoParamBox, SharedParameterBox materialParamBox) const;

        void BeginRenderState(
            const ModelRendererContext& context, 
            SharedRenderStateSet renderStateSetIndex) const;

        const RenderCore::Techniques::PredefinedCBLayout* GetCBLayout(SharedTechniqueConfig shaderName);

        class CaptureMarker
        {
        public:
            CaptureMarker();
            ~CaptureMarker();
            CaptureMarker(CaptureMarker&& moveFrom) never_throws;
            CaptureMarker& operator=(CaptureMarker&& moveFrom) never_throws;

        private:
            SharedStateSet* _state;
            RenderCore::Metal::DeviceContext* _metalContext;

            CaptureMarker(RenderCore::Metal::DeviceContext& metalContext, SharedStateSet& state);
            CaptureMarker(const CaptureMarker&) = delete;
            CaptureMarker& operator=(const CaptureMarker&) = delete;

            friend class SharedStateSet;
        };
        
        CaptureMarker CaptureState(
            RenderCore::IThreadContext& context,
            std::shared_ptr<RenderCore::Techniques::IRenderStateDelegate> stateResolver,
            std::shared_ptr<Utility::ParameterBox> environment);
        CaptureMarker CaptureState(
            RenderCore::Metal::DeviceContext& context,
            std::shared_ptr<RenderCore::Techniques::IRenderStateDelegate> stateResolver,
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
        mutable RenderCore::Metal::BoundUniforms* _currentBoundUniforms;
		mutable RenderCore::Metal::BoundInputLayout* _currentBoundLayout;

        void ReleaseState(RenderCore::Metal::DeviceContext& context);
        friend class CaptureMarker;
    };
  
    
}

