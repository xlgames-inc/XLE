// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)


#include "../Utility/perlinnoise.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

cbuffer Parameters
{
		//	common control parameters
	float2 Center;
	float Radius;
	float Adjustment;

	uint2 SurfaceMins;			// Minimum coord of the "CachedSurface", in terrain uber-surface coords
	uint2 SurfaceMaxs;			// Max coord of the "CachedSurface", in terrain uber-surface coords
	uint2 DispatchOffset;		// Terrain uber-surfacec coord when dispatchThreadId = uint3(0,0,0)
}

float LengthSquared(float2 input) { return dot(input, input); }

cbuffer PaintParameters
{
	uint paintValue;
}

RWTexture2D<uint> OutputSurface : register(u0);
Texture2D<uint> InputSurface;

[numthreads(16, 16, 1)]
	void Paint(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	uint2 surfaceSpaceCoord = DispatchOffset + dispatchThreadId.xy;
	float rsq = LengthSquared(float2(surfaceSpaceCoord) - Center);
	if (surfaceSpaceCoord.x <= SurfaceMaxs.x && surfaceSpaceCoord.y <= SurfaceMaxs.y && rsq < (Radius*Radius)) {
	  OutputSurface[surfaceSpaceCoord - SurfaceMins] = paintValue;
	}
}
