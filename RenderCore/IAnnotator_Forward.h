// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Prefix.h"

// // // //      Flexible interfaces configuration      // // // //
#define FLEX_USE_VTABLE_Annotator   1

namespace RenderCore
{
#define FLEX_INTERFACE Annotator
#include "FlexForward.h"
#undef FLEX_INTERFACE
}
