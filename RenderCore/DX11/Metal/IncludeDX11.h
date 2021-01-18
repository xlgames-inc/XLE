// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../../Core/Prefix.h"
#include "DX11.h"

#if PLATFORMOS_ACTIVE != PLATFORMOS_WINDOWS
    #pragma message("IncludeDX11.h -- only valid for Windows API targets. Exclude from inclusion for other targets.")
#endif

    // include windows in a controlled manner (before d3d11 includes it!)
    // (try to limit including DX11.h to just this place)

#include "../../../OSServices/WinAPI/IncludeWindows.h"

#if DX_VERSION == DX_VERSION_11_1
    #include <d3d11_1.h>
#else
    #include <d3d11.h>
#endif

