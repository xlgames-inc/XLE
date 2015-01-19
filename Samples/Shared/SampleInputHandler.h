// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderOverlays/DebuggingDisplay.h"
#include <memory>

namespace Tools { class HitTestResolver; }

namespace Sample
{
    class Character;
    class SampleInputHandler : public RenderOverlays::DebuggingDisplay::IInputListener
    {
    public:
        bool OnInputEvent(const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt);

        SampleInputHandler(
            std::shared_ptr<Character> playerCharacter, 
            const Tools::HitTestResolver& hitTestResolver);
    protected:
        std::shared_ptr<Character> _playerCharacter;
        std::unique_ptr<Tools::HitTestResolver> _hitTestResolver;
    };
}



