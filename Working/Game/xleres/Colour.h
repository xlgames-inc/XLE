// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(COLOUR_H)
#define COLOUR_H

float3 SRGBToLinear_Fast(float3 input)		{ return input*input; }
float3 LinearToSRGB_Fast(float3 input)		{ return sqrt(input); }

float3 LinearToSRGB(float3 input)		    { return pow(input, 1.f/2.2f); }
float3 SRGBToLinear(float3 input)		    { return pow(input, 2.2f); }

static const float LightingScale = 16.f;
static const bool ShadowsPerspectiveProjection = false;

#endif