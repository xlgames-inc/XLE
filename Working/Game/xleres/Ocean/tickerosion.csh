// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Ocean.h"
#include "OceanShallow.h"
#include "ShallowFlux.h"
#include "../Utility/MathConstants.h"

RWTexture2D<float>		OutputSurface		: register(u0);
RWTexture2D<float>		HardMaterials		: register(u1);
RWTexture2D<float>		SoftMaterials		: register(u2);

Texture2D<float>		InputSoftMaterials	: register(t1);

Texture2DArray<float>	WaterHeights		: register(t3);

cbuffer TickErosionSimConstants : register(b5)
{
	int2 gpuCacheOffset;
	int2 simulationSize;

	float ChangeToSoftConstant;			// = 0.0001f;
	float SoftFlowConstant;				// = 0.05f;
	float SoftChangeBackConstant;		// = 0.9f;
}

static const int CoordRatio = 1;		// ratio of surface coordinate grid elements to water sim grid elements (in 1 dimension)

float LoadWaterHeight(int2 pt)
{
	int3 coord = NormalizeGridCoord(pt * CoordRatio);
	if (coord.z < 0) return 0.f;
	return WaterHeights[coord];
}

float LoadInterpolatedSoftMaterials(float2 coord)
{
	int2 dims;
	InputSoftMaterials.GetDimensions(dims.x, dims.y);

	float2 floored = floor(coord);
	float2 alpha = coord - floored;
	float2 ceiled = floored + 1.0.xx;

		// clamp to the edge of the texture
	floored.x = min(max(floored.x, 0.f), dims.x-1);
	floored.y = min(max(floored.y, 0.f), dims.y-1);
	ceiled.x = min(max(ceiled.x, 0.f), dims.x-1);
	ceiled.y = min(max(ceiled.y, 0.f), dims.y-1);

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
	if (	baseCoord.x == 0 || baseCoord.y == 0
		|| 	baseCoord.x >= (simulationSize.x-1) || baseCoord.y >= (simulationSize.y-1)) {
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

	float centerWaterHeight = LoadWaterHeight(baseCoord);
	float rightWaterHeight = LoadWaterHeight(baseCoord + int2(1,0));
	float leftWaterHeight = LoadWaterHeight(baseCoord + int2(0,1));

	float centerTerrainHeight = LoadSurfaceHeight(baseCoord);
	float rightTerrainHeight = LoadSurfaceHeight(baseCoord + int2(1,0));
	float bottomTerrainHeight = LoadSurfaceHeight(baseCoord + int2(0,1));
	float terrainEleDist = 10.f; // spacing between terrain elements

	float3 terrainTangentXDir = float3(terrainEleDist, 0.f, rightTerrainHeight-centerTerrainHeight);
	float3 terrainTangentYDir = float3(0.f, terrainEleDist, bottomTerrainHeight-centerTerrainHeight);
	float3 terrainNormal = normalize(cross(terrainTangentXDir, terrainTangentYDir));

	float3 waterSurfaceTangentX = normalize(float3(1.f, 0.f, 0.f)); 	// rightWaterHeight-centerWaterHeight));
	float3 waterSurfaceTangentY = normalize(float3(0.f, 1.f, 0.f)); 	// bottomTerrainHeight-centerWaterHeight));

		// velXX values are water flowing in
	float vel[9];

#if defined(DUPLEX_VEL)

	float centerVel[AdjCellCount];
	LoadVelocities(centerVel, NormalizeGridCoord(baseCoord));

	for (uint c=0; c<AdjCellCount; ++c) {
		float temp[AdjCellCount];
		LoadVelocities(temp, NormalizeGridCoord(int2(baseCoord.xy) + AdjCellDir[c]));
		centerVel[c] = centerVel[c] - temp[AdjCellComplement[c]];
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

	float4 centerVelocity		= LoadVelocities(baseCoord);
	float4 rightVelocity		= LoadVelocities(baseCoord + int2( 1, 0));
	float4 bottomVelocity		= LoadVelocities(baseCoord + int2( 0, 1));
	float4 bottomLeftVelocity 	= LoadVelocities(baseCoord + int2(-1, 1));
	float4 bottomRightVelocity 	= LoadVelocities(baseCoord + int2( 1, 1));

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

		// Calculate the velocities in each direction
		//		0  1  2
		//		3  4  5
		//		6  7  8

	float2 vel2d = 0.0.xx;
	vel2d.x += 1.f/6.f * (vel[3] - vel[5]);
	vel2d.y += 1.f/6.f * (vel[1] - vel[7]);
	vel2d.x += 1.f/6.f * sqrtHalf * (vel[0] + vel[2] - vel[6] - vel[8]);
	vel2d.y += 1.f/6.f * sqrtHalf * (vel[0] + vel[6] - vel[2] - vel[8]);
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

	const float velScale = .5f / length(vel2d);
	float initialSediment = LoadInterpolatedSoftMaterials(baseCoord - velScale * vel2d);
	// initialSediment = max(0.f, initialSediment);  (if the sediment level becomes negative, it will cause the simulation to explode)
	float initialHard = HardMaterials[baseCoord.xy];

		// Calculate the "hard to soft" factor. This is the rate of
		// change of rock and hard material into soil and softer material
		// that can be transported along the grid.
	const float Kc = 1.f; // 1e-1;
	const float depthMax = 50.f;
	float depth = centerWaterHeight - centerTerrainHeight;
	float C = Kc * dot(-terrainNormal, V) * magv * saturate(1.f - (depth / depthMax));

	const float R = 1.f;		// (variable hardness)
	const float Ks = 0.25f;		// hard to soft rate
	const float Kd = 0.5f;		// soft to hard rate (deposition / settling)

	const float deltaTime = 1.f / 30.f;
	float hardToSoft;
	if (initialSediment < C) {
		hardToSoft = R * Ks * (C - initialSediment);
	} else {
		hardToSoft = max(-initialSediment/deltaTime, Kd * (C - initialSediment));
	}

		////////////////////////////////////////////

	float newSediment = initialSediment + deltaTime * hardToSoft;
	float newHard = initialHard - deltaTime * hardToSoft;

	SoftMaterials[baseCoord.xy] = newSediment;
	HardMaterials[baseCoord.xy] = newHard;

	OutputSurface[gpuCacheOffset + baseCoord.xy] = max(-1000.f, newHard + newSediment);

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
