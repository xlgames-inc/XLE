// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderCore/IThreadContext_Forward.h"

namespace GUILayer { class NativeEngineDevice; class RenderTargetWrapper; }
namespace ToolsRig { class IManipulator; class VisCameraSettings; }
namespace SceneEngine { class LightingParserContext; class IntersectionTestScene; class PlacementsEditor; }
namespace RenderCore { namespace Techniques { class ProjectionDesc; class CameraDesc; class ParsingContext; } }
namespace PlatformRig { class InputSnapshot; }
namespace Assets { class DirectorySearchRules; }
namespace ConsoleRig { class IProgress; class GlobalServices; }

#pragma make_public(GUILayer::RenderTargetWrapper)
#pragma make_public(GUILayer::NativeEngineDevice)
#pragma make_public(ToolsRig::IManipulator)
#pragma make_public(ToolsRig::VisCameraSettings)
#pragma make_public(SceneEngine::LightingParserContext)
#pragma make_public(SceneEngine::IntersectionTestScene)
#pragma make_public(SceneEngine::PlacementsEditor)
#pragma make_public(RenderCore::Techniques::ProjectionDesc)
#pragma make_public(RenderCore::Techniques::CameraDesc)
#pragma make_public(RenderCore::Techniques::ParsingContext)
#pragma make_public(RenderCore::IThreadContext)
#pragma make_public(PlatformRig::InputSnapshot)
#pragma make_public(Assets::DirectorySearchRules)
#pragma make_public(ConsoleRig::IProgress)
#pragma make_public(ConsoleRig::GlobalServices)
