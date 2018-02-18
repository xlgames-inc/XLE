// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(TONE_MAP_H)
#define TONE_MAP_H

#include "../Binding.h"

cbuffer ToneMapSettings BIND_MAT_B0
{
	float3  BloomScale;			        // = (2.f, 2.f, 2.f)
    float   BloomThreshold;             // = 11.f;
    float   BloomRampingFactor;         // = 0.33f;
    float   BloomDesaturationFactor;    // = 0.5f;
    float   SceneKey;			        // = 0.18f;
	float   LuminanceMin;		        // = 0.18f / 3.f;
	float   LuminanceMax;		        // = 0.18f / 0.25f;
	float   Whitepoint;			        // = 11.2f
}

#endif
