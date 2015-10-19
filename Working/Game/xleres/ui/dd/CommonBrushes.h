// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(COMMON_BRUSHES_H)
#define COMMON_BRUSHES_H

#include "Interfaces.h"

class SolidFill : IBrush
{
    float4 Calculate(DebuggingShapesCoords coords, float4 baseColor) { return baseColor; }
};

class WhiteOutline : IBrush
{
    float4 Calculate(DebuggingShapesCoords coords, float4 baseColor) { return 1.0.xxxx; }
};

class CrossHatchFill : IBrush
{
    float4 Calculate(
        DebuggingShapesCoords coords,
        float4 baseColor)
    {
        float4 color = baseColor;

            // cross hatch pattern -- (bright:dark == 1:1)
        uint p = uint(coords.position.x) + uint(coords.position.y);
        if (((p/4) % 2) != 0) {
            color.rgb *= 0.66f;
        }

        return color;
    }
};

#endif
