// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderCore/Assets/SimpleModelRenderer.h"
#include "../../PlatformRig/OverlaySystem.h"
#include "../../Assets/AssetsCore.h"
#include "../../Math/Vector.h"
#include <string>

namespace RenderCore { namespace Techniques { class CameraDesc; class TechniqueContext; } }
namespace SceneEngine { class LightDesc; class GlobalLightingDesc; }
namespace RenderOverlays { class IOverlayContext; }
namespace RenderOverlays { namespace DebuggingDisplay { struct Rect; }}
namespace Utility { class OnChangeCallback; }
namespace SceneEngine { class IScene; }

namespace ToolsRig
{
	class ChangeEvent
    {
    public:
        std::vector<std::shared_ptr<Utility::OnChangeCallback>> _callbacks;
        void Invoke();
        ~ChangeEvent();
    };

    class VisCameraSettings
    {
    public:
        Float3      _position;
        Float3      _focus;
        float       _nearClip, _farClip;

        enum class Projection { Perspective, Orthogonal };
        Projection  _projection;

        // perspective settings
        float       _verticalFieldOfView;

        // orthogonal settings
        float       _left, _top;
        float       _right, _bottom;

        VisCameraSettings();
    };

    VisCameraSettings AlignCameraToBoundingBox(
        float verticalFieldOfView, 
        const std::pair<Float3, Float3>& boxIn);

	RenderCore::Techniques::CameraDesc AsCameraDesc(const VisCameraSettings& camSettings);

	class VisEnvSettings
    {
	public:
		std::string _envConfigFile;

		VisEnvSettings();
		VisEnvSettings(const std::string& envConfigFile);
		~VisEnvSettings();
	};

	class VisOverlaySettings
	{
	public:
        unsigned _colourByMaterial = 0;
		unsigned _skeletonMode = 0;
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

	class VisAnimationState
	{
	public:
		struct AnimationDetails
		{
			std::string _name;
			float _beginTime, _endTime;
		};
		std::vector<AnimationDetails> _animationList;
		std::string _activeAnimation;
		float _animationTime = 0.f;
		unsigned _anchorTime = 0;
		enum class State { Stopped, Playing, BindPose };
		State _state = State::Stopped;

		ChangeEvent _changeEvent;
	};

	class IVisContent
	{
	public:
		virtual std::pair<Float3, Float3> GetBoundingBox() const = 0;

		virtual std::shared_ptr<RenderCore::Assets::SimpleModelRenderer::IPreDrawDelegate> SetPreDrawDelegate(const std::shared_ptr<RenderCore::Assets::SimpleModelRenderer::IPreDrawDelegate>&) = 0;
		virtual void RenderSkeleton(
			RenderCore::IThreadContext& context, 
			RenderCore::Techniques::ParsingContext& parserContext, 
			bool drawBoneNames) const = 0;

		struct DrawCallDetails { std::string _modelName, _materialName; };
		virtual DrawCallDetails GetDrawCallDetails(unsigned drawCallIndex) const = 0;

		virtual void BindAnimationState(const std::shared_ptr<VisAnimationState>& animState) = 0;

		virtual ~IVisContent();
	};

///////////////////////////////////////////////////////////////////////////////////////////////////

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
		void Set(const VisOverlaySettings& overlaySettings);
		void Set(const std::shared_ptr<VisAnimationState>&);

		const VisOverlaySettings& GetOverlaySettings() const;

        VisualisationOverlay(
			const VisOverlaySettings& overlaySettings,
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

	std::shared_ptr<PlatformRig::IOverlaySystem> MakeLayerForInput(
		const std::shared_ptr<PlatformRig::IOverlaySystem::IInputListener>& listener);

///////////////////////////////////////////////////////////////////////////////////////////////////

    inline VisCameraSettings::VisCameraSettings()
    {
        _position = Float3(-10.f, 0.f, 0.f);
        _focus = Zero<Float3>();
        _nearClip = 0.1f;
        _farClip = 1000.f;
        _projection = Projection::Perspective;
        _verticalFieldOfView = 40.f;
        _left = _top = -1.f;
        _right = _bottom = 1.f;
    }
}

