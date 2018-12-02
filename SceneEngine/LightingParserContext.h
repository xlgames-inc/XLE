// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "PreparedScene.h"
#include <functional>

namespace RenderCore { namespace Techniques { class ParsingContext; }}
namespace RenderCore { class IThreadContext; }

namespace SceneEngine
{
    class MetricsBox;
    class PreparedDMShadowFrustum;
    class PreparedRTShadowFrustum;
	class ILightingParserDelegate;
    class ILightingParserPlugin;
	class RenderSceneSettings;
	class PreparedScene;

    using LightId = unsigned;

    class LightingParserContext
    {
    public:
		ILightingParserDelegate*	_delegate = nullptr;
		PreparedScene*				_preparedScene = nullptr;
		unsigned					_sampleCount;

            //  ----------------- Global states -----------------
        MetricsBox*     GetMetricsBox()                     { return _metricsBox; }
        void            SetMetricsBox(MetricsBox* box);

            //  ----------------- Working shadow state ----------------- 
        std::vector<std::pair<LightId, PreparedDMShadowFrustum>>    _preparedDMShadows;
        std::vector<std::pair<LightId, PreparedRTShadowFrustum>>    _preparedRTShadows;

            //  ----------------- Plugins -----------------
        std::vector<std::shared_ptr<ILightingParserPlugin>> _plugins;

        void Reset();

		LightingParserContext();
		~LightingParserContext();
		LightingParserContext(LightingParserContext&& moveFrom);
		LightingParserContext& operator=(LightingParserContext&& moveFrom);

    private:
        MetricsBox*         _metricsBox = nullptr;
    };
}

