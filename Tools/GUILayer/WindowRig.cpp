// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "WindowRigInternal.h"
#include "../../PlatformRig/OverlappedWindow.h"
#include "../../PlatformRig/FrameRig.h"
#include "../../RenderCore/IDevice.h"
#include "../../Utility/PtrUtils.h"
#include "../../Core/WinAPI/IncludeWindows.h"

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

///////////////////////////////////////////////////////////////////////////////////////////////////

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
        _frameRig = std::make_unique<PlatformRig::FrameRig>(false); // (not "main" frame rig by default)

        AddWindowHandler(std::make_shared<ResizePresentationChain>(_presentationChain));
    }

    WindowRig::~WindowRig() {}


    IWindowRig::~IWindowRig() {}
}

