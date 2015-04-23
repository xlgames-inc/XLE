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

    SceneEngine::LightDesc DefaultDominantLight()
    {
        SceneEngine::LightDesc light;
        light._type = SceneEngine::LightDesc::Directional;
        light._negativeLightDirection = Normalize(Float3(-.1f, 0.33f, 1.f));
        light._radius = 10000.f;
        light._shadowFrustumIndex = ~unsigned(0x0);
        return light;
    }

    SceneEngine::GlobalLightingDesc DefaultGlobalLightingDesc()
    {
        SceneEngine::GlobalLightingDesc result;
        result._ambientLight = 5.f * Float3(0.25f, 0.25f, 0.25f);
        result._doAtmosphereBlur = false;
        result._doOcean = false;
        result._doToneMap = false;
        return result;
    }
}

