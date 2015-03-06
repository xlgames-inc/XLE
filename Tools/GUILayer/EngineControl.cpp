// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4793) //  : function compiled as native :

#include "EngineControl.h"
#include "EngineDevice.h"
#include "CLIXAutoPtr.h"
#include "../../PlatformRig/FrameRig.h"
#include "../../PlatformRig/OverlappedWindow.h"
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

    class ResizePresentationChain : public PlatformRig::IWindowHandler
    {
    public:
        void    OnResize(unsigned newWidth, unsigned newHeight);

        ResizePresentationChain(
            std::shared_ptr<RenderCore::IPresentationChain> presentationChain);
    protected:
        std::shared_ptr<RenderCore::IPresentationChain> _presentationChain;
    };

    void ResizePresentationChain::OnResize(unsigned newWidth, unsigned newHeight)
    {
        if (_presentationChain) {
                //  When we become an icon, we'll end up with zero width and height.
                //  We can't actually resize the presentation to zero. And we can't
                //  delete the presentation chain from here. So maybe just do nothing.
            if (newWidth && newHeight) {
                _presentationChain->Resize(newWidth, newHeight);
            }
        }
    }

    ResizePresentationChain::ResizePresentationChain(std::shared_ptr<RenderCore::IPresentationChain> presentationChain)
    : _presentationChain(presentationChain)
    {}


    class WindowRig
    {
    public:
        PlatformRig::FrameRig& GetFrameRig() { return *_frameRig; }
        std::shared_ptr<RenderCore::IPresentationChain>& GetPresentationChain() { return _presentationChain; }
        void AddWindowHandler(std::shared_ptr<PlatformRig::IWindowHandler> windowHandler);

        void OnResize(unsigned newWidth, unsigned newHeight);

        WindowRig(RenderCore::IDevice& device, const void* platformWindowHandle);
        ~WindowRig();
    protected:
        std::unique_ptr<PlatformRig::FrameRig> _frameRig;
        std::shared_ptr<RenderCore::IPresentationChain> _presentationChain;
        std::vector<std::shared_ptr<PlatformRig::IWindowHandler>> _windowHandlers;
    };

    void WindowRig::AddWindowHandler(std::shared_ptr<PlatformRig::IWindowHandler> windowHandler)
    {
        _windowHandlers.push_back(std::move(windowHandler));
    }

    void WindowRig::OnResize(unsigned newWidth, unsigned newHeight)
    {
        for (auto i=_windowHandlers.begin(); i!=_windowHandlers.end(); ++i) {
            (*i)->OnResize(newWidth, newHeight);
        }
    }

    WindowRig::WindowRig(RenderCore::IDevice& device, const void* platformWindowHandle)
    {
        ::RECT clientRect;
        GetClientRect((HWND)platformWindowHandle, &clientRect);

        _presentationChain = device.CreatePresentationChain(
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
        const char text[] = {
            "Hello World!... It's me, XLE!"
        };
        
        using namespace RenderOverlays;
        auto& res = RenderCore::Techniques::FindCachedBox<RenderPostSceneResources>(RenderPostSceneResources::Desc(64));
        TextStyle style(*res._font);
        ColorB col(0xffffffff);
        
        auto contextStateDesc = context->GetStateDesc();
        
            //      Render text using a IOverlayContext
        auto overlayContext = std::make_unique<ImmediateOverlayContext>(context);
        overlayContext->CaptureState();
        overlayContext->DrawText(
            std::make_tuple(
                Float3(0.f, 0.f, 0.f), 
                Float3(float(contextStateDesc._viewportDimensions[0]), float(contextStateDesc._viewportDimensions[1]), 0.f)),
            1.f, &style, col, TextAlignment::Center, text, nullptr);

        return PlatformRig::FrameRig::RenderResult(false);
    }

    void EngineControl::OnPaint(PaintEventArgs^ pe)
    {
        // Note -- we're suppressing base class paint events to
        // try to avoid flicker. See:
        //    https://msdn.microsoft.com/en-us/library/1e430ef4(v=vs.85).aspx
        // __super::OnPaint(pe);

        auto& frameRig = _pimpl->_windowRig->GetFrameRig();
        {
            auto engineDevice = EngineDevice::GetInstance();
            auto* renderDevice = engineDevice->GetRenderDevice();
            auto immediateContext = renderDevice->GetImmediateContext();
            frameRig.ExecuteFrame(
                immediateContext.get(),
                renderDevice,
                _pimpl->_windowRig->GetPresentationChain().get(),
                nullptr, nullptr, 
                DummyRenderFrame);

                // we need to tick buffer uploads in the main present thread at regular intervals...
            engineDevice->GetBufferUploads()->Update(immediateContext);
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

    EngineControl::EngineControl()
    {
        _pimpl = gcnew Pimpl;
        _pimpl->_windowRig.reset(
            new WindowRig(*EngineDevice::GetInstance()->GetRenderDevice(), this->Handle.ToPointer()));

        _pimpl->_windowRig->AddWindowHandler(
            std::make_shared<ResizePresentationChain>(_pimpl->_windowRig->GetPresentationChain()));
        InitializeComponent();
    }

    EngineControl::~EngineControl()
    {
        // RenderCore::Metal::DeviceContext::PrepareForDestruction(
        //     EngineDevice::GetInstance()->GetRenderDevice(), 
        //     _pimpl->_windowRig->GetPresentationChain().get());

        delete components;
        delete _pimpl;
    }
}

