// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ModelVisualisation.h"
#include "../SceneEngine/SceneParser.h"
#include "../SceneEngine/LightDesc.h"
#include "../SceneEngine/LightingParser.h"
#include "../SceneEngine/LightingParserContext.h"
#include "../RenderCore/IThreadContext.h"
#include "../RenderCore/Techniques/TechniqueUtils.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Assets/ModelRunTime.h"
#include "../RenderCore/Assets/SharedStateSet.h"
#include "../Math/Transformations.h"

namespace PlatformRig
{
    using RenderCore::Assets::ModelRenderer;
    using RenderCore::Assets::SharedStateSet;

    class ModelSceneParser : public SceneEngine::ISceneParser
    {
    public:
        RenderCore::Techniques::CameraDesc  GetCameraDesc() const
        {
            const float border = 0.0f;
            float maxHalfDimension = .5f * std::max(_boundingBox.second[1] - _boundingBox.first[1], _boundingBox.second[2] - _boundingBox.first[2]);
            RenderCore::Techniques::CameraDesc result;
            result._verticalFieldOfView = Deg2Rad(60.f);
            Float3 position = .5f * (_boundingBox.first + _boundingBox.second);
            position[0] = _boundingBox.first[0] - (maxHalfDimension * (1.f + border)) / XlTan(0.5f * result._verticalFieldOfView);
            result._cameraToWorld = Float4x4(
                0.f, 0.f, -1.f, position[0],
                -1.f, 0.f,  0.f, position[1],
                0.f, 1.f,  0.f, position[2],
                0.f, 0.f, 0.f, 1.f);
            Combine_InPlace(result._cameraToWorld, position);
            result._nearClip = 0.01f;
            result._farClip = 1000.f;
            result._temporaryMatrix = Identity<Float4x4>();
            return result;
        }

        void ExecuteScene(  RenderCore::Metal::DeviceContext* context, 
                            SceneEngine::LightingParserContext& parserContext, 
                            const SceneEngine::SceneParseSettings& parseSettings,
                            unsigned techniqueIndex) const 
        {
            if (    parseSettings._batchFilter == SceneEngine::SceneParseSettings::BatchFilter::Depth
                ||  parseSettings._batchFilter == SceneEngine::SceneParseSettings::BatchFilter::General) {
                _model->Render(
                    ModelRenderer::Context(context, parserContext, techniqueIndex, *_sharedStateSet),
                    Identity<Float4x4>());
            }
        }

        void ExecuteShadowScene(    RenderCore::Metal::DeviceContext* context, 
                                    SceneEngine::LightingParserContext& parserContext, 
                                    const SceneEngine::SceneParseSettings& parseSettings,
                                    unsigned index, unsigned techniqueIndex) const
        {
            ExecuteScene(context, parserContext, parseSettings, techniqueIndex);
        }

        unsigned GetShadowProjectionCount() const { return 0; }
        SceneEngine::ShadowProjectionDesc GetShadowProjectionDesc(unsigned index, const RenderCore::Techniques::ProjectionDesc& mainSceneProjectionDesc) const 
        { return SceneEngine::ShadowProjectionDesc(); }
        

        unsigned                        GetLightCount() const { return 0; }
        const SceneEngine::LightDesc&   GetLightDesc(unsigned index) const
        {
            static SceneEngine::LightDesc light;
            light._type = SceneEngine::LightDesc::Directional;
            light._lightColour = Float3(5.f, 5.f, 5.f);
            light._negativeLightDirection = Float3(0.f, 0.f, 1.f);
            light._radius = 1000.f;
            light._shadowFrustumIndex = ~unsigned(0x0);
            return light;
        }

        SceneEngine::GlobalLightingDesc GetGlobalLightingDesc() const
        {
            SceneEngine::GlobalLightingDesc result;
            result._ambientLight = Float3(0.25f, 0.25f, 0.25f);
            result._skyTexture = nullptr;
            result._doAtmosphereBlur = false;
            result._doOcean = false;
            result._doToneMap = false;
            return result;
        }

        float GetTimeValue() const { return 0.f; }

        ModelSceneParser(
            ModelRenderer& model, const std::pair<Float3, Float3>& boundingBox, const SharedStateSet& sharedStateSet) 
            : _model(&model), _boundingBox(boundingBox), _sharedStateSet(&sharedStateSet) {}
        ~ModelSceneParser() {}

    protected:
        ModelRenderer * _model;
        const SharedStateSet* _sharedStateSet;
        std::pair<Float3, Float3> _boundingBox;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ModelVisLayer::Pimpl
    {
    public:
        std::unique_ptr<ModelRenderer> _renderer;
        std::pair<Float3, Float3> _boundingBox;
        std::unique_ptr<SharedStateSet> _sharedStateSet;
    };

    auto ModelVisLayer::GetInputListener() -> std::shared_ptr<IInputListener>
    {
        return nullptr;
    }

    void ModelVisLayer::RenderToScene(
        RenderCore::IThreadContext* context, 
        SceneEngine::LightingParserContext& parserContext)
    {
        if (_pimpl->_renderer) {
            using namespace SceneEngine;
            
            ModelSceneParser sceneParser(*_pimpl->_renderer, _pimpl->_boundingBox, *_pimpl->_sharedStateSet);
            LightingParser_ExecuteScene(
                *context, parserContext, sceneParser,
                RenderingQualitySettings(context->GetStateDesc()._viewportDimensions));
        }
    }

    void ModelVisLayer::RenderWidgets(
        RenderCore::IThreadContext* device, 
        const RenderCore::Techniques::ProjectionDesc& projectionDesc)
    {
    }

    void ModelVisLayer::SetActivationState(bool newState)
    {}

    ModelVisLayer::ModelVisLayer() 
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_sharedStateSet = std::make_unique<SharedStateSet>();
    }

    ModelVisLayer::~ModelVisLayer() {}

}

