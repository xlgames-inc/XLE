// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "VulkanCore.h"
#include "IncludeVulkan.h"      // required because we're deriving from Vulkan types
#include "../../StateDesc.h"
#include "../../IDevice.h"
#include "../../../Utility/IteratorUtils.h"
#include <utility>

namespace RenderCore { namespace Metal_Vulkan
{
    class DeviceContext;
    class ObjectFactory;

    static const unsigned s_mrtLimit = 4;

////////////////////////////////////////////////////////////////////////////////////////////////

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
    class SamplerState : public ISampler
    {
    public:
        SamplerState(ObjectFactory&, const SamplerDesc&);
		~SamplerState();

        using UnderlyingType = VkSampler;
        VkSampler GetUnderlying() const { return _sampler.get(); }

    private:
        VulkanSharedPtr<VkSampler> _sampler;
    };

    namespace Internal
    {

        ////////////////////////////////////////////////////////////////////////////////////////////////

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
    class VulkanRasterizerState : public VkPipelineRasterizationStateCreateInfo
    {
    public:
        VulkanRasterizerState(const RasterizationDesc& desc = {});
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

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
    class VulkanBlendState : public VkPipelineColorBlendStateCreateInfo
    {
    public:
        VulkanBlendState(IteratorRange<const AttachmentBlendDesc*> blendStates);
        VulkanBlendState();
        VulkanBlendState(const VulkanBlendState& cloneFrom);
        VulkanBlendState& operator=(const VulkanBlendState& cloneFrom);
    private:
        VkPipelineColorBlendAttachmentState _attachments[s_mrtLimit];
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

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
    class VulkanDepthStencilState : public VkPipelineDepthStencilStateCreateInfo
    {
    public:
        VulkanDepthStencilState(const DepthStencilDesc& depthStencilState = {});
    };

    }
}}

