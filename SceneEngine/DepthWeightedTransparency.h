// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"

namespace RenderCore { namespace Techniques { class ParsingContext; }}

namespace SceneEngine
{
    class DepthWeightedTransparencyBox;

    class DepthWeightedTransparencyOp
    {
    public:
        void PrepareFirstPass(const RenderCore::Metal::DepthStencilView* dsv);
        void Resolve();

        DepthWeightedTransparencyOp(
            RenderCore::Metal::DeviceContext& context, 
            RenderCore::Techniques::ParsingContext& parserContext);
        ~DepthWeightedTransparencyOp();

        DepthWeightedTransparencyOp(const DepthWeightedTransparencyOp&) = delete;
        DepthWeightedTransparencyOp& operator=(const DepthWeightedTransparencyOp&) = delete;
    protected:
        DepthWeightedTransparencyBox*			_box;
        RenderCore::Metal::DeviceContext*		_context;
        RenderCore::Techniques::ParsingContext*	_parserContext;
    };
}

