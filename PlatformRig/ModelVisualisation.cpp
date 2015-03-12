// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define MODEL_FORMAT_RUNTIME 1
#define MODEL_FORMAT_SIMPLE 2
#define MODEL_FORMAT MODEL_FORMAT_RUNTIME

#include "ModelVisualisation.h"
#include "../SceneEngine/SceneParser.h"
#include "../SceneEngine/LightDesc.h"
#include "../SceneEngine/LightingParser.h"
#include "../SceneEngine/LightingParserContext.h"
#include "../RenderCore/IThreadContext.h"
#include "../RenderCore/Techniques/TechniqueUtils.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Assets/SharedStateSet.h"
#include "../Assets/AssetUtils.h"
#include "../Math/Transformations.h"
#include "../Utility/HeapUtils.h"

#if MODEL_FORMAT == MODEL_FORMAT_RUNTIME
    #include "../../RenderCore/Assets/ModelRunTime.h"
    #include "../../Assets/CompileAndAsyncManager.h"
    #include "../../Assets/IntermediateResources.h"
    #include "../../RenderCore/Assets/ColladaCompilerInterface.h"
#else
    #include "../../RenderCore/Assets/ModelSimple.h"
#endif

#include <map>

#if MODEL_FORMAT == MODEL_FORMAT_RUNTIME
    namespace Assets
    {
        template<> uint64 GetCompileProcessType<RenderCore::Assets::ModelScaffold>();
        // { 
        //     return RenderCore::Assets::ColladaCompiler::Type_Model; 
        // }
    }
#endif

namespace PlatformRig
{
    #if MODEL_FORMAT == MODEL_FORMAT_RUNTIME
        using RenderCore::Assets::ModelRenderer;
        using RenderCore::Assets::ModelScaffold;
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
        #if MODEL_FORMAT != MODEL_FORMAT_RUNTIME
            LRUCache<MaterialScaffold>  _materialScaffolds;
        #endif
        LRUCache<ModelRenderer>     _modelRenderers;

        std::shared_ptr<RenderCore::Assets::IModelFormat> _format;
        std::unique_ptr<SharedStateSet> _sharedStateSet;

        Pimpl();
    };
        
    ModelVisCache::Pimpl::Pimpl()
    : _modelScaffolds(2000)
    #if MODEL_FORMAT != MODEL_FORMAT_RUNTIME
        , _materialScaffolds(2000)
    #endif
    , _modelRenderers(50)
    {
    }

    auto ModelVisCache::GetModel(const Assets::ResChar filename[]) -> Model
    {
        #if MODEL_FORMAT == MODEL_FORMAT_SIMPLE
            if (!_pimpl->_format) {
                return std::make_pair(nullptr, 0);
            }
        #endif

        uint64 hashedName = Hash64(filename);
        auto model = _pimpl->_modelScaffolds.Get(hashedName);
        if (!model) {
            #if MODEL_FORMAT == MODEL_FORMAT_RUNTIME
                auto& man = ::Assets::CompileAndAsyncManager::GetInstance();
                auto& compilers = man.GetIntermediateCompilers();
                auto& store = man.GetIntermediateStore();
                const Assets::ResChar* inits[] = { filename };
                auto marker = compilers.PrepareResource(
                    ::Assets::GetCompileProcessType<ModelScaffold>(), 
                    inits, dimof(inits), store);
                model = std::make_shared<ModelScaffold>(std::move(marker));
            #else
                model = _pimpl->_format->CreateModel((const char*)utf8Filename);
            #endif
            _pimpl->_modelScaffolds.Insert(hashedName, model);
        }
            
        #if MODEL_FORMAT != MODEL_FORMAT_RUNTIME
            auto defMatName = _pimpl->_format->DefaultMaterialName(*model);
            if (defMatName.empty()) { return std::make_pair(nullptr, 0); }

            uint64 hashedMaterial = Hash64(defMatName);
            auto material = _pimpl->_materialScaffolds.Get(hashedMaterial);
            if (!material) {
                material = _pimpl->_format->CreateMaterial(defMatName.c_str());
                _pimpl->_materialScaffolds.Insert(hashedMaterial, material);
            }
            uint64 hashedModel = uint64(model.get()) | (uint64(material.get()) << 48);
        #else
            uint64 hashedModel = uint64(model.get());
        #endif
        
        auto renderer = _pimpl->_modelRenderers.Get(hashedModel);
        if (!renderer) {
            #if MODEL_FORMAT == MODEL_FORMAT_RUNTIME
                auto searchRules = ::Assets::DefaultDirectorySearchRules(filename);
                renderer = std::make_shared<ModelRenderer>(
                    std::ref(*model), std::ref(*_pimpl->_sharedStateSet), &searchRules, 0);
            #else
                renderer = _pimpl->_format->CreateRenderer(
                    std::ref(*model), std::ref(*material), std::ref(_pimpl->_sharedStateSet), 0);
            #endif

            _pimpl->_modelRenderers.Insert(hashedModel, renderer);
        }

            // cache the bounding box, because it's an expensive operation to recalculate
        BoundingBox boundingBox;
        auto boundingBoxI = _pimpl->_boundingBoxes.find(hashedName);
        if (boundingBoxI== _pimpl->_boundingBoxes.end()) {
            boundingBox = model->GetStaticBoundingBox(0);
            _pimpl->_boundingBoxes.insert(std::make_pair(hashedName, boundingBox));
        } else {
            boundingBox = boundingBoxI->second;
        }

        Model result;
        result._renderer = renderer.get();
        result._sharedStateSet = _pimpl->_sharedStateSet.get();
        result._boundingBox = boundingBox;
        result._hash = hashedName;
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

    class ModelSceneParser : public SceneEngine::ISceneParser
    {
    public:
        RenderCore::Techniques::CameraDesc  GetCameraDesc() const
        {
            const float border = 0.0f;
            static float fov = 40.f;

            RenderCore::Techniques::CameraDesc result;
            result._verticalFieldOfView = Deg2Rad(fov);
            Float3 position = .5f * (_boundingBox.first + _boundingBox.second);

                // push back to attempt to fill the viewport with the bounding box
            float verticalHalfDimension = .5f * _boundingBox.second[2] - _boundingBox.first[2];
            position[0] = _boundingBox.first[0] - (verticalHalfDimension * (1.f + border)) / XlTan(.5f * result._verticalFieldOfView);

            result._cameraToWorld = Float4x4(
                0.f, 0.f, -1.f, position[0],
                -1.f, 0.f,  0.f, position[1],
                0.f, 1.f,  0.f, position[2],
                0.f, 0.f, 0.f, 1.f);
            result._farClip = 1.25f * (_boundingBox.second[0] - position[0]);
            result._nearClip = result._farClip / 10000.f;
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

                if (_sharedStateSet) {
                    _sharedStateSet->CaptureState(context);
                }

                #if MODEL_FORMAT == MODEL_FORMAT_RUNTIME
                    _model->Render(
                        ModelRenderer::Context(context, parserContext, techniqueIndex, *_sharedStateSet),
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
            static SceneEngine::LightDesc light;
            light._type = SceneEngine::LightDesc::Directional;
            light._lightColour = 5.f * Float3(5.f, 5.f, 5.f);
            light._negativeLightDirection = Normalize(Float3(-.1f, 0.33f, 1.f));
            light._radius = 10000.f;
            light._shadowFrustumIndex = ~unsigned(0x0);
            return light;
        }

        SceneEngine::GlobalLightingDesc GetGlobalLightingDesc() const
        {
            SceneEngine::GlobalLightingDesc result;
            result._ambientLight = 5.f * Float3(0.25f, 0.25f, 0.25f);
            result._skyTexture = nullptr;
            result._doAtmosphereBlur = false;
            result._doOcean = false;
            result._doToneMap = false;
            return result;
        }

        float GetTimeValue() const { return 0.f; }

        ModelSceneParser(
            ModelRenderer& model, const std::pair<Float3, Float3>& boundingBox, SharedStateSet& sharedStateSet)
            : _model(&model), _boundingBox(boundingBox), _sharedStateSet(&sharedStateSet) {}
        ~ModelSceneParser() {}

    protected:
        ModelRenderer * _model;
        SharedStateSet* _sharedStateSet;
        std::pair<Float3, Float3> _boundingBox;
    };

    std::unique_ptr<SceneEngine::ISceneParser> CreateModelScene(const ModelVisCache::Model& model)
    {
        return std::make_unique<ModelSceneParser>(
            *model._renderer, model._boundingBox, *model._sharedStateSet);
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
            // Trigger(_pimpl->_settings->_changeCallbacks);
        }

        ModelSceneParser sceneParser(*model._renderer, model._boundingBox, *model._sharedStateSet);
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

    ModelVisSettings::ModelVisSettings()
    {
        _modelName = "game/model/galleon/galleon.dae";
        _pendingCameraAlignToModel = true;
        _doHighlightWireframe = false;
        _highlightRay = std::make_pair(Zero<Float3>(), Zero<Float3>());
        _highlightRayWidth = 0.f;
        _colourByMaterial = false;
    }
    
}

