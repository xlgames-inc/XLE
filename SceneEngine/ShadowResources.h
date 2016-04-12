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
#include <vector>

namespace BufferUploads { class ResourceLocator; }
namespace RenderCore { namespace Techniques { class IStateSetResolver; }}

namespace SceneEngine
{
    class ShadowTargetsBox
    {
    public:
        class Desc
        {
        public:
            unsigned        _width;
            unsigned        _height;
            unsigned        _targetCount;
            FormatStack     _formats;
            Desc(unsigned width, unsigned height, unsigned targetCount, const FormatStack& format) 
                : _width(width), _height(height), _targetCount(targetCount), _formats(format) {}
        };

        using SRV = RenderCore::Metal::ShaderResourceView;
        using DSV = RenderCore::Metal::DepthStencilView;
        using ResLocator = intrusive_ptr<BufferUploads::ResourceLocator>;
        SRV _shaderResource;
        DSV _depthStencilView;
        std::vector<DSV> _dsvBySlice;
        ResLocator _shadowTexture;

        ShadowTargetsBox(const Desc& desc);
        ~ShadowTargetsBox();
    };

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
            unsigned        _windingCullMode;

            Desc(   const RSDepthBias& singleSidedBias,
                    const RSDepthBias& doubleSidedBias,
                    unsigned windingCullMode) 
            : _singleSidedBias(singleSidedBias)
            , _doubleSidedBias(doubleSidedBias)
            , _windingCullMode(windingCullMode) {}
        };

        RenderCore::Metal::RasterizerState _rasterizerState;
        std::shared_ptr<RenderCore::Techniques::IStateSetResolver> _stateResolver;

        ShadowWriteResources(const Desc& desc);
        ~ShadowWriteResources();
    };
}
