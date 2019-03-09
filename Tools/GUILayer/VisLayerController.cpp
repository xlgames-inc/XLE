// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "VisLayerController.h"
#include "LayerControl.h"
#include "IWindowRig.h"
#include "../ToolsRig/VisualisationUtils.h"
#include "../ToolsRig/IManipulator.h"
#include "../ToolsRig/BasicManipulators.h"
#include "../../PlatformRig/InputTranslator.h"
#include "../../PlatformRig/FrameRig.h"
#include "../../PlatformRig/OverlaySystem.h"
#include "../../RenderOverlays/DebuggingDisplay.h"
#include "../../RenderOverlays/Font.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../SceneEngine/SceneParser.h"
// #include "../../Assets/AssetFuture.h"
#include "../../Utility/StringFormat.h"
#include <iomanip>

using namespace System;

namespace GUILayer 
{
	class VisLayerControllerPimpl 
    {
    public:
        std::shared_ptr<ToolsRig::VisualisationOverlay> _visOverlay;
		std::shared_ptr<ToolsRig::ModelVisLayer> _modelLayer;
		std::shared_ptr<PlatformRig::IOverlaySystem> _manipulatorLayer;
		std::shared_ptr<ToolsRig::MouseOverTrackingOverlay> _trackingLayer;
		std::shared_ptr<ToolsRig::VisMouseOver> _mouseOver;
		std::shared_ptr<ToolsRig::VisAnimationState> _animState;

		::Assets::FuturePtr<SceneEngine::IScene> _scene;

		ToolsRig::ModelVisSettings _modelSettings;
    };

	static void RenderTrackingOverlay(
        RenderOverlays::IOverlayContext& context,
		const RenderOverlays::DebuggingDisplay::Rect& viewport,
		const ToolsRig::VisMouseOver& mouseOver, 
		const SceneEngine::IScene& scene)
    {
        using namespace RenderOverlays::DebuggingDisplay;

        auto textHeight = (int)RenderOverlays::GetDefaultFont()->GetFontProperties()._lineHeight;
        String^ matName = "matName";
		auto* visContent = dynamic_cast<const ToolsRig::IVisContent*>(&scene);
		if (visContent)
			matName = clix::marshalString<clix::E_UTF8>(visContent->GetDrawCallDetails(mouseOver._drawCallIndex)._materialName);
        DrawText(
            &context,
            Rect(Coord2(viewport._topLeft[0]+3, viewport._bottomRight[1]-textHeight-3), Coord2(viewport._bottomRight[0]-3, viewport._bottomRight[1]-3)),
            nullptr, RenderOverlays::ColorB(0xffafafaf),
            StringMeld<512>() 
                << "Material: {Color:7f3faf}" << clix::marshalString<clix::E_UTF8>(matName)
                << "{Color:afafaf}, Draw call: " << mouseOver._drawCallIndex
                << std::setprecision(4)
                << ", (" << mouseOver._intersectionPt[0]
                << ", "  << mouseOver._intersectionPt[1]
                << ", "  << mouseOver._intersectionPt[2]
                << ")");
    }

	VisMouseOver^ VisLayerController::MouseOver::get()
	{
		return gcnew VisMouseOver(_pimpl->_mouseOver, nullptr); // _pimpl->_scene ? _pimpl->_scene->TryActualize() : nullptr);
	}

	VisAnimationState^ VisLayerController::AnimationState::get()
	{
		return gcnew VisAnimationState(_pimpl->_animState);
	}

	void VisLayerController::SetModelSettings(ModelVisSettings^ settings)
	{
		_pimpl->_modelSettings = *settings->GetUnderlying();
		_pimpl->_scene = ToolsRig::MakeScene(_pimpl->_modelSettings);
		_pimpl->_modelLayer->Set(_pimpl->_scene);
		_pimpl->_visOverlay->Set(_pimpl->_scene);
		_pimpl->_trackingLayer->Set(_pimpl->_scene);
	}

	ModelVisSettings^ VisLayerController::GetModelSettings()
	{
		return gcnew ModelVisSettings(
			std::make_shared<ToolsRig::ModelVisSettings>(_pimpl->_modelSettings));
	}

	void VisLayerController::SetOverlaySettings(VisOverlaySettings^ settings)
	{
		_pimpl->_visOverlay->Set(*settings->ConvertToNative());
	}

	VisOverlaySettings^ VisLayerController::GetOverlaySettings()
	{
		return VisOverlaySettings::ConvertFromNative(_pimpl->_visOverlay->GetOverlaySettings());
	}

	void VisLayerController::AttachToView(LayerControl^ view)
	{
		auto& overlaySet = *view->GetWindowRig().GetFrameRig().GetMainOverlaySystem();
        overlaySet.AddSystem(_pimpl->_modelLayer);
		overlaySet.AddSystem(_pimpl->_visOverlay);
		overlaySet.AddSystem(_pimpl->_manipulatorLayer);
		overlaySet.AddSystem(_pimpl->_trackingLayer);
	}

	void VisLayerController::DetachFromView(LayerControl^ view)
	{
		auto& overlaySet = *view->GetWindowRig().GetFrameRig().GetMainOverlaySystem();
		overlaySet.RemoveSystem(*_pimpl->_trackingLayer);
		overlaySet.RemoveSystem(*_pimpl->_manipulatorLayer);
		overlaySet.RemoveSystem(*_pimpl->_visOverlay);
		overlaySet.RemoveSystem(*_pimpl->_modelLayer);
	}

	void VisLayerController::OnEngineShutdown()
	{
		_pimpl.reset();
	}

	VisLayerController::VisLayerController()
	{
		_pimpl.reset(new VisLayerControllerPimpl());
		_pimpl->_mouseOver = std::make_shared<ToolsRig::VisMouseOver>();
		_pimpl->_animState = std::make_shared<ToolsRig::VisAnimationState>();

		_pimpl->_modelLayer = std::make_shared<ToolsRig::ModelVisLayer>();
		_pimpl->_modelLayer->Set(ToolsRig::VisEnvSettings{});

		_pimpl->_visOverlay = std::make_shared<ToolsRig::VisualisationOverlay>(
			ToolsRig::VisOverlaySettings{},
            _pimpl->_mouseOver);
		_pimpl->_visOverlay->Set(_pimpl->_modelLayer->GetCamera());
		_pimpl->_visOverlay->Set(_pimpl->_animState);
        
		auto techContext = std::make_shared<RenderCore::Techniques::TechniqueContext>();
		{
			/*
			auto immContext = EngineDevice::GetInstance()->GetNative().GetRenderDevice()->GetImmediateContext();

			auto intersectionScene = ToolsRig::CreateModelIntersectionScene(
				MakeStringSection(clix::marshalString<clix::E_UTF8>(settings->ModelName)),
				MakeStringSection(clix::marshalString<clix::E_UTF8>(settings->MaterialName)));
			auto intersectionContext = std::make_shared<SceneEngine::IntersectionTestContext>(
				immContext,
				RenderCore::Techniques::CameraDesc(), 
				GetWindowRig().GetPresentationChain()->GetDesc(),
				_pimpl->_globalTechniqueContext);
				*/

			auto manipulators = std::make_shared<ToolsRig::ManipulatorStack>(_pimpl->_modelLayer->GetCamera(), techContext);
			manipulators->Register(
				ToolsRig::ManipulatorStack::CameraManipulator,
				ToolsRig::CreateCameraManipulator(
					_pimpl->_modelLayer->GetCamera(), 
					ToolsRig::CameraManipulatorMode::Blender_RightButton));
			_pimpl->_manipulatorLayer = ToolsRig::MakeLayerForInput(manipulators);
		}

		{
			_pimpl->_trackingLayer = std::make_shared<ToolsRig::MouseOverTrackingOverlay>(
				_pimpl->_mouseOver,
				techContext,
				_pimpl->_modelLayer->GetCamera(), &RenderTrackingOverlay);
		}

		auto engineDevice = EngineDevice::GetInstance();
		engineDevice->AddOnShutdown(this);
	}

	VisLayerController::~VisLayerController()
	{
		_pimpl.reset();
	}

	VisLayerController::!VisLayerController()
	{
		if (_pimpl.get())
			System::Diagnostics::Debug::Assert(false, "Non deterministic delete of LayerControl");
	}
}

