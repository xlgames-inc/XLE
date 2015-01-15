// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Metal/RenderTargetView.h"
#include "../RenderCore/Metal/State.h"

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

        Desc _desc;
        intrusive_ptr<ID3D::Resource> _fragmentIdsTexture;
        intrusive_ptr<ID3D::Resource> _nodeListBuffer;

        RenderCore::Metal::UnorderedAccessView  _fragmentIdsTextureUAV;
        RenderCore::Metal::UnorderedAccessView  _nodeListBufferUAV;
        RenderCore::Metal::ShaderResourceView   _fragmentIdsTextureSRV;
        RenderCore::Metal::ShaderResourceView   _nodeListBufferSRV;
    };

    void OrderIndependentTransparency_ClearAndBind(
        RenderCore::Metal::DeviceContext* context, 
        TransparencyTargetsBox& transparencyTargets, 
        const RenderCore::Metal::ShaderResourceView& depthBufferDupe);
}

