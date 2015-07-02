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

	float ChangeToSoftConstant;		// = 0.0001f;
	float SoftFlowConstant;			// = 0.05f;
	float SoftChangeBackConstant;	// = 0.9f;
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

float2 CalculateVel2D(int2 baseCoord)
{
		// velXX values are water flowing in
	float vel[9];

#if defined(DUPLEX_VEL) ///////////////////////////////////////////////////////////////////////////

	float centerVel[AdjCellCount];
	LoadVelocities(centerVel, NormalizeGridCoord(int2(baseCoord.xy) + SimulatingIndex * SHALLOW_WATER_TILE_DIMENSION));

	for (uint c=0; c<AdjCellCount; ++c) {
		float temp[AdjCellCount];
		LoadVelocities(temp, NormalizeGridCoord(int2(baseCoord.xy) + SimulatingIndex * SHALLOW_WATER_TILE_DIMENSION + AdjCellDir[c]));
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

#else /////////////////////////////////////////////////////////////////////////////////////////////

	float4 centerVelocity		= LoadVelocities4D(baseCoord);
	float4 rightVelocity		= LoadVelocities4D(baseCoord + int2( 1, 0));
	float4 bottomVelocity		= LoadVelocities4D(baseCoord + int2( 0, 1));
	float4 bottomLeftVelocity 	= LoadVelocities4D(baseCoord + int2(-1, 1));
	float4 bottomRightVelocity 	= LoadVelocities4D(baseCoord + int2( 1, 1));

	vel[0] = -centerVelocity.x;
	vel[1] = -centerVelocity.y;
	vel[2] = -centerVelocity.z;
	vel[3] = -centerVelocity.w;
	vel[4] = 0.f;
	vel[5] = rightVelocity.w;
	vel[6] = bottomLeftVelocity.z;
	vel[7] = bottomVelocity.y;
	vel[8] = bottomRightVelocity.x;

#endif ////////////////////////////////////////////////////////////////////////////////////////////

		// Calculate the velocities in each direction
		//		0  1  2
		//		3  4  5
		//		6  7  8

	float2 vel2d = 0.0.xx;
	vel2d.x += 1.f/6.f * (vel[3] - vel[5]);
	vel2d.y += 1.f/6.f * (vel[1] - vel[7]);
	vel2d.x += 1.f/6.f * sqrtHalf * (vel[0] + vel[6] - vel[2] - vel[8]);
	vel2d.y += 1.f/6.f * sqrtHalf * (vel[0] + vel[2] - vel[6] - vel[8]);
	return vel2d;
}

//
// 		First, we update the sediment levels in a each cell, either disolving hard material
// 		into the water, or dispositing it back into hard materials. In this first step,
// 		we won't move any of the soft material.
//
[numthreads(16, 16, 1)]
	void		UpdateSediment(uint3 dispatchThreadId : SV_DispatchThreadID)
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
	float3 terrainNormal = normalize(cross(terrainTangentYDir, terrainTangentXDir));

	float3 waterSurfaceTangentX = normalize(float3(1.f, 0.f, 0.f)); 	// rightWaterHeight-centerWaterHeight));
	float3 waterSurfaceTangentY = normalize(float3(0.f, 1.f, 0.f)); 	// bottomTerrainHeight-centerWaterHeight));

	float2 vel2d = CalculateVel2D(baseCoord);
	float magv = length(vel2d);
	float3 Vdir;
	if (magv > 1e-5f) {
		Vdir = waterSurfaceTangentX * (vel2d.x / magv) + waterSurfaceTangentY * (vel2d.y/magv);
	} else {
		Vdir = 0.0.xxx;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	float initialSediment = SoftMaterials[baseCoord.xy];
	float initialHard = HardMaterials[baseCoord.xy];

		// Calculate the "hard to soft" factor. This is the rate of
		// change of rock and hard material into soil and softer material
		// that can be transported along the grid.
		//
		// This calculation very closely matches
		//		Fast Hydraulic and Thermal Erosion on the GPU, Bal´azs J´ak´o
		//

	const float Kc = 2.f; 		// 1e-1f;		(effectively, max sediment that can be moved in one second)
	const float R = 1.f;		// (variable hardness)
	const float Ks = 0.03f;		// hard to soft rate
	const float Kd = 0.1f; 		// soft to hard rate (deposition / settling)
	const float maxSediment = 2.f;	// max sediment per cell (ie, max value in the soft materials array)

	const float depthMax = 50.f;
	float depth = centerWaterHeight - centerTerrainHeight;
	float C = Kc * max(0, dot(-terrainNormal, Vdir)) * magv * saturate(1.f - (depth / depthMax));

	const float deltaTime = 1.f / 30.f;
	float hardToSoft;
	if (initialSediment < C) {
		hardToSoft = R * Ks * (C - initialSediment) * deltaTime;
		hardToSoft = min(hardToSoft, maxSediment-initialSediment);
	} else {
		hardToSoft = max(-initialSediment, Kd * (C - initialSediment) * deltaTime);
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

		// change hard <-> soft, as necessary
	float newSediment = initialSediment + hardToSoft;
	float newHard = initialHard - hardToSoft;

	SoftMaterials[baseCoord.xy] = newSediment;
	HardMaterials[baseCoord.xy] = newHard;
}

float CalculateFlowIn(int2 baseCoord)
{
#if defined(DUPLEX_VEL) ///////////////////////////////////////////////////////////////////////////

	float result = 0.f;
		// for each adjacent cell, let's calculate the percentage of it's
		// soft material that will flow into this cell.
	for (uint c=0; c<AdjCellCount; ++c) {
		float temp[AdjCellCount];
		int3 normCoord = NormalizeGridCoord(int2(baseCoord.xy) + SimulatingIndex * SHALLOW_WATER_TILE_DIMENSION + AdjCellDir[c]);
		LoadVelocities(temp, normCoord);

		float intoThis = temp[AdjCellComplement[c]];
		float flowTotal = 0.f;
		for (uint c2=0; c2<AdjCellCount; ++c2) flowTotal += temp[c2];

		float otherSoft = SoftMaterials[baseCoord.xy + AdjCellDir[c]];
		result += otherSoft * intoThis / flowTotal;
	}

	return result;

#else
	return 0.f;	// not implemented
#endif
}

//
//		Second, move the sediment along with the water flow
//
[numthreads(16, 16, 1)]
	void		ShiftSediment(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	int2 baseCoord = dispatchThreadId.xy;
	if (	baseCoord.x == 0 || baseCoord.y == 0
		|| 	baseCoord.x >= (simulationSize.x-1) || baseCoord.y >= (simulationSize.y-1)) {
		return;
	}

		// There are two ways to go about this:
		//		1) move a percentage of the soft material from the cells
		//			immediately adjacent based on the velocity
		//		2) query the linearly interpolated sediment value at
		//			the grid position plus the velocity value
		// Both methods have integration and accuracy problems. They should
		// show different results for movement of sediment. We could try to
		// do a more accurate integration method; but it's probably not worth it.

	float2 vel2d = CalculateVel2D(baseCoord);

	float newSediment;
	const uint flowType = 1;
	if (flowType == 0) {
		const float velScale = 1.f;
		newSediment = LoadInterpolatedSoftMaterials(baseCoord.xy - velScale * vel2d);
	} else {
		newSediment = CalculateFlowIn(baseCoord.xy);
	}

	newSediment = max(0.f, newSediment);  // (if the sediment level becomes negative, it will cause the simulation to explode)

	float initialHard = HardMaterials[baseCoord.xy];
	SoftMaterials[baseCoord.xy] = newSediment;
	OutputSurface[gpuCacheOffset + baseCoord.xy] = initialHard; // + newSediment;
}


[numthreads(16, 16, 1)]
	void		ThermalErosion(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	int2 baseCoord = dispatchThreadId.xy;
	if (	baseCoord.x == 0 || baseCoord.y == 0
		|| 	baseCoord.x >= (simulationSize.x-1) || baseCoord.y >= (simulationSize.y-1)) {
		return;
	}

	// This simulates the movement of material that is knocked loose from other means
	// (often just called "thermal erosion"). Material will fall down a slope until the
	// slope reaches a certain angle, called the "talus" angle, which differs from material
	// to material. We shift a small amount of material for every neighbour that exceeds this
	// angle.
	//
	// Note that "InputSoftMaterials" is actually hard materials here!
	//
	// We should not move material out of an area that is under water using this method.

	float localInitial = InputSoftMaterials[baseCoord.xy];
	float localWaterHeight = LoadWaterHeight(baseCoord);

	const float slopeAngle = 50.f * pi / 180.f;
	const float tanSlopeAngle = tan(slopeAngle);
	const float elementSpacing = 10.f;
	float spacingMul[AdjCellCount] =
	{
		sqrt2, 1, sqrt2,
		1,        1,
		sqrt2, 1, sqrt2
	};

	const float waterThreshold = 1e-2f;
	const float flowAmount = 0.05f;
	float flowAmountOut = flowAmount;
	if (localInitial < (localWaterHeight - waterThreshold)) flowAmountOut = 0.f;

	float flow = 0.f;
	[unroll] for (uint c=0; c<AdjCellCount; ++c) {
		float neighbour = InputSoftMaterials[baseCoord.xy + AdjCellDir[c]];	// (actually hard materials)
		float diff = neighbour - localInitial;

		float flowThreshold = elementSpacing * spacingMul[c] * tanSlopeAngle;
		if (diff > flowThreshold) {
			float neighbourWaterHeight = LoadWaterHeight(baseCoord.xy + AdjCellDir[c]);
			if (neighbour >= (neighbourWaterHeight - waterThreshold))
				flow += flowAmount;
		} else if (diff < -flowThreshold) {
			flow -= flowAmountOut;
		}
	}

	HardMaterials[baseCoord.xy] = localInitial + flow;
	OutputSurface[gpuCacheOffset + baseCoord.xy] = localInitial + flow;
}
