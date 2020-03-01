// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(NODES_CONVERSION_H)
#define NODES_CONVERSION_H

#include "../TechniqueLibrary/Utility/Colour.hlsl"

float3 ConvertSRGBToLinear(float3 srgbInput)    { return SRGBToLinear(srgbInput); }
float3 ConvertLinearToSRGB(float3 linearInput)  { return LinearToSRGB(linearInput); }

float Luminance(float3 srgbInput) { return SRGBLuminance(srgbInput); }

float3 RGB(float r, float g, float b) { return float3(r, g, b); }
float3 HSV(float h, float s, float v) { return HSV2RGB(float3(h, s, v)); }
float3 HSL(float h, float s, float l) { return HSL2RGB(float3(h, s, l)); }

#endif
