// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderOverlays/DebuggingDisplay.h"

namespace ConsoleRig { class Console; }

namespace PlatformRig { namespace Overlays
{
    using namespace RenderOverlays;
    using namespace RenderOverlays::DebuggingDisplay;

    class ConsoleDisplay : public IWidget ///////////////////////////////////////////////////////////
    {
    public:
        void    Render(IOverlayContext* context, Layout& layout, Interactables&interactables, InterfaceState& interfaceState);
        bool    ProcessInput(InterfaceState& interfaceState, const InputSnapshot& input);

        ConsoleDisplay(ConsoleRig::Console& console);
        ~ConsoleDisplay();
    private:
        std::u16string                  _currentLine;
        size_t                          _caret;
        size_t                          _selectionStart, _selectionEnd;
        unsigned                        _renderCounter;
        ConsoleRig::Console*            _console;

        std::vector<std::u16string>     _history;
        unsigned                        _historyCursor;

        std::vector<std::string>        _autoComplete;
        unsigned                        _autoCompleteCursor;

        unsigned                        _scrollBack;
        unsigned                        _scrollBackFractional;

        void DeleteSelectedPart();
    };
}}

