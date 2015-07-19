// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "BasicManipulators.h"
#include "IManipulator.h"
#include "ModelVisualisation.h"
#include "VisualisationUtils.h"
#include "../../RenderOverlays/DebuggingDisplay.h"
#include "../../SceneEngine/IntersectionTest.h"
#include "../../Math/Transformations.h"
#include "../../Math/Geometry.h"
#include <memory>

namespace ToolsRig
{
    using RenderOverlays::DebuggingDisplay::InputSnapshot;

    class CameraMovementManipulator : public IManipulator
    {
    public:
        bool OnInputEvent(
            const InputSnapshot& evnt, 
            const SceneEngine::IntersectionTestContext& hitTestContext,
            const SceneEngine::IntersectionTestScene& hitTestScene);
        void Render(
            RenderCore::IThreadContext* context, 
            SceneEngine::LightingParserContext& parserContext);

        const char* GetName() const;
        std::string GetStatusText() const;

        std::pair<FloatParameter*, size_t>  GetFloatParameters() const;
        std::pair<BoolParameter*, size_t>   GetBoolParameters() const;
        std::pair<IntParameter*, size_t>   GetIntParameters() const { return std::make_pair(nullptr, 0); }
        void SetActivationState(bool newState);

        CameraMovementManipulator(
            std::shared_ptr<VisCameraSettings> visCameraSettings);
        ~CameraMovementManipulator();

    protected:
        std::shared_ptr<VisCameraSettings> _visCameraSettings;
        float _translateSpeed, _orbitRotationSpeed, _wheelTranslateSpeed;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    bool CameraMovementManipulator::OnInputEvent(
        const InputSnapshot& evnt, 
        const SceneEngine::IntersectionTestContext& hitTestContext,
        const SceneEngine::IntersectionTestScene& hitTestScene)
    {
            //  This is a simple camera manipulator
            //  It should operate when the middle mouse button is down.
            //  We'd do max controls to start with:
            //      * no modifiers: translate along X, Y axis
            //      * alt: orbit around focus point
            //      * mouse wheel: translate in and out
            //  
            //  Note that blender controls are very similar...
            //      * no modifiers: orbit around focus point
            //      * shift: translate along X, Y axis
            //
            //  We could add some configuration options to make it a bit
            //  more generic.
            //
            //  It's hard to use the keyboard to move the camera around 
            //  because it tends to conflict with other key-binds.
            //  However, we could make a rule that keyboard input is 
            //  directed to the camera when the middle mouse button is down.
        if (!_visCameraSettings) { return false; }

        static auto ctrl = RenderOverlays::DebuggingDisplay::KeyId_Make("control");
        if (evnt.IsHeld(ctrl) && evnt.IsPress_LButton()) {

            if (!&hitTestContext || !&hitTestScene) return false;

            auto worldSpaceRay = SceneEngine::IntersectionTestContext::CalculateWorldSpaceRay(
                AsCameraDesc(*_visCameraSettings), evnt._mousePosition, hitTestContext.GetViewportSize());

            auto intr = hitTestScene.FirstRayIntersection(hitTestContext, worldSpaceRay);
            if (intr._type != 0) {
                _visCameraSettings->_focus = intr._worldSpaceCollision;
            }

            return false;
        }

            // cancel manipulator when the middle mouse button is released
        if (evnt.IsRelease_MButton()) { return false; }

        static auto alt = RenderOverlays::DebuggingDisplay::KeyId_Make("alt");
        static auto shift = RenderOverlays::DebuggingDisplay::KeyId_Make("shift");
        enum ModifierMode
        {
            Translate, Orbit
        };
        ModifierMode modifierMode = evnt.IsHeld(alt) ? Orbit : Translate;

        auto cameraToWorld = MakeCameraToWorld(
            Normalize(_visCameraSettings->_focus -_visCameraSettings->_position),
            Float3(0.f, 0.f, 1.f), _visCameraSettings->_position);
        auto up = ExtractUp_Cam(cameraToWorld);
        auto right = ExtractRight_Cam(cameraToWorld);
        auto forward = ExtractForward_Cam(cameraToWorld);
                
        if (evnt._mouseDelta[0] || evnt._mouseDelta[1]) {
            if (modifierMode == Translate) {

                float distanceToFocus = Magnitude(_visCameraSettings->_focus -_visCameraSettings->_position);
                float speedScale = distanceToFocus * XlTan(0.5f * Deg2Rad(_visCameraSettings->_verticalFieldOfView));

                    //  Translate the camera, but don't change forward direction
                    //  Speed should be related to the distance to the focus point -- so that
                    //  it works ok for both small models and large models.
                Float3 translation
                    =   (speedScale * _translateSpeed *  evnt._mouseDelta[1]) * up
                    +   (speedScale * _translateSpeed * -evnt._mouseDelta[0]) * right;

                _visCameraSettings->_position += translation;
                _visCameraSettings->_focus += translation;

            } else if (modifierMode == Orbit) {

                    //  We're going to orbit around the "focus" point marked in the
                    //  camera settings. Let's assume it's a reasonable point to orbit
                    //  about.
                    //
                    //  We could also attempt to recalculate an orbit point based
                    //  on a collision test against the scene.
                    //
                    //  Let's do the rotation using Spherical coordinates. This allows us
                    //  to clamp the maximum pitch.
                    //

                // Float4 plane = Expand(Float3(_visCameraSettings->_focus - _visCameraSettings->_position), 0.f);
                // float t = RayVsPlane(_visCameraSettings->_position, _visCameraSettings->_focus, plane);

                Float3 orbitCenter = _visCameraSettings->_focus; // _visCameraSettings->_position + t * (_visCameraSettings->_focus - _visCameraSettings->_position);
                auto spherical = CartesianToSpherical(orbitCenter - _visCameraSettings->_position);
                spherical[0] += evnt._mouseDelta[1] * _orbitRotationSpeed;
                spherical[0] = Clamp(spherical[0], gPI * 0.02f, gPI * 0.98f);
                spherical[1] -= evnt._mouseDelta[0] * _orbitRotationSpeed;
                _visCameraSettings->_position = orbitCenter - SphericalToCartesian(spherical);
                _visCameraSettings->_focus = orbitCenter;

            }
        }

        if (evnt._wheelDelta) {
            float distanceToFocus = Magnitude(_visCameraSettings->_focus -_visCameraSettings->_position);

            float speedScale = distanceToFocus * XlTan(0.5f * Deg2Rad(_visCameraSettings->_verticalFieldOfView));
            auto movement = std::min(evnt._wheelDelta * speedScale * _wheelTranslateSpeed, distanceToFocus - 0.1f);

            Float3 translation = movement * forward;
            _visCameraSettings->_position += translation;
        }

        return true;
    }

    void CameraMovementManipulator::Render(
        RenderCore::IThreadContext*, 
        SceneEngine::LightingParserContext&)
    {
        // we could draw some movement widgets here
    }

    const char* CameraMovementManipulator::GetName() const { return "Camera Movement"; }
    std::string CameraMovementManipulator::GetStatusText() const { return std::string(); }

    auto CameraMovementManipulator::GetFloatParameters() const -> std::pair<FloatParameter*, size_t> { return std::make_pair(nullptr, 0); }
    auto CameraMovementManipulator::GetBoolParameters() const -> std::pair<BoolParameter*, size_t> { return std::make_pair(nullptr, 0); }
    void CameraMovementManipulator::SetActivationState(bool) {}

    CameraMovementManipulator::CameraMovementManipulator(
        std::shared_ptr<VisCameraSettings> visCameraSettings)
    : _visCameraSettings(visCameraSettings)
    {
        _translateSpeed = 0.002f;
        _orbitRotationSpeed = .01f * gPI;
        _wheelTranslateSpeed = _translateSpeed;
    }

    CameraMovementManipulator::~CameraMovementManipulator()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    std::shared_ptr<IManipulator> CreateCameraManipulator(
        std::shared_ptr<VisCameraSettings> visCameraSettings)
    {
        return std::make_shared<CameraMovementManipulator>(visCameraSettings);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    bool    ManipulatorStack::OnInputEvent(const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt)
    {
        static auto ctrl = RenderOverlays::DebuggingDisplay::KeyId_Make("control");
        if (evnt.IsPress_MButton() || (evnt.IsHeld(ctrl) && evnt.IsPress_LButton())) {
            auto i = LowerBound(_registeredManipulators, CameraManipulator);
            if (i!=_registeredManipulators.end() && i->first == CameraManipulator) {
                    // remove this manipulator if it already is on the active manipulators list...
                auto e = std::find(_activeManipulators.begin(), _activeManipulators.end(), i->second);
                if (e!=_activeManipulators.end()) { _activeManipulators.erase(e); }

                _activeManipulators.push_back(i->second);
            }
        }

        if (!_activeManipulators.empty()) {
            bool r = _activeManipulators[_activeManipulators.size()-1]->OnInputEvent(
                evnt, *_intrContext, *_intrScene);

            if (!r) { 
                _activeManipulators.erase(_activeManipulators.begin() + (_activeManipulators.size()-1));
            }
        }

        return false;
    }

    void    ManipulatorStack::Register(uint64 id, std::shared_ptr<ToolsRig::IManipulator> manipulator)
    {
        auto i = LowerBound(_registeredManipulators, id);
        if (i!=_registeredManipulators.end() && i->first == id) {
            i->second = manipulator;
        } else {
            _registeredManipulators.insert(i, std::make_pair(id, std::move(manipulator)));
        }
    }

    ManipulatorStack::ManipulatorStack(
        std::shared_ptr<SceneEngine::IntersectionTestContext> intrContext,
        std::shared_ptr<SceneEngine::IntersectionTestScene> intrScene)
    : _intrContext(intrContext), _intrScene(intrScene)
    {}
    ManipulatorStack::~ManipulatorStack()
    {}
    
}

