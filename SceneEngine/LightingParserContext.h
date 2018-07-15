// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "PreparedScene.h"
#include <functional>

namespace RenderCore { namespace Techniques 
{
    class CameraDesc; class ProjectionDesc; 
    class TechniqueContext;
    class TechniqueInterface;
	class ParsingContext;
}}

namespace RenderCore { class IThreadContext; }

namespace SceneEngine
{
    class MetricsBox;
    class ISceneParser;
    class PreparedDMShadowFrustum;
    class PreparedRTShadowFrustum;
    class ShadowProjectionConstants;
    class ILightingParserPlugin;
    class RenderingQualitySettings;

    using LightId = unsigned;

    class LightingParserContext
    {
    public:

            //  ----------------- Global states -----------------
        MetricsBox*     GetMetricsBox()                     { return _metricsBox; }
        void            SetMetricsBox(MetricsBox* box);
        ISceneParser*   GetSceneParser()                    { return _sceneParser; }
		const ISceneParser*   GetSceneParser() const		{ return _sceneParser; }

            //  ----------------- Working shadow state ----------------- 
        std::vector<std::pair<LightId, PreparedDMShadowFrustum>>    _preparedDMShadows;
        std::vector<std::pair<LightId, PreparedRTShadowFrustum>>    _preparedRTShadows;

            //  ----------------- Plugins -----------------
        std::vector<std::shared_ptr<ILightingParserPlugin>> _plugins;

        void Reset();

		LightingParserContext();
		~LightingParserContext();

    private:
        MetricsBox*         _metricsBox = nullptr;
        ISceneParser*       _sceneParser = nullptr;

        friend class AttachedSceneMarker;
        AttachedSceneMarker SetSceneParser(ISceneParser* sceneParser);
        friend AttachedSceneMarker LightingParser_SetupScene(
            RenderCore::IThreadContext&, RenderCore::Techniques::ParsingContext&, LightingParserContext&, 
            ISceneParser*, unsigned, unsigned);
    };

    class AttachedSceneMarker
    {
    public:
        PreparedScene& GetPreparedScene() { return _preparedScene; }

        AttachedSceneMarker() : _parserContext(nullptr) {}
        AttachedSceneMarker(AttachedSceneMarker&& moveFrom) never_throws
        : _parserContext(moveFrom._parserContext)
        {
            moveFrom._parserContext = nullptr; 
            moveFrom._preparedScene = std::move(moveFrom._preparedScene); 
        }
        const AttachedSceneMarker& operator=(AttachedSceneMarker&& moveFrom) never_throws
        {
            _parserContext = moveFrom._parserContext;
            moveFrom._parserContext = nullptr;
            moveFrom._preparedScene = std::move(moveFrom._preparedScene); 
        }
        ~AttachedSceneMarker() { if (_parserContext) _parserContext->_sceneParser = nullptr; }

        AttachedSceneMarker(const AttachedSceneMarker&) = delete;
        const AttachedSceneMarker& operator=(const AttachedSceneMarker&) = delete;
    private:
        AttachedSceneMarker(LightingParserContext& parserContext) : _parserContext(&parserContext) {}
        LightingParserContext*  _parserContext;
        PreparedScene           _preparedScene;

        friend class LightingParserContext;
    };
}

