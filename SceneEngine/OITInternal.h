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
            Desc(unsigned width, unsigned height, bool storeColour);
            unsigned _width, _height;
            bool _storeColour;
        };

        TransparencyTargetsBox(const Desc& desc);
        ~TransparencyTargetsBox();

        using ResLocator = intrusive_ptr<BufferUploads::ResourceLocator>;
        using UAV = RenderCore::Metal::UnorderedAccessView;
        using SRV = RenderCore::Metal::ShaderResourceView;

        Desc        _desc;
        ResLocator  _fragmentIdsTexture;
        ResLocator  _nodeListBuffer;
        UAV         _fragmentIdsTextureUAV;
        UAV         _nodeListBufferUAV;
        SRV         _fragmentIdsTextureSRV;
        SRV         _nodeListBufferSRV;
    };

    void OrderIndependentTransparency_ClearAndBind(
        RenderCore::Metal::DeviceContext& context, 
        TransparencyTargetsBox& transparencyTargets, 
        const RenderCore::Metal::ShaderResourceView& depthBufferDupe);
}

