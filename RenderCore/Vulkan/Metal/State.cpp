// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "State.h"
#include "ObjectFactory.h"
#include "DeviceContext.h"
#include "../../../Utility/MemoryUtils.h"

namespace RenderCore { namespace Metal_Vulkan
{
    static VkCullModeFlags AsVkCullMode(CullMode cullmode)
    {
        switch (cullmode) {
        default:
        case CullMode::None: return VK_CULL_MODE_NONE;
        case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
        case CullMode::Back: return VK_CULL_MODE_BACK_BIT;
        }

        // (VK_CULL_MODE_FRONT_AND_BACK not accessable)
    }

    static VkPolygonMode AsVkPolygonMode(FillMode cullmode)
    {
        switch (cullmode) {
        default:
        case FillMode::Solid: return VK_POLYGON_MODE_FILL;
        case FillMode::Wireframe: return VK_POLYGON_MODE_LINE;
        }

        // (VK_POLYGON_MODE_POINT not accessable)
    }

    static VkBlendOp AsVkBlendOp(BlendOp blendOp)
    {
        switch (blendOp) {
        default:
        case BlendOp::NoBlending:
        case BlendOp::Add:          return VK_BLEND_OP_ADD;
        case BlendOp::Subtract:     return VK_BLEND_OP_SUBTRACT;
        case BlendOp::RevSubtract:  return VK_BLEND_OP_REVERSE_SUBTRACT;
        case BlendOp::Min:          return VK_BLEND_OP_MIN;
        case BlendOp::Max:          return VK_BLEND_OP_MAX;
        }
    }

    static VkBlendFactor AsVkBlendFactor(Blend blendOp)
    {
        switch (blendOp)
        {
        case Blend::Zero: return VK_BLEND_FACTOR_ZERO;
        default:
        case Blend::One: return VK_BLEND_FACTOR_ONE;

        case Blend::SrcColor: return VK_BLEND_FACTOR_SRC_COLOR;
        case Blend::InvSrcColor: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case Blend::DestColor: return VK_BLEND_FACTOR_DST_COLOR;
        case Blend::InvDestColor: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;

        case Blend::SrcAlpha: return VK_BLEND_FACTOR_SRC_ALPHA;
        case Blend::InvSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case Blend::DestAlpha: return VK_BLEND_FACTOR_DST_ALPHA;
        case Blend::InvDestAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        }

        // not accessable:
        // VK_BLEND_FACTOR_CONSTANT_COLOR
        // VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR
        // VK_BLEND_FACTOR_CONSTANT_ALPHA
        // VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA
        // VK_BLEND_FACTOR_SRC_ALPHA_SATURATE
        // VK_BLEND_FACTOR_SRC1_COLOR
        // VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR
        // VK_BLEND_FACTOR_SRC1_ALPHA
        // VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA
    }

    static VkCompareOp AsVkCompareOp(CompareOp comparison)
    {
        switch (comparison)
        {
        case CompareOp::Never: return VK_COMPARE_OP_NEVER;
        case CompareOp::Less: return VK_COMPARE_OP_LESS;
        case CompareOp::Equal: return VK_COMPARE_OP_EQUAL;
        case CompareOp::LessEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CompareOp::Greater: return VK_COMPARE_OP_GREATER;
        case CompareOp::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
        case CompareOp::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        default:
        case CompareOp::Always: return VK_COMPARE_OP_ALWAYS;
        }
    }

    static VkStencilOp AsVkStencilOp(StencilOp stencilOp)
    {
        switch (stencilOp)
        {
        default:
        case StencilOp::DontWrite:      return VK_STENCIL_OP_KEEP;
        case StencilOp::Zero:           return VK_STENCIL_OP_ZERO;
        case StencilOp::Replace:        return VK_STENCIL_OP_REPLACE;
        case StencilOp::IncreaseSat:    return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case StencilOp::DecreaseSat:    return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case StencilOp::Invert:         return VK_STENCIL_OP_INVERT;
        case StencilOp::Increase:       return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case StencilOp::Decrease:       return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        }
    }

    static VkSamplerAddressMode AsVkAddressMode(AddressMode addressMode)
    {
        switch (addressMode)
        {
        default:
        case AddressMode::Wrap: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case AddressMode::Mirror: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case AddressMode::Clamp: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case AddressMode::Border: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        }
    }

    RasterizerState::RasterizerState(
        CullMode cullmode, 
        bool frontCounterClockwise)
    {
        sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        pNext = nullptr;
        flags = 0;
        polygonMode = VK_POLYGON_MODE_FILL;
        cullMode = AsVkCullMode(cullmode);
        frontFace = frontCounterClockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
        depthClampEnable = VK_FALSE;
        rasterizerDiscardEnable = VK_FALSE;
        depthBiasEnable = VK_FALSE;
        depthBiasConstantFactor = 0;
        depthBiasClamp = 0;
        depthBiasSlopeFactor = 0;
        lineWidth = 1.0f;	// (set to 1.0f when this feature is disabled)
    }

    RasterizerState::RasterizerState(
        CullMode cullmode, bool frontCounterClockwise,
        FillMode fillmode,
        int depthBias, float iDepthBiasClamp, float slopeScaledBias)
    {
        sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        pNext = nullptr;
        flags = 0;
        polygonMode = AsVkPolygonMode(fillmode);
        cullMode = AsVkCullMode(cullmode);
        frontFace = frontCounterClockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
        depthClampEnable = VK_FALSE;
        rasterizerDiscardEnable = VK_FALSE;
        depthBiasEnable = VK_TRUE;
        depthBiasConstantFactor = *(float*)&depthBias;
        depthBiasClamp = iDepthBiasClamp;
        depthBiasSlopeFactor = slopeScaledBias;
        lineWidth = 1.0f;	// (set to 1.0f when this feature is disabled)
    }

    BlendState::BlendState( 
        BlendOp blendingOperation, 
        Blend srcBlend,
        Blend dstBlend,
        BlendOp alphaBlendingOperation, 
        Blend alphaSrcBlend,
        Blend alphaDstBlend)
    {
        XlZeroMemory(_attachments);
        for (unsigned c=0; c<dimof(_attachments); ++c) {
            _attachments[c].colorWriteMask = 0xf;

            _attachments[c].blendEnable = 
                   (blendingOperation != BlendOp::NoBlending)
                || (alphaBlendingOperation != BlendOp::NoBlending);

            if (blendingOperation == BlendOp::NoBlending) {
                _attachments[c].colorBlendOp = VK_BLEND_OP_ADD;
                _attachments[c].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                _attachments[c].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            } else {
                _attachments[c].colorBlendOp = AsVkBlendOp(blendingOperation);
                _attachments[c].srcColorBlendFactor = AsVkBlendFactor(srcBlend);
                _attachments[c].dstColorBlendFactor = AsVkBlendFactor(dstBlend);
            }

            if (blendingOperation == BlendOp::NoBlending) {
                _attachments[c].alphaBlendOp = VK_BLEND_OP_ADD;
                _attachments[c].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                _attachments[c].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            } else {
                _attachments[c].alphaBlendOp = AsVkBlendOp(alphaBlendingOperation);
                _attachments[c].srcAlphaBlendFactor = AsVkBlendFactor(srcBlend);
                _attachments[c].dstAlphaBlendFactor = AsVkBlendFactor(dstBlend);
            }
        }

        sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        pNext = nullptr;
        flags = 0;
        attachmentCount = dimof(_attachments);
        pAttachments = _attachments;
        logicOpEnable = VK_FALSE;
        logicOp = VK_LOGIC_OP_NO_OP;
        blendConstants[0] = 1.0f;
        blendConstants[1] = 1.0f;
        blendConstants[2] = 1.0f;
        blendConstants[3] = 1.0f;
    }

    BlendState::BlendState(const BlendState& cloneFrom)
    {
        *(VkPipelineColorBlendStateCreateInfo*)this = *(VkPipelineColorBlendStateCreateInfo*)&cloneFrom;
        for (unsigned c=0; c<attachmentCount; ++c)
            _attachments[c] = cloneFrom._attachments[c];
        pAttachments = _attachments;
    }

    BlendState& BlendState::operator=(const BlendState& cloneFrom)
    {
        *(VkPipelineColorBlendStateCreateInfo*)this = *(VkPipelineColorBlendStateCreateInfo*)&cloneFrom;
        for (unsigned c=0; c<attachmentCount; ++c)
            _attachments[c] = cloneFrom._attachments[c];
        pAttachments = _attachments;
        return *this;
    }

    StencilMode StencilMode::NoEffect(CompareOp::Always, StencilOp::DontWrite, StencilOp::DontWrite, StencilOp::DontWrite);
    StencilMode StencilMode::AlwaysWrite(CompareOp::Always, StencilOp::Replace, StencilOp::DontWrite, StencilOp::DontWrite);

    DepthStencilState::DepthStencilState(bool enabled, bool writeEnabled, CompareOp comparison)
    {
        sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        pNext = nullptr;
        flags = 0;
        depthTestEnable = enabled;
        depthWriteEnable = writeEnabled;
        depthCompareOp = AsVkCompareOp(comparison);
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
        const StencilMode& frontFaceStencil,
        const StencilMode& backFaceStencil)
    {
        sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        pNext = nullptr;
        flags = 0;
        depthTestEnable = depthTestEnabled;
        depthWriteEnable = writeEnabled;
        depthCompareOp = depthTestEnabled?VK_COMPARE_OP_LESS_OR_EQUAL:VK_COMPARE_OP_ALWAYS;
        depthBoundsTestEnable = VK_FALSE;
        minDepthBounds = 0;
        maxDepthBounds = 0;
        stencilTestEnable = VK_TRUE;

        front.failOp = AsVkStencilOp(frontFaceStencil._onStencilFail);
        front.passOp = AsVkStencilOp(frontFaceStencil._onPass);
        front.compareOp = AsVkCompareOp(frontFaceStencil._comparison);
        front.compareMask = stencilReadMask;
        front.reference = 0;
        front.depthFailOp = AsVkStencilOp(frontFaceStencil._onDepthFail);
        front.writeMask = stencilWriteMask;

        back.failOp = AsVkStencilOp(backFaceStencil._onStencilFail);
        back.passOp = AsVkStencilOp(backFaceStencil._onPass);
        back.compareOp = AsVkCompareOp(backFaceStencil._comparison);
        back.compareMask = stencilReadMask;
        back.reference = 0;
        back.depthFailOp = AsVkStencilOp(backFaceStencil._onDepthFail);
        back.writeMask = stencilWriteMask;
    }


    SamplerState::SamplerState(   
        FilterMode filter,
        AddressMode addressU, 
        AddressMode addressV, 
        AddressMode addressW,
		CompareOp comparison)
    {
        VkSamplerCreateInfo samplerCreateInfo = {};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.pNext = nullptr;
        samplerCreateInfo.flags = 0;

        samplerCreateInfo.compareEnable = VK_FALSE;
        samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerCreateInfo.anisotropyEnable = VK_FALSE;
        samplerCreateInfo.maxAnisotropy = 0;

        switch (filter) {
        default:
        case FilterMode::Point:                 
            samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
            samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
            samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        case FilterMode::Anisotropic:
            samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerCreateInfo.anisotropyEnable = VK_TRUE;
            samplerCreateInfo.maxAnisotropy = 16;
            break;
        case FilterMode::ComparisonBilinear:    
            samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            samplerCreateInfo.compareEnable = VK_TRUE;
            samplerCreateInfo.compareOp = AsVkCompareOp(comparison);
            break;
        case FilterMode::Bilinear:              
            samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        case FilterMode::Trilinear:             
            samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            break;
        }
        
        samplerCreateInfo.addressModeU = AsVkAddressMode(addressU);
        samplerCreateInfo.addressModeV = AsVkAddressMode(addressV);
        samplerCreateInfo.addressModeW = AsVkAddressMode(addressW);

        samplerCreateInfo.mipLodBias = 0.f;
        samplerCreateInfo.minLod = 0.f;
        samplerCreateInfo.maxLod = std::numeric_limits<float>::max();
        samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;       // (interesting)
        _sampler = GetObjectFactory().CreateSampler(samplerCreateInfo);
    }

    SamplerState::~SamplerState() {}


    ViewportDesc::ViewportDesc(const DeviceContext& context)
    {
        *this = context.GetBoundViewport();
    }

    ViewportDesc::ViewportDesc()
    {
        TopLeftX = TopLeftY = 0.f;
        Width = Height = 0.f;
        MinDepth = 0.f;
        MaxDepth = 1.f;
    }
}}
