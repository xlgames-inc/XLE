// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Math/Vector.h"
#include "../Utility/IteratorUtils.h"
#include <string>

namespace SceneEngine
{
    class IScene;
    class CompiledSceneTechnique;
}

namespace RenderCore { class IThreadContext; }
namespace RenderCore { enum class Format; namespace Techniques { class ParsingContext; class CameraDesc; } }

namespace PlatformRig
{
    void TiledScreenshot(
        RenderCore::IThreadContext& context,
		RenderCore::Techniques::ParsingContext& parserContext,
        SceneEngine::IScene& sceneParser,
        const RenderCore::Techniques::CameraDesc& camera,
        const SceneEngine::CompiledSceneTechnique& qualitySettings,
        UInt2 sampleCount);

    void SaveImage(
        const std::string& destinationFile,
        const void* imageData,
        VectorPattern<unsigned,2> dimensions,
        unsigned rowPitch,
        RenderCore::Format format);
}

