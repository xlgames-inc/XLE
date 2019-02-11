// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../PlatformRig/OverlaySystem.h"
#include "../../Utility/StringUtils.h"
#include <functional>
#include <memory>

namespace FixedFunctionModel
{
    class ModelRenderer;
    class SharedStateSet;
	class ModelCache;
	class ModelCacheModel;
}

namespace RenderCore { namespace Assets
{
	class ModelScaffold;
    class MaterialScaffold;
}}

namespace RenderCore { namespace Techniques 
{
    class TechniqueContext;
    class CameraDesc;
    class ParsingContext;
}}

namespace SceneEngine { class IntersectionTestScene; class IScene; }
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
		std::string _animationFileName;
		std::string _skeletonFileName;
        // std::string _envSettingsFile;
        // bool _pendingCameraAlignToModel;

		ModelVisSettings();
	};

	class VisOverlaySettings
	{
	public:
        bool _doHighlightWireframe = false;
        std::pair<Float3, Float3> _highlightRay = std::make_pair(Zero<Float3>(), Zero<Float3>());
        float _highlightRayWidth = 1.f;

        unsigned _colourByMaterial = false;
        bool _drawNormals = false;
        bool _drawWireframe = false;
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
        virtual void Render(
            RenderCore::IThreadContext& context,
			const RenderCore::IResourcePtr& renderTarget,
            RenderCore::Techniques::ParsingContext& parserContext) override;

        void Set(const VisEnvSettings& envSettings);
		void Set(const ModelVisSettings& settings);

		const std::shared_ptr<VisCameraSettings>& GetCamera();

        ModelVisLayer();
        ~ModelVisLayer();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

    class VisualisationOverlay : public PlatformRig::IOverlaySystem
    {
    public:
        virtual std::shared_ptr<IInputListener> GetInputListener();

        virtual void Render(
            RenderCore::IThreadContext& context,
			const RenderCore::IResourcePtr& renderTarget,
            RenderCore::Techniques::ParsingContext& parserContext);
        virtual void SetActivationState(bool newState);

        VisualisationOverlay(
            std::shared_ptr<ModelVisSettings> settings,
			std::shared_ptr<VisOverlaySettings> overlaySettings,
			std::shared_ptr<FixedFunctionModel::ModelCache> cache,
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

        virtual void Render(
            RenderCore::IThreadContext& context, 
			const RenderCore::IResourcePtr& renderTarget,
            RenderCore::Techniques::ParsingContext& parserContext);

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

    // std::unique_ptr<SceneEngine::IScene> CreateModelScene(const FixedFunctionModel::ModelCacheModel& model);
    std::shared_ptr<SceneEngine::IntersectionTestScene> CreateModelIntersectionScene(
        StringSection<> modelName, StringSection<> materialName);
}

