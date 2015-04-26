// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../PlatformRig/OverlaySystem.h"
#include "../../Assets/Assets.h"

namespace RenderCore { namespace Assets 
{
    class IModelFormat; 
    class ModelRenderer;
    class SharedStateSet;
    class ModelScaffold;
    class MaterialScaffold;
}}

namespace RenderCore { namespace Techniques 
{
    class TechniqueContext;
    class CameraDesc;
}}

namespace SceneEngine { class ISceneParser; }

namespace ToolsRig
{
    class VisCameraSettings;

    class ModelVisCache
    {
    public:
        class Model
        {
        public:
            RenderCore::Assets::ModelRenderer* _renderer;
            RenderCore::Assets::SharedStateSet* _sharedStateSet;
            std::pair<Float3, Float3> _boundingBox;
            uint64 _hashedModelName;
            uint64 _hashedMaterialName;

            Model() : _renderer(nullptr), _sharedStateSet(nullptr), _hashedModelName(0), _hashedMaterialName(0) {}
        };

        class Scaffolds
        {
        public:
            std::shared_ptr<RenderCore::Assets::ModelScaffold> _model;
            std::shared_ptr<RenderCore::Assets::MaterialScaffold> _material;
            uint64 _hashedModelName;
            uint64 _hashedMaterialName;
        };

        Model GetModel(const Assets::ResChar modelFilename[], const Assets::ResChar materialFilename[]);
        Scaffolds GetScaffolds(const Assets::ResChar modelFilename[], const Assets::ResChar materialFilename[]);
        std::string HashToModelName(uint64 hash);

        ModelVisCache(std::shared_ptr<RenderCore::Assets::IModelFormat> format);
        ~ModelVisCache();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

    class ChangeEvent
    {
    public:
        std::vector<std::shared_ptr<OnChangeCallback>> _callbacks;
        void Trigger();
        ~ChangeEvent();
    };

    /// <summary>Settings related to the visualisation of a model</summary>
    /// This is a "model" part of a MVC pattern related to the way a model
    /// is presented in a viewport. Typically some other controls might 
    /// write to this when something changes (for example, if a different
    /// model is selected to be viewed).
    /// The settings could come from anywhere though -- it's purposefully
    /// kept free of dependencies so that it can be driven by different sources.
    /// We have a limited set of different rendering options for special
    /// visualisation modes, etc.
    class ModelVisSettings
    {
    public:
        std::string _modelName;
        std::string _materialName;
        std::shared_ptr<VisCameraSettings> _camera;
        bool _pendingCameraAlignToModel;

        bool _doHighlightWireframe;
        std::pair<Float3, Float3> _highlightRay;
        float _highlightRayWidth;

        unsigned _colourByMaterial;

        ChangeEvent _changeEvent;

        ModelVisSettings();
    };

    class VisMouseOver
    {
    public:
        bool _hasMouseOver;
        Float3 _intersectionPt;
        unsigned _drawCallIndex;
        uint64 _materialGuid;

        ChangeEvent _changeEvent;

        VisMouseOver() 
            : _hasMouseOver(false), _intersectionPt(Zero<Float3>())
            , _drawCallIndex(0), _materialGuid(0)
            {}
    };

    class ModelVisLayer : public PlatformRig::IOverlaySystem
    {
    public:
        virtual std::shared_ptr<IInputListener> GetInputListener();

        virtual void RenderToScene(
            RenderCore::IThreadContext* context, 
            SceneEngine::LightingParserContext& parserContext); 
        virtual void RenderWidgets(
            RenderCore::IThreadContext* context, 
            const RenderCore::Techniques::ProjectionDesc& projectionDesc);
        virtual void SetActivationState(bool newState);

        ModelVisLayer(
            std::shared_ptr<ModelVisSettings> settings,
            std::shared_ptr<ModelVisCache> cache);
        ~ModelVisLayer();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

    class VisualisationOverlay : public PlatformRig::IOverlaySystem
    {
    public:
        virtual std::shared_ptr<IInputListener> GetInputListener();

        virtual void RenderToScene(
            RenderCore::IThreadContext* context, 
            SceneEngine::LightingParserContext& parserContext); 
        virtual void RenderWidgets(
            RenderCore::IThreadContext* context, 
            const RenderCore::Techniques::ProjectionDesc& projectionDesc);
        virtual void SetActivationState(bool newState);

        VisualisationOverlay(
            std::shared_ptr<ModelVisSettings> settings,
            std::shared_ptr<ModelVisCache> cache,
            std::shared_ptr<VisMouseOver> mouseOver);
        ~VisualisationOverlay();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

    class MouseOverTrackingOverlay : public PlatformRig::IOverlaySystem
    {
    public:
        virtual std::shared_ptr<IInputListener> GetInputListener();

        virtual void RenderToScene(
            RenderCore::IThreadContext* context, 
            SceneEngine::LightingParserContext& parserContext); 
        virtual void RenderWidgets(
            RenderCore::IThreadContext* context, 
            const RenderCore::Techniques::ProjectionDesc& projectionDesc);
        virtual void SetActivationState(bool newState);

        MouseOverTrackingOverlay(
            std::shared_ptr<VisMouseOver> mouseOver,
            std::shared_ptr<RenderCore::IThreadContext> threadContext,
            std::shared_ptr<RenderCore::Techniques::TechniqueContext> techniqueContext,
            std::shared_ptr<ModelVisSettings> settings,
            std::shared_ptr<ModelVisCache> cache);
        ~MouseOverTrackingOverlay();
    protected:
        std::shared_ptr<IInputListener> _inputListener;
    };

    std::unique_ptr<SceneEngine::ISceneParser> CreateModelScene(const ModelVisCache::Model& model);
}

