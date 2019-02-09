// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ObjectFactory.h"
#include "../../Types.h"
#include "../../../Core/Exceptions.h"

namespace RenderCore { namespace Metal_OpenGLES
{
    class DeviceContext;

////////////////////////////////////////////////////////////////////////////////////////////////////

    /// Equivalent to MTLStencilDescriptor or D3D12_DEPTH_STENCILOP_DESC or VkStencilOpState
    /// Note that OpenGLES2 & Vulkan allow for separate readmask/writemask/reference values per
    /// face, but DX & Metal do not.
    class StencilDesc
    {
    public:
        StencilOp       _passOp = StencilOp::Keep;        ///< pass stencil & depth tests
        StencilOp       _failOp = StencilOp::Keep;        ///< fail stencil test
        StencilOp       _depthFailOp = StencilOp::Keep;   ///< pass stencil but fail depth tests
        CompareOp       _comparisonOp = CompareOp::Always;
    };

    /// Equivalent to MTLDepthStencilDescriptor or D3D12_DEPTH_STENCIL_DESC or VkPipelineDepthStencilStateCreateInfo
    class DepthStencilDesc
    {
    public:
        CompareOp       _depthTest = CompareOp::LessEqual;
        bool            _depthWrite = true;
        bool            _stencilEnable = false;
        uint8_t         _stencilReadMask = 0x0;
        uint8_t         _stencilWriteMask = 0x0;
        uint8_t         _stencilReference = 0x0;
        StencilDesc     _frontFaceStencil;
        StencilDesc     _backFaceStencil;
    };

    /// Similar to VkPipelineRasterizationStateCreateInfo or D3D12_RASTERIZER_DESC
    /// (Metal just has separate function calls)
    class RasterizationDesc
    {
    public:
        CullMode        _cullMode = CullMode::Back;
        FaceWinding     _frontFaceWinding = FaceWinding::CCW;
    };

    /// Similar to ?
    class SamplerStateDesc
    {
    public:
        RenderCore::FilterMode _filter = RenderCore::FilterMode::Trilinear;
        RenderCore::AddressMode _addressU = RenderCore::AddressMode::Wrap;
        RenderCore::AddressMode _addressV = RenderCore::AddressMode::Wrap;
        RenderCore::CompareOp _comparison = RenderCore::CompareOp::Never;
    };

    namespace ColorWriteMask
    {
        enum Channels
        {
            Red     = (1<<0),
            Green   = (1<<1),
            Blue    = (1<<2),
            Alpha   = (1<<3)
        };
        using BitField = unsigned;

        const BitField All = (Red | Green | Blue | Alpha);
        const BitField None = 0;
    };

    /**
     * Similar to MTLRenderPipelineColorAttachmentDescriptor or D3D12_RENDER_TARGET_BLEND_DESC or VkPipelineColorBlendAttachmentState
     */
    class AttachmentBlendDesc {
    public:
        bool _blendEnable;
        RenderCore::Blend _srcColorBlendFactor;
        RenderCore::Blend _dstColorBlendFactor;
        RenderCore::BlendOp _colorBlendOp;
        RenderCore::Blend _srcAlphaBlendFactor;
        RenderCore::Blend _dstAlphaBlendFactor;
        RenderCore::BlendOp _alphaBlendOp;
        ColorWriteMask::BitField _writeMask;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////

    class Resource;
    class CapturedStates;

    class SamplerState
    {
    public:
        SamplerState(   FilterMode filter,
                        AddressMode addressU = AddressMode::Wrap,
                        AddressMode addressV = AddressMode::Wrap,
                        AddressMode addressW = AddressMode::Wrap,
                        CompareOp comparison = CompareOp::Never);
        SamplerState();

        void Apply(
            CapturedStates& capture,
            unsigned textureUnit, unsigned bindingTarget,
            const Resource* res,
            bool enableMipmaps) const never_throws;

        void Apply(unsigned textureUnit, unsigned bindingTarget, bool enableMipmaps) const never_throws;

        typedef SamplerState UnderlyingType;
        UnderlyingType GetUnderlying() const never_throws { return *this; }

    private:
        unsigned _minFilter, _maxFilter;
        unsigned _wrapS, _wrapT, _wrapR;
        unsigned _compareMode, _compareFunc;

        intrusive_ptr<OpenGL::Sampler> _prebuiltSamplerMipmaps;
        intrusive_ptr<OpenGL::Sampler> _prebuiltSamplerNoMipmaps;

        unsigned _guid;
        bool _gles300Factory;
    };

    class BlendState
    {
    public:
        BlendState();
        void Apply() const never_throws;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////

    class ViewportDesc
    {
    public:
            // (naming convention as per D3D11_VIEWPORT)
        float TopLeftX, TopLeftY;
        float Width, Height;
        float MinDepth, MaxDepth;

        ViewportDesc(DeviceContext&);
        ViewportDesc(float topLeftX=0.f, float topLeftY=0.f, float width=0.f, float height=0.f, float minDepth=0.f, float maxDepth=1.f)
        : TopLeftX(topLeftX), TopLeftY(topLeftY)
        , Width(width), Height(height)
        , MinDepth(minDepth), MaxDepth(maxDepth)
        {}
    };
}}

