// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/IThreadContext_Forward.h"

namespace SceneEngine
{
    class LightingParserContext;
    class ISceneParser;
    class RenderingQualitySettings;
}

namespace RenderCore { namespace Techniques { class CameraDesc; } }

namespace PlatformRig
{
    void TiledScreenshot(
        RenderCore::IThreadContext& context,
        SceneEngine::LightingParserContext& parserContext,
        SceneEngine::ISceneParser& sceneParser,
        const RenderCore::Techniques::CameraDesc& camera,
        const SceneEngine::RenderingQualitySettings& qualitySettings);
}

