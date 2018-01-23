// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../PlatformRig/OverlaySystem.h"
#include <functional>
#include <memory>

namespace RenderCore { namespace Assets 
{
    class ModelRenderer;
    class SharedStateSet;
    class ModelScaffold;
    class MaterialScaffold;
	class ModelCache;
	class ModelCacheModel;
}}

namespace RenderCore { namespace Techniques 
{
    class TechniqueContext;
    class CameraDesc;
    class ParsingContext;
}}

namespace SceneEngine { class ISceneParser; class IntersectionTestScene; }
namespace RenderOverlays { class IOverlayContext; }
namespace Utility { class OnChangeCallback; }

namespace ToolsRig
{
    class VisCameraSettings;
    class VisEnvSettings;

    class ChangeEvent
    {
    public:
        std::vector<std::shared_ptr<Utility::OnChangeCallback>> _callbacks;
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
        std::string _supplements;
        unsigned _levelOfDetail;
        std::string _envSettingsFile;
        std::shared_ptr<VisCameraSettings> _camera;
        bool _pendingCameraAlignToModel;

        bool _doHighlightWireframe;
        std::pair<Float3, Float3> _highlightRay;
        float _highlightRayWidth;

        unsigned _colourByMaterial;
        bool _drawNormals;
        bool _drawWireframe;

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
            RenderCore::IThreadContext& context, 
            SceneEngine::LightingParserContext& parserContext); 
        virtual void RenderWidgets(
            RenderCore::IThreadContext& context, 
            RenderCore::Techniques::ParsingContext& parsingContext);
        virtual void SetActivationState(bool newState);

        void SetEnvironment(std::shared_ptr<VisEnvSettings> envSettings);

        ModelVisLayer(
            std::shared_ptr<ModelVisSettings> settings,
            std::shared_ptr<RenderCore::Assets::ModelCache> cache);
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
            RenderCore::IThreadContext& context, 
            SceneEngine::LightingParserContext& parserContext); 
        virtual void RenderWidgets(
            RenderCore::IThreadContext& context, 
            RenderCore::Techniques::ParsingContext& parserContext);
        virtual void SetActivationState(bool newState);

        VisualisationOverlay(
            std::shared_ptr<ModelVisSettings> settings,
            std::shared_ptr<RenderCore::Assets::ModelCache> cache,
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
            RenderCore::IThreadContext& context, 
            SceneEngine::LightingParserContext& parserContext); 
        virtual void RenderWidgets(
            RenderCore::IThreadContext& context, 
            RenderCore::Techniques::ParsingContext& parserContext);
        virtual void SetActivationState(bool newState);

        using OverlayFn = std::function<void(RenderOverlays::IOverlayContext&, const ToolsRig::VisMouseOver&)>;

        MouseOverTrackingOverlay(
            std::shared_ptr<VisMouseOver> mouseOver,
            std::shared_ptr<RenderCore::IThreadContext> threadContext,
            std::shared_ptr<RenderCore::Techniques::TechniqueContext> techniqueContext,
            std::shared_ptr<VisCameraSettings> camera,
            std::shared_ptr<SceneEngine::IntersectionTestScene> scene,
            OverlayFn&& overlayFn);
        ~MouseOverTrackingOverlay();
    protected:
        std::shared_ptr<IInputListener> _inputListener;
        std::shared_ptr<VisCameraSettings> _camera;
        std::shared_ptr<VisMouseOver> _mouseOver;
        OverlayFn _overlayFn;
    };

    std::unique_ptr<SceneEngine::ISceneParser> CreateModelScene(const RenderCore::Assets::ModelCacheModel& model);
    std::shared_ptr<SceneEngine::IntersectionTestScene> CreateModelIntersectionScene(
        std::shared_ptr<ModelVisSettings> settings, std::shared_ptr<RenderCore::Assets::ModelCache> cache);
}

