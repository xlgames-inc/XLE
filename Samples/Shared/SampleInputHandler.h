// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderOverlays/DebuggingDisplay.h"
#include <memory>

namespace SceneEngine { class IntersectionTestContext; class TerrainManager; }

namespace Sample
{
    class Character;
    class SampleInputHandler : public RenderOverlays::DebuggingDisplay::IInputListener
    {
    public:
        bool OnInputEvent(const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt);

        SampleInputHandler(
            std::shared_ptr<Character> playerCharacter, 
            std::shared_ptr<SceneEngine::TerrainManager> terrain,
            std::shared_ptr<SceneEngine::IntersectionTestContext> intersectionTestContext);
        ~SampleInputHandler();
    protected:
        std::shared_ptr<Character> _playerCharacter;
        std::shared_ptr<SceneEngine::TerrainManager> _terrain;
        std::shared_ptr<SceneEngine::IntersectionTestContext> _intersectionTestContext;
    };
}



