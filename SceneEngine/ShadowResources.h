// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once 

#include "SceneEngineUtils.h"
#include "../RenderCore/Techniques/RenderStateResolver.h"
#include "../RenderCore/Metal/TextureView.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Types.h"
#include <vector>

namespace BufferUploads { class ResourceLocator; }
namespace RenderCore { namespace Techniques { class IRenderStateDelegate; }}

namespace SceneEngine
{
    class ShadowResourcesBox
    {
    public:
        class Desc {};

        RenderCore::Metal::ConstantBuffer       _sampleKernel32;

        ShadowResourcesBox(const Desc& desc);
        ~ShadowResourcesBox();
    };

    class ShadowWriteResources
    {
    public:
        class Desc
        {
        public:
            using RSDepthBias = RenderCore::Techniques::RSDepthBias;
            RSDepthBias     _singleSidedBias;
            RSDepthBias     _doubleSidedBias;
            RenderCore::CullMode	_windingCullMode;

            Desc(   const RSDepthBias& singleSidedBias,
                    const RSDepthBias& doubleSidedBias,
                    RenderCore::CullMode windingCullMode) 
            : _singleSidedBias(singleSidedBias)
            , _doubleSidedBias(doubleSidedBias)
            , _windingCullMode(windingCullMode) {}
        };

        RenderCore::Metal::RasterizerState _rasterizerState;
        std::shared_ptr<RenderCore::Techniques::IRenderStateDelegate> _stateResolver;

        ShadowWriteResources(const Desc& desc);
        ~ShadowWriteResources();
    };
}
