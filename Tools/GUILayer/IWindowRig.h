// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>
#include <functional>

namespace PlatformRig { class FrameRig; class IWindowHandler; }
namespace RenderCore { class IPresentationChain; class IThreadContext; }

namespace GUILayer
{
    class IWindowRig
    {
    public:
        virtual PlatformRig::FrameRig& GetFrameRig() = 0;
        virtual std::shared_ptr<RenderCore::IPresentationChain>& GetPresentationChain() = 0;

        virtual void AddWindowHandler(std::shared_ptr<PlatformRig::IWindowHandler> windowHandler) = 0;
        virtual void OnResize(unsigned newWidth, unsigned newHeight) = 0;

        virtual ~IWindowRig();
    };
}

