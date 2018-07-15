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
            RenderCore::IThreadContext&, RenderCore::Techniques::ParsingContext&, LightingParserContext&, ISceneParser&, PreparedScene&) const;
        virtual void OnLightingResolvePrepare(
            RenderCore::IThreadContext& context, 
			RenderCore::Techniques::ParsingContext&, 
            LightingParserContext& parserContext,
			ISceneParser&, LightingResolveContext& resolveContext) const;
        virtual void OnPostSceneRender(
            RenderCore::IThreadContext& context, RenderCore::Techniques::ParsingContext&, LightingParserContext& parserContext, 
			ISceneParser&, const SceneParseSettings& parseSettings, unsigned techniqueIndex) const;
        virtual void InitBasicLightEnvironment(
            RenderCore::IThreadContext&, RenderCore::Techniques::ParsingContext&, LightingParserContext&, 
			ISceneParser&, ShaderLightDesc::BasicEnvironment& env) const;
    };
    
}

