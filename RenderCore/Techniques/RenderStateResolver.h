// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../StateDesc.h"
#include "../../Core/Types.h"
#include <memory>

namespace Utility { class ParameterBox; }
namespace RenderCore { namespace Assets { class RenderStateSet; } }

namespace RenderCore { namespace Techniques
{
    class CompiledRenderStateSet;

    class IRenderStateDelegate
    {
    public:
        /// <summary>Given the current global state settings and a technique, build the low-level states for draw call<summary>
        /// There are only 3 influences on render states while rendering models:
        /// <list>
        ///     <item>Local states set on the draw call object
        ///     <item>The global state settings (eg, perhaps set by the lighting parser)
        ///     <item>The technique index/guid (ie, the type of rendering being performed)
        /// </list>
        /// These should be combined together to generate the low level state objects.
        virtual CompiledRenderStateSet Resolve(
            const Assets::RenderStateSet& states, 
            unsigned techniqueIndex) = 0;
        virtual uint64 GetHash() = 0;
        virtual ~IRenderStateDelegate();
    };

    std::shared_ptr<IRenderStateDelegate> CreateRenderStateDelegate_Default();
    std::shared_ptr<IRenderStateDelegate> CreateRenderStateDelegate_Forward();
    std::shared_ptr<IRenderStateDelegate> CreateRenderStateDelegate_Deferred();

    struct RSDepthBias
    {
    public:
        int _depthBias; float _depthBiasClamp; float _slopeScaledBias;
        RSDepthBias(int depthBias=0, float depthBiasClamp=0, float slopeScaledBias=0.f)
            : _depthBias(depthBias), _depthBiasClamp(depthBiasClamp), _slopeScaledBias(slopeScaledBias) {}
    };
    std::shared_ptr<IRenderStateDelegate> CreateRenderStateDelegate_DepthOnly(
        const RSDepthBias& singleSidedBias = RSDepthBias(),
        const RSDepthBias& doubleSidedBias = RSDepthBias(),
        CullMode cullMode = CullMode::Back);
    
}}

