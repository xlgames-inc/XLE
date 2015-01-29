// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)


RWTexture2D<float4> OutputTexture;
Texture2D<float4> InputTexture;

[numthreads(8, 8, 1)] 
	void Resample(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	uint2 outputDims; uint2 inputDims;
	OutputTexture.GetDimensions(outputDims.x, outputDims.y);
	InputTexture.GetDimensions(inputDims.x, inputDims.y);

	float2 inputCoord = float2(dispatchThreadId.xy) * float2(inputDims) / float2(outputDims);
	float2 base = float2(floor(inputCoord.x), floor(inputCoord.y));
	float2 alpha = inputCoord - base;

		// bilinear filter resampling

	float4 s0 = InputTexture[min(uint2(base)+uint2(0,0), inputDims)];
	float4 s1 = InputTexture[min(uint2(base)+uint2(1,0), inputDims)];
	float4 s2 = InputTexture[min(uint2(base)+uint2(0,1), inputDims)];
	float4 s3 = InputTexture[min(uint2(base)+uint2(1,1), inputDims)];

	float w0 = (1.f-alpha.x) * (1.f-alpha.y);
	float w1 = alpha.x * (1.f-alpha.y);
	float w2 = (1.f-alpha.x) * alpha.y;
	float w3 = alpha.x * alpha.y;

		//	actually getting better results with point resampling.
		//	After the compression, the bilinear sampled texture looks quite dithered
		//	and ugly... it might be a consequence of compressing twice?
	// OutputTexture[dispatchThreadId.xy] = w0 * s0 + w1 * s1 + w2 * s2 + w2 * s2;
	OutputTexture[dispatchThreadId.xy] = s0;
}

