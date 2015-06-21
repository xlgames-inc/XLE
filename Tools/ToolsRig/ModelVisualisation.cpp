// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ModelVisualisation.h"
#include "VisualisationUtils.h"
#include "HighlightEffects.h"
#include "../../PlatformRig/InputTranslator.h"
#include "../../SceneEngine/SceneParser.h"
#include "../../SceneEngine/LightDesc.h"
#include "../../SceneEngine/LightingParser.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/RayVsModel.h"
#include "../../SceneEngine/IntersectionTest.h"
#include "../../RenderOverlays/DebuggingDisplay.h"
#include "../../RenderCore/IThreadContext.h"
#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Assets/SharedStateSet.h"
#include "../../Assets/AssetUtils.h"
#include "../../Math/Transformations.h"
#include "../../Utility/HeapUtils.h"
#include "../../Utility/StringFormat.h"

#include "../../RenderCore/Assets/ModelCache.h"
#include "../../RenderCore/Assets/ModelRunTime.h"

#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../RenderCore/DX11/Metal/DX11Utils.h"

#include <map>

namespace ToolsRig
{
    using RenderCore::Assets::ModelRenderer;
    using RenderCore::Assets::ModelScaffold;
    using RenderCore::Assets::MaterialScaffold;
    using RenderCore::Assets::SharedStateSet;
    using RenderCore::Assets::ModelCache;

///////////////////////////////////////////////////////////////////////////////////////////////////

    VisCameraSettings AlignCameraToBoundingBox(float verticalFieldOfView, const std::pair<Float3, Float3>& boxIn)
    {
        auto box = boxIn;

            // convert empty/inverted boxes into something rational...
        if (box.first[0] >= box.second[0] || box.first[1] >= box.second[1] || box.first[1] >= box.second[1]) {
            box.first = Float3(-10.f, -10.f, -10.f);
            box.second = Float3( 10.f,  10.f,  10.f);
        }

        const float border = 0.0f;
        Float3 position = .5f * (box.first + box.second);

            // push back to attempt to fill the viewport with the bounding box
        float verticalHalfDimension = .5f * box.second[2] - box.first[2];
        position[0] = box.first[0] - (verticalHalfDimension * (1.f + border)) / XlTan(.5f * Deg2Rad(verticalFieldOfView));

        VisCameraSettings result;
        result._position = position;
        result._focus = .5f * (box.first + box.second);
        result._verticalFieldOfView = verticalFieldOfView;
        result._farClip = 1.25f * (box.second[0] - position[0]);
        result._nearClip = result._farClip / 10000.f;
        return result;
    }
    
    class ModelSceneParser : public VisSceneParser
    {
    public:
        void ExecuteScene(  RenderCore::Metal::DeviceContext* context, 
                            SceneEngine::LightingParserContext& parserContext, 
                            const SceneEngine::SceneParseSettings& parseSettings,
                            unsigned techniqueIndex) const 
        {
            if (    parseSettings._batchFilter == SceneEngine::SceneParseSettings::BatchFilter::Depth
                ||  parseSettings._batchFilter == SceneEngine::SceneParseSettings::BatchFilter::General) {

                if (_sharedStateSet) {
                    _sharedStateSet->CaptureState(context);
                }

                _model->Render(
                    RenderCore::Assets::ModelRendererContext(context, parserContext, techniqueIndex),
                    *_sharedStateSet,
                    Identity<Float4x4>());

                if (_sharedStateSet) {
                    _sharedStateSet->ReleaseState(context);
                }
            }
        }

        void ExecuteShadowScene(    RenderCore::Metal::DeviceContext* context, 
                                    SceneEngine::LightingParserContext& parserContext, 
                                    const SceneEngine::SceneParseSettings& parseSettings,
                                    unsigned index, unsigned techniqueIndex) const
        {
            ExecuteScene(context, parserContext, parseSettings, techniqueIndex);
        }

        ModelSceneParser(
            const ModelVisSettings& settings,
            const VisEnvSettings& envSettings,
            ModelRenderer& model, const std::pair<Float3, Float3>& boundingBox, SharedStateSet& sharedStateSet)
            : VisSceneParser(settings._camera, envSettings)
            , _model(&model), _boundingBox(boundingBox), _sharedStateSet(&sharedStateSet)
            , _settings(&settings) {}
        ~ModelSceneParser() {}

    protected:
        ModelRenderer * _model;
        SharedStateSet* _sharedStateSet;
        std::pair<Float3, Float3> _boundingBox;
        const ModelVisSettings* _settings;
    };

    std::unique_ptr<SceneEngine::ISceneParser> CreateModelScene(const ModelCache::Model& model)
    {
        ModelVisSettings settings;
        *settings._camera = AlignCameraToBoundingBox(40.f, model._boundingBox);
        static VisEnvSettings tempHack;
        return std::make_unique<ModelSceneParser>(
            settings, tempHack,
            *model._renderer, model._boundingBox, *model._sharedStateSet);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ModelVisLayer::Pimpl
    {
    public:
        std::shared_ptr<ModelCache> _cache;
        std::shared_ptr<ModelVisSettings> _settings;
        std::shared_ptr<VisEnvSettings> _envSettings;
    };

    auto ModelVisLayer::GetInputListener() -> std::shared_ptr<IInputListener>
    {
        return nullptr;
    }

    void ModelVisLayer::RenderToScene(
        RenderCore::IThreadContext* context, 
        SceneEngine::LightingParserContext& parserContext)
    {
        using namespace SceneEngine;

        auto model = _pimpl->_cache->GetModel(_pimpl->_settings->_modelName.c_str(), _pimpl->_settings->_materialName.c_str());
        assert(model._renderer && model._sharedStateSet);

        if (_pimpl->_settings->_pendingCameraAlignToModel) {
                // After the model is loaded, if we have a pending camera align,
                // we should reset the camera to the match the model.
                // We also need to trigger the change event after we make a change...
            *_pimpl->_settings->_camera = AlignCameraToBoundingBox(
                _pimpl->_settings->_camera->_verticalFieldOfView,
                model._boundingBox);
            _pimpl->_settings->_pendingCameraAlignToModel = false;
            _pimpl->_settings->_changeEvent.Trigger();
        }

        ModelSceneParser sceneParser(
            *_pimpl->_settings, *_pimpl->_envSettings,
            *model._renderer, model._boundingBox, *model._sharedStateSet);
        sceneParser.Prepare();
        LightingParser_ExecuteScene(
            *context, parserContext, sceneParser,
            RenderingQualitySettings(context->GetStateDesc()._viewportDimensions));
    }

    void ModelVisLayer::RenderWidgets(
        RenderCore::IThreadContext* device, 
        const RenderCore::Techniques::ProjectionDesc& projectionDesc)
    {
    }

    void ModelVisLayer::SetActivationState(bool newState)
    {}

    ModelVisLayer::ModelVisLayer(
        std::shared_ptr<ModelVisSettings> settings,
        std::shared_ptr<VisEnvSettings> envSettings,
        std::shared_ptr<ModelCache> cache) 
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_settings = std::move(settings);
        _pimpl->_cache = std::move(cache);
        _pimpl->_envSettings = std::move(envSettings);
    }

    ModelVisLayer::~ModelVisLayer() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class VisualisationOverlay::Pimpl
    {
    public:
        std::shared_ptr<ModelCache> _cache;
        std::shared_ptr<ModelVisSettings> _settings;
        std::shared_ptr<VisMouseOver> _mouseOver;
    };
    
    void VisualisationOverlay::RenderToScene(
        RenderCore::IThreadContext* context, 
        SceneEngine::LightingParserContext& parserContext)
    {
            //  Draw an overlay over the scene, 
            //  containing debugging / profiling information
        if (_pimpl->_settings->_colourByMaterial) {

            if (_pimpl->_settings->_colourByMaterial == 2 && !_pimpl->_mouseOver->_hasMouseOver) {
                return;
            }

            TRY 
            {
                using namespace RenderCore;
                auto metalContext = Metal::DeviceContext::Get(*context);

                HighlightByStencilSettings settings;

                    //  We need to query the model to build a lookup table between draw call index
                    //  and material binding index. The shader reads a draw call index from the 
                    //  stencil buffer and remaps that into a material index using this table.
                if (_pimpl->_cache) {
                    auto model = _pimpl->_cache->GetModel(_pimpl->_settings->_modelName.c_str(), _pimpl->_settings->_materialName.c_str());
                    assert(model._renderer && model._sharedStateSet);

                    auto bindingVec = model._renderer->DrawCallToMaterialBinding();
                    unsigned t = 0;
                    settings._stencilToMarkerMap[t++] = UInt4(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff);
                    for (auto i=bindingVec.cbegin(); i!=bindingVec.cend() && t < dimof(settings._stencilToMarkerMap); ++i, ++t) {
                        settings._stencilToMarkerMap[t] = UInt4((unsigned)*i, (unsigned)*i, (unsigned)*i, (unsigned)*i);
                    }

                    auto guid = _pimpl->_mouseOver->_materialGuid;
                    settings._highlightedMarker = UInt4(unsigned(guid), unsigned(guid), unsigned(guid), unsigned(guid));
                }

                SceneEngine::SavedTargets savedTargets(metalContext.get());

                Metal::ShaderResourceView depthSrv;
                if (savedTargets.GetDepthStencilView())
                    depthSrv = Metal::ShaderResourceView(
                        Metal::ExtractResource<ID3D::Resource>(savedTargets.GetDepthStencilView()).get(), 
                        (Metal::NativeFormat::Enum)DXGI_FORMAT_X24_TYPELESS_G8_UINT);

                metalContext->GetUnderlying()->OMSetRenderTargets(1, savedTargets.GetRenderTargets(), nullptr); // (unbind depth)
                ExecuteHighlightByStencil(*metalContext, depthSrv, settings, _pimpl->_settings->_colourByMaterial==2);
                savedTargets.ResetToOldTargets(metalContext.get());
            }
            CATCH (const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); } 
            CATCH (const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); } 
            CATCH_END
        }

        if (_pimpl->_settings->_drawWireframe) {

            TRY 
            {
                using namespace RenderCore;
                auto metalContext = Metal::DeviceContext::Get(*context);

                auto model = _pimpl->_cache->GetModel(_pimpl->_settings->_modelName.c_str(), _pimpl->_settings->_materialName.c_str());
                assert(model._renderer && model._sharedStateSet);

                if (model._sharedStateSet) {
                    model._sharedStateSet->CaptureState(metalContext.get());
                }

                const auto techniqueIndex = 8u;

                model._renderer->Render(
                    RenderCore::Assets::ModelRendererContext(metalContext.get(), parserContext, techniqueIndex),
                    *model._sharedStateSet,
                    Identity<Float4x4>());

                if (model._sharedStateSet) {
                    model._sharedStateSet->ReleaseState(metalContext.get());
                }

            }
            CATCH (const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); } 
            CATCH (const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); } 
            CATCH_END
        }

        if (_pimpl->_settings->_drawNormals) {

            TRY 
            {
                using namespace RenderCore;
                auto metalContext = Metal::DeviceContext::Get(*context);

                auto model = _pimpl->_cache->GetModel(_pimpl->_settings->_modelName.c_str(), _pimpl->_settings->_materialName.c_str());
                assert(model._renderer && model._sharedStateSet);

                if (model._sharedStateSet) {
                    model._sharedStateSet->CaptureState(metalContext.get());
                }

                const auto techniqueIndex = 7u;

                model._renderer->Render(
                    RenderCore::Assets::ModelRendererContext(metalContext.get(), parserContext, techniqueIndex),
                    *model._sharedStateSet,
                    Identity<Float4x4>());

                if (model._sharedStateSet) {
                    model._sharedStateSet->ReleaseState(metalContext.get());
                }

            }
            CATCH (const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); } 
            CATCH (const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); } 
            CATCH_END
        }
    }

    void VisualisationOverlay::RenderWidgets(
        RenderCore::IThreadContext*, const RenderCore::Techniques::ProjectionDesc&)
    {}

    auto VisualisationOverlay::GetInputListener() -> std::shared_ptr<IInputListener>
    { return nullptr; }

    void VisualisationOverlay::SetActivationState(bool) {}

    VisualisationOverlay::VisualisationOverlay(
        std::shared_ptr<ModelVisSettings> settings,
        std::shared_ptr<ModelCache> cache,
        std::shared_ptr<VisMouseOver> mouseOver)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_settings = std::move(settings);
        _pimpl->_cache = std::move(cache);
        _pimpl->_mouseOver = std::move(mouseOver);
    }

    VisualisationOverlay::~VisualisationOverlay() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class SingleModelIntersectionResolver : public SceneEngine::IIntersectionTester
    {
    public:
        virtual Result FirstRayIntersection(
            const SceneEngine::IntersectionTestContext& context,
            std::pair<Float3, Float3> worldSpaceRay) const;

        virtual void FrustumIntersection(
            std::vector<Result>& results,
            const SceneEngine::IntersectionTestContext& context,
            const Float4x4& worldToProjection) const;

        SingleModelIntersectionResolver(
            std::shared_ptr<ModelVisSettings> settings,
            std::shared_ptr<ModelCache> cache);
        ~SingleModelIntersectionResolver();
    protected:
        std::shared_ptr<ModelVisSettings> _settings;
        std::shared_ptr<ModelCache> _cache;
    };

    auto SingleModelIntersectionResolver::FirstRayIntersection(
        const SceneEngine::IntersectionTestContext& context,
        std::pair<Float3, Float3> worldSpaceRay) const -> Result
    {
        using namespace SceneEngine;

        auto model = _cache->GetModel(_settings->_modelName.c_str(), _settings->_materialName.c_str());
        assert(model._renderer && model._sharedStateSet);

        auto metalContext = RenderCore::Metal::DeviceContext::Get(*context.GetThreadContext());

        auto cam = context.GetCameraDesc();
        ModelIntersectionStateContext stateContext(
            ModelIntersectionStateContext::RayTest,
            context.GetThreadContext(), context.GetTechniqueContext(), 
            &cam);
        LightingParserContext parserContext(context.GetTechniqueContext());
        stateContext.SetRay(worldSpaceRay);

        model._sharedStateSet->CaptureState(metalContext.get());
        model._renderer->Render(
            RenderCore::Assets::ModelRendererContext(metalContext.get(), parserContext, 6),
            *model._sharedStateSet, Identity<Float4x4>());
        model._sharedStateSet->ReleaseState(metalContext.get());

        auto results = stateContext.GetResults();
        if (!results.empty()) {
            const auto& r = results[0];

            Result result;
            result._type = IntersectionTestScene::Type::Extra;
            result._worldSpaceCollision = 
                worldSpaceRay.first + r._intersectionDepth * Normalize(worldSpaceRay.second - worldSpaceRay.first);
            result._distance = r._intersectionDepth;
            result._drawCallIndex = r._drawCallIndex;
            result._materialGuid = r._materialGuid;
            result._materialName = _settings->_materialName;
            result._modelName = _settings->_modelName;

                // fill in the material guid if it wasn't correctly set by the shader
            if (result._materialGuid == ~0x0ull) {
                auto matBinding = model._renderer->DrawCallToMaterialBinding();
                if (result._drawCallIndex < matBinding.size())
                    result._materialGuid = matBinding[result._drawCallIndex];
            }
            return result;
        }

        return Result();
    }

    void SingleModelIntersectionResolver::FrustumIntersection(
        std::vector<Result>& results,
        const SceneEngine::IntersectionTestContext& context,
        const Float4x4& worldToProjection) const
    {}

    SingleModelIntersectionResolver::SingleModelIntersectionResolver(
        std::shared_ptr<ModelVisSettings> settings,
        std::shared_ptr<ModelCache> cache)
    : _settings(settings), _cache(cache)
    {}

    SingleModelIntersectionResolver::~SingleModelIntersectionResolver()
    {}

    std::shared_ptr<SceneEngine::IntersectionTestScene> CreateModelIntersectionScene(
        std::shared_ptr<ModelVisSettings> settings, std::shared_ptr<ModelCache> cache)
    {
        std::shared_ptr<SceneEngine::IIntersectionTester> resolver = 
            std::make_shared<SingleModelIntersectionResolver>(std::move(settings), std::move(cache));
        return std::shared_ptr<SceneEngine::IntersectionTestScene>(
            new SceneEngine::IntersectionTestScene(nullptr, nullptr, { resolver }));
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class MouseOverTrackingListener : public RenderOverlays::DebuggingDisplay::IInputListener
    {
    public:
        bool OnInputEvent(const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt)
        {
            using namespace SceneEngine;

            auto cam = AsCameraDesc(*_camera);
            IntersectionTestContext testContext(
                _threadContext, cam, 
                std::make_shared<RenderCore::ViewportContext>(PlatformRig::InputTranslator::s_hackWindowSize),
                _techniqueContext);
            auto worldSpaceRay = testContext.CalculateWorldSpaceRay(
                evnt._mousePosition);
                
            auto intr = _scene->FirstRayIntersection(testContext, worldSpaceRay);
            if (intr._type != 0) {
                if (        intr._drawCallIndex != _mouseOver->_drawCallIndex
                        ||  intr._materialGuid != _mouseOver->_materialGuid
                        ||  !_mouseOver->_hasMouseOver) {

                    _mouseOver->_hasMouseOver = true;
                    _mouseOver->_drawCallIndex = intr._drawCallIndex;
                    _mouseOver->_materialGuid = intr._materialGuid;
                    _mouseOver->_changeEvent.Trigger();
                }
            } else {
                if (_mouseOver->_hasMouseOver) {
                    _mouseOver->_hasMouseOver = false;
                    _mouseOver->_changeEvent.Trigger();
                }
            }

            return false;
        }

        MouseOverTrackingListener(
            std::shared_ptr<VisMouseOver> mouseOver,
            std::shared_ptr<RenderCore::IThreadContext> threadContext,
            std::shared_ptr<RenderCore::Techniques::TechniqueContext> techniqueContext,
            std::shared_ptr<VisCameraSettings> camera,
            std::shared_ptr<SceneEngine::IntersectionTestScene> scene)
            : _mouseOver(std::move(mouseOver))
            , _threadContext(std::move(threadContext))
            , _techniqueContext(std::move(techniqueContext))
            , _camera(std::move(camera))
            , _scene(std::move(scene))
        {}
        MouseOverTrackingListener::~MouseOverTrackingListener() {}

    protected:
        std::shared_ptr<VisMouseOver> _mouseOver;
        std::shared_ptr<RenderCore::IThreadContext> _threadContext;
        std::shared_ptr<RenderCore::Techniques::TechniqueContext> _techniqueContext;
        std::shared_ptr<VisCameraSettings> _camera;
        std::shared_ptr<SceneEngine::IntersectionTestScene> _scene;
    };

    auto MouseOverTrackingOverlay::GetInputListener() -> std::shared_ptr<IInputListener>
    {
        return _inputListener;
    }

    void MouseOverTrackingOverlay::RenderToScene(
        RenderCore::IThreadContext*, 
        SceneEngine::LightingParserContext&) {}
    void MouseOverTrackingOverlay::RenderWidgets(
        RenderCore::IThreadContext*, 
        const RenderCore::Techniques::ProjectionDesc&) {}
    void MouseOverTrackingOverlay::SetActivationState(bool) {}

    MouseOverTrackingOverlay::MouseOverTrackingOverlay(
        std::shared_ptr<VisMouseOver> mouseOver,
        std::shared_ptr<RenderCore::IThreadContext> threadContext,
        std::shared_ptr<RenderCore::Techniques::TechniqueContext> techniqueContext,
        std::shared_ptr<VisCameraSettings> camera,
        std::shared_ptr<SceneEngine::IntersectionTestScene> scene)
    {
        _inputListener = std::make_shared<MouseOverTrackingListener>(
            std::move(mouseOver),
            std::move(threadContext), std::move(techniqueContext), 
            std::move(camera), std::move(scene));
    }

    MouseOverTrackingOverlay::~MouseOverTrackingOverlay() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    void ChangeEvent::Trigger() 
    {
        for (auto i=_callbacks.begin(); i!=_callbacks.end(); ++i) {
            (*i)->OnChange();
        }
    }
    ChangeEvent::~ChangeEvent() {}

    ModelVisSettings::ModelVisSettings()
    {
        _modelName = "game/model/galleon/galleon.dae";
        _materialName = "game/model/galleon/galleon.material";
        _pendingCameraAlignToModel = true;
        _doHighlightWireframe = false;
        _highlightRay = std::make_pair(Zero<Float3>(), Zero<Float3>());
        _highlightRayWidth = 0.f;
        _colourByMaterial = 0;
        _camera = std::make_shared<VisCameraSettings>();
        _drawNormals = false;
        _drawWireframe = false;
    }

}

