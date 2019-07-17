// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"

namespace RenderCore { namespace Techniques { class ParsingContext; } }

namespace SceneEngine
{
    class StochasticTransparencyBox;
	class MetricsBox;

    class StochasticTransparencyOp
    {
    public:
        void PrepareFirstPass(const RenderCore::Metal::ShaderResourceView& mainDSV);
        void PrepareSecondPass(RenderCore::Metal::DepthStencilView& mainDSV, const MetricsBox& metricsBox);
        void Resolve();

        StochasticTransparencyOp(
            RenderCore::Metal::DeviceContext& context, 
            RenderCore::Techniques::ParsingContext& parserContext);
        ~StochasticTransparencyOp();

        StochasticTransparencyOp(const StochasticTransparencyOp&) = delete;
        StochasticTransparencyOp& operator=(const StochasticTransparencyOp&) = delete;

    protected:
        StochasticTransparencyBox*				_box;
        RenderCore::Metal::DeviceContext*		_context;
        RenderCore::Techniques::ParsingContext*	_parserContext;
    };
}
