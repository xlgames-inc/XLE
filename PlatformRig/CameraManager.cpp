// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CameraManager.h"
#include "UnitCamera.h"
#include "../RenderCore/RenderUtils.h"
#include "../Math/Transformations.h"

namespace PlatformRig { namespace Camera
{
    static void UpdateCamera_Slew(  RenderCore::Techniques::CameraDesc& camera, float dt, 
                                    const RenderOverlays::DebuggingDisplay::InputSnapshot& input)
    {
            // (Derived from Archeage/x2standalone camera mode)
        const float cl_sensitivity = 20.f;
        const float fr_fspeed_scale = 100.f * 1.f;
        const float fr_fturn_scale = 2.f;
        const float fr_speed_scale = 100.f * 1.f/3.f;
        const float fr_turn_scale = 1.f;
        const float fr_xspeed = 40.f;
        const float fr_yspeed = 40.f;
        const float fr_zspeed = 40.f;
        const float fr_xturn = 60.f;
        const float fr_yturn = 60.f;
        

            //
            //      Our "camera" coordinate space:
            //          (different from Crytek camera space)
            //      
            //      *	Right handed
            //      *	+X to the right
            //      *	+Y up
            //      *	-Z into the screen
            //

        using namespace RenderOverlays::DebuggingDisplay;
        static const KeyId shift        = KeyId_Make("shift");
        static const KeyId ctrl         = KeyId_Make("control");
        static const KeyId forward      = KeyId_Make("w");
        static const KeyId back         = KeyId_Make("s");
        static const KeyId left         = KeyId_Make("a");
        static const KeyId right        = KeyId_Make("d");
        static const KeyId up           = KeyId_Make("home");
        static const KeyId down         = KeyId_Make("end");
        static const KeyId turnLeft     = KeyId_Make("left");
        static const KeyId turnRight    = KeyId_Make("right");
        static const KeyId turnUp       = KeyId_Make("up");
        static const KeyId turnDown     = KeyId_Make("down");

            // change move/turn speed
        bool fastMove = input.IsHeld(shift);
        bool slowMove = input.IsHeld(ctrl);
        float moveScale = fastMove ? fr_fspeed_scale : (slowMove? (fr_speed_scale/100) : fr_speed_scale);
        float turnScale = fastMove ? fr_fturn_scale : fr_turn_scale;

        float moveSpeedX = fr_xspeed * moveScale;
        float moveSpeedY = fr_yspeed * moveScale;
        float moveSpeedZ = fr_zspeed * moveScale;
        float yawSpeed   = fr_xturn  * turnScale;
        float pitchSpeed = fr_yturn  * turnScale;

            // panning & rotation
        Float3 deltaPos(0,0,0);
        float deltaCameraYaw = 0.f, deltaCameraPitch = 0.f;

            // move forward and sideways and up and down
        deltaPos[2] -= input.IsHeld(forward)    * 1.0f;
        deltaPos[2] += input.IsHeld(back)       * 1.0f;
        deltaPos[0] -= input.IsHeld(left)       * 1.0f;
        deltaPos[0] += input.IsHeld(right)      * 1.0f;
        deltaPos[1] += input.IsHeld(up)         * 1.0f;
        deltaPos[1] -= input.IsHeld(down)       * 1.0f;

        auto mouseX = input._mouseDelta[0], mouseY = input._mouseDelta[1];
        const bool rightButton      = input.IsHeld_RButton();
        if (rightButton) {
            float mouseSensitivity = -0.01f * std::max(0.01f, cl_sensitivity);
            mouseSensitivity    *= gPI / 180.0f;
            deltaCameraYaw      +=  mouseX * mouseSensitivity; 
            deltaCameraPitch    +=  mouseY * mouseSensitivity;
        } else {
            deltaCameraYaw      += input.IsHeld(left)     * yawSpeed   / 180.f;
            deltaCameraYaw      -= input.IsHeld(right)    * yawSpeed   / 180.f;
            deltaCameraPitch    += input.IsHeld(up)       * pitchSpeed / 180.f;
            deltaCameraPitch    -= input.IsHeld(down)     * pitchSpeed / 180.f;
            deltaCameraYaw      *= dt;
            deltaCameraPitch    *= dt;
        }

        deltaPos[0] *= moveSpeedX;
        deltaPos[1] *= moveSpeedY;
        deltaPos[2] *= moveSpeedZ;

            // apply rotation
        static cml::EulerOrder eulerOrder = cml::euler_order_zxz;      
        Float3 ypr = cml::matrix_to_euler<Float4x4, Float4x4::value_type>(camera._cameraToWorld, eulerOrder);
        ypr[2] += deltaCameraYaw;
        ypr[1] += deltaCameraPitch;
        ypr[1] = Clamp(ypr[1], 0.1f, 3.1f);

        Float3 camPos = Truncate(camera._cameraToWorld * Expand(Float3(dt * deltaPos), Float3::value_type(1)));
        Float3x3 rotationPart;
        cml::matrix_rotation_euler(rotationPart, ypr[0], ypr[1], ypr[2], eulerOrder);
        camera._cameraToWorld = Expand(rotationPart, camPos);
    }

    static void UpdateCamera_Orbit( RenderCore::Techniques::CameraDesc& camera, float dt, Float3& focusPoint,
                                    const RenderOverlays::DebuggingDisplay::InputSnapshot& input)
    {
        const float cl_sensitivity   = 20.f;
        const float fr_fspeed_scale  = 1.f;
        const float fr_speed_scale   = 1.f/3.f;

        using namespace RenderOverlays::DebuggingDisplay;
        static const KeyId shift        = KeyId_Make("shift");
        static const KeyId forward      = KeyId_Make("w");
        static const KeyId back         = KeyId_Make("s");
        static const KeyId left         = KeyId_Make("a");
        static const KeyId right        = KeyId_Make("d");
        static const KeyId up           = KeyId_Make("home");
        static const KeyId down         = KeyId_Make("end");
        static const KeyId turnLeft     = KeyId_Make("left");
        static const KeyId turnRight    = KeyId_Make("right");
        static const KeyId turnUp       = KeyId_Make("up");
        static const KeyId turnDown     = KeyId_Make("down");

        bool fastMove     = input.IsHeld(shift);
        float moveScale   = fastMove ? fr_fspeed_scale : fr_speed_scale;
        moveScale        *= std::max(0.2f, Magnitude(ExtractTranslation(camera._cameraToWorld) - focusPoint));

        float deltaRotationX = 0.f, deltaRotationY = 0.f;
        Float3 deltaPos(0,0,0);

            // move forward and sideways and up and down
        deltaPos[2] += input.IsHeld(forward)    * 1.0f;
        deltaPos[2] -= input.IsHeld(back)       * 1.0f;
        deltaPos[0] -= input.IsHeld(left)       * 1.0f;
        deltaPos[0] += input.IsHeld(right)      * 1.0f;
        deltaPos[1] += input.IsHeld(up)         * 1.0f;
        deltaPos[1] -= input.IsHeld(down)       * 1.0f;

        auto mouseX = input._mouseDelta[0], mouseY = input._mouseDelta[1];
        const bool rightButton      = input.IsHeld_RButton();
        if (rightButton) {
            float mouseSensitivity = -0.01f * std::max(0.01f, cl_sensitivity);
            mouseSensitivity    *= gPI / 180.0f;
            deltaRotationX      +=  mouseX * mouseSensitivity; 
            deltaRotationY      +=  mouseY * mouseSensitivity;
        }

        deltaPos[0] *= moveScale;
        deltaPos[1] *= moveScale;
        deltaPos[2] *= moveScale;

        Float3 rotYAxis = Truncate(camera._cameraToWorld * Float4(1.0f, 0.f, 0.f, 0.f));

        Float4x4 cameraToWorld = camera._cameraToWorld;
        Combine_InPlace(cameraToWorld, Float3(-focusPoint));
        cameraToWorld = Combine(cameraToWorld, MakeRotationMatrix(rotYAxis, deltaRotationY));
        Combine_InPlace(cameraToWorld, RotationZ(deltaRotationX));
        Combine_InPlace(cameraToWorld, focusPoint);
        Combine_InPlace(cameraToWorld, Float3(deltaPos[2] * Normalize(focusPoint - ExtractTranslation(camera._cameraToWorld))));

        Float3 cameraFocusDrift = 
            Float3(0.f, 0.f, deltaPos[1])
            + deltaPos[0] * Truncate(camera._cameraToWorld * Float4(1.0f, 0.f, 0.f, 0.f));
        Combine_InPlace(cameraToWorld, cameraFocusDrift);
        focusPoint += cameraFocusDrift;

        camera._cameraToWorld = cameraToWorld;
    }

    bool    CameraInputHandler::OnInputEvent(const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt)
    {
        _accumulatedState.Accumulate(evnt, _prevAccumulatedState);
        return false;
    }

    void    CameraInputHandler::Commit(float dt)
    {
        // UpdateCamera_Orbit(*_camera, dt, _accumulatedState);

        using namespace RenderOverlays::DebuggingDisplay;
        const KeyId shift = KeyId_Make("shift");
        const KeyId tab = KeyId_Make("tab");

        static unsigned mode = 0;
        if (_accumulatedState.IsPress(tab)) {
            mode = (mode+1)%2;
        }

        if (mode==0) {
            Camera::ClientUnit clientUnit;
            clientUnit._localToWorld = _playerCharacter->GetLocalToWorld();
            static bool isInitialised = false;
            if (!isInitialised) {
                _unitCamera->InitUnitCamera(&clientUnit);
                isInitialised = true;
            }
            if (_accumulatedState.IsPress_RButton()) {
                _unitCamera->AlignUnitToCamera(&clientUnit, _unitCamera->GetUnitCamera().yaw);
                _playerCharacter->SetLocalToWorld(clientUnit._localToWorld);     // push this back into the character transform
            }

            if (!_accumulatedState.IsHeld(shift)) {

                auto camResult = _unitCamera->UpdateUnitCamera(dt, &clientUnit, _accumulatedState);
                assert(ExtractTranslation(camResult._cameraToWorld)[0] == ExtractTranslation(camResult._cameraToWorld)[0]);
                assert(ExtractTranslation(camResult._cameraToWorld)[1] == ExtractTranslation(camResult._cameraToWorld)[1]);
                assert(ExtractTranslation(camResult._cameraToWorld)[2] == ExtractTranslation(camResult._cameraToWorld)[2]);
                _camera->_cameraToWorld = camResult._cameraToWorld;
                _camera->_verticalFieldOfView = camResult._fov;

                    //  Convert from object-to-world transform into
                    //  camera-to-world transform
                std::swap(_camera->_cameraToWorld(0,1), _camera->_cameraToWorld(0,2));
                std::swap(_camera->_cameraToWorld(1,1), _camera->_cameraToWorld(1,2));
                std::swap(_camera->_cameraToWorld(2,1), _camera->_cameraToWorld(2,2));
                _camera->_cameraToWorld(0,2) = -_camera->_cameraToWorld(0,2);
                _camera->_cameraToWorld(1,2) = -_camera->_cameraToWorld(1,2);
                _camera->_cameraToWorld(2,2) = -_camera->_cameraToWorld(2,2);

            }
        } else if (mode==1) {
            UpdateCamera_Slew(*_camera, dt, _accumulatedState);
        } else if (mode==2) {
            auto orbitFocus = _orbitFocus;
            if (_playerCharacter) {
                orbitFocus = TransformPoint(_playerCharacter->GetLocalToWorld(), orbitFocus);
                UpdateCamera_Orbit(*_camera, dt, orbitFocus, _accumulatedState);
                orbitFocus = TransformPoint(InvertOrthonormalTransform(_playerCharacter->GetLocalToWorld()), orbitFocus);
            } else {
                UpdateCamera_Orbit(*_camera, dt, _orbitFocus, _accumulatedState);
            }
        }

        _prevAccumulatedState = _accumulatedState;
        _accumulatedState.Reset();
    }

    CameraInputHandler::CameraInputHandler(
        std::shared_ptr<RenderCore::Techniques::CameraDesc> camera, 
        std::shared_ptr<ICameraAttach> playerCharacter,
        float charactersScale) 
    : _camera(std::move(camera)) 
    , _playerCharacter(std::move(playerCharacter))
    , _orbitFocus(0.f, 0.f, 0.f)
    {
        _unitCamera = std::make_unique<UnitCamManager>(charactersScale);
    }


}}

