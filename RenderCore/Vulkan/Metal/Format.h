// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Types_Forward.h"

extern "C" { typedef enum VkFormat VkFormat; }

namespace RenderCore { namespace Metal_Vulkan
{
    VkFormat AsVkFormat(Format);
	Format AsFormat(VkFormat);
    void InitFormatConversionTables();
}}

