// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Assets/AssetsCore.h"
#include <memory>

namespace SceneEngine { class IScene; }
namespace RenderCore { namespace Techniques { class ITechniqueDelegate; }}
namespace ShaderPatcher { class INodeGraphProvider; }

namespace ToolsRig
{
    class MaterialVisSettings
    {
    public:
        enum class GeometryType { Sphere, Cube, Plane2D };
        GeometryType _geometryType = GeometryType::Sphere;
    };

	::Assets::FuturePtr<SceneEngine::IScene> MakeScene(const MaterialVisSettings& visObject);

	std::unique_ptr<RenderCore::Techniques::ITechniqueDelegate> MakeNodeGraphPreviewDelegate(
		const std::shared_ptr<ShaderPatcher::INodeGraphProvider>& provider,
		const std::string& psMainName);
}
