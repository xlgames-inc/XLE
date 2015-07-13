// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(RANGE_FOG_RESOLVE_H)
#define RANGE_FOG_RESOLVE_H

#include "LightDesc.h"

void LightResolve_RangeFog(RangeFogDesc desc, float distance, out float outscatterScale, out float3 inscatter)
{
    // Based purely on distance, calculate the effect of range fog
    // Attentuation performed using base Beer-Lambert law
    // Just basic linear inscatter currently. This won't correctly deal with the amount of
    // inscattered light that later gets outscattered.

    outscatterScale = exp(-desc.OpticalThickness * distance);
    inscatter = desc.Inscatter * distance;
}

#endif
