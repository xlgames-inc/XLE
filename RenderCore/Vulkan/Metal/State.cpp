// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "State.h"
#include "../../../Utility/MemoryUtils.h"

namespace RenderCore { namespace Metal_Vulkan
{
    RasterizerState::RasterizerState(
        CullMode::Enum cullmode = CullMode::Back, 
        bool frontCounterClockwise = true)
    {
        sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        pNext = nullptr;
        flags = 0;
        polygonMode = VK_POLYGON_MODE_FILL;
        cullMode = VK_CULL_MODE_BACK_BIT;
        frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        depthClampEnable = VK_TRUE;
        rasterizerDiscardEnable = VK_FALSE;
        depthBiasEnable = VK_FALSE;
        depthBiasConstantFactor = 0;
        depthBiasClamp = 0;
        depthBiasSlopeFactor = 0;
        lineWidth = 0;
    }

    RasterizerState::RasterizerState(
        CullMode::Enum cullmode, bool frontCounterClockwise,
        FillMode::Enum fillmode,
        int depthBias, float depthBiasClamp, float slopeScaledBias)
    : RasterizerState() {}

    BlendState::BlendState( 
        BlendOp::Enum blendingOperation = BlendOp::Add, 
        Blend::Enum srcBlend = Blend::SrcAlpha,
        Blend::Enum dstBlend = Blend::InvSrcAlpha)
    {
        XlZeroMemory(_attachments);
        _attachments[0].colorWriteMask = 0xf;
        _attachments[0].blendEnable = VK_FALSE;
        _attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
        _attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
        _attachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        _attachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        _attachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        _attachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

        sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        pNext = nullptr;
        flags = 0;
        attachmentCount = 1;
        pAttachments = _attachments;
        logicOpEnable = VK_FALSE;
        logicOp = VK_LOGIC_OP_NO_OP;
        blendConstants[0] = 1.0f;
        blendConstants[1] = 1.0f;
        blendConstants[2] = 1.0f;
        blendConstants[3] = 1.0f;
    }

    BlendState::BlendState( 
        BlendOp::Enum blendingOperation, 
        Blend::Enum srcBlend,
        Blend::Enum dstBlend,
        BlendOp::Enum alphaBlendingOperation, 
        Blend::Enum alphaSrcBlend,
        Blend::Enum alphaDstBlend)
    : BlendState() {}


    DepthStencilState::DepthStencilState(bool enabled=true, bool writeEnabled=true, Comparison::Enum comparison = Comparison::LessEqual)
    {
        sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        pNext = nullptr;
        flags = 0;
        depthTestEnable = VK_TRUE;
        depthWriteEnable = VK_TRUE;
        depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        depthBoundsTestEnable = VK_FALSE;
        minDepthBounds = 0;
        maxDepthBounds = 0;
        stencilTestEnable = VK_FALSE;
        back.failOp = VK_STENCIL_OP_KEEP;
        back.passOp = VK_STENCIL_OP_KEEP;
        back.compareOp = VK_COMPARE_OP_ALWAYS;
        back.compareMask = 0;
        back.reference = 0;
        back.depthFailOp = VK_STENCIL_OP_KEEP;
        back.writeMask = 0;
        front = back;
    }

    DepthStencilState::DepthStencilState(
        bool depthTestEnabled, bool writeEnabled, 
        unsigned stencilReadMask, unsigned stencilWriteMask,
        const StencilMode& frontFaceStencil = StencilMode::NoEffect,
        const StencilMode& backFaceStencil = StencilMode::NoEffect)
    : DepthStencilState() {}

}}
