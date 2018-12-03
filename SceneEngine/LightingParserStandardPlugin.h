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
            RenderCore::IThreadContext&, RenderCore::Techniques::ParsingContext&, LightingParserContext&) const override;
        virtual void OnLightingResolvePrepare(
            RenderCore::IThreadContext&, RenderCore::Techniques::ParsingContext&,  LightingParserContext& parserContext,
			LightingResolveContext& resolveContext) const override;
        virtual void OnPostSceneRender(
            RenderCore::IThreadContext&, RenderCore::Techniques::ParsingContext&, LightingParserContext& parserContext, 
			BatchFilter filter, unsigned techniqueIndex) const override;
        virtual void InitBasicLightEnvironment(
            RenderCore::IThreadContext&, RenderCore::Techniques::ParsingContext&, LightingParserContext&, 
			ShaderLightDesc::BasicEnvironment& env) const override;
    };
    
}

