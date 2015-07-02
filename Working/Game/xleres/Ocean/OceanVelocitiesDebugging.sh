// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Ocean.h"
#include "OceanShallow.h"
#include "ShallowFlux.h"
#include "../Transform.h"
#include "../Utility/MathConstants.h"

Texture2DArray<float>	ShallowWaterHeights	: register(t3);

class VSOutput
{
	float4 position : SV_Position;
	float2 gridCoords : GRIDCOORDS;
};

VSOutput vs_main(uint vertexId : SV_VertexId)
{
	const float TileDimension = float(SHALLOW_WATER_TILE_DIMENSION);
	uint2 gridCoords = uint2(vertexId%TileDimension, vertexId/TileDimension);
	float waterHeight = ShallowWaterHeights.Load(uint4(gridCoords, ArrayIndex, 0));
	float3 worldPosition = float3(WorldPositionFromElementIndex(gridCoords), waterHeight);

	worldPosition.z += 2.f;

	VSOutput output;
	output.position = mul(WorldToClip, float4(worldPosition,1));
	output.gridCoords = SimulatingIndex * TileDimension + gridCoords;
	return output;
}

float4 LoadVelocities4D(uint3 coord)
{
	if (coord.z < 0.f) { return 0.0.xxxx; }
	return float4(Velocities0[coord], Velocities1[coord], Velocities2[coord], Velocities3[coord]);
}

bool ArrowStencil(float2 tc)
{
		// Draws an array in the +X direction in texture coordinate space

	float x = min(tc.x, 1.f-tc.x);
	if (x < 0.1f) return false;
	x = (tc.x - .5f) * (.5f/.4f) + .5f;

	float y = min(tc.y, 1.f-tc.y);
	if (x > .5f) {
		x = (1.f - x) / .5f;
		return (2.f * (.5f - y)) < x;
	}
	return y > .3f;
}

float4 ps_main(VSOutput input) : SV_Target0
{
		// load the velocities information from the grid coordinates

		//	Calculate the velocities in each direction
		//		00    10    20
		//		01          21
		//      02    12    22

	int2 baseCoord				= int2(floor(input.gridCoords));

		// velXX values are water flowing in
	float vel[9];

	#if defined(DUPLEX_VEL)

		float centerVel[AdjCellCount];
		LoadVelocities(centerVel, NormalizeGridCoord(baseCoord));

		for (uint c=0; c<AdjCellCount; ++c) {
			float temp[AdjCellCount];
			LoadVelocities(temp, NormalizeGridCoord(int2(baseCoord.xy) + AdjCellDir[c]));
			// centerVel[c] = centerVel[c] - temp[AdjCellComplement[c]];
		}

		vel[0] = -centerVel[0];
		vel[1] = -centerVel[1];
		vel[2] = -centerVel[2];
		vel[3] = -centerVel[3];
		vel[4] = 0;
		vel[5] = -centerVel[4];
		vel[6] = -centerVel[5];
		vel[7] = -centerVel[6];
		vel[8] = -centerVel[7];

	#else
		float4 centerVelocity		= LoadVelocities4D(NormalizeGridCoord(baseCoord));
		float4 rightVelocity		= LoadVelocities4D(NormalizeGridCoord(baseCoord + int2( 1, 0)));
		float4 bottomVelocity		= LoadVelocities4D(NormalizeGridCoord(baseCoord + int2( 0, 1)));
		float4 bottomLeftVelocity	= LoadVelocities4D(NormalizeGridCoord(baseCoord + int2(-1, 1)));
		float4 bottomRightVelocity	= LoadVelocities4D(NormalizeGridCoord(baseCoord + int2( 1, 1)));

		vel[0] = -centerVelocity.x;
		vel[1] = -centerVelocity.y;
		vel[2] = -centerVelocity.z;
		vel[3] = -centerVelocity.w;
		vel[4] = 0.f;
		vel[5] = rightVelocity.w;
		vel[6] = bottomLeftVelocity.z;
		vel[7] = bottomVelocity.y;
		vel[8] = bottomRightVelocity.x;
	#endif

#if 1

	// 0 1 2
	// 3 4 5
	// 6 7 8
	float2 vel2d = 0.0.xx;
	vel2d.x += 1.f/6.f * (vel[3] - vel[5]);
	vel2d.y += 1.f/6.f * (vel[1] - vel[7]);
	vel2d.x += 1.f/6.f * sqrtHalf * (vel[0] + vel[6] - vel[2] - vel[8]);
	vel2d.y += 1.f/6.f * sqrtHalf * (vel[0] + vel[2] - vel[6] - vel[8]);

	float vlen = length(vel2d);
	if (vlen < 1e-3f) return float4(0.0.xxx, 1.f);

	float2 v = vel2d / vlen;

	// rotate grid coords
	float2x2 rotationMatrix = float2x2(
		v.x, v.y,
		v.y, -v.x);
	float2 adjustedGridCoords = mul(rotationMatrix, frac(input.gridCoords) - 0.5.xx) + 0.5.xx;
	return float4(1.0.xxx * ArrowStencil(adjustedGridCoords), 1.f);

#else

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
#endif
}
