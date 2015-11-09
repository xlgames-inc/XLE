// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(COLOUR_H)
#define COLOUR_H

float3 SRGBToLinear_Fast(float3 input)		{ return input*input; }
float3 LinearToSRGB_Fast(float3 input)		{ return sqrt(input); }

float3 LinearToSRGB(float3 input)		    { return pow(max(0.0.xxx, input), 1.f/2.2f); }
float3 SRGBToLinear(float3 input)		    { return pow(max(0.0.xxx, input), 2.2f); }

static const float LightingScale = 1.f;     // note -- LightingScale is currently not working with high res screenshots (it is applied twice, so only 1 is safe)

float4 ByteColor(uint r, uint g, uint b, uint a) { return float4(r/float(0xff), g/float(0xff), b/float(0xff), a/float(0xff)); }

float SRGBLuminance(float3 rgb)
{
    const float3 constants = float3(0.2126f, 0.7152f, 0.0722f);
    return dot(constants, rgb);
}

#endif
