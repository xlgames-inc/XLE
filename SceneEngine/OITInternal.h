// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Metal/RenderTargetView.h"
#include "../RenderCore/Metal/State.h"

namespace BufferUploads { class ResourceLocator; }

namespace SceneEngine
{
    class TransparencyTargetsBox
    {
    public:
        class Desc
        {
        public:
            unsigned _width, _height;
            bool _storeColour;
            bool _checkInfiniteLoops;
            Desc(   unsigned width, unsigned height, 
                    bool storeColour, bool checkInfiniteLoops);
        };

        TransparencyTargetsBox(const Desc& desc);
        ~TransparencyTargetsBox();

        using ResLocator = intrusive_ptr<BufferUploads::ResourceLocator>;
        using UAV = RenderCore::Metal::UnorderedAccessView;
        using SRV = RenderCore::Metal::ShaderResourceView;
        using RTV = RenderCore::Metal::RenderTargetView;

        Desc        _desc;
        bool        _pendingInitialClear;

        ResLocator  _fragmentIdsTexture;
        ResLocator  _nodeListBuffer;
        UAV         _fragmentIdsTextureUAV;
        UAV         _nodeListBufferUAV;
        SRV         _fragmentIdsTextureSRV;
        SRV         _nodeListBufferSRV;
        
        ResLocator  _infiniteLoopTexture;
        RTV         _infiniteLoopRTV;
        SRV         _infiniteLoopSRV;
    };

    void OrderIndependentTransparency_ClearAndBind(
        RenderCore::Metal::DeviceContext& context, 
        TransparencyTargetsBox& transparencyTargets, 
        const RenderCore::Metal::ShaderResourceView& depthBufferDupe);
}

