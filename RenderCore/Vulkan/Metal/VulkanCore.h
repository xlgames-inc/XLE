// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../../Core/Exceptions.h"
#include <type_traits>
#include <utility>
#include <memory>

extern "C" { typedef enum VkResult VkResult; }

namespace RenderCore { namespace Metal_Vulkan
{
    namespace Internal
    {
        template<typename Type>
            struct VulkanShared
                { typedef std::shared_ptr<typename std::remove_reference<decltype(*std::declval<Type>())>::type> Ptr; };
    }

    template<typename Type>
	    using VulkanSharedPtr = typename Internal::VulkanShared<Type>::Ptr;

    class VulkanAPIFailure : public ::Exceptions::BasicLabel
    {
    public:
        VulkanAPIFailure(VkResult res, const char message[]);
    };
}}

