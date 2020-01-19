// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "VisLayerController.h"
#include "LayerControl.h"
#include "IWindowRig.h"
#include "GUILayerUtil.h"
#include "NativeEngineDevice.h"
#include "../ToolsRig/VisualisationUtils.h"
#include "../ToolsRig/ModelVisualisation.h"
#include "../ToolsRig/MaterialVisualisation.h"
#include "../ToolsRig/IManipulator.h"
#include "../ToolsRig/BasicManipulators.h"
#include "../ToolsRig/MaterialOverridesDelegate.h"
#include "../../PlatformRig/InputTranslator.h"
#include "../../PlatformRig/FrameRig.h"
#include "../../PlatformRig/OverlaySystem.h"
#include "../../RenderOverlays/DebuggingDisplay.h"
#include "../../RenderOverlays/Font.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Assets/MaterialScaffold.h"
#include "../../SceneEngine/SceneParser.h"
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

		std::shared_ptr<RenderCore::Techniques::CompiledShaderPatchCollection> _patchCollection;
		::Assets::FuturePtr<SceneEngine::IScene> _scene;

		ToolsRig::ModelVisSettings _modelSettings;
		ToolsRig::MaterialVisSettings _materialVisSettings;
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
			matName = clix::marshalString<clix::E_UTF8>(visContent->GetDrawCallDetails(mouseOver._drawCallIndex, mouseOver._materialGuid)._materialName);
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
		return gcnew VisMouseOver(_pimpl->_mouseOver, _pimpl->_scene ? ToolsRig::TryActualize(*_pimpl->_scene) : nullptr);
	}

	VisAnimationState^ VisLayerController::AnimationState::get()
	{
		return gcnew VisAnimationState(_pimpl->_animState);
	}

	void VisLayerController::SetModelSettings(ModelVisSettings^ settings)
	{
		auto pipelineAcceleratorPool = EngineDevice::GetInstance()->GetNative().GetMainPipelineAcceleratorPool();
		_pimpl->_modelSettings = *settings->GetUnderlying();
		_pimpl->_scene = ToolsRig::MakeScene(pipelineAcceleratorPool, _pimpl->_modelSettings);
		_pimpl->_modelLayer->Set(_pimpl->_scene);
		_pimpl->_visOverlay->Set(_pimpl->_scene);
		_pimpl->_trackingLayer->Set(_pimpl->_scene);
	}

	ModelVisSettings^ VisLayerController::GetModelSettings()
	{
		return gcnew ModelVisSettings(
			std::make_shared<ToolsRig::ModelVisSettings>(_pimpl->_modelSettings));
	}

	void VisLayerController::SetMaterialVisSettings(MaterialVisSettings^ settings)
	{
		auto pipelineAcceleratorPool = EngineDevice::GetInstance()->GetNative().GetMainPipelineAcceleratorPool();
		_pimpl->_materialVisSettings = *settings->ConvertToNative();
		_pimpl->_scene = ToolsRig::MakeScene(pipelineAcceleratorPool, _pimpl->_materialVisSettings, _pimpl->_patchCollection);
		_pimpl->_modelLayer->Set(_pimpl->_scene);
		_pimpl->_visOverlay->Set(_pimpl->_scene);
		_pimpl->_trackingLayer->Set(_pimpl->_scene);
	}

	MaterialVisSettings^ VisLayerController::GetMaterialVisSettings()
	{
		return MaterialVisSettings::ConvertFromNative(_pimpl->_materialVisSettings);
	}

	void VisLayerController::SetOverlaySettings(VisOverlaySettings^ settings)
	{
		_pimpl->_visOverlay->Set(*settings->ConvertToNative());
	}

	VisOverlaySettings^ VisLayerController::GetOverlaySettings()
	{
		return VisOverlaySettings::ConvertFromNative(_pimpl->_visOverlay->GetOverlaySettings());
	}

	void VisLayerController::ListChangeHandler(System::Object^ sender, ListChangedEventArgs^ args) { RebuildMaterialOverrides(); }
    void VisLayerController::PropChangeHandler(System::Object^ sender, PropertyChangedEventArgs^ args) { RebuildMaterialOverrides(); }

	void VisLayerController::RebuildMaterialOverrides()
	{
		/*::Assets::DirectorySearchRules searchRules;		// todo -- include model directory in search path
		auto nativeMaterial = ResolveNativeMaterial(_boundRawMaterials, searchRules);
		_pimpl->_modelLayer->SetOverrides(ToolsRig::MakeMaterialOverrideDelegate(nativeMaterial));*/
	}

	void VisLayerController::SetMaterialOverrides(
		System::Collections::Generic::IEnumerable<RawMaterial^>^ materialOverrides)
	{
		/*auto listChangeHandler = gcnew ListChangedEventHandler(this, &VisLayerController::ListChangeHandler);
		auto propChangeHandler = gcnew PropertyChangedEventHandler(this, &VisLayerController::PropChangeHandler);

		if (_boundRawMaterials != nullptr) {
			for each(auto mat in _boundRawMaterials) {
				mat->MaterialParameterBox->ListChanged -= listChangeHandler;
				mat->ShaderConstants->ListChanged -= listChangeHandler;
				mat->ResourceBindings->ListChanged -= listChangeHandler;
				mat->StateSet->PropertyChanged -= propChangeHandler;
			}
			delete _boundRawMaterials;
			_boundRawMaterials = nullptr;
		}

		if (materialOverrides) {
			_boundRawMaterials = gcnew System::Collections::Generic::List<RawMaterial^>();
			for each(auto mat in materialOverrides) {
				_boundRawMaterials->Add(mat);
				mat->MaterialParameterBox->ListChanged += listChangeHandler;
				mat->ShaderConstants->ListChanged += listChangeHandler;
				mat->ResourceBindings->ListChanged += listChangeHandler;
				mat->StateSet->PropertyChanged += propChangeHandler;
			}
			RebuildMaterialOverrides();
		} else {
			_pimpl->_modelLayer->SetOverrides(std::shared_ptr<RenderCore::Techniques::IMaterialDelegate>{});
		}*/
	}

	/*void VisLayerController::SetMaterialDelegate(MaterialDelegateWrapper^ materialDelegate)
	{
		SetMaterialOverrides(nullptr);
		if (materialDelegate) {
			_pimpl->_modelLayer->SetOverrides(materialDelegate->_materialDelegate.GetNativePtr());
		}
		else {
			_pimpl->_modelLayer->SetOverrides(std::shared_ptr<RenderCore::Techniques::IMaterialDelegate>{});
		}
	}*/

	void VisLayerController::SetPatchCollectionOverrides(CompiledShaderPatchCollectionWrapper^ patchCollection)
	{
		if (patchCollection) {
			_pimpl->_patchCollection = patchCollection->_patchCollection.GetNativePtr();
		} else {
			_pimpl->_patchCollection = nullptr;
		}

		// rebuild and reset the material vis scene ...
		auto pipelineAcceleratorPool = EngineDevice::GetInstance()->GetNative().GetMainPipelineAcceleratorPool();
		_pimpl->_scene = ToolsRig::MakeScene(pipelineAcceleratorPool, _pimpl->_materialVisSettings, _pimpl->_patchCollection);
		_pimpl->_modelLayer->Set(_pimpl->_scene);
		_pimpl->_visOverlay->Set(_pimpl->_scene);
		_pimpl->_trackingLayer->Set(_pimpl->_scene);
	}

	void VisLayerController::SetTechniqueOverrides(TechniqueDelegateWrapper^ techniqueDelegate)
	{
		if (techniqueDelegate) {
			_pimpl->_modelLayer->SetOverrides(techniqueDelegate->_techniqueDelegate.GetNativePtr());
		} else {
			_pimpl->_modelLayer->SetOverrides(std::shared_ptr<RenderCore::Techniques::ITechniqueDelegate>{});
		}
	}

	void VisLayerController::ResetCamera()
	{
		_pimpl->_modelLayer->ResetCamera();
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
		auto pipelineAcceleratorPool = EngineDevice::GetInstance()->GetNative().GetMainPipelineAcceleratorPool();

		_pimpl.reset(new VisLayerControllerPimpl());
		_pimpl->_mouseOver = std::make_shared<ToolsRig::VisMouseOver>();
		_pimpl->_animState = std::make_shared<ToolsRig::VisAnimationState>();

		_pimpl->_modelLayer = std::make_shared<ToolsRig::ModelVisLayer>(pipelineAcceleratorPool);
		_pimpl->_modelLayer->Set(ToolsRig::VisEnvSettings{});

		_pimpl->_visOverlay = std::make_shared<ToolsRig::VisualisationOverlay>(
			ToolsRig::VisOverlaySettings{},
            _pimpl->_mouseOver);
		_pimpl->_visOverlay->Set(_pimpl->_modelLayer->GetCamera());
		_pimpl->_visOverlay->Set(_pimpl->_animState);

		_boundRawMaterials = nullptr;
        
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
				techContext, pipelineAcceleratorPool,
				_pimpl->_modelLayer->GetCamera(), &RenderTrackingOverlay);
		}

		auto engineDevice = EngineDevice::GetInstance();
		engineDevice->AddOnShutdown(this);
	}

	VisLayerController::~VisLayerController()
	{
		SetMaterialOverrides(nullptr);		// unbind bound materials
		_pimpl.reset();		
	}

	VisLayerController::!VisLayerController()
	{
		if (_pimpl.get())
			System::Diagnostics::Debug::Assert(false, "Non deterministic delete of LayerControl");
	}
}

