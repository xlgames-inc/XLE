// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

cbuffer Parameters : register(b0)
{
	int2 SourceMin;
	int2 SourceMax;
	int2 UpdateMin;
	int2 UpdateMax;
	int3 DstTileAddress;
	int SampleArea;
	uint2 TileSize;
}

struct TileCoords
{
	float MinHeight;
	float HeightScale;
	uint WorkingMinHeight;
	uint WorkingMaxHeight;
	float ElementSpacing;
	float HeightOffsetValue;
};

#if VALUE_FORMAT == 62
	#define ValueType uint
#else
	#define ValueType float
	#define QUANTIZE_HEIGHTS
#endif

Texture2D<ValueType>	Input : register(t0);
RWTexture2D<ValueType>	MidwayOutput : register(u1);
RWTexture2D<uint> 		MidwayMaterialFlags : register(u2);

#if (ENCODED_GRADIENT_FLAGS!=0)
	static const uint 		RawHeightMask = 0x3fff;
	static const uint		MaterialFlagsShift = 14;
#else
	static const uint 		RawHeightMask = 0xffff;
	static const uint		MaterialFlagsShift = 16;
#endif

#if defined(QUANTIZE_HEIGHTS)
		// (used for height map updates)
	Texture2DArray<uint>			OldHeights : register(t1);
	RWStructuredBuffer<TileCoords>	TileCoordsBuffer : register(u3);
#endif

#if !defined(FILTER_TYPE)
	#define FILTER_TYPE 1
#endif

ValueType CalculateNewValue(uint3 dispatchThreadId)
{
	int2 origin = SampleArea * dispatchThreadId.xy;

		// SourceMins / SourceMaxs defines the area that we can read from
		// The system should ensure that the source area is much bigger than
		// the "update area" with borders on all sides. We need area around
		// the update area to do downsampling and filtering
		// We need to make sure we don't attempt to read outside of this region.
	if (	origin.x >= SourceMin.x && (origin.x + SampleArea - 1) <= SourceMax.x
		&&	origin.y >= SourceMin.y && (origin.y + SampleArea - 1) <= SourceMax.y) {

		#if FILTER_TYPE == 1
				//	simple box filter for downsampling to the correct LOD
			ValueType sampleTotal = 0;
			for (int y=0; y<SampleArea; ++y)
				for (int x=0; x<SampleArea; ++x)
					sampleTotal += Input[origin + int2(x,y) - SourceMin];
			sampleTotal /= SampleArea * SampleArea;
		#elif FILTER_TYPE == 2
				//	max filter (good for samples that we can't interpolate between)
			ValueType sampleTotal = 0;
			for (int y=0; y<SampleArea; ++y)
				for (int x=0; x<SampleArea; ++x)
					sampleTotal = max(sampleTotal, Input[origin + int2(x,y) - SourceMin]);
		#endif

		return sampleTotal;
	}

	#if defined(QUANTIZE_HEIGHTS)
			//	note that we can't read from a uint16 RWTexture, unfortunately...
			//	that means we need to do 2 steps:
			//		first step --	read from old heights as a SRV
			//		second step --	write to output UAV
		uint compressedHeight = OldHeights[DstTileAddress + uint3(dispatchThreadId.xy, 0)] & RawHeightMask;
		return TileCoordsBuffer[0].MinHeight + float(compressedHeight) * TileCoordsBuffer[0].HeightScale;
	#else
		return 0;
	#endif
}

uint CalculateGradientFlags_TopLOD(int2 baseCoord, float spacing);

uint CalculateGradientFlags(uint2 dispatchThreadId)
{
	int2 origin = SampleArea * dispatchThreadId.xy;

		// SourceMins / SourceMaxs defines the area that we can read from
		// The system should ensure that the source area is much bigger than
		// the "update area" with borders on all sides. We need area around
		// the update area to do downsampling and filtering
		// We need to make sure we don't attempt to read outside of this region.

	if (	origin.x >= SourceMin.x && (origin.x + SampleArea - 1) <= SourceMax.x
		&&	origin.y >= SourceMin.y && (origin.y + SampleArea - 1) <= SourceMax.y) {

			//	simple box filter for downsampling to the correct LOD
			//	we could also try min or max filter for these!
		uint sampleTotal = 0;
		for (int y=0; y<SampleArea; ++y)
			for (int x=0; x<SampleArea; ++x)
				sampleTotal += CalculateGradientFlags_TopLOD(
					origin + int2(x,y), TileCoordsBuffer[0].ElementSpacing);
		sampleTotal /= SampleArea * SampleArea;

		return sampleTotal;
	}

	#if defined(QUANTIZE_HEIGHTS)
		return OldHeights[DstTileAddress + uint3(dispatchThreadId.xy, 0)] >> MaterialFlagsShift;
	#else
		return 0;
	#endif
}

uint HeightValueToUInt(float height)
{
		// note --	there's a problem with negative heights here
		//			we convert to integer, and then do min/max on
		//			that integer. It works fine for positive floating
		//			point numbers (actually the IEEE standards ensure it
		//			will work)... But there are problems with negative numbers
		//			So, make sure the result is always positive
	return asuint(max(0.f, height + TileCoordsBuffer[0].HeightOffsetValue));
}

float UIntToHeightValue(uint input)
{
	return asfloat(input) - TileCoordsBuffer[0].HeightOffsetValue;
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

		#if defined(QUANTIZE_HEIGHTS)
			MidwayMaterialFlags[dispatchThreadId.xy] = CalculateGradientFlags(dispatchThreadId.xy);
		#endif
	}
}

RWTexture2DArray<uint> Destination : register(u0);

[numthreads(6, 6, 1)]
	void CommitToFinal(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	ValueType newHeight = MidwayOutput[dispatchThreadId.xy];

	if (dispatchThreadId.x < TileSize.x && dispatchThreadId.y < TileSize.y) {

			// When doing QUANTIZE_HEIGHTS, we need to re-write the entire tile...
			// this is because the min/max values will have changed. That means every
			// height value needs to adapter to the new min/max values.
		#if defined(QUANTIZE_HEIGHTS)

					// finally calculate the new compressed height & write to the buffer
			float minHeight = UIntToHeightValue(TileCoordsBuffer[0].WorkingMinHeight);
			float maxHeight = UIntToHeightValue(TileCoordsBuffer[0].WorkingMaxHeight);

				// we have to write everything -- because min/max height may have changed!
			uint finalCompressedHeight = uint(
				clamp((newHeight - minHeight) * float(RawHeightMask) / (maxHeight - minHeight),
				0, float(RawHeightMask)));

			#if (ENCODED_GRADIENT_FLAGS!=0)
				finalCompressedHeight |= (MidwayMaterialFlags[dispatchThreadId.xy] & 3) << MaterialFlagsShift;
			#endif

			Destination[DstTileAddress + uint3(dispatchThreadId.xy, 0)] = finalCompressedHeight;

		#else

			if (	(int)dispatchThreadId.x >= UpdateMin.x && (int)dispatchThreadId.x <= UpdateMax.x
				&& 	(int)dispatchThreadId.y >= UpdateMin.y && (int)dispatchThreadId.y <= UpdateMax.y) {

				Destination[DstTileAddress + uint3(dispatchThreadId.xy, 0)] = newHeight;
			}

		#endif

	}
}

[numthreads(6, 6, 1)]
	void DirectToFinal(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (	dispatchThreadId.x < TileSize.x && dispatchThreadId.y < TileSize.y
		&&	(int)dispatchThreadId.x * SampleArea >= UpdateMin.x && (int)dispatchThreadId.x * SampleArea <= UpdateMax.x
		&& 	(int)dispatchThreadId.y * SampleArea >= UpdateMin.y && (int)dispatchThreadId.y * SampleArea <= UpdateMax.y) {

		ValueType newValue = CalculateNewValue(dispatchThreadId);
		Destination[DstTileAddress + uint3(dispatchThreadId.xy, 0)] = newValue;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool CoordIsValid(int2 coord)
{
	return coord.x >= SourceMin.x && coord.x <= SourceMax.x && coord.y >= SourceMin.y && coord.y <= SourceMax.y;
}

float GetHeight(int2 coord)
{
	if (CoordIsValid(coord)) {
		return Input[coord - SourceMin];
	}
	return 0.f;
}

#include "../Objects/Terrain/GradientFlags.h"

uint CalculateGradientFlags_TopLOD(int2 baseCoord, float spacing)
{
	return CalculateRawGradientFlags(baseCoord, spacing, SlopeThresholdDefault, TransThresholdDefault);
}
