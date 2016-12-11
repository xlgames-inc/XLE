// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../PlatformRig/BasicSceneParser.h"
#include "../../Math/Vector.h"

namespace RenderCore { namespace Techniques { class CameraDesc; } }
namespace SceneEngine { class LightDesc; class GlobalLightingDesc; }

namespace ToolsRig
{
    class VisCameraSettings
    {
    public:
        Float3      _position;
        Float3      _focus;
        float       _nearClip, _farClip;

        enum class Projection { Perspective, Orthogonal };
        Projection  _projection;

        // perspective settings
        float       _verticalFieldOfView;

        // orthogonal settings
        float       _left, _top;
        float       _right, _bottom;

        VisCameraSettings();
    };

    VisCameraSettings AlignCameraToBoundingBox(
        float verticalFieldOfView, 
        const std::pair<Float3, Float3>& boxIn);

    class VisEnvSettings
    {
    public:
        PlatformRig::EnvironmentSettings _activeSetting;
        const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }

        VisEnvSettings();
        VisEnvSettings(StringSection<::Assets::ResChar> filename);
        ~VisEnvSettings();
    protected:
        ::Assets::DepValPtr _depVal;
    };

    RenderCore::Techniques::CameraDesc AsCameraDesc(const VisCameraSettings& camSettings);

    class VisSceneParser : public PlatformRig::BasicSceneParser
    {
    public:
        RenderCore::Techniques::CameraDesc  GetCameraDesc() const;
        float GetTimeValue() const;
        void Prepare();

        VisSceneParser(
            std::shared_ptr<VisCameraSettings> settings, 
            const VisEnvSettings& envSettings);
        ~VisSceneParser();
    protected:
        std::shared_ptr<VisCameraSettings> _settings;
        const VisEnvSettings* _envSettings;
        virtual const PlatformRig::EnvironmentSettings& GetEnvSettings() const;
    };

    inline VisCameraSettings::VisCameraSettings()
    {
        _position = Float3(-10.f, 0.f, 0.f);
        _focus = Zero<Float3>();
        _nearClip = 0.1f;
        _farClip = 1000.f;
        _projection = Projection::Perspective;
        _verticalFieldOfView = 40.f;
        _left = _top = -1.f;
        _right = _bottom = 1.f;
    }
}

