// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Format.h"
#include "IncludeAppleMetal.h"

namespace RenderCore { namespace Metal_AppleMetal
{
    MTLPixelFormat AsMTLPixelFormat(RenderCore::Format fmt);
    RenderCore::Format AsRenderCoreFormat(MTLPixelFormat fmt);
}}
