// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../../Core/SelectConfiguration.h"

#if PLATFORMOS_TARGET  == PLATFORMOS_WINDOWS
	#define VK_USE_PLATFORM_WIN32_KHR

    // Vulkan includes <windows.h> -- so we must include it first to get our
    // framework of compatibility macros!
    #include "../../../Core/WinAPI/IncludeWindows.h"
#endif

#include "vulkan/vulkan.h"
