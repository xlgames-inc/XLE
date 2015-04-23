// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define MODEL_FORMAT_RUNTIME 1
#define MODEL_FORMAT_SIMPLE 2
#define MODEL_FORMAT MODEL_FORMAT_RUNTIME

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
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Assets/SharedStateSet.h"
#include "../../Assets/AssetUtils.h"
#include "../../Math/Transformations.h"
#include "../../Utility/HeapUtils.h"
#include "../../Utility/StringFormat.h"

#if MODEL_FORMAT == MODEL_FORMAT_RUNTIME
    #include "../../RenderCore/Assets/ModelRunTime.h"
    #include "../../RenderCore/Assets/MaterialScaffold.h"
    #include "../../Assets/CompileAndAsyncManager.h"
    #include "../../Assets/IntermediateResources.h"
    #include "../../RenderCore/Assets/ColladaCompilerInterface.h"
#else
    #include "../../RenderCore/Assets/ModelSimple.h"
#endif

#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../RenderCore/DX11/Metal/DX11Utils.h"

#include <map>

namespace ToolsRig
{
    #if MODEL_FORMAT == MODEL_FORMAT_RUNTIME
        using RenderCore::Assets::ModelRenderer;
        using RenderCore::Assets::ModelScaffold;
        using RenderCore::Assets::MaterialScaffold;
    #else
        using RenderCore::Assets::Simple::ModelRenderer;
        using RenderCore::Assets::Simple::ModelScaffold;
        using RenderCore::Assets::Simple::MaterialScaffold;
    #endif
    using RenderCore::Assets::SharedStateSet;

    typedef std::pair<Float3, Float3> BoundingBox;
    
    class ModelVisCache::Pimpl
    {
    public:
        std::map<uint64, BoundingBox> _boundingBoxes;

        LRUCache<ModelScaffold>     _modelScaffolds;
        LRUCache<MaterialScaffold>  _materialScaffolds;
        LRUCache<ModelRenderer>     _modelRenderers;

        std::shared_ptr<RenderCore::Assets::IModelFormat> _format;
        std::unique_ptr<SharedStateSet> _sharedStateSet;

        Pimpl();
    };
        
    ModelVisCache::Pimpl::Pimpl()
    : _modelScaffolds(2000)
    , _materialScaffolds(2000)
    , _modelRenderers(50)
    {
    }

    namespace Internal
    {
        std::shared_ptr<ModelScaffold> CreateModelScaffold(const ::Assets::ResChar filename[], RenderCore::Assets::IModelFormat& modelFormat)
        {
            #if MODEL_FORMAT == MODEL_FORMAT_RUNTIME
                auto& compilers = ::Assets::CompileAndAsyncManager::GetInstance().GetIntermediateCompilers();
                auto& store = ::Assets::CompileAndAsyncManager::GetInstance().GetIntermediateStore();
                auto marker = compilers.PrepareResource(
                    RenderCore::Assets::ColladaCompiler::Type_Model, 
                    (const char**)&filename, 1, store);
                return std::make_shared<ModelScaffold>(std::move(marker));
            #else
                return modelFormat.CreateModel(filename);
            #endif
        }

        std::shared_ptr<MaterialScaffold> CreateMaterialScaffold(const ::Assets::ResChar filename[], RenderCore::Assets::IModelFormat& modelFormat)
        {
            #if MODEL_FORMAT == MODEL_FORMAT_RUNTIME
                auto& compilers = ::Assets::CompileAndAsyncManager::GetInstance().GetIntermediateCompilers();
                auto& store = ::Assets::CompileAndAsyncManager::GetInstance().GetIntermediateStore();
                auto marker = compilers.PrepareResource(
                    MaterialScaffold::CompileProcessType, 
                    (const char**)&filename, 1, store);
                return std::make_shared<MaterialScaffold>(std::move(marker));
            #else
                return modelFormat.CreateMaterial(filename);
            #endif
        }
    }

    auto ModelVisCache::GetScaffolds(const Assets::ResChar filename[]) -> Scaffolds
    {
        Scaffolds result;

        result._hashedModelName = Hash64(filename);
        result._model = _pimpl->_modelScaffolds.Get(result._hashedModelName);
        if (!result._model || result._model->GetDependencyValidation().GetValidationIndex() > 0) {
            result._model = Internal::CreateModelScaffold(filename, *_pimpl->_format);
            _pimpl->_modelScaffolds.Insert(result._hashedModelName, result._model);
        }
            
        #if MODEL_FORMAT != MODEL_FORMAT_RUNTIME
            auto defMatName = _pimpl->_format->DefaultMaterialName(*model);
            if (defMatName.empty()) { return std::make_pair(nullptr, 0); }
            uint64 hashedMaterial = Hash64(defMatName);
            auto matNamePtr = defMatName.c_str();
        #else
            uint64 hashedMaterial = result._hashedModelName;
            auto matNamePtr = filename;
        #endif

        result._material = _pimpl->_materialScaffolds.Get(hashedMaterial);
        if (!result._material || result._material->GetDependencyValidation().GetValidationIndex() > 0) {
            result._material = Internal::CreateMaterialScaffold(matNamePtr, *_pimpl->_format);
            _pimpl->_materialScaffolds.Insert(hashedMaterial, result._material);
        }

        return result;
    }

    auto ModelVisCache::GetModel(const Assets::ResChar filename[]) -> Model
    {
        auto scaffold = GetScaffolds(filename);
        if (!scaffold._model || !scaffold._material) { return Model(); }

        uint64 hashedModel = uint64(scaffold._model.get()) | (uint64(scaffold._material.get()) << 48);
        auto renderer = _pimpl->_modelRenderers.Get(hashedModel);
        if (!renderer) {
            #if MODEL_FORMAT == MODEL_FORMAT_RUNTIME
                auto searchRules = ::Assets::DefaultDirectorySearchRules(filename);
                renderer = std::make_shared<ModelRenderer>(
                    std::ref(*scaffold._model), std::ref(*scaffold._material), std::ref(*_pimpl->_sharedStateSet), &searchRules, 0);
            #else
                renderer = _pimpl->_format->CreateRenderer(
                    std::ref(*model), std::ref(*material), std::ref(_pimpl->_sharedStateSet), 0);
            #endif

            _pimpl->_modelRenderers.Insert(hashedModel, renderer);
        }

            // cache the bounding box, because it's an expensive operation to recalculate
        BoundingBox boundingBox;
        auto boundingBoxI = _pimpl->_boundingBoxes.find(scaffold._hashedModelName);
        if (boundingBoxI== _pimpl->_boundingBoxes.end()) {
            boundingBox = scaffold._model->GetStaticBoundingBox(0);
            _pimpl->_boundingBoxes.insert(std::make_pair(scaffold._hashedModelName, boundingBox));
        } else {
            boundingBox = boundingBoxI->second;
        }

        Model result;
        result._renderer = renderer.get();
        result._sharedStateSet = _pimpl->_sharedStateSet.get();
        result._boundingBox = boundingBox;
        result._hashedModelName = scaffold._hashedModelName;
        return result;
    }

    ModelVisCache::ModelVisCache(std::shared_ptr<RenderCore::Assets::IModelFormat> format)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_sharedStateSet = std::make_unique<SharedStateSet>();
        _pimpl->_format = std::move(format);
    }

    ModelVisCache::~ModelVisCache()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    VisCameraSettings AlignCameraToBoundingBox(float verticalFieldOfView, const std::pair<Float3, Float3>& box)
    {
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
    
    class ModelSceneParser : public SceneEngine::ISceneParser
    {
    public:
        RenderCore::Techniques::CameraDesc  GetCameraDesc() const
        {
            return AsCameraDesc(*_settings->_camera);
        }

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

                #if MODEL_FORMAT == MODEL_FORMAT_RUNTIME
                    _model->Render(
                        RenderCore::Assets::ModelRendererContext(context, parserContext, techniqueIndex),
                        *_sharedStateSet,
                        Identity<Float4x4>());
                #else
                    _model->Render(context, parserContext, techniqueIndex, *_sharedStateSet, Identity<Float4x4>(), 0);
                #endif

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

        unsigned GetShadowProjectionCount() const { return 0; }
        SceneEngine::ShadowProjectionDesc GetShadowProjectionDesc(unsigned index, const RenderCore::Techniques::ProjectionDesc& mainSceneProjectionDesc) const 
        { return SceneEngine::ShadowProjectionDesc(); }
        

        unsigned                        GetLightCount() const { return 0; }
        const SceneEngine::LightDesc&   GetLightDesc(unsigned index) const
        {
            static SceneEngine::LightDesc light = DefaultDominantLight();
            return light;
        }

        SceneEngine::GlobalLightingDesc GetGlobalLightingDesc() const
        {
            return DefaultGlobalLightingDesc();
        }

        float GetTimeValue() const { return 0.f; }

        ModelSceneParser(
            const ModelVisSettings& settings,
            ModelRenderer& model, const std::pair<Float3, Float3>& boundingBox, SharedStateSet& sharedStateSet)
            : _model(&model), _boundingBox(boundingBox), _sharedStateSet(&sharedStateSet), _settings(&settings) {}
        ~ModelSceneParser() {}

    protected:
        ModelRenderer * _model;
        SharedStateSet* _sharedStateSet;
        std::pair<Float3, Float3> _boundingBox;
        const ModelVisSettings* _settings;
    };

    std::unique_ptr<SceneEngine::ISceneParser> CreateModelScene(const ModelVisCache::Model& model)
    {
        ModelVisSettings settings;
        *settings._camera = AlignCameraToBoundingBox(40.f, model._boundingBox);
        return std::make_unique<ModelSceneParser>(
            settings, *model._renderer, model._boundingBox, *model._sharedStateSet);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ModelVisLayer::Pimpl
    {
    public:
        std::shared_ptr<ModelVisCache> _cache;
        std::shared_ptr<ModelVisSettings> _settings;
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

        auto model = _pimpl->_cache->GetModel(_pimpl->_settings->_modelName.c_str());
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
            *_pimpl->_settings,
            *model._renderer, model._boundingBox, *model._sharedStateSet);
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
        std::shared_ptr<ModelVisCache> cache) 
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_settings = std::move(settings);
        _pimpl->_cache = std::move(cache);
    }

    ModelVisLayer::~ModelVisLayer() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class VisualisationOverlay::Pimpl
    {
    public:
        std::shared_ptr<ModelVisCache> _cache;
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
                    auto model = _pimpl->_cache->GetModel(_pimpl->_settings->_modelName.c_str());
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
    }

    void VisualisationOverlay::RenderWidgets(
        RenderCore::IThreadContext*, const RenderCore::Techniques::ProjectionDesc&)
    {}

    auto VisualisationOverlay::GetInputListener() -> std::shared_ptr<IInputListener>
    { return nullptr; }

    void VisualisationOverlay::SetActivationState(bool) {}

    VisualisationOverlay::VisualisationOverlay(
        std::shared_ptr<ModelVisSettings> settings,
        std::shared_ptr<ModelVisCache> cache,
        std::shared_ptr<VisMouseOver> mouseOver)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_settings = std::move(settings);
        _pimpl->_cache = std::move(cache);
        _pimpl->_mouseOver = std::move(mouseOver);
    }

    VisualisationOverlay::~VisualisationOverlay() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class MouseOverTrackingListener : public RenderOverlays::DebuggingDisplay::IInputListener
    {
    public:
        bool OnInputEvent(const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt)
        {
            using namespace SceneEngine;

            auto model = _cache->GetModel(_settings->_modelName.c_str());
            assert(model._renderer && model._sharedStateSet);

            auto metalContext = RenderCore::Metal::DeviceContext::Get(*_threadContext);
            auto cam = AsCameraDesc(*_settings->_camera);
            auto worldSpaceRay = IntersectionTestContext::CalculateWorldSpaceRay(
                cam, evnt._mousePosition, PlatformRig::InputTranslator::s_hackWindowSize);
            
            std::vector<ModelIntersectionStateContext::ResultEntry> results;
            TRY {
                ModelIntersectionStateContext stateContext(
                ModelIntersectionStateContext::RayTest,
                    _threadContext, *_techniqueContext, &cam);
                LightingParserContext parserContext(*_techniqueContext);
                stateContext.SetRay(worldSpaceRay);
                model._sharedStateSet->CaptureState(metalContext.get());
                model._renderer->Render(
                    RenderCore::Assets::ModelRendererContext(metalContext.get(), parserContext, 6),
                    *model._sharedStateSet, Identity<Float4x4>());
                model._sharedStateSet->ReleaseState(metalContext.get());

                results = stateContext.GetResults();
            }
            CATCH (const ::Assets::Exceptions::InvalidResource&) {}
            CATCH (const ::Assets::Exceptions::PendingResource&) {}
            CATCH_END

            if (!results.empty()) {
                    // find the closest intersection, and let's get so information from that...
                std::sort(results.begin(), results.end(), ModelIntersectionStateContext::ResultEntry::CompareDepth);

                _mouseOver->_intersectionPt = 
                    worldSpaceRay.first + results[0]._intersectionDepth * Normalize(worldSpaceRay.second - worldSpaceRay.first);

                auto drawCallIndex = results[0]._drawCallIndex;
                auto matBinding = model._renderer->DrawCallToMaterialBinding();
                uint64 materialGuid; 
                if (results[0]._drawCallIndex < matBinding.size()) {
                    materialGuid = matBinding[results[0]._drawCallIndex];
                } else {
                    materialGuid = ~0x0ull;
                }

                if (    drawCallIndex != _mouseOver->_drawCallIndex
                    ||  materialGuid != _mouseOver->_materialGuid
                    ||  !_mouseOver->_hasMouseOver) {

                    _mouseOver->_hasMouseOver = true;
                    _mouseOver->_drawCallIndex = drawCallIndex;
                    _mouseOver->_materialGuid = materialGuid;
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
            std::shared_ptr<ModelVisSettings> settings,
            std::shared_ptr<ModelVisCache> cache)
            : _mouseOver(std::move(mouseOver))
            , _threadContext(std::move(threadContext))
            , _techniqueContext(std::move(techniqueContext))
            , _settings(std::move(settings))
            , _cache(std::move(cache))
        {}
        MouseOverTrackingListener::~MouseOverTrackingListener() {}

    protected:
        std::shared_ptr<VisMouseOver> _mouseOver;
        std::shared_ptr<RenderCore::IThreadContext> _threadContext;
        std::shared_ptr<RenderCore::Techniques::TechniqueContext> _techniqueContext;
        std::shared_ptr<ModelVisSettings> _settings;
        std::shared_ptr<ModelVisCache> _cache;
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
        std::shared_ptr<ModelVisSettings> settings,
        std::shared_ptr<ModelVisCache> cache)
    {
        _inputListener = std::make_shared<MouseOverTrackingListener>(
            std::move(mouseOver),
            std::move(threadContext), std::move(techniqueContext), 
            std::move(settings), std::move(cache));
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
        // _modelName = "game/model/galleon/galleon.dae";
        _modelName = "Game/Model/Nature/BushTree/BushE";
        _pendingCameraAlignToModel = true;
        _doHighlightWireframe = false;
        _highlightRay = std::make_pair(Zero<Float3>(), Zero<Float3>());
        _highlightRayWidth = 0.f;
        _colourByMaterial = 0;
        _camera = std::make_shared<VisCameraSettings>();
    }
    
    VisCameraSettings::VisCameraSettings()
    {
        _position = Zero<Float3>();
        _focus = Zero<Float3>();
        _verticalFieldOfView = 40.f;
        _nearClip = 0.1f;
        _farClip = 1000.f;
    }

}

