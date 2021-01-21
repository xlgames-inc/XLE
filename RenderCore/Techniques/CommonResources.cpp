// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CommonResources.h"
#include "TechniqueUtils.h" // just for sizeof(LocalTransformConstants)
#include "../Metal/ObjectFactory.h"
#include "../../ConsoleRig/ResourceBox.h"

namespace RenderCore { namespace Techniques
{
    CommonResourceBox::CommonResourceBox(const Desc&)
    {
        using namespace RenderCore::Metal;
#if GFXAPI_TARGET == GFXAPI_DX11
        _dssReadWrite = DepthStencilState();
        _dssReadOnly = DepthStencilState(true, false);
        _dssDisable = DepthStencilState(false, false);
        _dssReadWriteWriteStencil = DepthStencilState(true, true, 0xff, 0xff, StencilMode::AlwaysWrite, StencilMode::AlwaysWrite);
        _dssWriteOnly = DepthStencilState(true, true, CompareOp::Always);

        _blendStraightAlpha = BlendState(BlendOp::Add, Blend::SrcAlpha, Blend::InvSrcAlpha);
        _blendAlphaPremultiplied = BlendState(BlendOp::Add, Blend::One, Blend::InvSrcAlpha);
        _blendOneSrcAlpha = BlendState(BlendOp::Add, Blend::One, Blend::SrcAlpha);
        _blendAdditive = BlendState(BlendOp::Add, Blend::One, Blend::One);
        _blendOpaque = BlendOp::NoBlending;

        _defaultRasterizer = CullMode::Back;
        _cullDisable = CullMode::None;
        _cullReverse = RasterizerState(CullMode::Back, false);

        _localTransformBuffer = MakeConstantBuffer(GetObjectFactory(), sizeof(LocalTransformConstants));
#endif

        _linearClampSampler = SamplerState(FilterMode::Trilinear, AddressMode::Wrap, AddressMode::Wrap);
        _linearClampSampler = SamplerState(FilterMode::Trilinear, AddressMode::Clamp, AddressMode::Clamp);
        _pointClampSampler = SamplerState(FilterMode::Point, AddressMode::Clamp, AddressMode::Clamp);

		_dsReadWrite = DepthStencilDesc {};
		_dsReadOnly = DepthStencilDesc { CompareOp::LessEqual, false };
        _dsDisable = DepthStencilDesc { CompareOp::Always, false };
        _dsReadWriteWriteStencil = DepthStencilDesc { CompareOp::LessEqual, true, true, 0xff, 0xff, 0xff, StencilDesc::AlwaysWrite, StencilDesc::AlwaysWrite };
		_dsWriteOnly = DepthStencilDesc { CompareOp::Always, true };

		_abStraightAlpha = AttachmentBlendDesc { true, Blend::SrcAlpha, Blend::InvSrcAlpha, BlendOp::Add };
		_abAlphaPremultiplied = AttachmentBlendDesc { true, Blend::One, Blend::InvSrcAlpha, BlendOp::Add };
		_abOneSrcAlpha = AttachmentBlendDesc { true, Blend::One, Blend::SrcAlpha, BlendOp::Add };
		_abAdditive = AttachmentBlendDesc { true, Blend::One, Blend::One, BlendOp::Add };
		_abOpaque = AttachmentBlendDesc { };

        _rsDefault = RasterizationDesc { CullMode::Back };
        _rsCullDisable = RasterizationDesc { CullMode::None };
        _rsCullReverse = RasterizationDesc { CullMode::Back, FaceWinding::CW };
    }

    CommonResourceBox& CommonResources()
    {
        return ConsoleRig::FindCachedBox<CommonResourceBox>(CommonResourceBox::Desc());
    }
}}
