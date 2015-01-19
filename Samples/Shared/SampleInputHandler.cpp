// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SampleInputHandler.h"
#include "Character.h"
#include "../../PlatformRig/ManipulatorsUtil.h"
#include "../../Math/Transformations.h"
#include "../../Utility/PtrUtils.h"

namespace SceneEngine { extern float SunDirectionAngle; }

namespace Sample
{
    bool SampleInputHandler::OnInputEvent(
        const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt)
    {
        using namespace RenderOverlays::DebuggingDisplay;
            //  on left button release; if we ctrl-clicked the terrain
            //  then let's moved the player character to that location
        static KeyId ctrl = KeyId_Make("control");
        const KeyId shift = KeyId_Make("shift");

        if (evnt.IsRelease_LButton() && evnt.IsHeld(ctrl)) {
            auto intersection = _hitTestResolver->DoHitTest(evnt._mousePosition);
            if (intersection._type == Tools::HitTestResolver::Result::Terrain) {
                _playerCharacter->SetLocalToWorld(AsFloat4x4(intersection._worldSpaceCollision));
                return true;
            }
        }

        if (evnt.IsHeld(shift) && evnt.IsHeld_MButton()) {
            auto deltaMouse = evnt._mouseDelta;
            SceneEngine::SunDirectionAngle = Clamp(
                SceneEngine::SunDirectionAngle + deltaMouse[0] * 1.0f * gPI / 180.f, 
                -.5f * gPI, .5f * gPI);
        }

        return false;
    }

    SampleInputHandler::SampleInputHandler(
        std::shared_ptr<Character> playerCharacter, 
        const Tools::HitTestResolver& hitTestResolverI)
    {
        auto hitTestResolver = std::make_unique<Tools::HitTestResolver>(hitTestResolverI);
        _playerCharacter = std::move(playerCharacter);
        _hitTestResolver = std::move(hitTestResolver);
    }
}

