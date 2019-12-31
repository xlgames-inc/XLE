// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "State.h"
#include "DeviceContext.h"
#include "ObjectFactory.h"
#include "../../RenderUtils.h"
#include "../../../Utility/IntrusivePtr.h"
#include "../../../Core/Exceptions.h"
#include "DX11Utils.h"
#include <algorithm>

namespace RenderCore { namespace Metal_DX11
{
    
        ////////////////////////////////////////////////////////////////////////////////////////////////

    SamplerState::SamplerState( FilterMode filter,
                                AddressMode addressU, AddressMode addressV, AddressMode addressW,
                                CompareOp comparison)
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

        _underlying = GetObjectFactory().CreateSamplerState(&samplerDesc);
    }

    SamplerState::~SamplerState() {}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    RasterizerState::RasterizerState(CullMode cullmode, bool frontCounterClockwise)
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

        _underlying = GetObjectFactory().CreateRasterizerState(&rasterizerDesc);
    }

    RasterizerState::RasterizerState(
        CullMode cullmode, bool frontCounterClockwise,
        FillMode fillmode,
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
        _underlying = GetObjectFactory().CreateRasterizerState(&rasterizerDesc);
    }

    RasterizerState::RasterizerState(intrusive_ptr<ID3D::RasterizerState>&& moveFrom)
    : _underlying(std::move(moveFrom))
    {}

    RasterizerState::RasterizerState(DeviceContext& context)
    {
        ID3D::RasterizerState* rawptr = nullptr;
        context.GetUnderlying()->RSGetState(&rawptr);
        _underlying = moveptr(rawptr);
    }

    RasterizerState::~RasterizerState() {}

    RasterizerState RasterizerState::Null()
    {
        return RasterizerState(intrusive_ptr<ID3D::RasterizerState>());
    }

        ////////////////////////////////////////////////////////////////////////////////////////////////

    BlendState::BlendState(intrusive_ptr<ID3D::BlendState>&& moveFrom)
    : _underlying(std::move(moveFrom))
    {}

	BlendState::BlendState(const AttachmentBlendDesc& desc)
	{
		D3D11_BLEND_DESC blendStateDesc;
        blendStateDesc.AlphaToCoverageEnable = false;
        blendStateDesc.IndependentBlendEnable = false;
        for (unsigned c=0; c<dimof(blendStateDesc.RenderTarget); ++c) {
			// The values of all these enums happen to match up -- 
            blendStateDesc.RenderTarget[c].BlendEnable = desc._blendEnable;
            blendStateDesc.RenderTarget[c].SrcBlend = (D3D11_BLEND)desc._srcColorBlendFactor;
            blendStateDesc.RenderTarget[c].DestBlend = (D3D11_BLEND)desc._dstColorBlendFactor;
            blendStateDesc.RenderTarget[c].BlendOp = (D3D11_BLEND_OP)desc._colorBlendOp;

            blendStateDesc.RenderTarget[c].SrcBlendAlpha = (D3D11_BLEND)desc._srcAlphaBlendFactor;
            blendStateDesc.RenderTarget[c].DestBlendAlpha = (D3D11_BLEND)desc._dstAlphaBlendFactor;
            blendStateDesc.RenderTarget[c].BlendOpAlpha = (D3D11_BLEND_OP)desc._alphaBlendOp;
            blendStateDesc.RenderTarget[c].RenderTargetWriteMask = (UINT8)desc._colorBlendOp;
        }

        _underlying = GetObjectFactory().CreateBlendState(&blendStateDesc);
	}

    BlendState::BlendState(BlendOp blendingOperation, Blend srcBlend, Blend dstBlend)
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
            blendStateDesc.RenderTarget[c].SrcBlendAlpha = D3D11_BLEND_ONE;
            blendStateDesc.RenderTarget[c].DestBlendAlpha = D3D11_BLEND_ZERO;
            blendStateDesc.RenderTarget[c].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            blendStateDesc.RenderTarget[c].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        }

        _underlying = GetObjectFactory().CreateBlendState(&blendStateDesc);
    }

    BlendState::BlendState( 
        BlendOp blendingOperation, 
        Blend srcBlend,
        Blend dstBlend,
        BlendOp alphaBlendingOperation, 
        Blend alphaSrcBlend,
        Blend alphaDstBlend)
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

            blendStateDesc.RenderTarget[c].SrcBlendAlpha = (D3D11_BLEND)alphaSrcBlend;
            blendStateDesc.RenderTarget[c].DestBlendAlpha = (D3D11_BLEND)alphaDstBlend;
            blendStateDesc.RenderTarget[c].BlendOpAlpha = (D3D11_BLEND_OP)alphaBlendingOperation;

            blendStateDesc.RenderTarget[c].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        }

        _underlying = GetObjectFactory().CreateBlendState(&blendStateDesc);
    }

    BlendState BlendState::OutputDisabled()
    {
        D3D11_BLEND_DESC blendStateDesc;
        blendStateDesc.AlphaToCoverageEnable = false;
        blendStateDesc.IndependentBlendEnable = false;
        for (unsigned c=0; c<dimof(blendStateDesc.RenderTarget); ++c) {
            blendStateDesc.RenderTarget[c].BlendEnable = false;
            blendStateDesc.RenderTarget[c].SrcBlend = D3D11_BLEND_ONE;
            blendStateDesc.RenderTarget[c].DestBlend = D3D11_BLEND_ZERO;
            blendStateDesc.RenderTarget[c].BlendOp = D3D11_BLEND_OP_ADD;
            blendStateDesc.RenderTarget[c].SrcBlendAlpha = D3D11_BLEND_ONE;
            blendStateDesc.RenderTarget[c].DestBlendAlpha = D3D11_BLEND_ZERO;
            blendStateDesc.RenderTarget[c].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            blendStateDesc.RenderTarget[c].RenderTargetWriteMask = 0;
        }

        return BlendState(GetObjectFactory().CreateBlendState(&blendStateDesc));
    }

    BlendState BlendState::Null()
    {
        return BlendState(intrusive_ptr<ID3D::BlendState>());
    }

    BlendState::BlendState(DeviceContext& context)
    {
        ID3D::BlendState* rawptr = nullptr;
        float blendFactor[4]; unsigned sampleMask = 0;
        context.GetUnderlying()->OMGetBlendState(&rawptr, blendFactor, &sampleMask);
        _underlying = moveptr(rawptr);
    }

    BlendState::~BlendState() {}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    DepthStencilState::DepthStencilState(bool enabled, bool writeEnabled, CompareOp comparison)
    {
        D3D11_DEPTH_STENCIL_DESC depthStencilStateDesc;
        depthStencilStateDesc.DepthEnable = enabled;
        depthStencilStateDesc.DepthWriteMask = (enabled&&writeEnabled)?D3D11_DEPTH_WRITE_MASK_ALL:D3D11_DEPTH_WRITE_MASK_ZERO;
        depthStencilStateDesc.DepthFunc = (D3D11_COMPARISON_FUNC)comparison;
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

        _underlying = GetObjectFactory().CreateDepthStencilState(&depthStencilStateDesc);
    }

    StencilMode StencilMode::NoEffect(CompareOp::Always, StencilOp::DontWrite, StencilOp::DontWrite, StencilOp::DontWrite);
    StencilMode StencilMode::AlwaysWrite(CompareOp::Always, StencilOp::Replace, StencilOp::DontWrite, StencilOp::DontWrite);

	DepthStencilState::DepthStencilState(const DepthStencilDesc& desc)
	{
		D3D11_DEPTH_STENCIL_DESC depthStencilStateDesc;
        depthStencilStateDesc.DepthEnable = (desc._depthTest != CompareOp::Always)|desc._depthWrite;
        depthStencilStateDesc.DepthWriteMask = desc._depthWrite?D3D11_DEPTH_WRITE_MASK_ALL:D3D11_DEPTH_WRITE_MASK_ZERO;
        depthStencilStateDesc.DepthFunc = (D3D11_COMPARISON_FUNC)desc._depthTest;
        depthStencilStateDesc.StencilEnable = true;
        depthStencilStateDesc.StencilReadMask = (UINT8)desc._stencilReadMask;
        depthStencilStateDesc.StencilWriteMask = (UINT8)desc._stencilWriteMask;
        depthStencilStateDesc.FrontFace.StencilFailOp = (D3D11_STENCIL_OP)desc._frontFaceStencil._failOp;
        depthStencilStateDesc.FrontFace.StencilDepthFailOp = (D3D11_STENCIL_OP)desc._frontFaceStencil._depthFailOp;
        depthStencilStateDesc.FrontFace.StencilPassOp = (D3D11_STENCIL_OP)desc._frontFaceStencil._passOp;
        depthStencilStateDesc.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)desc._frontFaceStencil._comparisonOp;
        depthStencilStateDesc.BackFace.StencilFailOp = (D3D11_STENCIL_OP)desc._backFaceStencil._failOp;
        depthStencilStateDesc.BackFace.StencilDepthFailOp = (D3D11_STENCIL_OP)desc._backFaceStencil._depthFailOp;
        depthStencilStateDesc.BackFace.StencilPassOp = (D3D11_STENCIL_OP)desc._backFaceStencil._passOp;
        depthStencilStateDesc.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)desc._backFaceStencil._comparisonOp;

        _underlying = GetObjectFactory().CreateDepthStencilState(&depthStencilStateDesc);
	}

    DepthStencilState::DepthStencilState(
        bool depthTestEnabled, bool writeEnabled,
        unsigned stencilReadMask, unsigned stencilWriteMask,
        const StencilDesc& frontFaceStencil,
        const StencilDesc& backFaceStencil)
    {
        D3D11_DEPTH_STENCIL_DESC depthStencilStateDesc;
        depthStencilStateDesc.DepthEnable = depthTestEnabled|writeEnabled;
        depthStencilStateDesc.DepthWriteMask = writeEnabled?D3D11_DEPTH_WRITE_MASK_ALL:D3D11_DEPTH_WRITE_MASK_ZERO;
        depthStencilStateDesc.DepthFunc = depthTestEnabled?D3D11_COMPARISON_LESS_EQUAL:D3D11_COMPARISON_ALWAYS;
        depthStencilStateDesc.StencilEnable = true;
        depthStencilStateDesc.StencilReadMask = (UINT8)stencilReadMask;
        depthStencilStateDesc.StencilWriteMask = (UINT8)stencilWriteMask;
        depthStencilStateDesc.FrontFace.StencilFailOp = (D3D11_STENCIL_OP)frontFaceStencil._failOp;
        depthStencilStateDesc.FrontFace.StencilDepthFailOp = (D3D11_STENCIL_OP)frontFaceStencil._depthFailOp;
        depthStencilStateDesc.FrontFace.StencilPassOp = (D3D11_STENCIL_OP)frontFaceStencil._passOp;
        depthStencilStateDesc.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)frontFaceStencil._comparisonOp;
        depthStencilStateDesc.BackFace.StencilFailOp = (D3D11_STENCIL_OP)backFaceStencil._failOp;
        depthStencilStateDesc.BackFace.StencilDepthFailOp = (D3D11_STENCIL_OP)backFaceStencil._depthFailOp;
        depthStencilStateDesc.BackFace.StencilPassOp = (D3D11_STENCIL_OP)backFaceStencil._passOp;
        depthStencilStateDesc.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)backFaceStencil._comparisonOp;

        _underlying = GetObjectFactory().CreateDepthStencilState(&depthStencilStateDesc);
    }

    DepthStencilState::DepthStencilState(DeviceContext& context)
    {
        ID3D::DepthStencilState* rawdss = nullptr; UINT originalStencil = 0;
        context.GetUnderlying()->OMGetDepthStencilState(&rawdss, &originalStencil);
        _underlying = moveptr(rawdss);
    }

    DepthStencilState::~DepthStencilState() {}


	uint64_t DepthStencilDesc::Hash() const
    {
        assert((unsigned(_depthTest) & ~0xfu) == 0);
        assert((unsigned(_stencilReadMask) & ~0xffu) == 0);
        assert((unsigned(_stencilWriteMask) & ~0xffu) == 0);
        assert((unsigned(_stencilReference) & ~0xffu) == 0);
        assert((unsigned(_frontFaceStencil._passOp) & ~0xfu) == 0);
        assert((unsigned(_frontFaceStencil._failOp) & ~0xfu) == 0);
        assert((unsigned(_frontFaceStencil._depthFailOp) & ~0xfu) == 0);
        assert((unsigned(_frontFaceStencil._comparisonOp) & ~0xfu) == 0);
        assert((unsigned(_backFaceStencil._passOp) & ~0xfu) == 0);
        assert((unsigned(_backFaceStencil._failOp) & ~0xfu) == 0);
        assert((unsigned(_backFaceStencil._depthFailOp) & ~0xfu) == 0);
        assert((unsigned(_backFaceStencil._comparisonOp) & ~0xfu) == 0);

        return  ((uint64_t(_depthTest) & 0xf) << 0ull)

            |   ((uint64_t(_frontFaceStencil._passOp) & 0xf) << 4ull)
            |   ((uint64_t(_frontFaceStencil._failOp) & 0xf) << 8ull)
            |   ((uint64_t(_frontFaceStencil._depthFailOp) & 0xf) << 12ull)
            |   ((uint64_t(_frontFaceStencil._comparisonOp) & 0xf) << 16ull)

            |   ((uint64_t(_backFaceStencil._passOp) & 0xf) << 20ull)
            |   ((uint64_t(_backFaceStencil._failOp) & 0xf) << 24ull)
            |   ((uint64_t(_backFaceStencil._depthFailOp) & 0xf) << 28ull)
            |   ((uint64_t(_backFaceStencil._comparisonOp) & 0xf) << 32ull)

            |   ((uint64_t(_stencilReadMask) & 0xf) << 36ull)
            |   ((uint64_t(_stencilWriteMask) & 0xf) << 44ull)
            |   ((uint64_t(_stencilReference) & 0xf) << 52ull)      // todo -- remove stencil reference

            |   ((uint64_t(_depthWrite) & 0x1) << 60ull)
            |   ((uint64_t(_stencilEnable) & 0x1) << 61ull)
            ;

    }

    uint64_t AttachmentBlendDesc::Hash() const
    {
        // Note that we're checking that each element fits in 4 bits, and then space them out
        // to give each 8 bits (well, there's room for expansion)
        assert((unsigned(_srcColorBlendFactor) & ~0xfu) == 0);
        assert((unsigned(_dstColorBlendFactor) & ~0xfu) == 0);
        assert((unsigned(_colorBlendOp) & ~0xfu) == 0);
        assert((unsigned(_srcAlphaBlendFactor) & ~0xfu) == 0);
        assert((unsigned(_dstAlphaBlendFactor) & ~0xfu) == 0);
        assert((unsigned(_alphaBlendOp) & ~0xfu) == 0);
        assert((unsigned(_writeMask) & ~0xfu) == 0);
        return  ((uint64_t(_blendEnable) & 1) << 0ull)

            |   ((uint64_t(_srcColorBlendFactor) & 0xf) << 8ull)
            |   ((uint64_t(_dstColorBlendFactor) & 0xf) << 16ull)
            |   ((uint64_t(_colorBlendOp) & 0xf) << 24ull)

            |   ((uint64_t(_srcAlphaBlendFactor) & 0xf) << 32ull)
            |   ((uint64_t(_dstAlphaBlendFactor) & 0xf) << 40ull)
            |   ((uint64_t(_alphaBlendOp) & 0xf) << 48ull)

            |   ((uint64_t(_writeMask) & 0xf) << 56ull)
            ;
    }

	uint64_t RasterizationDesc::Hash() const
	{
		assert((unsigned(_cullMode) & ~0xff) == 0);
        assert((unsigned(_frontFaceWinding) & ~0xff) == 0);
		return uint64_t(_cullMode) | (uint64_t(_frontFaceWinding) << 8ull);
	}

}}

