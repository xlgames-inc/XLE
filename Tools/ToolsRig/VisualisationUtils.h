// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

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

    RenderCore::Techniques::CameraDesc AsCameraDesc(const VisCameraSettings& camSettings);

    SceneEngine::LightDesc DefaultDominantLight();
    SceneEngine::GlobalLightingDesc DefaultGlobalLightingDesc();
}

