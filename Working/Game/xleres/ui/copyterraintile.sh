// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

cbuffer Parameters
{
	int2 SourceMin;
	int2 SourceMax;
	int2 UpdateMin;
	int2 UpdateMax;
	int3 DstTileAddress;
	int SampleArea;
}

struct TileCoords
{
	float MinHeight;
	float HeightScale;
	uint WorkingMinHeight;
	uint WorkingMaxHeight;
};

#if VALUE_FORMAT == 62
	#define ValueType uint
#else
	#define ValueType float
	#define QUANTIZE_HEIGHTS
#endif


Texture2D<ValueType>			Input : register(t0);
RWTexture2D<ValueType>			MidwayOutput : register(u1);

#if defined(QUANTIZE_HEIGHTS)
		// (used for height map updates)
	Texture2DArray<uint>			OldHeights : register(t1);
	RWStructuredBuffer<TileCoords>	TileCoordsBuffer : register(u2);
#endif

ValueType CalculateNewValue(uint3 dispatchThreadId)
{
	int2 origin = SampleArea * dispatchThreadId.xy;
	//if (	origin.x >= UpdateMin.x && (origin.x + SampleArea - 1) <= UpdateMax.x
	//	&&	origin.y >= UpdateMin.y && (origin.y + SampleArea - 1) <= UpdateMax.y) {

			//	simple box filter for downsampling to the correct LOD
		ValueType sampleTotal = 0;
		for (int y=0; y<SampleArea; ++y)
			for (int x=0; x<SampleArea; ++x) {
				sampleTotal += Input[origin + int2(x,y) - SourceMin];
			}
		sampleTotal /= SampleArea * SampleArea;

		// sampleTotal += 100.f;

		return sampleTotal;
	// }

		//	note that we can't read from a uint16 RWTexture, unfortunately...
		//	that means we need to do 2 steps:
		//		first step --	read from old heights as a SRV
		//		second step --	write to output UAV
	uint compressedHeight = OldHeights[DstTileAddress + uint3(dispatchThreadId.xy, 0)];
	return TileCoordsBuffer[0].MinHeight + float(compressedHeight) * TileCoordsBuffer[0].HeightScale;
}

uint HeightValueToUInt(float height)
{
		// note --	there's a problem with negative heights here
		//			we convert to integer, and then do min/max on
		//			that integer. It works fine for positive floating
		//			point numbers (actually the IEEE standards ensure it
		//			will work)... But there are problems with negative numbers
		//			So, make sure the result is always positive
	return asuint(max(0, height));	// ideally we'd add some value here (eg + 5000.f... but it's not working)
}

float UIntToHeightValue(uint input)
{
	return asfloat(input);
}

[numthreads(6, 6, 1)]
	void WriteToMidway(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	ValueType newHeight = CalculateNewValue(dispatchThreadId);

	#if defined(QUANTIZE_HEIGHTS)
			//	We need to find the min/max height
			//		note that with a 33x33 tile grid, we are just 1 row and 1 column too many
			//		to use groupshared variables for this! We have to do min/max into a RWBuffer
			//		.. we also have to split things into 2 separate dispatch calls
		uint heightAsUInt = HeightValueToUInt(newHeight);
		uint ignore;
		InterlockedMin(TileCoordsBuffer[0].WorkingMinHeight, heightAsUInt, ignore);
		InterlockedMax(TileCoordsBuffer[0].WorkingMaxHeight, heightAsUInt, ignore);
	#endif

	uint2 midwayDims;
	MidwayOutput.GetDimensions(midwayDims.x, midwayDims.y);
	if (dispatchThreadId.x < midwayDims.x && dispatchThreadId.y < midwayDims.y) {
		MidwayOutput[dispatchThreadId.xy] = newHeight;
	}
}

RWTexture2DArray<uint>			Destination : register(u0);

[numthreads(6, 6, 1)]
	void CommitToFinal(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	ValueType newHeight = MidwayOutput[dispatchThreadId.xy];

	uint2 midwayDims;
	MidwayOutput.GetDimensions(midwayDims.x, midwayDims.y);

	if (dispatchThreadId.x < midwayDims.x && dispatchThreadId.y < midwayDims.y) {
		#if defined(QUANTIZE_HEIGHTS)
				// finally calculate the new compressed height & write to the buffer
			float minHeight = UIntToHeightValue(TileCoordsBuffer[0].WorkingMinHeight);
			float maxHeight = UIntToHeightValue(TileCoordsBuffer[0].WorkingMaxHeight);

				// we have to write everything -- because min/max height may have changed!
			uint finalCompressedHeight = uint(clamp((newHeight - minHeight) * float(0xffff) / (maxHeight - minHeight), 0, float(0xffff)));

			Destination[DstTileAddress + uint3(dispatchThreadId.xy, 0)] = finalCompressedHeight;
		#else
			Destination[DstTileAddress + uint3(dispatchThreadId.xy, 0)] = newHeight;
		#endif
	}
}
