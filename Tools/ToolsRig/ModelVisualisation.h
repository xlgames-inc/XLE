// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderCore/Assets/SimpleModelRenderer.h"
#include "../../PlatformRig/OverlaySystem.h"
#include "../../Assets/AssetsCore.h"
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

namespace RenderOverlays { namespace DebuggingDisplay { struct Rect; }}

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
        void Invoke();
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

		ModelVisSettings();
	};

	class VisOverlaySettings
	{
	public:
        bool _doHighlightWireframe = false;
        unsigned _colourByMaterial = false;
        bool _drawNormals = false;
        bool _drawWireframe = false;
    };

    class VisMouseOver
    {
    public:
        bool _hasMouseOver = false;
        Float3 _intersectionPt = Zero<Float3>();
        unsigned _drawCallIndex = 0u;
        uint64 _materialGuid = 0;

        ChangeEvent _changeEvent;
    };

	class IVisContent
	{
	public:
		virtual std::string GetModelName() const = 0;
		virtual std::string GetMaterialName() const = 0;
		virtual std::pair<Float3, Float3> GetBoundingBox() const = 0;
		virtual std::shared_ptr<RenderCore::Assets::SimpleModelRenderer::IPreDrawDelegate> SetPreDrawDelegate(const std::shared_ptr<RenderCore::Assets::SimpleModelRenderer::IPreDrawDelegate>&) = 0;
		virtual ~IVisContent();
	};

	::Assets::FuturePtr<SceneEngine::IScene> MakeScene(const ModelVisSettings& settings);

    class ModelVisLayer : public PlatformRig::IOverlaySystem
    {
    public:
        virtual void Render(
            RenderCore::IThreadContext& context,
			const RenderCore::IResourcePtr& renderTarget,
            RenderCore::Techniques::ParsingContext& parserContext) override;

        void Set(const VisEnvSettings& envSettings);
		void Set(const ::Assets::FuturePtr<SceneEngine::IScene>& scene);

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

		void Set(const ::Assets::FuturePtr<SceneEngine::IScene>& scene);
		void Set(const std::shared_ptr<VisCameraSettings>&);

        VisualisationOverlay(
			std::shared_ptr<VisOverlaySettings> overlaySettings,
            std::shared_ptr<VisMouseOver> mouseOver);
        ~VisualisationOverlay();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

	class MouseOverTrackingListener;

    class MouseOverTrackingOverlay : public PlatformRig::IOverlaySystem
    {
    public:
        virtual std::shared_ptr<IInputListener> GetInputListener();

        virtual void Render(
            RenderCore::IThreadContext& context, 
			const RenderCore::IResourcePtr& renderTarget,
            RenderCore::Techniques::ParsingContext& parserContext);

		void Set(const ::Assets::FuturePtr<SceneEngine::IScene>& scene);

        using OverlayFn = std::function<
			void(
				RenderOverlays::IOverlayContext&,
				const RenderOverlays::DebuggingDisplay::Rect&,
				const ToolsRig::VisMouseOver&, 
				const SceneEngine::IScene& scene)>;

        MouseOverTrackingOverlay(
            const std::shared_ptr<VisMouseOver>& mouseOver,
            const std::shared_ptr<RenderCore::Techniques::TechniqueContext>& techniqueContext,
            const std::shared_ptr<VisCameraSettings>& camera,
            OverlayFn&& overlayFn);
        ~MouseOverTrackingOverlay();
    protected:
        std::shared_ptr<MouseOverTrackingListener> _inputListener;
        std::shared_ptr<VisCameraSettings> _camera;
        std::shared_ptr<VisMouseOver> _mouseOver;
        OverlayFn _overlayFn;
    };

    /*std::unique_ptr<SceneEngine::IScene> CreateModelScene(const FixedFunctionModel::ModelCacheModel& model);
    std::shared_ptr<SceneEngine::IntersectionTestScene> CreateModelIntersectionScene(
        StringSection<> modelName, StringSection<> materialName);*/
}

