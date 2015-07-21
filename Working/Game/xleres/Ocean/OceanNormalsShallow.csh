// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Ocean.h"
#include "OceanShallow.h"

RWTexture2DArray<uint2> OutputDerivativesTexture;
Texture2DArray<float>	ShallowWaterHeights : register(t0);
Texture2D<uint>			LookupTable : register(t1);

	//		Build the derivatives and derivative mip maps for
	//		the shallow water simulation... Supports an array of
	//		inputs (arary index is in the z component of dispatch)

#define BLOCK_DIMENSION 8

cbuffer BuildDerivativesConstants : register(b2)
{
	int2	GridIndex[128];
}

void SetupXHeights(out float heights[BLOCK_DIMENSION+1], uint2 baseCoord, uint arrayIndex, uint rightArrayIndex)
{
	uint x=0;
	for (; x<BLOCK_DIMENSION; ++x) {
		heights[x] = ShallowWaterHeights[uint3(x+baseCoord.x, baseCoord.y, arrayIndex)];
	}

		//	handle border condition... We might need to
		//	fall into a neighbour grid. Note that this is only
		//	required for thread groups that lie on the right
		//	or bottom
	if ((x+baseCoord.x) >= SHALLOW_WATER_TILE_DIMENSION) {
		if (rightArrayIndex < 16) {
			heights[x] = ShallowWaterHeights[uint3(0, baseCoord.y, rightArrayIndex)];
		} else {
				// no tile there... Just smear across
			heights[x] = heights[x-1];
		}
	} else {
		heights[x] = ShallowWaterHeights[uint3(x+baseCoord.x, baseCoord.y, arrayIndex)];
	}
}

[numthreads(SHALLOW_WATER_TILE_DIMENSION/BLOCK_DIMENSION, SHALLOW_WATER_TILE_DIMENSION/BLOCK_DIMENSION, 1)]
	void BuildDerivatives(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	uint arrayIndex = dispatchThreadId.z;

	uint rightArrayIndex		= RightGrid; 		// CalculateShallowWaterArrayIndex(LookupTable, GridIndex[arrayIndex]+int2(1,0));
	uint bottomArrayIndex		= BottomGrid; 		// CalculateShallowWaterArrayIndex(LookupTable, GridIndex[arrayIndex]+int2(0,1));
	uint bottomRightArrayIndex	= BottomRightGrid;	// CalculateShallowWaterArrayIndex(LookupTable, GridIndex[arrayIndex]+int2(1,1));

		// could also do this with a "groupshared" array and do fewer loops in here
	float heights[BLOCK_DIMENSION+1][BLOCK_DIMENSION+1];
	{
		uint y=0;
		for (; y<BLOCK_DIMENSION; ++y) {
			SetupXHeights(
				heights[y],
				uint2(dispatchThreadId.x*BLOCK_DIMENSION, y+dispatchThreadId.y*BLOCK_DIMENSION),
				arrayIndex, rightArrayIndex);
		}

		if ((y+dispatchThreadId.y*BLOCK_DIMENSION) >= SHALLOW_WATER_TILE_DIMENSION) {
			if (bottomArrayIndex < 16) {
				SetupXHeights(
					heights[y], uint2(dispatchThreadId.x*BLOCK_DIMENSION, 0),
					bottomArrayIndex, bottomRightArrayIndex);
			} else {
					// nothing there; just smear from above
					//	(note that we might still have a bottomright array, but just ignore that.
				for (uint x=0; x<BLOCK_DIMENSION+1; ++x) {
					heights[y][x] = heights[y-1][x];
				}
			}
		} else {
			SetupXHeights(
				heights[y], uint2(dispatchThreadId.x*BLOCK_DIMENSION, y+dispatchThreadId.y*BLOCK_DIMENSION),
				arrayIndex, rightArrayIndex);
		}
	}

		//	In the shallow water simulation, we just move the
		//	grids up and down... So calculating the derivative is easy
		//	(and X & Y derivatives remain constant)
	float cellSize = ShallowGridPhysicalDimension / SHALLOW_WATER_TILE_DIMENSION;
	for (uint y=0; y<BLOCK_DIMENSION; ++y) {
		for (uint x=0; x<BLOCK_DIMENSION; ++x) {
			float dhdx0 = (heights[y+0][x+1] - heights[y+0][x]) / cellSize;
			float dhdx1 = (heights[y+1][x+1] - heights[y+1][x]) / cellSize;
			float dhdy0 = (heights[y+1][x+0] - heights[y][x+0]) / cellSize;
			float dhdy1 = (heights[y+1][x+1] - heights[y][x+1]) / cellSize;

			float2 result;
			result.x = lerp(dhdx0, dhdx1, 0.5f);
			result.y = lerp(dhdy0, dhdy1, 0.5f);

			uint3 outCoord = uint3(
				x+dispatchThreadId.x*BLOCK_DIMENSION,
				y+dispatchThreadId.y*BLOCK_DIMENSION,
				arrayIndex);
			const float normalizingScale = .5f;
			result = 0.5f + 0.5f * normalizingScale * result;
			OutputDerivativesTexture[outCoord] = uint2(saturate(result) * 255.f);
		}
	}
}

cbuffer BuildMipsConstants
{
	uint2	OutputDimensions;
};

Texture2DArray<uint2>	SourceDerivativesTexture : register(t4);

[numthreads(8, 8, 1)]
	void BuildDerivativesMipmap(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	uint3 destinationCoords = dispatchThreadId.xyz;
	if (destinationCoords.x < OutputDimensions.x && destinationCoords.y < OutputDimensions.y) {
		uint2 sourceCoords00 = destinationCoords.xy*2;
		uint2 sourceCoords01 = sourceCoords00 + uint2(0,1);
		uint2 sourceCoords10 = sourceCoords00 + uint2(1,0);
		uint2 sourceCoords11 = sourceCoords00 + uint2(1,1);

		uint2 source00 = SourceDerivativesTexture[uint3(sourceCoords00%uint2(OutputDimensions*2),dispatchThreadId.z)];
		uint2 source01 = SourceDerivativesTexture[uint3(sourceCoords01%uint2(OutputDimensions*2),dispatchThreadId.z)];
		uint2 source10 = SourceDerivativesTexture[uint3(sourceCoords10%uint2(OutputDimensions*2),dispatchThreadId.z)];
		uint2 source11 = SourceDerivativesTexture[uint3(sourceCoords11%uint2(OutputDimensions*2),dispatchThreadId.z)];

		uint2 result = (source00 + source01 + source10 + source11) / 4;
		OutputDerivativesTexture[destinationCoords] = uint4(result, 0, 0);
	}
}
