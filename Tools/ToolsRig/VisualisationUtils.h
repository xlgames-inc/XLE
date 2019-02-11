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

	RenderCore::Techniques::CameraDesc AsCameraDesc(const VisCameraSettings& camSettings);

	class VisEnvSettings
    {
	public:
		std::string _envConfigFile;

		VisEnvSettings();
		VisEnvSettings(const std::string& envConfigFile);
		~VisEnvSettings();
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

