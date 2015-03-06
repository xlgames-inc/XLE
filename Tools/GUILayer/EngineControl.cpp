// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "EngineControl.h"
#include "EngineDevice.h"
#include "CLIXAutoPtr.h"
#include "../../PlatformRig/FrameRig.h"
#include "../../RenderCore/IDevice.h"
#include "../../Utility/PtrUtils.h"
#include "../../Core/WinAPI/IncludeWindows.h"

namespace GUILayer 
{

///////////////////////////////////////////////////////////////////////////////////////////////////

    class WindowRig
    {
    public:
        PlatformRig::FrameRig& GetFrameRig() { return *_frameRig; }
        std::shared_ptr<RenderCore::IPresentationChain>& GetPresentationChain() { return _presentationChain; }

        WindowRig(EngineDevice& device, const void* platformWindowHandle);
        ~WindowRig();
    protected:
        std::unique_ptr<PlatformRig::FrameRig> _frameRig;
        std::shared_ptr<RenderCore::IPresentationChain> _presentationChain;
    };

    WindowRig::WindowRig(EngineDevice& device, const void* platformWindowHandle)
    {
        ::RECT clientRect;
        GetClientRect((HWND)platformWindowHandle, &clientRect);

        _presentationChain = device.GetRenderDevice()->CreatePresentationChain(
            platformWindowHandle,
            clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
        _frameRig = std::make_unique<PlatformRig::FrameRig>();
    }

    WindowRig::~WindowRig() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    ref class EngineControl::Pimpl
    {
    public:
        clix::auto_ptr<WindowRig> _windowRig;
    };

    static PlatformRig::FrameRig::RenderResult DummyRenderFrame(
        RenderCore::IThreadContext* context)
    {
        return PlatformRig::FrameRig::RenderResult(false);
    }

    void EngineControl::OnPaint(PaintEventArgs pe)
    {
        auto& frameRig = _pimpl->_windowRig->GetFrameRig();
        {
            auto& engineDevice = EngineDevice::GetInstance();
            auto* renderDevice = engineDevice.GetRenderDevice();
            frameRig.ExecuteFrame(
                renderDevice->GetImmediateContext().get(),
                renderDevice,
                _pimpl->_windowRig->GetPresentationChain().get(),
                nullptr, nullptr, 
                DummyRenderFrame);
        }

        // Note -- we're suppressing base class paint events to
        // try to avoid flicker. See:
        //    https://msdn.microsoft.com/en-us/library/1e430ef4(v=vs.85).aspx
        // __super::OnPaint(pe);
    }

    void EngineControl::OnPaintBackground(PaintEventArgs)
    {
        // never draw the background. We want to avoid cases where
        // the background draws over a valid rendered surface (particularly
        // since the rendering might be desynchronised from the normal window
        // update process
        // __super::OnPaintBackground(pe);
    }

    EngineControl::EngineControl()
    {
        _pimpl = gcnew Pimpl;
        _pimpl->_windowRig.reset(new WindowRig(EngineDevice::GetInstance(), this->Handle.ToPointer()));
        InitializeComponent();
    }

    EngineControl::~EngineControl()
    {
        delete components;
        delete _pimpl;
    }
}

