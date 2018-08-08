// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "VisualisationUtils.h"
#include "../../SceneEngine/LightDesc.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../Assets/ConfigFileContainer.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/Assets.h"
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
        result._left = camSettings._left;
        result._top = camSettings._top;
        result._right = camSettings._right;
        result._bottom = camSettings._bottom;
        result._projection = 
            (camSettings._projection == VisCameraSettings::Projection::Orthogonal)
             ? RenderCore::Techniques::CameraDesc::Projection::Orthogonal
             : RenderCore::Techniques::CameraDesc::Projection::Perspective;
        assert(std::isfinite(result._cameraToWorld(0,0)) && !std::isnan(result._cameraToWorld(0,0)));
        return result;
    }

    VisCameraSettings AlignCameraToBoundingBox(
        float verticalFieldOfView, 
        const std::pair<Float3, Float3>& boxIn)
    {
        auto box = boxIn;

            // convert empty/inverted boxes into something rational...
        if (    box.first[0] >= box.second[0] 
            ||  box.first[1] >= box.second[1] 
            ||  box.first[2] >= box.second[2]) {
            box.first = Float3(-10.f, -10.f, -10.f);
            box.second = Float3( 10.f,  10.f,  10.f);
        }

        const float border = 0.0f;
        Float3 position = .5f * (box.first + box.second);

            // push back to attempt to fill the viewport with the bounding box
        float verticalHalfDimension = .5f * box.second[2] - box.first[2];
        position[0] = box.first[0] - (verticalHalfDimension * (1.f + border)) / XlTan(.5f * Deg2Rad(verticalFieldOfView));

        VisCameraSettings result;
        result._position = position;
        result._focus = .5f * (box.first + box.second);
        result._verticalFieldOfView = verticalFieldOfView;
        result._farClip = 1.25f * (box.second[0] - position[0]);
        result._nearClip = result._farClip / 10000.f;
        return result;
    }


    RenderCore::Techniques::CameraDesc  VisSceneParser::GetCameraDesc() const { return AsCameraDesc(*_settings); }
    float VisSceneParser::GetTimeValue() const { return 0.f; }

    VisSceneParser::VisSceneParser(
        const std::shared_ptr<VisCameraSettings>& settings, 
        const std::shared_ptr<VisEnvSettings>& envSettings)
    : _settings(settings) 
    , _envSettings(envSettings)
    {}
    VisSceneParser::~VisSceneParser() {}

    void VisSceneParser::Prepare() {}
    const PlatformRig::EnvironmentSettings& VisSceneParser::GetEnvSettings() const { return _envSettings->_activeSetting; }

    VisEnvSettings::VisEnvSettings()
    {
        _activeSetting = PlatformRig::DefaultEnvironmentSettings();
        _depVal = std::make_shared<::Assets::DependencyValidation>();
    }

    VisEnvSettings::VisEnvSettings(StringSection<::Assets::ResChar> filename)
    {
        TRY {
            _activeSetting = ::Assets::GetAssetDep<PlatformRig::EnvironmentSettings>(filename);
            _depVal = std::make_shared<::Assets::DependencyValidation>();
            ::Assets::RegisterFileDependency(_depVal, filename);
        } CATCH (const ::Assets::Exceptions::InvalidAsset&) {
            _activeSetting = PlatformRig::DefaultEnvironmentSettings();
        } CATCH_END
    }

    VisEnvSettings::~VisEnvSettings() {}
}

