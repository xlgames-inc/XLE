// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/ResourceDesc.h"
#include "../RenderCore/Metal/Forward.h"
#include "../Math/Vector.h"
#include <vector>
#include <memory>

namespace RenderCore { namespace Techniques { class ParsingContext; }}
namespace RenderCore { class IThreadContext; class IResource; }
namespace RenderCore { class TextureViewDesc; }

namespace SceneEngine
{
    class MetricsBox;
    class PreparedDMShadowFrustum;
    class PreparedRTShadowFrustum;
	class ILightingParserDelegate;
    class ILightingParserPlugin;
	class RenderSceneSettings;
	class PreparedScene;
	class MainTargets;

    using LightId = unsigned;

	class MainTargets
    {
    public:
		std::vector<std::pair<uint64_t, unsigned>> _namedTargetsMapping;
		UInt2 _dimensions;
		unsigned _samplingCount;

        using SRV = RenderCore::Metal::ShaderResourceView;
        SRV      GetSRV(RenderCore::Techniques::ParsingContext& context, uint64_t semantic, const RenderCore::TextureViewDesc& window = {}) const;
		std::shared_ptr<RenderCore::IResource> GetResource(RenderCore::Techniques::ParsingContext& context, uint64_t semantic) const;

		UInt2		GetDimensions() const;
		unsigned    GetSamplingCount() const;

		MainTargets();
		~MainTargets();
    };

    class LightingParserContext
    {
    public:
		const ILightingParserDelegate*	_delegate = nullptr;
		PreparedScene*				_preparedScene = nullptr;
		unsigned					_sampleCount = 0;
		unsigned					_gbufferType = 0;

		const MainTargets&	GetMainTargets() const { return _mainTargets; }
		MainTargets			_mainTargets;

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
        MetricsBox*			_metricsBox = nullptr;
    };
}

