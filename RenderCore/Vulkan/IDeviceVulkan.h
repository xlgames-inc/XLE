// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../IDevice.h"
#include "../IThreadContext.h"
#include "Metal/VulkanForward.h"
#include <memory>

#define FLEX_USE_VTABLE_DeviceVulkan FLEX_USE_VTABLE_Device
#define FLEX_USE_VTABLE_ThreadContextVulkan FLEX_USE_VTABLE_ThreadContext

namespace RenderCore
{
    namespace Metal_Vulkan { class DeviceContext; class GlobalPools; class PipelineLayout; }

    ////////////////////////////////////////////////////////////////////////////////

#define FLEX_INTERFACE DeviceVulkan
/*-----------------*/ #include "../FlexBegin.h" /*-----------------*/
    
        /// <summary>IDevice extension for DX11</summary>
        /// Use IDevice::QueryInterface to query for this type from a
        /// plain IDevice.
        class __declspec( uuid("{48764F24-01A1-47C9-9105-C6E57E29493D}") ) ICLASSNAME(DeviceVulkan)
        {
        public:
            IMETHOD VkInstance	GetVulkanInstance() IPURE;
		    IMETHOD VkDevice	GetUnderlyingDevice() IPURE;
            IMETHOD VkQueue     GetRenderingQueue() IPURE;
            IMETHOD Metal_Vulkan::GlobalPools& GetGlobalPools() IPURE;
            IMETHOD const std::shared_ptr<Metal_Vulkan::PipelineLayout>& ShareGraphicsPipelineLayout() IPURE;
            IMETHOD const std::shared_ptr<Metal_Vulkan::PipelineLayout>& ShareComputePipelineLayout() IPURE;
            IDESTRUCTOR
        };

        #if !defined(FLEX_CONTEXT_DeviceVulkan)
            #define FLEX_CONTEXT_DeviceVulkan     FLEX_CONTEXT_INTERFACE
        #endif

        #if defined(DOXYGEN)
            typedef IDeviceVulkan Base_DeviceVulkan;
        #endif

/*-----------------*/ #include "../FlexEnd.h" /*-----------------*/


    ////////////////////////////////////////////////////////////////////////////////

#define FLEX_INTERFACE ThreadContextVulkan
/*-----------------*/ #include "../FlexBegin.h" /*-----------------*/
    
        /// <summary>IThreadContext extension for DX11</summary>
        class __declspec( uuid("{BC1B03FD-6770-4714-82B7-D7819142ED4A}") ) ICLASSNAME(ThreadContextVulkan)
        {
        public:
            IMETHOD const std::shared_ptr<Metal_Vulkan::DeviceContext>& GetMetalContext() IPURE;
            IDESTRUCTOR
        };

        #if !defined(FLEX_CONTEXT_ThreadContextVulkan)
            #define FLEX_CONTEXT_ThreadContextVulkan     FLEX_CONTEXT_INTERFACE
        #endif

        #if defined(DOXYGEN)
            typedef IThreadContextVulkan Base_ThreadContextVulkan;
        #endif

/*-----------------*/ #include "../FlexEnd.h" /*-----------------*/

}
