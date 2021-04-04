// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CommonResources.h"
#include "../IDevice.h"
#include "../Metal/Metal.h"

#if GFXAPI_TARGET == GFXAPI_DX11
    #include "TechniqueUtils.h" // just for sizeof(LocalTransformConstants)
    #include "../Metal/ObjectFactory.h"
#endif

namespace RenderCore { namespace Techniques
{
    DepthStencilDesc CommonResourceBox::s_dsReadWrite {};
    DepthStencilDesc CommonResourceBox::s_dsReadOnly { CompareOp::LessEqual, false };
    DepthStencilDesc CommonResourceBox::s_dsDisable { CompareOp::Always, false };
    DepthStencilDesc CommonResourceBox::s_dsReadWriteWriteStencil { CompareOp::LessEqual, true, true, 0xff, 0xff, 0xff, StencilDesc::AlwaysWrite, StencilDesc::AlwaysWrite };
    DepthStencilDesc CommonResourceBox::s_dsWriteOnly { CompareOp::Always, true };

    AttachmentBlendDesc CommonResourceBox::s_abStraightAlpha { true, Blend::SrcAlpha, Blend::InvSrcAlpha, BlendOp::Add };
    AttachmentBlendDesc CommonResourceBox::s_abAlphaPremultiplied { true, Blend::One, Blend::InvSrcAlpha, BlendOp::Add };
    AttachmentBlendDesc CommonResourceBox::s_abOneSrcAlpha { true, Blend::One, Blend::SrcAlpha, BlendOp::Add };
    AttachmentBlendDesc CommonResourceBox::s_abAdditive { true, Blend::One, Blend::One, BlendOp::Add };
    AttachmentBlendDesc CommonResourceBox::s_abOpaque { };

    RasterizationDesc CommonResourceBox::s_rsDefault { CullMode::Back };
    RasterizationDesc CommonResourceBox::s_rsCullDisable { CullMode::None };
    RasterizationDesc CommonResourceBox::s_rsCullReverse { CullMode::Back, FaceWinding::CW };

    CommonResourceBox::CommonResourceBox(IDevice& device)
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

        _linearClampSampler = device.CreateSampler(SamplerDesc{FilterMode::Trilinear, AddressMode::Wrap, AddressMode::Wrap});
        _linearWrapSampler = device.CreateSampler(SamplerDesc{FilterMode::Trilinear, AddressMode::Clamp, AddressMode::Clamp});
        _pointClampSampler = device.CreateSampler(SamplerDesc{FilterMode::Point, AddressMode::Clamp, AddressMode::Clamp});
        _defaultSampler = _linearWrapSampler;
    }

    CommonResourceBox::~CommonResourceBox()
    {}
}}
