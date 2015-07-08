// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Math/Vector.h"

namespace GUILayer
{
    public value struct Vector2 sealed
    {
        property float X;
        property float Y;

        Vector2(float x, float y) { X = x; Y = y; }
    };
    
    public value struct Vector3 sealed
    {
        property float X;
        property float Y;
        property float Z;

        Vector3(float x, float y, float z) { X = x; Y = y; Z = z; }
    };

    public value struct Vector4 sealed
    {
        property float X;
        property float Y;
        property float Z;
        property float W;

        Vector4(float x, float y, float z, float w) { X = x; Y = y; Z = z; W = w; }
    };

    public value struct VectorUInt2 sealed
    {
        property unsigned X;
        property unsigned Y;

        VectorUInt2(unsigned x, unsigned y) { X = x; Y = y; }
    };

    inline Float2 AsFloat2(Vector2 input) { return Float2(input.X, input.Y); }
    inline Vector2 AsVector2(Float2 input) { return Vector2(input[0], input[1]); }
    
    inline Float3 AsFloat3(Vector3 input) { return Float3(input.X, input.Y, input.Z); }
    inline Vector3 AsVector3(Float3 input) { return Vector3(input[0], input[1], input[2]); }

    inline Float4 AsFloat4(Vector4 input) { return Float4(input.X, input.Y, input.Z, input.W); }
    inline Vector4 AsVector4(Float4 input) { return Vector4(input[0], input[1], input[2], input[3]); }

    inline UInt2 AsUInt2(VectorUInt2 input) { return UInt2(input.X, input.Y); }
    inline VectorUInt2 AsVectorUInt2(UInt2 input) { return VectorUInt2(input[0], input[1]); }
}

