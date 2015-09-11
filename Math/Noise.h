// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Vector.h"

namespace XLEMath
{
    float SimplexNoise(Float2 input);
    float SimplexNoise(Float3 input);
    float SimplexNoise(Float4 input);

    template<typename Type>
        float SimplexFBM(Type pos, float hgrid, float gain, float lacunarity, int octaves);
}
