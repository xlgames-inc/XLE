// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderCore/IThreadContext_Forward.h"

namespace GUILayer { class NativeEngineDevice; }
namespace ToolsRig { class IManipulator; class VisCameraSettings; }
namespace SceneEngine { class LightingParserContext; }
namespace RenderCore { namespace Techniques { class ProjectionDesc; class CameraDesc; } }

#pragma make_public(GUILayer::NativeEngineDevice)
#pragma make_public(ToolsRig::IManipulator)
#pragma make_public(ToolsRig::VisCameraSettings)
#pragma make_public(SceneEngine::LightingParserContext)
#pragma make_public(RenderCore::Techniques::ProjectionDesc)
#pragma make_public(RenderCore::Techniques::CameraDesc)
#pragma make_public(RenderCore::IThreadContext)

