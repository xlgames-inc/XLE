// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "VulkanCore.h"
#include "IncludeVulkan.h"
#include <utility>

namespace RenderCore { namespace Metal_Vulkan
{
    class DeviceContext;

    static const unsigned s_mrtLimit = 4;

        ////////////////////////////////////////////////////////////////////////////////////////////////

    /// Container for AddressMode::Enum
    namespace AddressMode
    {
        /// <summary>Texture address modes</summary>
        ///
        ///     These are used to determine how the texture sampler
        ///     reads texture data outside of the [0, 1] range.
        ///     Normally Wrap and Clamp are used.
        ///     <seealso cref="SamplerState"/>
        enum Enum
        {
            Wrap = 1,   // D3D11_TEXTURE_ADDRESS_WRAP
            Mirror = 2, // D3D11_TEXTURE_ADDRESS_MIRROR
            Clamp = 3,  // D3D11_TEXTURE_ADDRESS_CLAMP
            Border = 4  // D3D11_TEXTURE_ADDRESS_BORDER
        };
    }

    /// Container for FilterMode::Enum
    namespace FilterMode
    {
        /// <summary>Texture filtering modes</summary>
        ///
        ///     These are used when sampling a texture at a floating
        ///     point address. In other words, when sampling at a
        ///     midway point between texels, how do we filter the 
        ///     surrounding texels?
        ///     <seealso cref="SamplerState"/>
        enum Enum
        {
            Point = 0,                  // D3D11_FILTER_MIN_MAG_MIP_POINT
            Trilinear = 0x15,           // D3D11_FILTER_MIN_MAG_MIP_LINEAR
            Anisotropic = 0x55,         // D3D11_FILTER_ANISOTROPIC
            Bilinear = 0x14,            // D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT
            ComparisonBilinear = 0x94   // D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT
        };
    }

    namespace Comparison
    {
        enum Enum 
        {
            Never = 1,          // D3D11_COMPARISON_NEVER
            Less = 2,           // D3D11_COMPARISON_LESS
            Equal = 3,          // D3D11_COMPARISON_EQUAL
            LessEqual = 4,      // D3D11_COMPARISON_LESS_EQUAL
            Greater = 5,        // D3D11_COMPARISON_GREATER
            NotEqual = 6,       // D3D11_COMPARISON_NOT_EQUAL
            GreaterEqual = 7,   // D3D11_COMPARISON_GREATER_EQUAL
            Always = 8          // D3D11_COMPARISON_ALWAYS
        };
    }

    /// <summary>States for sampling from textures</summary>
    /// 
    ///     Sets states that are used by the hardware when sampling from textures. Most useful
    ///     states are:
    ///         <list>
    ///              <item> filtering mode (FilterMode::Enum)
    ///                      Set to point / linear / trilinear / anisotrophic (etc)
    ///              <item> addressing mode (AddressMode::Enum)
    ///                      Wrapping / mirroring / clamping on texture edges
    ///         </list>
    ///
    ///     Currently the following D3D states are not exposed by this interface:
    ///         <list>
    ///             <item> MipLODBias
    ///             <item> MaxAnisotrophy
    ///             <item> ComparisonFunc
    ///             <item> BorderColor
    ///             <item> MinLOD
    ///             <item> MaxLOD
    ///         </list>
    ///
    ///     It's best to limit the total number of SamplerState objects used by
    ///     the system. In an ideal game, there should be D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT (16)
    ///     sampler states or less. Then, they could all be bound at the start of the frame, and
    ///     avoid any further state thrashing.
    /// 
    /// <exception cref="::RenderCore::Exceptions::AllocationFailure">
    ///     Failed to create underlying object. Could be caused by invalid input values, or a corrupt/lost device.
    /// </exception>
    class SamplerState
    {
    public:
        SamplerState(   FilterMode::Enum filter = FilterMode::Trilinear,
                        AddressMode::Enum addressU = AddressMode::Wrap, 
                        AddressMode::Enum addressV = AddressMode::Wrap, 
                        AddressMode::Enum addressW = AddressMode::Wrap,
						Comparison::Enum comparison = Comparison::Never);
		~SamplerState();

        using UnderlyingType = VkSampler;
        VkSampler GetUnderlying() const { return _sampler.get(); }

    private:
        VulkanSharedPtr<VkSampler> _sampler;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    /// Container for CullMode::Enum
    namespace CullMode
    {
        /// <summary>Back face culling mode</summary>
        ///
        ///     Used to determine which side of a triangle to cull.
        ///
        ///     Note that there is another flag the in rasteriser state
        ///     that determines which side of a triangle is the "back"
        ///     (ie, clockwise or counterclockwise order). 
        ///     Only use the "Front" option if you really want to cull
        ///     the front facing triangles (useful for some effects)
        ///     <seealso cref="RasterizerState"/>
        enum Enum
        {
            None = 1,   // D3D11_CULL_NONE,
            Front = 2,  // D3D11_CULL_FRONT,
            Back = 3    // D3D11_CULL_BACK
        };
    };

    /// Container for FillMode::Enum
    namespace FillMode
    {
        enum Enum
        {
            Solid = 3,      // D3D11_FILL_SOLID
            Wireframe = 2   // D3D11_FILL_WIREFRAME
        };
    };

    /// <summary>States used while rasterising triangles</summary>
    ///
    ///     These states are used during triangle setup for interpolator
    ///     initialising and culling.
    ///
    ///     Common states:
    ///         <list>
    ///              <item> cull mode (CullMode::Enum)
    ///                     this is the most commonly used state
    ///         </list>
    ///
    ///     D3D states not exposed by this interface:
    ///         <list>
    ///             <item> FillMode
    ///             <item> FrontCounterClockwise
    ///             <item> DepthBias
    ///             <item> DepthBiasClamp
    ///             <item> SlopeScaledDepthBias
    ///             <item> DepthClipEnable
    ///             <item> ScissorEnable
    ///             <item> MultisampleEnable        (defaults to true)
    ///             <item> AntialiasedLineEnable
    ///         </list>
    ///
    /// <exception cref="::RenderCore::Exceptions::AllocationFailure">
    ///     Failed to create underlying object. Could be caused by invalid input values, or a corrupt/lost device.
    /// </exception>
    class RasterizerState : public VkPipelineRasterizationStateCreateInfo
    {
    public:
        RasterizerState(CullMode::Enum cullmode = CullMode::Back, bool frontCounterClockwise = true);
        RasterizerState(
            CullMode::Enum cullmode, bool frontCounterClockwise,
            FillMode::Enum fillmode,
            int depthBias, float depthBiasClamp, float slopeScaledBias);

		static RasterizerState Null() { return RasterizerState(); }

        using UnderlyingType = const RasterizerState*;
        UnderlyingType GetUnderlying() const { return this; }
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    /// Container for Blend::Enum
    namespace Blend
    {
        /// <summary>Settings used for describing a blend state</summary>
        ///
        ///     The blend operation takes the form:
        ///         out colour = Operation(Param1 * (Source colour), Param2 * (Destination colour))
        ///         out alpha = Operation(Param1 * (Source alpha), Param2 * (Destination alpha))
        ///
        ///     Where "Operation" is typically addition.
        ///
        ///     This enum is used for "Param1" and "Param2"
        ///     <seealso cref="BlendOp::Enum"/>
        ///     <seealso cref="BlendState"/>
        enum Enum
        {
            Zero = 1, // D3D11_BLEND_ZERO,
            One = 2, // D3D11_BLEND_ONE,

            SrcColor = 3, // D3D11_BLEND_SRC_COLOR,
            InvSrcColor = 4, // D3D11_BLEND_INV_SRC_COLOR,
            DestColor = 9, // D3D11_BLEND_DEST_COLOR,
            InvDestColor = 10, // D3D11_BLEND_INV_DEST_COLOR,

            SrcAlpha = 5, // D3D11_BLEND_SRC_ALPHA,
            InvSrcAlpha = 6, // D3D11_BLEND_INV_SRC_ALPHA,
            DestAlpha = 7, // D3D11_BLEND_DEST_ALPHA,
            InvDestAlpha = 8 // D3D11_BLEND_INV_DEST_ALPHA
        };
    };

    /// Container for BlendOp::Enum
    namespace BlendOp
    {
        /// <summary>Settings used for describing a blend state</summary>
        ///
        ///     The blend operation takes the form:
        ///         out colour = Operation(Param1 * (Source colour), Param2 * (Destination colour))
        ///         out alpha = Operation(Param1 * (Source alpha), Param2 * (Destination alpha))
        ///
        ///     This enum is used for "Operation"
        ///     <seealso cref="BlendOp::Enum"/>
        ///     <seealso cref="BlendState"/>
        enum Enum
        {
            NoBlending,
            Add = 1, // D3D11_BLEND_OP_ADD,
            Subtract = 2, // D3D11_BLEND_OP_SUBTRACT,
            RevSubtract = 3, // D3D11_BLEND_OP_REV_SUBTRACT,
            Min = 4, // D3D11_BLEND_OP_MIN,
            Max = 5 // D3D11_BLEND_OP_MAX
        };
    }

    /// <summary>States used while drawing pixels to the render targets</summary>
    ///
    ///     These states are used to blend the pixel shader output with
    ///     the existing colours in the render targets.
    ///
    ///     In D3D11, we can define separate blending modes for each render target
    ///     (and separate blending for colour and alpha parts).
    ///
    ///     However, most of the time we want to just use the same blend mode
    ///     for all render targets. So, the simple interface will smear the
    ///     given blend mode across all render target settings.
    ///
    ///     BlendState can be constructed with just "BlendOp::NoBlending" if you
    ///     want to disable all blending. For example:
    ///         <code>
    ///             context->Bind(BlendOp::NoBlending);
    ///         </code>
    ///
    ///     Common states:
    ///         <list>
    ///             <item> blending operation (BlendOp::Enum)
    ///             <item> blending parameter for source (Blend::Enum)
    ///             <item> blending parameter for destination (Blend::Enum)
    ///         </list>
    ///
    ///     D3D states not exposed by this interface:
    ///         <list>
    ///             <item> AlphaToCoverageEnable
    ///             <item> IndependentBlendEnable
    ///             <item> RenderTargetWriteMask
    ///         </list>
    ///
    /// <exception cref="::RenderCore::Exceptions::AllocationFailure">
    ///     Failed to create underlying object. Could be caused by invalid input values, or a corrupt/lost device.
    /// </exception>
    class BlendState : public VkPipelineColorBlendStateCreateInfo
    {
    public:
        BlendState( BlendOp::Enum blendingOperation = BlendOp::Add, 
                    Blend::Enum srcBlend = Blend::SrcAlpha,
                    Blend::Enum dstBlend = Blend::InvSrcAlpha);
        BlendState( BlendOp::Enum blendingOperation, 
                    Blend::Enum srcBlend,
                    Blend::Enum dstBlend,
                    BlendOp::Enum alphaBlendingOperation, 
                    Blend::Enum alphaSrcBlend,
                    Blend::Enum alphaDstBlend);

		static BlendState Null() { return BlendState(); }

        using UnderlyingType = const BlendState*;
        UnderlyingType GetUnderlying() const { return this; }

    private:
        VkPipelineColorBlendAttachmentState _attachments[s_mrtLimit];
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    namespace StencilOp
    {
        enum Enum
        {
            DontWrite = 1,      // D3D11_STENCIL_OP_KEEP
            Zero = 2,           // D3D11_STENCIL_OP_ZERO
            Replace = 3,        // D3D11_STENCIL_OP_REPLACE
            IncreaseSat = 4,    // D3D11_STENCIL_OP_INCR_SAT
            DecreaseSat = 5,    // D3D11_STENCIL_OP_DECR_SAT
            Invert = 6,         // D3D11_STENCIL_OP_INVERT
            Increase = 7,       // D3D11_STENCIL_OP_INCR
            Decrease = 8        // D3D11_STENCIL_OP_DECR
        };
    }

    class StencilMode
    {
    public:
        Comparison::Enum _comparison;
        StencilOp::Enum _onPass;
        StencilOp::Enum _onDepthFail;
        StencilOp::Enum _onStencilFail;
        StencilMode(
            Comparison::Enum comparison     = Comparison::Always,
            StencilOp::Enum onPass          = StencilOp::Replace,
            StencilOp::Enum onStencilFail   = StencilOp::DontWrite,
            StencilOp::Enum onDepthFail     = StencilOp::DontWrite)
            : _comparison(comparison)
            , _onPass(onPass)
            , _onStencilFail(onStencilFail)
            , _onDepthFail(onDepthFail) {}

        static StencilMode NoEffect;
        static StencilMode AlwaysWrite;
    };

    /// <summary>States used reading and writing to the depth buffer</summary>
    ///
    ///     These states are used by the hardware to determine how pixels are
    ///     rejected by the depth buffer, and whether we write new depth values to
    ///     the depth buffer.
    ///
    ///     Common states:
    ///         <list>
    ///             <item> depth enable (enables/disables both reading to and writing from the depth buffer)
    ///             <item> write enable (enables/disables writing to the depth buffer)
    ///         </list>
    ///
    ///     Stencil states:
    ///         <list>
    ///             <item> For both front and back faces:
    //                  <list>
    ///                     <item> comparison mode
    ///                     <item> operation for stencil and depth pass
    ///                     <item> operation for stencil fail
    ///                     <item> operation for depth fail
    ///                 </list>
    ///         </list>
    ///
    ///     D3D states not exposed by this interface:
    ///         <list>
    ///             <item> DepthFunc (always D3D11_COMPARISON_LESS_EQUAL)
    ///             <item> StencilReadMask
    ///             <item> StencilWriteMask
    ///         </list>
    ///
    /// <exception cref="::RenderCore::Exceptions::AllocationFailure">
    ///     Failed to create underlying object. Could be caused by invalid input values, or a corrupt/lost device.
    /// </exception>
    class DepthStencilState : public VkPipelineDepthStencilStateCreateInfo
    {
    public:
        explicit DepthStencilState(bool enabled=true, bool writeEnabled=true, Comparison::Enum comparison = Comparison::LessEqual);
        DepthStencilState(
            bool depthTestEnabled, bool writeEnabled, 
            unsigned stencilReadMask, unsigned stencilWriteMask,
            const StencilMode& frontFaceStencil = StencilMode::NoEffect,
            const StencilMode& backFaceStencil = StencilMode::NoEffect);

        using UnderlyingType = const DepthStencilState*;
        UnderlyingType GetUnderlying() const { return this; }
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    /// <summary>Utility for querying low level viewport</summary>
    ///
    ///     Pass the device context to the constructor of "ViewportDesc" to
    ///     query the current low level viewport.
    ///
    ///     For example:
    ///         <code>
    ///             ViewportDesc currentViewport0(*context);
    ///             auto width = currentViewport0.Width;
    ///         </code>
    ///
    ///     Note that the type is designed for compatibility with the D3D11 type,
    ///     D3D11_VIEWPORT (for convenience).
    ///
    ///     There can actually be multiple viewports in D3D11. But usually only
    ///     the 0th viewport is used.
    class ViewportDesc
    {
    public:
            // (compatible with D3D11_VIEWPORT)
        float TopLeftX;
        float TopLeftY;
        float Width;
        float Height;
        float MinDepth;
        float MaxDepth;

		ViewportDesc(const DeviceContext& context) {}
        ViewportDesc(float topLeftX, float topLeftY, float width, float height, float minDepth=0.f, float maxDepth=1.f)
            : TopLeftX(topLeftX), TopLeftY(topLeftY), Width(width), Height(height)
            , MinDepth(minDepth), MaxDepth(maxDepth) {}
        ViewportDesc() {}
    };
}}

