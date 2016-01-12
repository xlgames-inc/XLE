// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(BASIC_H)
#define BASIC_H

#include "CommonResources.h"

struct ColourOutput
{
    float4 target0 : SV_Target0;
};

cbuffer CommonInputs
{
    Texture2D DefaultTexture;
}


float4 SampleTexture(Texture2D inputTexture, float2 texCoords)
{
    return inputTexture.Sample(DefaultSampler, texCoords);
}

float4 Multiply4Scalar(float4 lhs, float rhs)        { return lhs * rhs; }
float3 Multiply3Scalar(float3 lhs, float rhs)        { return lhs * rhs; }
float2 Multiply2Scalar(float2 lhs, float rhs)        { return lhs * rhs; }

float4 Multiply4(float4 lhs, float4 rhs)            { return lhs * rhs; }
float3 Multiply3(float3 lhs, float3 rhs)            { return lhs * rhs; }
float2 Multiply2(float2 lhs, float2 rhs)            { return lhs * rhs; }
float Multiply1(float lhs, float rhs)                { return lhs * rhs; }

float Add1(float lhs, float rhs)            { return lhs + rhs; }
float Subtract1(float lhs, float rhs)       { return lhs - rhs; }
float Divide1(float lhs, float rhs)         { return lhs / rhs; }
float Round1(float lhs, float multipleOf)   { return round(lhs / multipleOf) * multipleOf; }

float4 SplitAlpha(float3 inputColour, float inputAlpha)     { return float4(inputColour, inputAlpha); }

float4 OutputGray() 										{ return float4(0.5f, 0.5f, 0.5f, 1.f); }
float OutputOne()											{ return 1.f; }

float4 PassThrough(float4 input) { return input; }

float4 Add4(float4 lhs, float4 rhs) { return float4(lhs.rgb + rhs.rgb, 1.f); }

float3 Normalize3(float3 input) { return normalize(input); }
float3 Negate3(float3 input) { return -input; }

float Lerp(float min, float max, float alpha) { return lerp(min, max, alpha); }

void Separate3(float3 input, out float x, out float y, out float z)
{
    x = input.x; y = input.y; z = input.z;
}

#endif
