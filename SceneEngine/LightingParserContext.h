// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Metal/Forward.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../Math/Matrix.h"
#include "../Utility/MemoryUtils.h"
#include <functional>

namespace RenderCore { class CameraDesc; class ProjectionDesc; }
namespace Assets { namespace Exceptions { class InvalidResource; class PendingResource; } }

namespace SceneEngine
{
    class MetricsBox;
    class ISceneParser;
    class PreparedShadowFrustum;
    class TechniqueContext;
    class TechniqueInterface;
    class ShadowProjectionConstants;
    class ILightingParserPlugin;

    class LightingParserContext : public RenderCore::Techniques::ParsingContext
    {
    public:

            //  ----------------- Global states -----------------
        MetricsBox*     GetMetricsBox()                     { return _metricsBox; }
        void            SetMetricsBox(MetricsBox* box);
        ISceneParser*   GetSceneParser()                    { return _sceneParser; }

            //  ----------------- Working shadow state ----------------- 
        std::vector<PreparedShadowFrustum>     _preparedShadows;

            //  ----------------- Overlays for late rendering -----------------
        typedef std::function<void(RenderCore::Metal::DeviceContext*, LightingParserContext&)> PendingOverlay;
        std::vector<PendingOverlay> _pendingOverlays;

            //  ----------------- Plugins -----------------
        std::vector<std::shared_ptr<ILightingParserPlugin>> _plugins;

        LightingParserContext(ISceneParser* sceneParser, const TechniqueContext& techniqueContext);
        ~LightingParserContext();

    private:
        MetricsBox*                         _metricsBox;
        ISceneParser*                       _sceneParser;
    };
}

