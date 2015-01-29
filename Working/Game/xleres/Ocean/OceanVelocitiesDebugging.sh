// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Ocean.h"
#include "OceanShallow.h"
#include "../Transform.h"

Texture2DArray<float>	ShallowWaterHeights	: register(t3);
Texture2D<uint>			LookupTable			: register(t4);

Texture2DArray<float>	Velocities0			: register(t5);
Texture2DArray<float>	Velocities1			: register(t6);
Texture2DArray<float>	Velocities2			: register(t7);
Texture2DArray<float>	Velocities3			: register(t8);


cbuffer ShallowWaterGridConstants
{
	int2	GridIndex;
	uint	ArrayIndex;
	float2	Offset;
}

class VSOutput
{
	float4 position : SV_Position;
	float2 gridCoords : GRIDCOORDS;
};

int3 NormalizeGridCoord(int2 coord)
{
	int2 tile = coord.xy / SHALLOW_WATER_TILE_DIMENSION;
	uint gridIndex = CalculateShallowWaterArrayIndex(LookupTable, tile);
	if (gridIndex < 128) {
		return int3(coord.xy - tile * SHALLOW_WATER_TILE_DIMENSION, gridIndex);
	}

	return int3(0,0,-1);	// off the edge of the simulation area
}

VSOutput vs_main(uint vertexId : SV_VertexId)
{
	const float TileDimension = float(SHALLOW_WATER_TILE_DIMENSION);
	uint2 gridCoords = uint2(vertexId%TileDimension, vertexId/TileDimension);
	float waterHeight = ShallowWaterHeights.Load(uint4(gridCoords, ArrayIndex, 0));

	float3 worldPosition = float3(
		Offset.x + (GridIndex.x + gridCoords.x / TileDimension) * ShallowGridPhysicalDimension,
		Offset.y + (GridIndex.y + gridCoords.y / TileDimension) * ShallowGridPhysicalDimension,
		waterHeight);
	VSOutput output;
	output.position = mul(WorldToClip, float4(worldPosition,1));
	output.gridCoords = GridIndex * TileDimension + gridCoords;
	return output;
}

float4 LoadVelocities(uint3 coord)
{
	if (coord.z < 0.f) { return 0.0.xxxx; }
	return float4(Velocities0[coord], Velocities1[coord], Velocities2[coord], Velocities3[coord]);
}

float4 ps_main(VSOutput input) : SV_Target0
{
		// load the velocities information from the grid coordinates
	
		//	Calculate the velocities in each direction
		//		00    10    20
		//		01          21
		//      02    12    22

	int2 baseCoord				= int2(floor(input.gridCoords));
	float4 centerVelocity		= LoadVelocities(NormalizeGridCoord(baseCoord));
	float4 rightVelocity		= LoadVelocities(NormalizeGridCoord(baseCoord + int2( 1, 0)));
	float4 bottomVelocity		= LoadVelocities(NormalizeGridCoord(baseCoord + int2( 0, 1)));
	float4 bottomLeftVelocity	= LoadVelocities(NormalizeGridCoord(baseCoord + int2(-1, 1)));
	float4 bottomRightVelocity	= LoadVelocities(NormalizeGridCoord(baseCoord + int2( 1, 1)));

		// velXX values are water flowing in
	float vel[9];
	vel[0] = -centerVelocity.x;
	vel[1] = -centerVelocity.y;
	vel[2] = -centerVelocity.z;
	vel[3] = -centerVelocity.w;
	vel[4] = 0.f;
	vel[5] = rightVelocity.w;
	vel[6] = bottomLeftVelocity.z;
	vel[7] = bottomVelocity.y;
	vel[8] = bottomRightVelocity.x;

		// what part of the grid are we in?
	float2 f = frac(input.gridCoords) - .5.xx;
	// float angle = atan(f.y/f.x);
	float angle = atan2(f.y, f.x);

	int segment;
	const float segmentAngle = 2.f * 3.14159f / 8.f;
	if (angle < -3.5f * segmentAngle) {
		segment = 3;
	} else if (angle < -2.5f * segmentAngle) {
		segment = 6;
	} else if (angle < -1.5f * segmentAngle) {
		segment = 7;
	} else if (angle < -.5f * segmentAngle) {
		segment = 8;
	} else if (angle < .5f * segmentAngle) {
		segment = 5;
	} else if (angle < 1.5f * segmentAngle) {
		segment = 2;
	} else if (angle < 2.5f * segmentAngle) {
		segment = 1;
	} else if (angle < 3.5f * segmentAngle) {
		segment = 0;
	} else {
		segment = 3;
	}

	int2 segmentDirection[9] = 
	{
		int2(-1, -1), int2(0, -1), int2(1, -1),
		int2(-1,  0), int2(0,  0), int2(1,  0),
		int2(-1,  1), int2(0,  1), int2(1,  1)
	};

	float2 flowDirection  = normalize(segmentDirection[segment] * -vel[segment]);
	flowDirection = flowDirection *.5f + .5.xx;
	return float4(flowDirection.x, 0, flowDirection.y, 1);

	if (abs(vel[segment]) < 0.001f)	{ return float4(0, 0, 0, 1); }
	else if (vel[segment] < 0.f)	{ return float4(1, 0, 0, 1); }

	return float4(0, 0, 1, 1);
}

