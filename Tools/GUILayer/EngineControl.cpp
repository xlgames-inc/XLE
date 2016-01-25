// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4793) //  : function compiled as native :

#include "EngineControl.h"
#include "EngineDevice.h"
#include "NativeEngineDevice.h"
#include "IWindowRig.h"
#include "DelayedDeleteQueue.h"
#include "ExportedNativeTypes.h"
#include "../../PlatformRig/FrameRig.h"
#include "../../PlatformRig/OverlappedWindow.h"
#include "../../PlatformRig/OverlaySystem.h"
#include "../../PlatformRig/InputTranslator.h"
#include "../../RenderCore/IDevice.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../Utility/PtrUtils.h"

namespace GUILayer 
{
///////////////////////////////////////////////////////////////////////////////////////////////////

    void EngineControl::OnPaint(Control^ ctrl, PaintEventArgs^ pe)
    {
        // Note -- we're suppressing base class paint events to
        // try to avoid flicker. See:
        //    https://msdn.microsoft.com/en-us/library/1e430ef4(v=vs.85).aspx
        // __super::OnPaint(pe);

        if (!Render())
            ctrl->Invalidate();
    }

    bool EngineControl::Render()
    {
        auto engineDevice = EngineDevice::GetInstance();
        auto* renderDevice = engineDevice->GetNative().GetRenderDevice().get();
        auto immediateContext = renderDevice->GetImmediateContext();
        bool result = Render(*immediateContext.get(), *_pimpl->_windowRig.get());

            // perform our delayed deletes now (in the main thread)
        DelayedDeleteQueue::FlushQueue();
        return result;
    }

    void EngineControl::Evnt_Resize(Object^ sender, System::EventArgs^ e)
    {
        auto ctrl = dynamic_cast<Control^>(sender);
        if (!ctrl) return;
        _pimpl->_windowRig->OnResize(ctrl->Size.Width, ctrl->Size.Height);
    }

    void EngineControl::Evnt_KeyDown(Object^ sender, KeyEventArgs^ e)
    {
        if (_pimpl->_inputTranslator) { 
            auto ctrl = dynamic_cast<Control^>(sender);
            if (!ctrl) return;

            _pimpl->_inputTranslator->OnKeyChange(e->KeyValue, true); 
            e->Handled = true;
            ctrl->Invalidate();
        }
    }

    void EngineControl::Evnt_KeyUp(Object^ sender, KeyEventArgs^ e)
    {
        if (_pimpl->_inputTranslator) { 
            auto ctrl = dynamic_cast<Control^>(sender);
            if (!ctrl) return;

            _pimpl->_inputTranslator->OnKeyChange(e->KeyValue, false); 
            e->Handled = true;
            ctrl->Invalidate();
        }
    }

    void EngineControl::Evnt_KeyPress(Object^ sender, KeyPressEventArgs^ e)
    {
        if (_pimpl->_inputTranslator) {
            auto ctrl = dynamic_cast<Control^>(sender);
            if (!ctrl) return;
            
            _pimpl->_inputTranslator->OnChar(e->KeyChar);
            e->Handled = true;
            ctrl->Invalidate();
        }
    }

    void EngineControl::Evnt_MouseMove(Object^ sender, MouseEventArgs^ e)
    {
            // (todo -- only when activated?)
        if (_pimpl->_inputTranslator) {
            auto ctrl = dynamic_cast<Control^>(sender);
            if (!ctrl) return;
            
            PlatformRig::InputTranslator::s_hackWindowSize = UInt2(unsigned(ctrl->Size.Width), unsigned(ctrl->Size.Height));
            _pimpl->_inputTranslator->OnMouseMove(e->Location.X, e->Location.Y);
            ctrl->Invalidate();
        }
    }

    static unsigned AsIndex(MouseButtons button)
    {
        switch (button) {
        case MouseButtons::Left: return 0;
        case MouseButtons::Right: return 1;
        case MouseButtons::Middle: return 2;
        default: return 3;
        }
    }
    
    void EngineControl::Evnt_MouseDown(Object^ sender, MouseEventArgs^ e)
    {
        if (_pimpl->_inputTranslator) {
            auto ctrl = dynamic_cast<Control^>(sender);
            if (!ctrl) return;

            _pimpl->_inputTranslator->OnMouseButtonChange(AsIndex(e->Button), true);
            ctrl->Invalidate();
        }
    }

    void EngineControl::Evnt_MouseUp(Object^ sender, MouseEventArgs^ e)
    {
        if (_pimpl->_inputTranslator) {
            auto ctrl = dynamic_cast<Control^>(sender);
            if (!ctrl) return;
            
            _pimpl->_inputTranslator->OnMouseButtonChange(AsIndex(e->Button), false);
            ctrl->Invalidate();
        }
    }

    void EngineControl::Evnt_MouseWheel(Object^ sender, MouseEventArgs^ e)
    {
        if (_pimpl->_inputTranslator) {
            auto ctrl = dynamic_cast<Control^>(sender);
            if (!ctrl) return;
            
            _pimpl->_inputTranslator->OnMouseWheel(e->Delta);
            ctrl->Invalidate();
        }
    }

    void EngineControl::Evnt_DoubleClick(Object^ sender, MouseEventArgs^ e)
    {
        if (_pimpl->_inputTranslator) {
            auto ctrl = dynamic_cast<Control^>(sender);
            if (!ctrl) return;
            
            _pimpl->_inputTranslator->OnMouseButtonDblClk(AsIndex(e->Button));
            ctrl->Invalidate();
        }
    }

    void EngineControl::Evnt_FocusChange(Object ^sender, System::EventArgs ^e)
    {
        // when we've lost or gained the focus, we need to reset the input translator 
        //  (because we might miss key up/down message when not focused)
        if (_pimpl.get() && _pimpl->_inputTranslator.get()) {       // (this can sometimes be called after the dispose, which ends up with an invalid _pimpl)
            _pimpl->_inputTranslator->OnFocusChange();
        }
    }


    IWindowRig& EngineControl::GetWindowRig()
    {
        assert(_pimpl->_windowRig);
        return *_pimpl->_windowRig;
    }

    bool EngineControl::IsInputKey(Keys keyData)
    {
            // return true for any keys we want to handle as a normal (non-system)
            // key event
        switch (keyData)
        {
        case Keys::Left:
        case Keys::Right:
        case Keys::Up:
        case Keys::Down:
        case Keys::Tab:
            return true;

        default:
            return false;
        }
    }

    EngineControl::EngineControl(Control^ control)
    {
        _pimpl.reset(new EngineControlPimpl);
        _pimpl->_windowRig = EngineDevice::GetInstance()->GetNative().CreateWindowRig(control->Handle.ToPointer());
        _pimpl->_inputTranslator = std::make_unique<PlatformRig::InputTranslator>();
        _pimpl->_inputTranslator->AddListener(_pimpl->_windowRig->GetFrameRig().GetMainOverlaySystem()->GetInputListener());

        control->KeyDown += gcnew System::Windows::Forms::KeyEventHandler(this, &EngineControl::Evnt_KeyDown);
        control->KeyUp += gcnew System::Windows::Forms::KeyEventHandler(this, &EngineControl::Evnt_KeyUp);
        control->KeyPress += gcnew System::Windows::Forms::KeyPressEventHandler(this, &EngineControl::Evnt_KeyPress);
        control->MouseMove += gcnew System::Windows::Forms::MouseEventHandler(this, &EngineControl::Evnt_MouseMove);
        control->MouseDown += gcnew System::Windows::Forms::MouseEventHandler(this, &EngineControl::Evnt_MouseDown);
        control->MouseUp += gcnew System::Windows::Forms::MouseEventHandler(this, &EngineControl::Evnt_MouseUp);
        control->MouseWheel += gcnew System::Windows::Forms::MouseEventHandler(this, &EngineControl::Evnt_MouseWheel);
        control->MouseDoubleClick += gcnew System::Windows::Forms::MouseEventHandler(this, &EngineControl::Evnt_DoubleClick);
        control->GotFocus += gcnew System::EventHandler(this, &GUILayer::EngineControl::Evnt_FocusChange);
        control->LostFocus += gcnew System::EventHandler(this, &GUILayer::EngineControl::Evnt_FocusChange);
        control->Resize += gcnew System::EventHandler(this, &GUILayer::EngineControl::Evnt_Resize);
    }

    EngineControl::~EngineControl()
    {
        _pimpl.reset();
    }

    EngineControl::!EngineControl()
    {
        _pimpl.reset();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    EngineControlPimpl::~EngineControlPimpl() {}

}

