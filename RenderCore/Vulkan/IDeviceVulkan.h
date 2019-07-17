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

namespace RenderCore
{
    namespace Metal_Vulkan { class DeviceContext; class GlobalPools; class PipelineLayout; }

    ////////////////////////////////////////////////////////////////////////////////

    /// <summary>IDevice extension for DX11</summary>
    /// Use IDevice::QueryInterface to query for this type from a
    /// plain IDevice.
    class IDeviceVulkan
    {
    public:
        virtual VkInstance	GetVulkanInstance() = 0;
		virtual VkDevice	GetUnderlyingDevice() = 0;
		virtual VkQueue     GetRenderingQueue() = 0;
		virtual Metal_Vulkan::GlobalPools& GetGlobalPools() = 0;
		~IDeviceVulkan();
    };

    ////////////////////////////////////////////////////////////////////////////////

    /// <summary>IThreadContext extension for DX11</summary>
    class IThreadContextVulkan
    {
    public:
		virtual const std::shared_ptr<Metal_Vulkan::DeviceContext>& GetMetalContext() = 0;
		~IThreadContextVulkan();
    };

}
