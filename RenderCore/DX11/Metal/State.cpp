// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "State.h"
#include "DeviceContext.h"
#include "../../RenderUtils.h"
#include "../../../Utility/IntrusivePtr.h"
#include "../../../Core/Exceptions.h"
#include "DX11Utils.h"
#include <algorithm>

namespace RenderCore { namespace Metal_DX11
{
    
        ////////////////////////////////////////////////////////////////////////////////////////////////

    SamplerState::SamplerState( FilterMode::Enum filter,
                                AddressMode::Enum addressU, AddressMode::Enum addressV, AddressMode::Enum addressW,
                                Comparison::Enum comparison)
    {
        D3D11_SAMPLER_DESC samplerDesc;
        switch (filter) {
        case FilterMode::Point:                 samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; break;
        case FilterMode::Anisotropic:           samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC; break;
        case FilterMode::ComparisonBilinear:    samplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT; break;
        case FilterMode::Bilinear:              samplerDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT; break;
        case FilterMode::Trilinear:             samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; break;
        default:
            assert(0);
            samplerDesc.Filter = (D3D11_FILTER)filter;      // enums are equivalent, anyway
        }
        samplerDesc.AddressU = (D3D11_TEXTURE_ADDRESS_MODE)addressU;
        samplerDesc.AddressV = (D3D11_TEXTURE_ADDRESS_MODE)addressV;
        samplerDesc.AddressW = (D3D11_TEXTURE_ADDRESS_MODE)addressW;
        samplerDesc.MipLODBias = 0.f;
        samplerDesc.MaxAnisotropy = 16;
        samplerDesc.ComparisonFunc = (D3D11_COMPARISON_FUNC)comparison;
        std::fill(samplerDesc.BorderColor, &samplerDesc.BorderColor[dimof(samplerDesc.BorderColor)], 1.f);
        samplerDesc.MinLOD = 0.f;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

        _underlying = ObjectFactory().CreateSamplerState(&samplerDesc);
    }

    SamplerState::~SamplerState() {}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    RasterizerState::RasterizerState(RasterizerState&& moveFrom)
    : _underlying(std::move(moveFrom._underlying))
    {}

    RasterizerState& RasterizerState::operator=(RasterizerState&& moveFrom)
    {
        _underlying = std::move(moveFrom._underlying);
        return *this;
    }

    RasterizerState::RasterizerState(const RasterizerState& copyFrom)
    : _underlying(copyFrom._underlying)
    {}

    RasterizerState& RasterizerState::operator=(const RasterizerState& copyFrom)
    {
        _underlying = copyFrom._underlying;
        return *this;
    }

    RasterizerState::RasterizerState(CullMode::Enum cullmode, bool frontCounterClockwise)
    {
        D3D11_RASTERIZER_DESC rasterizerDesc;
        rasterizerDesc.FillMode = D3D11_FILL_SOLID;
        rasterizerDesc.CullMode = (D3D11_CULL_MODE)cullmode;
        rasterizerDesc.FrontCounterClockwise = frontCounterClockwise;
        rasterizerDesc.DepthBias = 0;
        rasterizerDesc.DepthBiasClamp = 0.f;
        rasterizerDesc.SlopeScaledDepthBias = 0.f;
        rasterizerDesc.DepthClipEnable = true;          // (note this defaults to true -- and cannot be disabled on some feature levels)
        rasterizerDesc.ScissorEnable = false;
        rasterizerDesc.MultisampleEnable = true;
        rasterizerDesc.AntialiasedLineEnable = false;

        _underlying = ObjectFactory().CreateRasterizerState(&rasterizerDesc);
    }

    RasterizerState::RasterizerState(
        CullMode::Enum cullmode, bool frontCounterClockwise,
        FillMode::Enum fillmode,
        int depthBias, float depthBiasClamp, float slopeScaledBias)
    {
        D3D11_RASTERIZER_DESC rasterizerDesc;
        rasterizerDesc.FillMode = (D3D11_FILL_MODE)fillmode;
        rasterizerDesc.CullMode = (D3D11_CULL_MODE)cullmode;
        rasterizerDesc.FrontCounterClockwise = frontCounterClockwise;
        rasterizerDesc.DepthBias = depthBias;
        rasterizerDesc.DepthBiasClamp = depthBiasClamp;
        rasterizerDesc.SlopeScaledDepthBias = slopeScaledBias;
        rasterizerDesc.DepthClipEnable = true;          // (note this defaults to true -- and cannot be disabled on some feature levels)
        rasterizerDesc.ScissorEnable = false;
        rasterizerDesc.MultisampleEnable = true;
        rasterizerDesc.AntialiasedLineEnable = false;
        _underlying = ObjectFactory().CreateRasterizerState(&rasterizerDesc);
    }

    RasterizerState::~RasterizerState() {}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    BlendState::BlendState(BlendState&& moveFrom)
    : _underlying(std::move(moveFrom._underlying))
    {}

    BlendState& BlendState::operator=(BlendState&& moveFrom)
    {
        _underlying = std::move(moveFrom._underlying);
        return *this;
    }

    BlendState::BlendState(const BlendState& copyFrom)
    : _underlying(copyFrom._underlying)
    {}

    BlendState& BlendState::operator=(const BlendState& copyFrom)
    {
        _underlying = copyFrom._underlying;
        return *this;
    }

    BlendState::BlendState(BlendOp::Enum blendingOperation, Blend::Enum srcBlend, Blend::Enum dstBlend, bool terrainLayer)
    {
        D3D11_BLEND_DESC blendStateDesc;
        blendStateDesc.AlphaToCoverageEnable = false;
        blendStateDesc.IndependentBlendEnable = false;
        for (unsigned c=0; c<dimof(blendStateDesc.RenderTarget); ++c) {
            if (blendingOperation == BlendOp::NoBlending) {
                blendStateDesc.RenderTarget[c].BlendEnable = false;
                blendStateDesc.RenderTarget[c].SrcBlend = D3D11_BLEND_ONE;
                blendStateDesc.RenderTarget[c].DestBlend = D3D11_BLEND_ZERO;
                blendStateDesc.RenderTarget[c].BlendOp = D3D11_BLEND_OP_ADD;
            } else {
                blendStateDesc.RenderTarget[c].BlendEnable = true;
                blendStateDesc.RenderTarget[c].SrcBlend = (D3D11_BLEND)srcBlend;
                blendStateDesc.RenderTarget[c].DestBlend = (D3D11_BLEND)dstBlend;
                blendStateDesc.RenderTarget[c].BlendOp = (D3D11_BLEND_OP)blendingOperation;
            }
            if (!terrainLayer) {
                blendStateDesc.RenderTarget[c].SrcBlendAlpha = D3D11_BLEND_ONE;
                blendStateDesc.RenderTarget[c].DestBlendAlpha = D3D11_BLEND_ZERO;
            } else {
                blendStateDesc.RenderTarget[c].SrcBlendAlpha = D3D11_BLEND_ZERO;
                blendStateDesc.RenderTarget[c].DestBlendAlpha = D3D11_BLEND_ONE;    // hack -- when drawing terrain layer, we need to leave the alpha channel unchanged
            }
            blendStateDesc.RenderTarget[c].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            blendStateDesc.RenderTarget[c].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        }

        _underlying = ObjectFactory().CreateBlendState(&blendStateDesc);
    }

    BlendState::~BlendState() {}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    DepthStencilState::DepthStencilState(bool enabled, bool writeEnabled)
    {
        D3D11_DEPTH_STENCIL_DESC depthStencilStateDesc;
        depthStencilStateDesc.DepthEnable = enabled;
        depthStencilStateDesc.DepthWriteMask = (enabled&&writeEnabled)?D3D11_DEPTH_WRITE_MASK_ALL:D3D11_DEPTH_WRITE_MASK_ZERO;
        depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        depthStencilStateDesc.StencilEnable = false;
        depthStencilStateDesc.StencilReadMask = 0x0;
        depthStencilStateDesc.StencilWriteMask = 0x0;
        depthStencilStateDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        depthStencilStateDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        depthStencilStateDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        depthStencilStateDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        depthStencilStateDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        depthStencilStateDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        depthStencilStateDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        depthStencilStateDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

        _underlying = ObjectFactory().CreateDepthStencilState(&depthStencilStateDesc);
    }

    StencilMode StencilMode::NoEffect(Comparison::Always, StencilOp::DontWrite, StencilOp::DontWrite, StencilOp::DontWrite);
    StencilMode StencilMode::AlwaysWrite(Comparison::Always, StencilOp::Replace, StencilOp::DontWrite, StencilOp::DontWrite);

    DepthStencilState::DepthStencilState(
        bool depthTestEnabled, bool writeEnabled,
        unsigned stencilReadMask, unsigned stencilWriteMask,
        const StencilMode& frontFaceStencil,
        const StencilMode& backFaceStencil)
    {
        D3D11_DEPTH_STENCIL_DESC depthStencilStateDesc;
        depthStencilStateDesc.DepthEnable = depthTestEnabled|writeEnabled;
        depthStencilStateDesc.DepthWriteMask = writeEnabled?D3D11_DEPTH_WRITE_MASK_ALL:D3D11_DEPTH_WRITE_MASK_ZERO;
        depthStencilStateDesc.DepthFunc = depthTestEnabled?D3D11_COMPARISON_LESS_EQUAL:D3D11_COMPARISON_ALWAYS;
        depthStencilStateDesc.StencilEnable = true;
        depthStencilStateDesc.StencilReadMask = (UINT8)stencilReadMask;
        depthStencilStateDesc.StencilWriteMask = (UINT8)stencilWriteMask;
        depthStencilStateDesc.FrontFace.StencilFailOp = (D3D11_STENCIL_OP)frontFaceStencil._onStencilFail;
        depthStencilStateDesc.FrontFace.StencilDepthFailOp = (D3D11_STENCIL_OP)frontFaceStencil._onDepthFail;
        depthStencilStateDesc.FrontFace.StencilPassOp = (D3D11_STENCIL_OP)frontFaceStencil._onPass;
        depthStencilStateDesc.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)frontFaceStencil._comparison;
        depthStencilStateDesc.BackFace.StencilFailOp = (D3D11_STENCIL_OP)backFaceStencil._onStencilFail;
        depthStencilStateDesc.BackFace.StencilDepthFailOp = (D3D11_STENCIL_OP)backFaceStencil._onDepthFail;
        depthStencilStateDesc.BackFace.StencilPassOp = (D3D11_STENCIL_OP)backFaceStencil._onPass;
        depthStencilStateDesc.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)backFaceStencil._comparison;

        _underlying = ObjectFactory().CreateDepthStencilState(&depthStencilStateDesc);
    }

    DepthStencilState::DepthStencilState(DepthStencilState&& moveFrom)
    : _underlying(std::move(moveFrom._underlying))
    {}

    DepthStencilState& DepthStencilState::operator=(DepthStencilState&& moveFrom)
    {
        _underlying = std::move(moveFrom._underlying);
        return *this;
    }

    DepthStencilState::~DepthStencilState() {}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    static_assert(  offsetof(ViewportDesc, TopLeftX) == offsetof(D3D11_VIEWPORT, TopLeftX)
                &&  offsetof(ViewportDesc, TopLeftY) == offsetof(D3D11_VIEWPORT, TopLeftY)
                &&  offsetof(ViewportDesc, Width) == offsetof(D3D11_VIEWPORT, Width)
                &&  offsetof(ViewportDesc, Height) == offsetof(D3D11_VIEWPORT, Height)
                &&  offsetof(ViewportDesc, MinDepth) == offsetof(D3D11_VIEWPORT, MinDepth)
                &&  offsetof(ViewportDesc, MaxDepth) == offsetof(D3D11_VIEWPORT, MaxDepth),
                "ViewportDesc is no longer compatible with D3D11_VIEWPORT");
    ViewportDesc::ViewportDesc(ID3D::DeviceContext* context)
    {
        if (context) {
            UINT viewportsToGet = 1;
            context->RSGetViewports(&viewportsToGet, (D3D11_VIEWPORT*)this);
        }
    }

    ViewportDesc::ViewportDesc(const DeviceContext& context)
    {
        UINT viewportsToGet = 1;
        context.GetUnderlying()->RSGetViewports(&viewportsToGet, (D3D11_VIEWPORT*)this);
    }


}}

