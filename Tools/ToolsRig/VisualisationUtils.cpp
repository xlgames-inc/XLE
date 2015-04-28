// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "VisualisationUtils.h"
#include "../../SceneEngine/LightDesc.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../Math/Transformations.h"

namespace ToolsRig
{
    RenderCore::Techniques::CameraDesc AsCameraDesc(const VisCameraSettings& camSettings)
    {
        RenderCore::Techniques::CameraDesc result;
        result._cameraToWorld = MakeCameraToWorld(
            Normalize(camSettings._focus - camSettings._position),
            Float3(0.f, 0.f, 1.f), camSettings._position);
        result._farClip = camSettings._farClip;
        result._nearClip = camSettings._nearClip;
        result._verticalFieldOfView = Deg2Rad(camSettings._verticalFieldOfView);
        result._temporaryMatrix = Identity<Float4x4>();
        return result;
    }


    RenderCore::Techniques::CameraDesc  VisSceneParser::GetCameraDesc() const { return AsCameraDesc(*_settings); }
    float VisSceneParser::GetTimeValue() const { return 0.f; }

    VisSceneParser::VisSceneParser(
        std::shared_ptr<VisCameraSettings> settings, 
        const VisEnvSettings& envSettings)
    : _settings(std::move(settings)) 
    , _envSettings(&envSettings)
    {}
    VisSceneParser::~VisSceneParser() {}

    void VisSceneParser::Prepare() {}
    const PlatformRig::EnvironmentSettings& VisSceneParser::GetEnvSettings() const { return _envSettings->_activeSetting; }

    VisEnvSettings::VisEnvSettings()
    {
        _activeSetting = PlatformRig::DefaultEnvironmentSettings();
    }

    VisEnvSettings::~VisEnvSettings() {}
}

