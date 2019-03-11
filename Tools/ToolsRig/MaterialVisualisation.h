// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Assets/AssetsCore.h"

namespace SceneEngine { class IScene; }

namespace ToolsRig
{
    class MaterialVisSettings
    {
    public:
        enum class GeometryType { Sphere, Cube, Plane2D };
        GeometryType _geometryType = GeometryType::Sphere;
    };

	::Assets::FuturePtr<SceneEngine::IScene> MakeScene(const MaterialVisSettings& visObject);
}
