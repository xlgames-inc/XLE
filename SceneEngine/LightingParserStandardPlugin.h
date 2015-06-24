// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "LightingParser.h"

namespace SceneEngine
{
    class LightingParserStandardPlugin : public ILightingParserPlugin
    {
    public:
        virtual void OnPreScenePrepare(
            RenderCore::Metal::DeviceContext* context, LightingParserContext&) const;
        virtual void OnLightingResolvePrepare(
            RenderCore::Metal::DeviceContext* context, 
            LightingParserContext& parserContext,
            LightingResolveContext& resolveContext) const;
        virtual void OnPostSceneRender(
            RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext, 
            const SceneParseSettings& parseSettings, unsigned techniqueIndex) const;
    };
    
}

