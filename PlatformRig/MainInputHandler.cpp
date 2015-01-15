// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MainInputHandler.h"
#include "../Utility/StringUtils.h"

namespace PlatformRig
{
    bool    MainInputHandler::OnInputEvent(const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt)
    {
        bool consumed = false;
        for (auto i=_listeners.cbegin(); i!=_listeners.cend() && !consumed; ++i) {
            consumed |= (*i)->OnInputEvent(evnt);
        }
        return consumed;
    }

    void    MainInputHandler::AddListener(std::shared_ptr<RenderOverlays::DebuggingDisplay::IInputListener> listener)
    {
        _listeners.push_back(std::move(listener));
    }

    MainInputHandler::MainInputHandler()
    {}

    MainInputHandler::~MainInputHandler() {}



    bool    DebugScreensInputHandler::OnInputEvent(const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt)
    {
        using namespace RenderOverlays::DebuggingDisplay;
        static const KeyId consoleKey = KeyId_Make("~");
        static const KeyId shiftKey = KeyId_Make("shift");
        static const KeyId escape = KeyId_Make("escape");
        if (evnt.IsPress(consoleKey) && evnt.IsHeld(shiftKey)) {
            const char* currentScreen = _debugScreens->CurrentScreen(0);
            if (currentScreen && !XlCompareStringI(currentScreen, "[Console] Console")) {
                _debugScreens->SwitchToScreen(0, nullptr);
            } else {
                _debugScreens->SwitchToScreen(0, "[Console] Console");
            }
            return true;
        } else if (evnt.IsPress(escape)) {
            if (_debugScreens->CurrentScreen(0)) {
                _debugScreens->SwitchToScreen(0, nullptr);
                return true;
            }
        }

        return _debugScreens && _debugScreens->OnInputEvent(evnt);
    }

    DebugScreensInputHandler::DebugScreensInputHandler(std::shared_ptr<RenderOverlays::DebuggingDisplay::DebugScreensSystem> debugScreens)
    : _debugScreens(std::move(debugScreens)) 
    {}

    DebugScreensInputHandler::~DebugScreensInputHandler() {}
}

