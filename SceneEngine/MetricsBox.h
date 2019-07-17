// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/TextureView.h"
#include "../Assets/AssetsCore.h"

namespace RenderCore { namespace Techniques { class ParsingContext; } }

namespace SceneEngine
{
    class MetricsBox
    {
    public:
        class Desc
        {
        public:
            Desc() {}
        };

        MetricsBox(const Desc& desc);
        ~MetricsBox();

        RenderCore::Metal::UnorderedAccessView  _metricsBufferUAV;
        RenderCore::Metal::ShaderResourceView   _metricsBufferSRV;
    };

    void RenderGPUMetrics(
        RenderCore::Metal::DeviceContext& context,
        RenderCore::Techniques::ParsingContext& parsingContext,
		const MetricsBox& metricBox,
        const ::Assets::ResChar shaderName[],
        std::initializer_list<const ::Assets::ResChar*> valueSources,
        unsigned protectStates = ~0u);       // ProtectState::States::BitField
}
