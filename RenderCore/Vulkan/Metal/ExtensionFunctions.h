// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IncludeVulkan.h"

namespace RenderCore { namespace Metal_Vulkan
{
    class ExtensionFunctions
    {
    public:
        PFN_vkCmdBeginTransformFeedbackEXT _beginTransformFeedback = nullptr;
        PFN_vkCmdBindTransformFeedbackBuffersEXT _bindTransformFeedbackBuffers = nullptr;
        PFN_vkCmdEndTransformFeedbackEXT _endTransformFeedback = nullptr; 
        ExtensionFunctions(VkInstance instance);
    };
}}
