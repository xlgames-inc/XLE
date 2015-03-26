// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4793) //  : function compiled as native :

#include "EngineControl.h"
#include "EngineControlInternal.h"
#include "EngineDevice.h"
#include "NativeEngineDevice.h"
#include "IWindowRig.h"
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

    void EngineControl::OnPaint(PaintEventArgs^ pe)
    {
        // Note -- we're suppressing base class paint events to
        // try to avoid flicker. See:
        //    https://msdn.microsoft.com/en-us/library/1e430ef4(v=vs.85).aspx
        // __super::OnPaint(pe);

        Render();
    }

    void EngineControl::Render()
    {
        auto engineDevice = EngineDevice::GetInstance();
        auto* renderDevice = engineDevice->GetNative().GetRenderDevice();
        auto immediateContext = renderDevice->GetImmediateContext();
        Render(*immediateContext.get(), *_pimpl->_windowRig.get());
    }

    void EngineControl::OnResize(System::EventArgs^ e)
    {
        _pimpl->_windowRig->OnResize(_control->Size.Width, _control->Size.Height);
        // __super::OnResize(e);
    }

    void EngineControl::Evnt_KeyDown(Object^, KeyEventArgs^ e)
    {
        if (_pimpl->_inputTranslator) { 
            _pimpl->_inputTranslator->OnKeyChange(e->KeyValue, true); 
            e->Handled = true;
            _control->Invalidate();
        }
    }

    void EngineControl::Evnt_KeyUp(Object^, KeyEventArgs^ e)
    {
        if (_pimpl->_inputTranslator) { 
            _pimpl->_inputTranslator->OnKeyChange(e->KeyValue, false); 
            e->Handled = true;
            _control->Invalidate();
        }
    }

    void EngineControl::Evnt_KeyPress(Object^, KeyPressEventArgs^ e)
    {
        if (_pimpl->_inputTranslator) {
            _pimpl->_inputTranslator->OnChar(e->KeyChar);
            e->Handled = true;
            _control->Invalidate();
        }
    }

    void EngineControl::Evnt_MouseMove(Object^, MouseEventArgs^ e)
    {
            // (todo -- only when activated?)
        if (_pimpl->_inputTranslator) {
            PlatformRig::InputTranslator::s_hackWindowSize = UInt2(unsigned(_control->Size.Width), unsigned(_control->Size.Height));
            _pimpl->_inputTranslator->OnMouseMove(e->Location.X, e->Location.Y);
            _control->Invalidate();
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
    
    void EngineControl::Evnt_MouseDown(Object^, MouseEventArgs^ e)
    {
        if (_pimpl->_inputTranslator) {
            _pimpl->_inputTranslator->OnMouseButtonChange(AsIndex(e->Button), true);
            _control->Invalidate();
        }
    }

    void EngineControl::Evnt_MouseUp(Object^, MouseEventArgs^ e)
    {
        if (_pimpl->_inputTranslator) {
            _pimpl->_inputTranslator->OnMouseButtonChange(AsIndex(e->Button), false);
            _control->Invalidate();
        }
    }

    void EngineControl::Evnt_MouseWheel(Object^, MouseEventArgs^ e)
    {
        if (_pimpl->_inputTranslator) {
            _pimpl->_inputTranslator->OnMouseWheel(e->Delta);
            _control->Invalidate();
        }
    }

    void EngineControl::Evnt_DoubleClick(Object^, MouseEventArgs^ e)
    {
        if (_pimpl->_inputTranslator) {
            _pimpl->_inputTranslator->OnMouseButtonDblClk(AsIndex(e->Button));
            _control->Invalidate();
        }
    }

    IWindowRig& EngineControl::GetWindowRig()
    {
        assert(_pimpl->_windowRig);
        return *_pimpl->_windowRig;
    }

    EngineControl::EngineControl(Control^ control)
        : _control(control)
    {
        _pimpl.reset(new EngineControlPimpl);
        _pimpl->_windowRig = EngineDevice::GetInstance()->GetNative().CreateWindowRig(_control->Handle.ToPointer());
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
    }

    EngineControl::~EngineControl()
    {
        // RenderCore::Metal::DeviceContext::PrepareForDestruction(
        //     EngineDevice::GetInstance()->GetRenderDevice(), 
        //     _pimpl->_windowRig->GetPresentationChain().get());

        delete _pimpl;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
}

