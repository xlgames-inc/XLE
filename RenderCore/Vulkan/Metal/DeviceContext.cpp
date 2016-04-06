// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DeviceContext.h"
#include "ObjectFactory.h"
#include "InputLayout.h"

namespace RenderCore { namespace Metal_Vulkan
{

    void        PipelineBuilder::Bind(const RasterizerState& rasterizer)
    {
        _rasterizerState = rasterizer;
    }
    
    void        PipelineBuilder::Bind(const BlendState& blendState)
    {
        _blendState = blendState;
    }
    
    void        PipelineBuilder::Bind(const DepthStencilState& depthStencilState, unsigned stencilRef)
    {
        _depthStencilState = depthStencilState;
    }

    void        PipelineBuilder::Bind(const BoundInputLayout& inputLayout)
    {
    }

    static const VkPrimitiveTopology AsNativeTopology(Topology::Enum topology)
    {
        return (VkPrimitiveTopology)topology;
    }
    
    void        PipelineBuilder::Bind(Topology::Enum topology)
    {
        _topology = AsNativeTopology(topology);
    }

    VulkanSharedPtr<VkPipeline> PipelineBuilder::CreatePipeline(const ObjectFactory& factory, VkPipelineCache cache)
    {
        VkGraphicsPipelineCreateInfo createInfo;

        VkDynamicState dynamicStateEnables[VK_DYNAMIC_STATE_RANGE_SIZE];
        VkPipelineDynamicStateCreateInfo dynamicState = {};
        memset(dynamicStateEnables, 0, sizeof dynamicStateEnables);
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.pNext = nullptr;
        dynamicState.pDynamicStates = dynamicStateEnables;
        dynamicState.dynamicStateCount = 0;

        VkPipelineVertexInputStateCreateInfo vi;
        vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vi.pNext = nullptr;
        vi.flags = 0;
        vi.vertexBindingDescriptionCount = 0;
        vi.pVertexBindingDescriptions = nullptr;
        vi.vertexAttributeDescriptionCount = 0;
        vi.pVertexAttributeDescriptions = nullptr;

        if (_inputLayout) {
            auto attribs = _inputLayout->GetAttributes();
            vi.vertexAttributeDescriptionCount = (uint32)attribs.size();
            vi.pVertexAttributeDescriptions = attribs.begin();
        }

        VkPipelineInputAssemblyStateCreateInfo ia;
        ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.pNext = nullptr;
        ia.flags = 0;
        ia.primitiveRestartEnable = VK_FALSE;
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo vp = {};
        vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vp.pNext = nullptr;
        vp.flags = 0;
        vp.viewportCount = 1;
        dynamicStateEnables[dynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT;
        vp.scissorCount = 1;
        dynamicStateEnables[dynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR;
        vp.pScissors = nullptr;
        vp.pViewports = nullptr;

        VkPipelineMultisampleStateCreateInfo ms;
        ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.pNext = nullptr;
        ms.flags = 0;
        ms.pSampleMask = nullptr;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        ms.sampleShadingEnable = VK_FALSE;
        ms.alphaToCoverageEnable = VK_FALSE;
        ms.alphaToOneEnable = VK_FALSE;
        ms.minSampleShading = 0.0;

        VkGraphicsPipelineCreateInfo pipeline;
        pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline.pNext = nullptr;
        pipeline.layout = info.pipeline_layout;
        pipeline.basePipelineHandle = VK_NULL_HANDLE;
        pipeline.basePipelineIndex = 0;
        pipeline.flags = 0;
        pipeline.pVertexInputState = &vi;
        pipeline.pInputAssemblyState = &ia;
        pipeline.pRasterizationState = &_rasterizerState;
        pipeline.pColorBlendState = &_blendState;
        pipeline.pTessellationState = nullptr;
        pipeline.pMultisampleState = &ms;
        pipeline.pDynamicState = &dynamicState;
        pipeline.pViewportState = &vp;
        pipeline.pDepthStencilState = &_depthStencilState;
        pipeline.pStages = info.shaderStages;
        pipeline.stageCount = 2;
        pipeline.renderPass = info.render_pass;
        pipeline.subpass = 0;

        return factory.CreateGraphicsPipeline(cache, pipeline);
    }

    PipelineBuilder::PipelineBuilder()
    {
        _inputLayout = nullptr;
        _topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }

    PipelineBuilder::~PipelineBuilder() {}

    void        DeviceContext::Bind(const ViewportDesc& viewport)
    {
    }

    void        DeviceContext::Draw(unsigned vertexCount, unsigned startVertexLocation=0)
    {
    }
    
    void        DeviceContext::DrawIndexed(unsigned indexCount, unsigned startIndexLocation=0, unsigned baseVertexLocation=0)
    {
    }

    void        DeviceContext::DrawAuto() 
    {
    }

    void        DeviceContext::Dispatch(unsigned countX, unsigned countY=1, unsigned countZ=1)
    {
    }

}}

