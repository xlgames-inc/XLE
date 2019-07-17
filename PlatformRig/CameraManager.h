// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "InputListener.h"
#include "../Math/Matrix.h"

namespace RenderCore { namespace Techniques { class CameraDesc; } }

namespace PlatformRig { namespace Camera
{
    class ICameraAttach
    {
    public:
        virtual const Float4x4& GetLocalToWorld() const = 0;
        virtual void SetLocalToWorld(const Float4x4& newTransform) = 0;
    };

    class UnitCamManager;

    class CameraInputHandler : public IInputListener
    {
    public:
        bool    OnInputEvent(const InputContext& context, const InputSnapshot& evnt);
        void    Commit(float dt);
        
        CameraInputHandler(
            std::shared_ptr<RenderCore::Techniques::CameraDesc> camera, 
            std::shared_ptr<ICameraAttach> playerCharacter,
            float charactersScale);

    protected:
        std::shared_ptr<RenderCore::Techniques::CameraDesc> _camera;
        std::shared_ptr<ICameraAttach> _playerCharacter;
        std::unique_ptr<UnitCamManager> _unitCamera;
        Float3 _orbitFocus;

        InputSnapshot     _accumulatedState;
        InputSnapshot     _prevAccumulatedState;
    };
}}

