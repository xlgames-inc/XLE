// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4564)

#include "NativeManipulators.h"
#include "ManipulatorOverlay.h"
#include "XLELayerUtils.h"
#include "../../../Tools/ToolsRig/IManipulator.h"
#include "../../../Tools/GUILayer/NativeEngineDevice.h"
#include "../../../RenderOverlays/DebuggingDisplay.h"
#include "../../../RenderCore/IDevice.h"

namespace XLEBridgeUtils
{

    bool NativeManipulatorLayer::MouseMove(IViewContext^ vc, Point scrPt)
    {
        using namespace RenderOverlays::DebuggingDisplay;
		InputSnapshot evnt(0, 0, 0, Coord2(scrPt.X, scrPt.Y), Coord2(0, 0));
			// "return true" has two effects --
			//		1. sets the cursor to a moving cursor
			//		2. turns mouse down into "drag-begin" event
		return SendInputEvent(vc, evnt);
    }

    void NativeManipulatorLayer::Render()
    {
			//	We can't get any context information from here!
			//	EditorSceneManager must be a singleton, otherwise
			//	there's no way to get it. Ideally the ViewControl
			//	could tell us something, but there's no way to attach
			//	more context information on the render call
        if (!ManipulatorOverlay::s_currentParsingContext) return;
        auto underlying = _manipContext->GetNativeManipulator();
        if (!underlying) return;

        underlying->Render(
            *GUILayer::EngineDevice::GetInstance()->GetNativeImmediateContext(),
            *ManipulatorOverlay::s_currentParsingContext);
    }

    void NativeManipulatorLayer::OnBeginDrag() { _pendingBeginDrag = true; }
    void NativeManipulatorLayer::OnDragging(IViewContext^ vc, Point scrPt) 
	{
			//  We need to create a fake "mouse over" event and pass it through to
			//  the currently selected manipulator. We might also need to set the state
			//  for buttons and keys pressed down.
            //  For the first "OnDragging" operation after a "OnBeginDrag", we should
            //  emulate a mouse down event.
		using namespace RenderOverlays::DebuggingDisplay;
        auto btnState = 1<<0;
		InputSnapshot evnt(
			btnState, _pendingBeginDrag ? btnState : 0, 0,
			Coord2(scrPt.X, scrPt.Y), Coord2(0, 0));
        GUILayer::EditorInterfaceUtils::SetupModifierKeys(evnt);

		SendInputEvent(vc, evnt);
        _pendingBeginDrag = false;
	}

    void NativeManipulatorLayer::OnEndDrag(IViewContext^ vc, Point scrPt) 
	{
            // Emulate a "mouse up" operation 
        using namespace RenderOverlays::DebuggingDisplay;
        auto btnState = 1<<0;
		InputSnapshot evnt(
			0, btnState, 0,
			Coord2(scrPt.X, scrPt.Y), Coord2(0, 0));
        GUILayer::EditorInterfaceUtils::SetupModifierKeys(evnt);
        SendInputEvent(vc, evnt);
	}

    void NativeManipulatorLayer::OnMouseWheel(IViewContext^ vc, Point scrPt, int delta)
    {
        using namespace RenderOverlays::DebuggingDisplay;
		InputSnapshot evnt(
			0, 0, delta,
			Coord2(scrPt.X, scrPt.Y), Coord2(0, 0));
        GUILayer::EditorInterfaceUtils::SetupModifierKeys(evnt);
        SendInputEvent(vc, evnt);
    }

    NativeManipulatorLayer::NativeManipulatorLayer(ActiveManipulatorContext^ manipContext) 
    : _manipContext(manipContext)
    {
        _pendingBeginDrag = false;
    }

    NativeManipulatorLayer::~NativeManipulatorLayer()
    {
    }

    bool NativeManipulatorLayer::SendInputEvent(
        IViewContext^ vc, 
		const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt)
	{
		auto underlying = _manipContext->GetNativeManipulator();
        if (!underlying) return false;

		auto hitTestContext = GUILayer::EditorInterfaceUtils::CreateIntersectionTestContext(
			vc->EngineDevice, nullptr,
			Utils::AsCameraDesc(vc->Camera),
            vc->ViewportSize.Width, vc->ViewportSize.Height);
		auto hitTestScene = vc->SceneManager->GetIntersectionScene();

		underlying->OnInputEvent(evnt, hitTestContext->GetNative(), hitTestScene->GetNative());
		delete hitTestContext;
		delete hitTestScene;
		return true;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

    GUILayer::IManipulatorSet^ ActiveManipulatorContext::ManipulatorSet::get() { return _manipulatorSet; }
    void ActiveManipulatorContext::ManipulatorSet::set(GUILayer::IManipulatorSet^ value)
    {
        if (value != _manipulatorSet) {
            delete _manipulatorSet;
            _manipulatorSet = value;
            OnManipulatorSetChange(this, nullptr);
        }
    }

    String^ ActiveManipulatorContext::ActiveManipulator::get() { return _activeManipulator; }
    void ActiveManipulatorContext::ActiveManipulator::set(String^ value)
    {
        if (value != _activeManipulator) {
            auto oldNativeManip = GetNativeManipulator();
            _activeManipulator = value;
            auto newNativeManip = GetNativeManipulator();

            if (oldNativeManip != newNativeManip) {
                if (oldNativeManip) oldNativeManip->SetActivationState(false);
                if (newNativeManip) newNativeManip->SetActivationState(true);
            }
            OnActiveManipulatorChange(this, nullptr);
        }
    }

    ActiveManipulatorContext::ActiveManipulatorContext()
    {
        _manipulatorSet = nullptr;
        _activeManipulator = "";
    }

    ActiveManipulatorContext::~ActiveManipulatorContext()
    {
        delete _manipulatorSet;
    }

    ToolsRig::IManipulator* ActiveManipulatorContext::GetNativeManipulator()
    {
        auto set = ManipulatorSet;
        auto active = ActiveManipulator;
		if (!set || !active) return nullptr;
		return set->GetManipulator(active).get();
    }

}

