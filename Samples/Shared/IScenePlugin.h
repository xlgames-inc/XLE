// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderCore/IThreadContext_Forward.h"
#include "../../RenderCore/Metal/Forward.h"

namespace SceneEngine
{
    class LightingParserContext;
    class SceneParseSettings;
}

namespace Sample
{
    /// <summary>Plugin for scene features</summary>
    /// Allows derived classes to load, prepare and render scene elements,
    /// (like effects and special case objects, etc)
    class IScenePlugin
    {
    public:
        virtual void LoadingPhase() = 0;
        virtual void PrepareFrame(
            RenderCore::IThreadContext& threadContext,
            SceneEngine::LightingParserContext& parserContext) = 0;
        virtual void ExecuteScene(
            RenderCore::Metal::DeviceContext* context, 
            SceneEngine::LightingParserContext& parserContext, 
            const SceneEngine::SceneParseSettings& parseSettings,
            unsigned techniqueIndex) const = 0;
        virtual bool HasContent(const SceneEngine::SceneParseSettings& parseSettings) const = 0;
        virtual ~IScenePlugin();
    };
}

