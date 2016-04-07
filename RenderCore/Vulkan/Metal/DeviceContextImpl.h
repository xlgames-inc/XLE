// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DeviceContext.h"

namespace RenderCore { namespace Metal_Vulkan
{

    template<int Count> 
        void DeviceContext::Bind(
            const ResourceList<VertexBuffer, Count>& VBs, 
            unsigned stride, unsigned offset) 
        {
            assert(Count <= s_maxBoundVBs);
            VkDeviceSize offsets[s_maxBoundVBs] = { offset, offset, offset, offset };
            SetVertexStrides({stride, stride, stride, stride});
            vkCmdBindVertexBuffers(
                _primaryCommandList.get(),
                VBs._startingPoint, Count,
                VBs._buffers, offsets);
        }

}}
