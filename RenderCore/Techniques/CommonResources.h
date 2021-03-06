// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../StateDesc.h"
#include "../IDevice_Forward.h"
#include "../Metal/Metal.h"

#if GFXAPI_TARGET == GFXAPI_DX11
    #include "../Metal/State.h
#endif

namespace RenderCore { namespace Techniques 
{
    class CommonResourceBox
    {
    public:
#if GFXAPI_TARGET == GFXAPI_DX11
        Metal::DepthStencilState _dssReadWrite;
        Metal::DepthStencilState _dssReadOnly;
        Metal::DepthStencilState _dssDisable;
        Metal::DepthStencilState _dssReadWriteWriteStencil;
        Metal::DepthStencilState _dssWriteOnly;

        Metal::BlendState _blendStraightAlpha;
        Metal::BlendState _blendAlphaPremultiplied;
        Metal::BlendState _blendOpaque;
        Metal::BlendState _blendOneSrcAlpha;
        Metal::BlendState _blendAdditive;

        Metal::RasterizerState _defaultRasterizer;
        Metal::RasterizerState _cullDisable;
        Metal::RasterizerState _cullReverse;
        
        Metal::ConstantBuffer _localTransformBuffer;
#endif

        std::shared_ptr<ISampler> _defaultSampler;
        std::shared_ptr<ISampler> _linearWrapSampler;
        std::shared_ptr<ISampler> _linearClampSampler;
        std::shared_ptr<ISampler> _pointClampSampler;

		///////////////////////////////////////

		DepthStencilDesc _dsReadWrite;
        DepthStencilDesc _dsReadOnly;
        DepthStencilDesc _dsDisable;
        DepthStencilDesc _dsReadWriteWriteStencil;
        DepthStencilDesc _dsWriteOnly;

		AttachmentBlendDesc _abStraightAlpha;
		AttachmentBlendDesc _abAlphaPremultiplied;
		AttachmentBlendDesc _abOpaque;
		AttachmentBlendDesc _abOneSrcAlpha;
		AttachmentBlendDesc _abAdditive;

        RasterizationDesc _rsDefault;
        RasterizationDesc _rsCullDisable;
        RasterizationDesc _rsCullReverse;

        CommonResourceBox(IDevice&);
        ~CommonResourceBox();

    private:
        CommonResourceBox(CommonResourceBox&);
        CommonResourceBox& operator=(const CommonResourceBox&);
    };

}}

