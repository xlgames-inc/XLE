// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/IThreadContext_Forward.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Metal/Forward.h"
#include <functional>

namespace RenderCore { namespace Techniques 
{
    class CameraDesc; class ProjectionDesc; 
    class TechniqueContext;
    class TechniqueInterface;
}}

namespace Assets { namespace Exceptions { class InvalidAsset; class PendingAsset; } }

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

    class LightingParserContext : public RenderCore::Techniques::ParsingContext
    {
    public:

            //  ----------------- Global states -----------------
        MetricsBox*     GetMetricsBox()                     { return _metricsBox; }
        void            SetMetricsBox(MetricsBox* box);
        ISceneParser*   GetSceneParser()                    { return _sceneParser; }

            //  ----------------- Working shadow state ----------------- 
        std::vector<std::pair<LightId, PreparedDMShadowFrustum>>    _preparedDMShadows;
        std::vector<std::pair<LightId, PreparedRTShadowFrustum>>    _preparedRTShadows;

            //  ----------------- Overlays for late rendering -----------------
        typedef std::function<void(RenderCore::Metal::DeviceContext*, LightingParserContext&)> PendingOverlay;
        std::vector<PendingOverlay> _pendingOverlays;

            //  ----------------- Plugins -----------------
        std::vector<std::shared_ptr<ILightingParserPlugin>> _plugins;

        LightingParserContext(const RenderCore::Techniques::TechniqueContext& techniqueContext);
        ~LightingParserContext();

    private:
        MetricsBox*                         _metricsBox;
        ISceneParser*                       _sceneParser;

        void SetSceneParser(ISceneParser* sceneParser);
        friend void LightingParser_ExecuteScene(
            RenderCore::IThreadContext&, LightingParserContext&, ISceneParser&, const RenderingQualitySettings&);
    };
}

