// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4793) //  : function compiled as native :

#include "EngineControl.h"
#include "EngineDevice.h"
#include "IWindowRig.h"
#include "../../PlatformRig/FrameRig.h"
#include "../../PlatformRig/OverlappedWindow.h"
#include "../../PlatformRig/InputTranslator.h"
#include "../../RenderCore/IDevice.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../Utility/PtrUtils.h"
#include "../../Core/WinAPI/IncludeWindows.h"


#include "../../RenderOverlays/OverlayContext.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/ResourceBox.h"
#include "../../RenderCore/IThreadContext.h"

namespace GUILayer 
{
///////////////////////////////////////////////////////////////////////////////////////////////////

    class EngineControlPimpl
    {
    public:
        std::unique_ptr<IWindowRig> _windowRig;
        std::unique_ptr<PlatformRig::InputTranslator> _inputTranslator;
    };

    class RenderPostSceneResources
    {
    public:
        class Desc 
        {
        public:
            unsigned _fontSize;
            Desc(unsigned fontSize) : _fontSize(fontSize) {}
        };
        intrusive_ptr<RenderOverlays::Font> _font;
        RenderPostSceneResources(const Desc& desc);
    };

    RenderPostSceneResources::RenderPostSceneResources(const Desc& desc)
    {
        _font = RenderOverlays::GetX2Font("DosisExtraBold", desc._fontSize);
    }

    static PlatformRig::FrameRig::RenderResult DummyRenderFrame(RenderCore::IThreadContext* context)
    {
        const char text[] = "Hello World!... It's me, XLE!";
        
        using namespace RenderOverlays;
        auto& res = RenderCore::Techniques::FindCachedBox<RenderPostSceneResources>(RenderPostSceneResources::Desc(64));
        TextStyle style(*res._font);
        ColorB col(0xffffffff);
        
        auto contextStateDesc = context->GetStateDesc();
        
            //      Render text using a IOverlayContext
		auto overlayContext = std::unique_ptr<ImmediateOverlayContext, AlignedDeletor>(
			(ImmediateOverlayContext*)XlMemAlign(sizeof(ImmediateOverlayContext), 16));
		#pragma push_macro("new")
		#undef new
				new(overlayContext.get()) ImmediateOverlayContext(context);
		#pragma pop_macro("new")
		
        overlayContext->CaptureState();
        overlayContext->DrawText(
            std::make_tuple(
                Float3(0.f, 0.f, 0.f), 
                Float3(float(contextStateDesc._viewportDimensions[0]), float(contextStateDesc._viewportDimensions[1]), 0.f)),
            1.f, &style, col, TextAlignment::Center, text, nullptr);

        return PlatformRig::FrameRig::RenderResult(false);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    void EngineControl::OnPaint(PaintEventArgs^ pe)
    {
        // Note -- we're suppressing base class paint events to
        // try to avoid flicker. See:
        //    https://msdn.microsoft.com/en-us/library/1e430ef4(v=vs.85).aspx
        // __super::OnPaint(pe);

        auto& frameRig = _pimpl->_windowRig->GetFrameRig();
        {
            auto engineDevice = EngineDevice::GetInstance();
            auto* renderDevice = engineDevice->GetNative().GetRenderDevice();
            auto immediateContext = renderDevice->GetImmediateContext();
            frameRig.ExecuteFrame(
                immediateContext.get(),
                renderDevice,
                _pimpl->_windowRig->GetPresentationChain().get(),
                nullptr, nullptr, 
                DummyRenderFrame);
        }
    }

    void EngineControl::OnPaintBackground(PaintEventArgs^)
    {
        // never draw the background. We want to avoid cases where
        // the background draws over a valid rendered surface (particularly
        // since the rendering might be desynchronised from the normal window
        // update process
        // __super::OnPaintBackground(pe);
    }

    void EngineControl::OnResize(EventArgs^ e)
    {
        _pimpl->_windowRig->OnResize(Size.Width, Size.Height);
        __super::OnResize(e);
    }

    void EngineControl::Evnt_KeyDown(Object^, KeyEventArgs^ e)
    {
        if (_pimpl->_inputTranslator) { 
            _pimpl->_inputTranslator->OnKeyChange(e->KeyValue, true); 
            e->Handled = true;
        }
    }

    void EngineControl::Evnt_KeyUp(Object^, KeyEventArgs^ e)
    {
        if (_pimpl->_inputTranslator) { 
            _pimpl->_inputTranslator->OnKeyChange(e->KeyValue, false); 
            e->Handled = true;
        }
    }

    void EngineControl::Evnt_KeyPress(Object^, KeyPressEventArgs^ e)
    {
        if (_pimpl->_inputTranslator) {
            _pimpl->_inputTranslator->OnChar(e->KeyChar);
            e->Handled = true;
        }
    }

    void EngineControl::Evnt_MouseMove(Object^, MouseEventArgs^ e)
    {
            // (todo -- only when activated?)
        if (_pimpl->_inputTranslator) {
            _pimpl->_inputTranslator->OnMouseMove(e->Location.X, e->Location.Y);
        }
    }

    static unsigned AsIndex(MouseButtons button)
    {
        switch (button) {
        case MouseButtons::Left: return 0;
        case MouseButtons::Middle: return 1;
        case MouseButtons::Right: return 2;
        default: return 3;
        }
    }
    
    void EngineControl::Evnt_MouseDown(Object^, MouseEventArgs^ e)
    {
        if (_pimpl->_inputTranslator) {
            _pimpl->_inputTranslator->OnMouseButtonChange(AsIndex(e->Button), true);
        }
    }

    void EngineControl::Evnt_MouseUp(Object^, MouseEventArgs^ e)
    {
        if (_pimpl->_inputTranslator) {
            _pimpl->_inputTranslator->OnMouseButtonChange(AsIndex(e->Button), false);
        }
    }

    void EngineControl::Evnt_DoubleClick(Object^, MouseEventArgs^ e)
    {
        if (_pimpl->_inputTranslator) {
            _pimpl->_inputTranslator->OnMouseButtonDblClk(AsIndex(e->Button));
        }
    }

    EngineControl::EngineControl()
    {
        _pimpl.reset(new EngineControlPimpl);
        _pimpl->_windowRig = EngineDevice::GetInstance()->GetNative().CreateWindowRig(this->Handle.ToPointer());
        _pimpl->_inputTranslator = std::make_unique<PlatformRig::InputTranslator>();
        InitializeComponent();

        KeyDown += gcnew System::Windows::Forms::KeyEventHandler(this, &EngineControl::Evnt_KeyDown);
        KeyUp += gcnew System::Windows::Forms::KeyEventHandler(this, &EngineControl::Evnt_KeyUp);
        KeyPress += gcnew System::Windows::Forms::KeyPressEventHandler(this, &EngineControl::Evnt_KeyPress);
        MouseMove += gcnew System::Windows::Forms::MouseEventHandler(this, &EngineControl::Evnt_MouseMove);
        MouseDown += gcnew System::Windows::Forms::MouseEventHandler(this, &EngineControl::Evnt_MouseDown);
        MouseUp += gcnew System::Windows::Forms::MouseEventHandler(this, &EngineControl::Evnt_MouseUp);
        MouseDoubleClick += gcnew System::Windows::Forms::MouseEventHandler(this, &EngineControl::Evnt_DoubleClick);
    }

    EngineControl::~EngineControl()
    {
        // RenderCore::Metal::DeviceContext::PrepareForDestruction(
        //     EngineDevice::GetInstance()->GetRenderDevice(), 
        //     _pimpl->_windowRig->GetPresentationChain().get());

        delete components;
        delete _pimpl;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
}

