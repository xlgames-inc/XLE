// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(BASIC_H)
#define BASIC_H

float4 Multiply4Scalar(float4 lhs, float rhs)        { return lhs * rhs; }
float3 Multiply3Scalar(float3 lhs, float rhs)        { return lhs * rhs; }
float2 Multiply2Scalar(float2 lhs, float rhs)        { return lhs * rhs; }

float4 Divide4Scalar(float4 lhs, float rhs)        { return lhs / rhs; }
float3 Divide3Scalar(float3 lhs, float rhs)        { return lhs / rhs; }
float2 Divide2Scalar(float2 lhs, float rhs)        { return lhs / rhs; }

float4 Multiply4(float4 lhs, float4 rhs)            { return lhs * rhs; }
float3 Multiply3(float3 lhs, float3 rhs)            { return lhs * rhs; }
float2 Multiply2(float2 lhs, float2 rhs)            { return lhs * rhs; }
float Multiply1(float lhs, float rhs)                { return lhs * rhs; }

float Add1(float lhs, float rhs)            { return lhs + rhs; }
float Subtract1(float lhs, float rhs)       { return lhs - rhs; }
float Divide1(float lhs, float rhs)         { return lhs / rhs; }
float Round1(float value)         { return round(value); }
float RoundToMultiple1(float lhs, float multipleOf)   { return round(lhs / multipleOf) * multipleOf; }
float Power1(float base, float exponent)    { return pow(base, exponent); }
float Saturate1(float input)                { return saturate(input); }
float Lerp1(float min, float max, float alpha) { return lerp(min, max, alpha); }
float Max1(float lhs, float rhs)            { return max(lhs, rhs); }
float AddMany1(float first, float second, float third, float forth) { return first + second + third + forth; }
float MultiplyMany1(float first, float second, float third, float forth) { return first * second * third * forth; }
float Abs1(float value) { return abs(value); }
float Square1(float value) { return value * value; }

float2 Add2(float2 lhs, float2 rhs)            { return lhs + rhs; }
float2 AddMany2(float2 first, float2 second, float2 third, float2 forth)    { return first + second + third + forth; }
float2 Subtract2(float2 lhs, float2 rhs)       { return lhs - rhs; }

float3 Add3(float3 lhs, float3 rhs)            { return lhs + rhs; }
float3 Subtract3(float3 lhs, float3 rhs)       { return lhs - rhs; }
float3 Divide3(float3 lhs, float3 rhs)         { return lhs / rhs; }
float3 Round3(float3 values)                   { return round(values); }
float3 Saturate3(float3 input)                 { return saturate(input); }
float3 Lerp3(float3 min, float3 max, float alpha) { return lerp(min, max, alpha); }

float4 Add4(float4 lhs, float4 rhs) { return float4(lhs.rgb + rhs.rgb, 1.f); }      // NOTE -- HACK!
float4 Subtract4(float4 lhs, float4 rhs) { return lhs - rhs; }
float4 Saturate4(float4 input)                 { return saturate(input); }
float4 Lerp4(float4 min, float4 max, float alpha) { return lerp(min, max, alpha); }
float4 Fract4(float4 input) { return frac(input); }
float4 Absolute4(float4 input) { return abs(input); }

float3 Normalize3(float3 input) { return normalize(input); }
float3 Negate3(float3 input) { return -input; }
float Negate1(float input) { return -input; }

void Separate2(float2 input, out float r, out float g) { r = input.r; g = input.g; }
void Combine2(float r, float g, out float2 fnResult) { fnResult = float2(r, g); }

void Separate3(float3 input, out float r, out float g, out float b) { r = input.r; g = input.g; b = input.b; }
void Combine3(float r, float g, float b, out float3 fnResult) { fnResult = float3(r, g, b); }

void Separate4(float4 input, out float r, out float g, out float b, out float a) { r = input.r; g = input.g; b = input.b; a = input.a; }
void Combine4(float r, float g, float b, float a, out float4 fnResult) { fnResult = float4(r, g, b, a); }

void SeparateAlpha(float4 input, out float3 rgb, out float alpha) { rgb = input.rgb; alpha = input.a; }
float4 CombineAlpha(float3 color, float alpha)     { return float4(color, alpha); }

float Cosine1(float x) { return cos(x); }
float Sine1(float x) { return sin(x); }
float Tangent1(float x) { return tan(x); }

float3 Mix3(float3 lhs, float3 rhs, float factor) { return lerp(lhs, rhs, factor); }
float Mix1(float lhs, float rhs, float factor) { return lerp(lhs, rhs, factor); }

float Dot3(float3 lhs, float3 rhs) { return dot(lhs, rhs); }

float PassThrough1(float input) { return input; }
float3 PassThrough3(float3 input) { return input; }
float4 PassThrough(float4 input) { return input; }

float Remap1(float input, float2 inputRange, float2 outputRange)
{
    return lerp(outputRange.x, outputRange.y, (input - inputRange.x) / (inputRange.y - inputRange.x));
}

#endif
