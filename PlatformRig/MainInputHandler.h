// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderOverlays/DebuggingDisplay.h"
#include <vector>
#include <memory>

namespace PlatformRig
{
    class MainInputHandler : public RenderOverlays::DebuggingDisplay::IInputListener
    {
    public:
        bool    OnInputEvent(
			const RenderOverlays::DebuggingDisplay::InputContext& context,
			const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt);
        void    AddListener(std::shared_ptr<RenderOverlays::DebuggingDisplay::IInputListener> listener);

        MainInputHandler();
        ~MainInputHandler();
    private:
        std::vector<std::shared_ptr<RenderOverlays::DebuggingDisplay::IInputListener>> _listeners;
    };

    class DebugScreensInputHandler : public RenderOverlays::DebuggingDisplay::IInputListener
    {
    public:
        bool    OnInputEvent(
			const RenderOverlays::DebuggingDisplay::InputContext& context,
			const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt);

        DebugScreensInputHandler(std::shared_ptr<RenderOverlays::DebuggingDisplay::DebugScreensSystem> debugScreens);
        ~DebugScreensInputHandler();
    private:
        std::shared_ptr<RenderOverlays::DebuggingDisplay::DebugScreensSystem>       _debugScreens;
    };

}

