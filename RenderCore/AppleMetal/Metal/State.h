// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Types.h"
#include "../../../Core/Exceptions.h"

namespace RenderCore { namespace Metal_AppleMetal
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

        uint64_t Hash() const;
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
        bool _enableMipmaps = true;
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
    class AttachmentBlendDesc
    {
    public:
        bool _blendEnable = false;
        Blend _srcColorBlendFactor = Blend::One;
        Blend _dstColorBlendFactor = Blend::Zero;
        BlendOp _colorBlendOp = BlendOp::Add;
        Blend _srcAlphaBlendFactor = Blend::One;
        Blend _dstAlphaBlendFactor = Blend::Zero;
        BlendOp _alphaBlendOp = BlendOp::Add;
        ColorWriteMask::BitField _writeMask = ColorWriteMask::All;

        uint64_t Hash() const;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////

    class SamplerState
    {
    public:
        SamplerState(   FilterMode filter,
                        AddressMode addressU = AddressMode::Wrap,
                        AddressMode addressV = AddressMode::Wrap,
                        AddressMode addressW = AddressMode::Wrap,
                        CompareOp comparison = CompareOp::Never,
                        bool enableMipmaps = true);
        SamplerState(); // inexpensive default constructor

        void Apply(DeviceContext& context, bool textureHasMipmaps, unsigned samplerIndex, ShaderStage stage) const never_throws;

        typedef SamplerState UnderlyingType;
        UnderlyingType GetUnderlying() const never_throws { return *this; }

    private:
        class Pimpl;
        std::shared_ptr<Pimpl> _pimpl;
    };

    class BlendState
    {
    public:
        BlendState();
        void Apply() const never_throws;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////

}}

