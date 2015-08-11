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
        Float3  _position;
        Float3  _focus;
        float   _verticalFieldOfView;
        float   _nearClip, _farClip;

        VisCameraSettings();
    };

    class VisEnvSettings
    {
    public:
        PlatformRig::EnvironmentSettings _activeSetting;

        VisEnvSettings();
        VisEnvSettings(const ::Assets::ResChar filename[]);
        ~VisEnvSettings();
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
        _position = Zero<Float3>();
        _focus = Zero<Float3>();
        _verticalFieldOfView = 40.f;
        _nearClip = 0.1f;
        _farClip = 1000.f;
    }
}

