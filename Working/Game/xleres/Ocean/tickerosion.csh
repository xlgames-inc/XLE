// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Ocean.h"
#include "OceanShallow.h"

RWTexture2D<float>		OutputSurface		: register(u0);
RWTexture2D<float>		HardMaterials		: register(u1);
RWTexture2D<float>		SoftMaterials		: register(u2);

Texture2D<float>		InputSoftMaterials	: register(t1);

Texture2DArray<float>	WaterHeights		: register(t3);		// final result will be written here
Texture2D<uint>			LookupTable			: register(t4);

Texture2DArray<float>	Velocities0			: register(t5);
Texture2DArray<float>	Velocities1			: register(t6);
Texture2DArray<float>	Velocities2			: register(t7);
Texture2DArray<float>	Velocities3			: register(t8);

cbuffer TickErosionSimConstants : register(b5)
{
	int2 gpuCacheOffset;
	int2 simulationSize;

	float ChangeToSoftConstant;			// = 0.0001f;
	float SoftFlowConstant;				// = 0.05f;
	float SoftChangeBackConstant;		// = 0.9f;
}

static const int CoordRatio = 1;		// ratio of surface coordinate grid elements to water sim grid elements (in 1 dimension)

float4 LoadVelocitiesFromTileCoord(uint3 coord)
{
		//	We're doing to use a simple box filter for downsampling
		//	from the water sim velocities to terrain coordinate velocities
		//	In some ways, this might be wierd... The velocity values actually
		//	represent the movement of water from one cell to the next. So, we
		//	could just use the cells on the boundary for this calculation. But
		//	we're using an average of a group of cells.
	float4 result = 0.0.xxxx;
	for (uint y=0; y<uint(CoordRatio); ++y)
		for (uint x=0; x<uint(CoordRatio); ++x) {
			result += float4(
				Velocities0[coord + uint3(x,y,0)], Velocities1[coord + uint3(x,y,0)],
				Velocities2[coord + uint3(x,y,0)], Velocities3[coord + uint3(x,y,0)]);
		}

	return result / float(CoordRatio*CoordRatio);
}

float4 LoadVelocities(int2 pt)
{
	pt *= CoordRatio;
	int2 tileCoord = pt / SHALLOW_WATER_TILE_DIMENSION;
	uint ti = CalculateShallowWaterArrayIndex(LookupTable, tileCoord);
	if (ti < 128) {
		return LoadVelocitiesFromTileCoord(int3(pt - tileCoord * SHALLOW_WATER_TILE_DIMENSION, ti));
	}

	return 0.0.xxxx;
}

float LoadWaterHeight(int2 pt)
{
	pt *= CoordRatio;
	int2 tileCoord = pt / SHALLOW_WATER_TILE_DIMENSION;
	uint ti = CalculateShallowWaterArrayIndex(LookupTable, tileCoord);
	if (ti < 128) {
		return WaterHeights[int3(pt - tileCoord * SHALLOW_WATER_TILE_DIMENSION, ti)];
	}

	return 0.f;
}

float LoadInterpolatedSoftMaterials(float2 coord)
{
	int2 dims;
	InputSoftMaterials.GetDimensions(dims.x, dims.y);

		// clamp to the edge of the texture
	coord.x = min(max(coord.x, 0), dims.x);
	coord.y = min(max(coord.y, 0), dims.y);

	float2 floored = floor(coord);
	float2 alpha = coord - floored;
	float2 ceiled = floored + 1.0.xx;

		// A B
		// C D
	float A = InputSoftMaterials[int2(floored)];
	float B = InputSoftMaterials[int2(ceiled.x, floored.y)];
	float C = InputSoftMaterials[int2(floored.x, ceiled.y)];
	float D = InputSoftMaterials[int2(ceiled)];

	return
		  A * (1.0f - alpha.x) * (1.0f - alpha.y)
		+ B * (alpha.x) * (1.0f - alpha.y)
		+ C * (1.0f - alpha.x) * (alpha.y)
		+ D * (alpha.x) * (alpha.y)
		;
}

[numthreads(16, 16, 1)]
	void		main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	int2 baseCoord = dispatchThreadId.xy;
	if (baseCoord.x >= simulationSize.x || baseCoord.y >= simulationSize.y) {
		return;
	}

		//
		//	Here's the basic algorithm.
		//		1. we convert "hard" material to "soft" material based on the velocity
		//			of water flowing about in this cell
		//		2. some of that soft material will get shifted to adjacent cells with
		//			the flow of water.
		//

		//	We need to know the velocity values for this point.
		//	That means reading the velocity values for 5 points,
		//	and combining those results;

		//	Calculate the velocities in each direction
		//		00    10    20
		//		01          21
		//      02    12    22

	float4 centerVelocity		= LoadVelocities(baseCoord);
	float4 rightVelocity		= LoadVelocities(baseCoord + int2( 1, 0));
	float4 bottomVelocity		= LoadVelocities(baseCoord + int2( 0, 1));
	float4 bottomLeftVelocity	= LoadVelocities(baseCoord + int2(-1, 1));
	float4 bottomRightVelocity	= LoadVelocities(baseCoord + int2( 1, 1));

	float centerWaterHeight = LoadWaterHeight(baseCoord);
	float rightWaterHeight = LoadWaterHeight(baseCoord + int2(1,0));
	float leftWaterHeight = LoadWaterHeight(baseCoord + int2(0,1));

	float centerTerrainHeight = LoadSurfaceHeight(baseCoord);
	float rightTerrainHeight = LoadSurfaceHeight(baseCoord + int2(1,0));
	float bottomTerrainHeight = LoadSurfaceHeight(baseCoord + int2(0,1));
	float terrainEleDist = 3.f; // ??

	float3 terrainTangentX = normalize(float3(terrainEleDist, 0.f, rightTerrainHeight-centerTerrainHeight));
	float3 terrainTangentY = normalize(float3(0.f, terrainEleDist, bottomTerrainHeight-centerTerrainHeight));
	float3 terrainNormal = cross(terrainTangentX, terrainTangentY);

	float3 waterSurfaceTangentX = normalize(float3(1.f, 0.f, rightWaterHeight-centerWaterHeight));
	float3 waterSurfaceTangentY = normalize(float3(1.f, 0.f, bottomTerrainHeight-centerWaterHeight));

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

		// Calculate the velocities in each direction
		//		0  1  2
		//		3  4  5
		//		6  7  8

	float2 vel2d = float2(vel[3] - vel[5], vel[1] - vel[7]);
	float magv = length(vel2d);
	float3 V = waterSurfaceTangentX * vel2d.x + waterSurfaceTangentY * vel2d.y;


		// float initialTransportedSediment = InputSoftMaterials[baseCoord.xy];

		// Get the new initial sediment value from by querying the grid at
		// an interpolated position based on the velocity vector.
		// There are two ways to go about this:
		//		1) move a percentage of the soft material from the cells
		//			immediately adjacent based on the velocity
		//		2) query the linearly interpolated sediment value at
		//			the grid position plus the velocity value
		// Both methods have integration and accuracy problems. They should
		// show different results for movement of sediment.
		// Trying method 2 here.
		//
		// Note that the integration is in a slightly different order to
		//		"Fast Hydraulic and Thermal Erosion on the GPU"
		//		http://www.cescg.org/CESCG-2011/papers/TUBudapest-Jako-Balazs.pdf
		//
		// The order in that paper would require a second pass (and require
		// calculating the velocity vector in both passes). It's a small optimisation,
		// hopefully it shouldn't have a significant negative effect on the result.

	float initialTransportedSediment = LoadInterpolatedSoftMaterials(baseCoord + vel2d);
	float initialHard = HardMaterials[baseCoord.xy];

		// Calculate the "hard to soft" factor. This is the rate of
		// change of rock and hard material into soil and softer material
		// that can be transported along the grid.
	const float Kc = 1.;
	const float depthMax = 100.f;
	float depth = centerWaterHeight - centerTerrainHeight;
	float C = Kc * dot(-terrainNormal, V) * magv * saturate(1.f - (depth / depthMax));

	const float R = 1.f;		// (variable hardness)
	const float Ks = 1.f;		// hard to soft rate
	const float Kd = 1.f;		// soft to hard rate (deposition / settling)

	float hardToSoft;
	if (initialTransportedSediment < C) {
		hardToSoft = R * Ks * (C - initialTransportedSediment);
	} else {
		hardToSoft = Kd * (C - initialTransportedSediment);
	}

		////////////////////////////////////////////

	const float deltaTime = 1.f;
	float newSediment = initialTransportedSediment + deltaTime * hardToSoft;
	float newHard = initialHard - deltaTime * hardToSoft;
#if 0
	SoftMaterials[baseCoord.xy] = newSediment;
	HardMaterials[baseCoord.xy] = newHard;

	OutputSurface[gpuCacheOffset + baseCoord.xy] = newHard + newSediment;
#else
	OutputSurface[gpuCacheOffset + baseCoord.xy] = SoftMaterials[baseCoord.xy] + initialHard;
#endif

#if 0

		//	We want to know quickly water is moving in the system. Lets
		//	use the sum of the absolute velocities to get an approximation
		//	of movement ferocity.
	float absoluteMovement = 0.f;
	for (uint q=0; q<9; ++q) {
		absoluteMovement += abs(vel[q]);
		vel[q] = sign(vel[q]);
	}

		//	this "absolute movement" can be used to turn hard materials (packed dirt, stone)
		//	into soft materials that will shift with the flow

	float initialSoft = InputSoftMaterials[baseCoord.xy];
	float initialHard = HardMaterials[baseCoord.xy];

		// the depth scalar value here really helps to reduce spikes!
		//	-- but it's a little wierd from a physical point of view
	float depth  = centerWaterHeight - (initialSoft + initialHard);
	float depthScalar = 1.f; // exp(-max(0, .125f * depth));

		// slow down change-to-soft if we already have soft material building up
	float alreadySoftScalar = exp(-max(0.f, .5f * initialSoft));

	float changeToSoft = alreadySoftScalar * depthScalar * ChangeToSoftConstant * absoluteMovement;

	// if (absoluteMovement > 0.01f) {
	{
		changeToSoft = 100.f * alreadySoftScalar * ChangeToSoftConstant;
	}
	changeToSoft -= initialSoft * SoftChangeBackConstant;	/// soft back to hard slowly
	changeToSoft = min(changeToSoft, initialHard);

	float newHard = initialHard - changeToSoft;

		//	Allow the soft materials from neighbouring cells to flow into this one
	int2 offsets[9] = {
		int2(-1, -1), int2( 0, -1), int2( 1, -1),
		int2(-1,  0), int2( 0,  0), int2( 1,  0),
		int2(-1,  1), int2( 0,  1), int2( 1,  1)
	};

	const float flowClamp = (1.f / 9.f) / 1.f;
	float softFlow = 0.f;
	for (uint c=0; c<9; ++c) {
		int2 p = baseCoord + offsets[c];
		if (p.x >= 0 && p.y >= 0 && p.x < simulationSize.x && p.y < simulationSize.y) {
			if (vel[c] > 0.f) {
					//	this is water flowing into this cell. Add a percentage
					//	of the soft material from our neighbour.
				softFlow += min(flowClamp, 1e2f * SoftFlowConstant * vel[c]) * InputSoftMaterials[p];
			} else {
					//	this is water flowing out of this cell. Remove a percentage
					//	of the soft material in this cell
				softFlow -= min(flowClamp, 1e2f * SoftFlowConstant * -vel[c]) * initialSoft;
			}
		}
	}

		//	clamp at zero... We can't have an excessive amount flowing up
		//	Note that this clamp can create material... because the adjacent
		//	cells will still get material.
	float newSoft = initialSoft + changeToSoft + softFlow;
	if (newSoft < 0.f) {
		newHard += newSoft;
		newSoft = 0.f;
	}

	SoftMaterials[baseCoord.xy] = newSoft;
	HardMaterials[baseCoord.xy] = newHard;

	OutputSurface[gpuCacheOffset + baseCoord.xy] = newHard + newSoft;
#endif
}
