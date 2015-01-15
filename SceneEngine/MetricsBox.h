// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Metal/RenderTargetView.h"
#include "../RenderCore/DX11/Metal/DX11Utils.h"

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

        intrusive_ptr<ID3D::Resource>              _metricsBufferResource;
        RenderCore::Metal::UnorderedAccessView  _metricsBufferUAV;
        RenderCore::Metal::ShaderResourceView   _metricsBufferSRV;
    };
}
