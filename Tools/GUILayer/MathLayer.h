// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Math/Vector.h"

namespace GUILayer
{
    public value struct Vector3 sealed
    {
        property float X;
        property float Y;
        property float Z;

        Vector3(float x, float y, float z) { X = x; Y = y; Z = z; }
    };

    inline Float3 AsFloat3(Vector3 input) { return Float3(input.X, input.Y, input.Z); }
    inline Vector3 AsVector3(Float3 input) { return Vector3(input[0], input[1], input[2]); }
}

