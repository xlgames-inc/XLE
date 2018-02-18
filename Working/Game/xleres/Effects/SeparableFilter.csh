// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Binding.h"
#include "../TextureAlgorithm.h"

Texture2D_MaybeMS<float4>	InputTexture BIND_NUMERIC_T0;
RWTexture2D<float4>			OutputTexture UAV_DYNAMIC_0;

cbuffer Constants BIND_MAT_B0
{
	float4	FilteringWeights0;		// weights for gaussian filter
	float4	FilteringWeights1;
	float4	FilteringWeights2;
}
cbuffer ClampingWindow BIND_MAT_B1
{
    float2 clampingMins;
    float2 clampingMaxs;
}

[numthreads(16, 16, 1)]
    void HorizontalBlur11NoScale(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	const int offset[] = { -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5 };
	const uint sampleCount = 11;

	float FixedWeights[11];
	FixedWeights[ 0] = FilteringWeights0.x;
	FixedWeights[ 1] = FilteringWeights0.y;
	FixedWeights[ 2] = FilteringWeights0.z;
	FixedWeights[ 3] = FilteringWeights0.w;
	FixedWeights[ 4] = FilteringWeights1.x;
	FixedWeights[ 5] = FilteringWeights1.y;
	FixedWeights[ 6] = FilteringWeights1.z;
	FixedWeights[ 7] = FilteringWeights1.w;
	FixedWeights[ 8] = FilteringWeights2.x;
	FixedWeights[ 9] = FilteringWeights2.y;
	FixedWeights[10] = FilteringWeights2.z;

    int2 pixelCoord = dispatchThreadId.xy;

	float4 result = 0.0.xxxx;
	[unroll] for (uint c=0; c<sampleCount; c++) {
		uint2 p;	// clamp may distort the blur around the edges, but it's the cheapest option
		float unclamped = pixelCoord.x+offset[c];
		#if USE_CLAMPING_WINDOW==1
			p.x = uint(max(clampingMins.x, min(clampingMaxs.x, unclamped)) + 0.5f);
		#else
			p.x = uint(max(0, min(inputDims.x-1, unclamped)) + 0.5f);
		#endif
		p.y = pixelCoord.y;
		float4 texSample = LoadFloat4(InputTexture, p, 0);
		result.rgba += texSample.rgba * FixedWeights[c];
	}

    OutputTexture[pixelCoord] = result;
}

[numthreads(16, 16, 1)]
    void VerticalBlur11NoScale(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	const int offset[] = { -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5 };
	const uint sampleCount = 11;

	float FixedWeights[11];
	FixedWeights[ 0] = FilteringWeights0.x;
	FixedWeights[ 1] = FilteringWeights0.y;
	FixedWeights[ 2] = FilteringWeights0.z;
	FixedWeights[ 3] = FilteringWeights0.w;
	FixedWeights[ 4] = FilteringWeights1.x;
	FixedWeights[ 5] = FilteringWeights1.y;
	FixedWeights[ 6] = FilteringWeights1.z;
	FixedWeights[ 7] = FilteringWeights1.w;
	FixedWeights[ 8] = FilteringWeights2.x;
	FixedWeights[ 9] = FilteringWeights2.y;
	FixedWeights[10] = FilteringWeights2.z;

    int2 pixelCoord = dispatchThreadId.xy;

	float4 result = 0.0.xxxx;
	[unroll] for (uint c=0; c<sampleCount; c++) {
		uint2 p;	// clamp may distort the blur around the edges, but it's the cheapest option
		p.x = pixelCoord.x;
		float unclamped = pixelCoord.y+offset[c];
		#if USE_CLAMPING_WINDOW==1
			p.y = uint(max(clampingMins.y, min(clampingMaxs.y, unclamped)) + 0.5f);
		#else
			p.y = uint(max(0, min(inputDims.y-1, unclamped)) + 0.5f);
		#endif
		float4 texSample = LoadFloat4(InputTexture, p, 0);
		result.rgba += texSample.rgba * FixedWeights[c];
	}

	OutputTexture[pixelCoord] = result;
}
