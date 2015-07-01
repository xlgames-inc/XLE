// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Ocean.h"
#include "OceanShallow.h"

#define DUPLEX_VEL

	//
	//		Inspired by "Dynamic Simulation of Splashing Fluids"
	//			James F. O'Brien and Jessica K. Hodgins	//	//		This is actually a really simple simulation of the water	//		surface (more or less ignoring the bottom of the water	//		volume). It uses the difference in heights of adjacent	//		to cells calculate a pressure differential, which becomes	//		an acceleration value. We maintain velocity values from	//		frame to frame, and update iteratively. It's very cheap!	//	//		But interaction with the shore line might not be correct,	//		because the term that takes into account the water depth	//		seems a bit rough.	//

#if defined(WRITING_HEIGHTS) /////////////////////////////

	RWTexture2DArray<float>	WaterHeights : register(u0);

	Texture2DArray<float>	Velocities0	 : register(t5);
	Texture2DArray<float>	Velocities1	 : register(t6);
	Texture2DArray<float>	Velocities2	 : register(t7);
	Texture2DArray<float>	Velocities3	 : register(t8);

	#if defined(DUPLEX_VEL)
		Texture2DArray<float>	Velocities4	 : register(t9);
		Texture2DArray<float>	Velocities5	 : register(t10);
		Texture2DArray<float>	Velocities6	 : register(t11);
		Texture2DArray<float>	Velocities7  : register(t12);
	#endif

#else /////////////////////////////////////////////////////

	Texture2DArray<float>	WaterHeights : register(t5);

	RWTexture2DArray<float>	Velocities0	 : register(u0);
	RWTexture2DArray<float>	Velocities1	 : register(u1);
	RWTexture2DArray<float>	Velocities2	 : register(u2);
	RWTexture2DArray<float>	Velocities3	 : register(u3);

	#if defined(DUPLEX_VEL)
		RWTexture2DArray<float>	Velocities4	 : register(u4);
		RWTexture2DArray<float>	Velocities5	 : register(u5);
		RWTexture2DArray<float>	Velocities6	 : register(u6);
		RWTexture2DArray<float>	Velocities7  : register(u7);
	#endif

#endif ////////////////////////////////////////////////////


Texture2D<uint>				LookupTable	 : register(t3);

cbuffer Constants : register(b2)
{
	int2	SimulatingIndex;
	uint	ArrayIndex;
}

static const float g = 9.8f;
static const float WaterDensity = 999.97;		// (kg/m^2)
static const float DeltaTime = 1.f / 60.f;
static const float VelResistance = .97f;
static const float4 EdgeVelocity = 0.0.xxxx;

///////////////////////////////////////////////////
	//   c o m p r e s s i o n   //
///////////////////////////////////////////////////

cbuffer CompressionConstants
{
	float3 CompressionMidPoint;
	float CompressionRadius;
}

float2 WorldPositionFromElementIndex(int2 eleIndex)
{
	return float2(SimulatingIndex + eleIndex / float(SHALLOW_WATER_TILE_DIMENSION)) * ShallowGridPhysicalDimension;
}

float CalculateExternalPressure(float2 worldPosition)
{
	float2 off = worldPosition - CompressionMidPoint.xy;
	float distance2DSq = dot(off, off);
	float radiusSq = 100.f * CompressionRadius * CompressionRadius;
	if (distance2DSq < radiusSq) {
		return 1e11f * (1.0f - (distance2DSq / radiusSq));
	}

	return 0.f;
}

///////////////////////////////////////////////////
	//   m a i n   s i m u l a t i o n   //
///////////////////////////////////////////////////

float AccelerationFromPressure(float h0, float h1, float waterDepth, float ep0, float ep1)
{
		//	here we imagine the volume volume as a lot columns (or pipes)
		//	that are applying pressure on each other. The pressure applied
		//	depends on the surface area between the pipes (hense the water
		//	depth term). Just be eye, however, it looks like water depth
		//	might have too large an effect at small depths?
	const float externalPressure0 = ep0, externalPressure1 = ep1;

		// we can factor out "WaterDensity" if there is no external pressure
	const float pressureScalar = 1000.f;
	return max(0, ((g * pressureScalar * (h0 - h1)) / waterDepth + (externalPressure1 - externalPressure0) / (WaterDensity * waterDepth)));
}

#if defined(DUPLEX_VEL)
	groupshared float CachedHeights[3][SHALLOW_WATER_TILE_DIMENSION+2];

		//
		//	Cells:
		//		0     1     2
		//		3   center  4
		//		5     6     7
		//
	static const uint AdjCellCount = 8;

	static const int2 AdjCellDir[] =
	{
		int2(-1, -1), int2( 0, -1), int2(+1, -1),
		int2(-1,  0), 				int2(+1,  0),
		int2(-1, +1), int2( 0, +1), int2(+1, +1)
	};

	static const uint AdjCellComplement[] =
	{
		7, 6, 5,
		4,    3,
		2, 1, 0
	};
#else
	groupshared float CachedHeights[2][SHALLOW_WATER_TILE_DIMENSION+2];
#endif

void CalculateAcceleration(
	out float accelerations[AdjCellCount], uint cellIndex,
	float waterDepth, float2 worldPosition)
{
		//
		//		This method attempts to find the velocity of water moving
		//		through points based on changes in heights. But actually it
		//		doesn't work very well. The get the results I want, this needs
		//		to be replaced with a much better method.
		//
		//		For a given sample point, we should update the velocities
		//		We only need to calculate half of the velocities (because
		//		adjacent cells will calculate the remainder... No need to
		//		double-up)
		//
		//			Velocity storage:
		//				x. to cell (-1, -1)
		//				y. to cell ( 0, -1)
		//				z. to cell (+1, -1)
		//				w. to cell (-1,  0)
		//
		//		We need to find the pressure differentials between this cell
		//		and adjacent cells.
		//
		//		We've already cached the height values for a 2 full rows.
		//		So we can easily find the heights of adjacent cells.
		//
		//		In this calculation, we're going to consider the height values
		//		in the textures to be the heights in the center of each cell.
		//		It may not be rendered that way, but it makes sense from a
		//		processing point of view.
		//

	float centerCellHeight  = CachedHeights[1][1 + cellIndex];

	float cellHeight[AdjCellCount];
	cellHeight[0] = CachedHeights[0][cellIndex];		// cell (-1, -1)
	cellHeight[1] = CachedHeights[0][1 + cellIndex];	// cell ( 0, -1)
	cellHeight[2] = CachedHeights[0][2 + cellIndex];	// cell (+1, -1)
	cellHeight[3] = CachedHeights[1][cellIndex];		// cell (-1,  0)

	#if defined(DUPLEX_VEL)
		cellHeight[4] = CachedHeights[1][1 + cellIndex];	// cell (+1,  0)
		cellHeight[5] = CachedHeights[2][cellIndex];		// cell (-1, +1)
		cellHeight[6] = CachedHeights[2][1 + cellIndex];	// cell ( 0, +1)
		cellHeight[7] = CachedHeights[2][2 + cellIndex];	// cell (+1, +1)
	#endif

		// external pressure from objects pressing on the water (eg, ships, etc)
	const float wsScale = ShallowGridPhysicalDimension / float(SHALLOW_WATER_TILE_DIMENSION);
	float centerEp  = CalculateExternalPressure(worldPosition);

	float ep[AdjCellCount];
	for (uint c=0; c<AdjCellCount; ++c)
		ep[c] = CalculateExternalPressure(worldPosition - AdjCellDir[c] * wsScale);

	for (uint c2=0; c2<AdjCellCount; ++c2) {
		accelerations[c2] = AccelerationFromPressure(centerCellHeight, cellHeight[c2], waterDepth, centerEp, ep[c2]);

			// note -- 	if there's a barrier, we can simulate it by setting the
			//			velocity to zero here, the cell heights are set to very
			//			low numbers for boundaries
		if (cellHeight[c2]<-1000.f) accelerations[c2] = 0.f;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////
	//   a d j a c e n t   g r i d s   //
////////////////////////////////////////////////////////////////////////////////////////////////

static const float EdgeHeight = -10000.f;	// WaterBaseHeight

float CalculateBoundingWaterHeight(int2 address, int2 offset)
{
	uint adjacentSimulatingGrid = CalculateShallowWaterArrayIndex(LookupTable, SimulatingIndex + sign(offset));
	if (adjacentSimulatingGrid < 128) {
		int2 dims = int2(SHALLOW_WATER_TILE_DIMENSION, SHALLOW_WATER_TILE_DIMENSION);
		int3 coords = int3((address.xy + offset.xy + dims) % dims, adjacentSimulatingGrid);
		return WaterHeights[coords];
	}

	return EdgeHeight;
}

void LoadVelocities(out float velocities[AdjCellCount], uint3 coord)
{
	velocities[0] = Velocities0[coord];
	velocities[1] = Velocities1[coord];
	velocities[2] = Velocities2[coord];
	velocities[3] = Velocities3[coord];

	#if defined(DUPLEX_VEL)
		velocities[4] = Velocities4[coord];
		velocities[5] = Velocities5[coord];
		velocities[6] = Velocities6[coord];
		velocities[7] = Velocities7[coord];
	#endif
}

void StoreVelocities(float velocities[AdjCellCount], uint3 coord)
{
#if !defined(WRITING_HEIGHTS)
	Velocities0[coord] = velocities[0];
	Velocities1[coord] = velocities[1];
	Velocities2[coord] = velocities[2];
	Velocities3[coord] = velocities[3];

	#if defined(DUPLEX_VEL)
		Velocities4[coord] = velocities[4];
		Velocities5[coord] = velocities[5];
		Velocities6[coord] = velocities[6];
		Velocities7[coord] = velocities[7];
	#endif
#endif
}

void LoadVelocitiesBoundaryCheck(out float velocities[AdjCellCount], int3 coord)
{
	if (coord.x >= 0 && coord.y >= 0
		&& coord.x < SHALLOW_WATER_TILE_DIMENSION && coord.y < SHALLOW_WATER_TILE_DIMENSION) {
		LoadVelocities(velocities, coord);
	} else {
		int2 absolute = SimulatingIndex * SHALLOW_WATER_TILE_DIMENSION + coord.xy;
		int2 gridIndex = absolute / SHALLOW_WATER_TILE_DIMENSION;
		uint arrayIndex = CalculateShallowWaterArrayIndex(LookupTable, gridIndex);
		if (arrayIndex < 128) {
			uint3 newCoord = uint3(absolute % SHALLOW_WATER_TILE_DIMENSION, arrayIndex);
			LoadVelocities(velocities, newCoord);
		} else {
			for (uint c=0; c<AdjCellCount; ++c)
				velocities[c] = EdgeVelocity;
		}
	}
}

float GetWaterDepth(float waterHeight, float surfaceHeight)
{
	return max(1e-2f, waterHeight - surfaceHeight);
}

////////////////////////////////////////////////////////////////////////////////////////////////
	//   e n t r y   p o i n t s   //
////////////////////////////////////////////////////////////////////////////////////////////////
[numthreads(SHALLOW_WATER_TILE_DIMENSION, 1, 1)]
	void		UpdateVelocities(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	int3 baseCoord = int3(dispatchThreadId.xy, ArrayIndex);

		//	In each thread, we're going to calculate the the new height value for a single
		//	cell. First thing we need to load is build the cached heights. We can distribute
		//	this across the thread group easily...

		//	Each tile is updated one after the other. We need to read from some adjacent tiles,
		//	but we can only read from tiles that haven't been updated this frame yet. This means
		//	we can only read top and left (and we need to make sure the tiles are updated in
		//	the right order)

	uint x = dispatchThreadId.x;
	float centerWaterHeight = WaterHeights[baseCoord];
	CachedHeights[1][1+x] = centerWaterHeight;
	if (x==0) {
		CachedHeights[1][0] = CalculateBoundingWaterHeight(baseCoord.xy, int2(-1,0));
		CachedHeights[1][SHALLOW_WATER_TILE_DIMENSION+1] = CalculateBoundingWaterHeight(baseCoord.xy, int2(SHALLOW_WATER_TILE_DIMENSION,0));
	}

		// We must also write a cache from immediately above. Except for the first row, it's easy
	if (dispatchThreadId.y==0) {
		CachedHeights[0][1+x] = CalculateBoundingWaterHeight(baseCoord.xy, int2(0,-1));
		if (x==0) {
			CachedHeights[0][0] = CalculateBoundingWaterHeight(baseCoord.xy, int2(-1,-1));
			CachedHeights[0][SHALLOW_WATER_TILE_DIMENSION+1] = CalculateBoundingWaterHeight(baseCoord.xy, int2(SHALLOW_WATER_TILE_DIMENSION,-1));
		}
	} else {
		CachedHeights[0][1+x] = WaterHeights[baseCoord+int3(0,-1,0)];
		if (x==0) {
			CachedHeights[0][0] = CalculateBoundingWaterHeight(baseCoord.xy+int2(0,-1), int2(-1,0));
			CachedHeights[0][SHALLOW_WATER_TILE_DIMENSION+1] = CalculateBoundingWaterHeight(baseCoord.xy+int2(0,-1), int2(SHALLOW_WATER_TILE_DIMENSION,0));
		}
	}

	#if defined(DUPLEX_VEL)

			// Also need to cache the height immediately below
		if (dispatchThreadId.y==(SHALLOW_WATER_TILE_DIMENSION-1)) {
			CachedHeights[2][1+x] = CalculateBoundingWaterHeight(baseCoord.xy, int2(0,1));
			if (x==0) {
				CachedHeights[2][0] = CalculateBoundingWaterHeight(baseCoord.xy, int2(-1,1));
				CachedHeights[2][SHALLOW_WATER_TILE_DIMENSION+1] = CalculateBoundingWaterHeight(baseCoord.xy, int2(SHALLOW_WATER_TILE_DIMENSION,1));
			}
		} else {
			CachedHeights[2][1+x] = WaterHeights[baseCoord+int3(0,1,0)];
			if (x==0) {
				CachedHeights[2][0] = CalculateBoundingWaterHeight(baseCoord.xy+int2(0,1), int2(-1,0));
				CachedHeights[2][SHALLOW_WATER_TILE_DIMENSION+1] = CalculateBoundingWaterHeight(baseCoord.xy+int2(0,1), int2(SHALLOW_WATER_TILE_DIMENSION,0));
			}
		}

	#endif

	float centerSurfaceHeight = LoadSurfaceHeight(baseCoord.xy);
	float2 worldPosition = WorldPositionFromElementIndex(baseCoord.xy);
	float waterDepth = GetWaterDepth(centerWaterHeight, centerSurfaceHeight);

	GroupMemoryBarrierWithGroupSync();

	float acceleration[AdjCellCount], velocities[AdjCellCount];
	CalculateAcceleration(acceleration, x, waterDepth, worldPosition);
	LoadVelocities(velocities, baseCoord);

	float velSum = 0.f;
	for (uint c=0; c<AdjCellCount; ++c) {
		float newVel = velocities[c] * VelResistance + DeltaTime * acceleration[c];
		velocities[c] = newVel;
		velSum += newVel;
	}

		// Prevent the velocity values getting too high when we have little water available
		// The velocity calculation gives us high velocity values for shallow water. This will
		// end up generating new water, if it's not clamped.
	float clamp = min(1, waterDepth / (velSum * DeltaTime));
	for (uint c=0; c<AdjCellCount; ++c)
		velocities[c] *= clamp;

	StoreVelocities(velocities, baseCoord);
}

////////////////////////////////////////////////////////////////////////////////////////////////

float4 LoadVelocities4D(uint3 coord)
{
	float vel[AdjCellCount];
	LoadVelocities(vel, coord);
	return float4(vel[0], vel[1], vel[2], vel[3]);
}

float4 GetRightVelocity(int2 address)
{
	uint rightSimulatingGrid = CalculateShallowWaterArrayIndex(LookupTable, SimulatingIndex + int2(1, 0));
	if (rightSimulatingGrid < 128) {
		uint3 coords = uint3(0, address.y, rightSimulatingGrid);
		return LoadVelocities4D(coords);
	}

	return EdgeVelocity;
}

float4 GetLeftVelocity(int2 address)
{
	uint leftSimulatingGrid = CalculateShallowWaterArrayIndex(LookupTable, SimulatingIndex + int2(-1, 0));
	if (leftSimulatingGrid < 128) {
		uint3 coords = uint3(SHALLOW_WATER_TILE_DIMENSION-1, address.y, leftSimulatingGrid);
		return LoadVelocities4D(coords);
	}

	return EdgeVelocity;
}

float4 GetBottomVelocity(int2 address)
{
	uint bottomSimulatingGrid = CalculateShallowWaterArrayIndex(LookupTable, SimulatingIndex + int2(0, 1));
	if (bottomSimulatingGrid < 128) {
		uint3 coords = uint3(address.x, 0, bottomSimulatingGrid);
		return LoadVelocities4D(coords);
	}

	return EdgeVelocity;
}

float4 GetBottomLeftVelocity(int2 address)
{
	uint bottomLeftSimulatingGrid = CalculateShallowWaterArrayIndex(LookupTable, SimulatingIndex + int2(-1, 1));
	if (bottomLeftSimulatingGrid < 128) {
		uint3 coords = uint3(SHALLOW_WATER_TILE_DIMENSION-1, 0, bottomLeftSimulatingGrid);
		return LoadVelocities4D(coords);
	}

	return EdgeVelocity;
}

float4 GetBottomRightVelocity(int2 address)
{
	uint bottomRightSimulatingGrid = CalculateShallowWaterArrayIndex(LookupTable, SimulatingIndex + int2(1, 1));
	if (bottomRightSimulatingGrid < 128) {
		uint3 coords = uint3(0, 0, bottomRightSimulatingGrid);
		return LoadVelocities4D(coords);
	}

	return EdgeVelocity;
}

////////////////////////////////////////////////////////////////////////////////////////////////

[numthreads(SHALLOW_WATER_TILE_DIMENSION, 1, 1)]
	void		UpdateHeights(uint3 dispatchThreadId : SV_DispatchThreadID)
{

		//	After we've calculated all of the velocities for every tile, we
		//	can update the height values. To update the height values for a cell,
		//	we need to know the velocities for all adjacent cells -- and that isn't
		//	known until we've done a complete pass through UpdateVelocities.

	int3 baseCoord = int3(dispatchThreadId.x, dispatchThreadId.y, ArrayIndex);

		// calculate the velocities in each direction
		//		00    10    20
		//		01          21
		//      02    12    22

#if defined(DUPLEX_VEL)

	float centerVel[AdjCellCount];
	LoadVelocities(centerVel, baseCoord);

	for (uint c=0; c<AdjCellCount; ++c) {
		float temp[AdjCellCount];
		LoadVelocitiesBoundaryCheck(temp, int3(baseCoord) +int3(AdjCellDir[c],0));
		centerVel[c] = centerVel[c] - temp[AdjCellComplement[c]];
	}

	float vel00 = -centerVel[0];
	float vel10 = -centerVel[1];
	float vel20 = -centerVel[2];
	float vel01 = -centerVel[3];
	float vel21 = -centerVel[4];
	float vel02 = -centerVel[5];
	float vel12 = -centerVel[6];
	float vel22 = -centerVel[7];

#else

		// In the "simplex" model, we store a velocity/flux value in only one
		// direction. This only works correctly if the calculation for the
		// flux is calculated identically for both cells (in both direction).
		// In the common pipe model, this isn't true. But we can make some
		// simplifications to make it true. In this simplex model, we store
		// less temporary data, and reduce the shader calculations quite a bit.

	float4 centerVelocity = LoadVelocities4D(baseCoord);
	float4 rightVelocity, bottomVelocity, bottomRightVelocity, bottomLeftVelocity;

	if (dispatchThreadId.y == SHALLOW_WATER_TILE_DIMENSION-1) {
		bottomVelocity = GetBottomVelocity(dispatchThreadId.xy);
		if (dispatchThreadId.x == SHALLOW_WATER_TILE_DIMENSION-1) {
			bottomRightVelocity = GetBottomRightVelocity(dispatchThreadId.xy);
		} else {
			bottomRightVelocity = GetBottomVelocity(dispatchThreadId.xy + uint2(1,0));
		}
		if (dispatchThreadId.x == 0) {
			bottomLeftVelocity = GetBottomLeftVelocity(dispatchThreadId.xy);
		} else {
			bottomLeftVelocity = GetBottomVelocity(dispatchThreadId.xy+int2(-1,1));
		}
	} else {
		bottomVelocity = LoadVelocities(baseCoord+uint3(0,1,0));
		if (dispatchThreadId.x == SHALLOW_WATER_TILE_DIMENSION-1) {
			bottomRightVelocity = GetRightVelocity(dispatchThreadId.xy+uint2(0,1));
		} else {
			bottomRightVelocity = LoadVelocities(baseCoord+uint3(1,1,0));
		}
		if (dispatchThreadId.x == 0) {
			bottomLeftVelocity = GetLeftVelocity(dispatchThreadId.xy+uint2(0,1));
		} else {
			bottomLeftVelocity = LoadVelocities(baseCoord+int3(-1,1,0));
		}
	}

	if (dispatchThreadId.x == SHALLOW_WATER_TILE_DIMENSION-1) {
		rightVelocity = GetRightVelocity(dispatchThreadId.xy);
	} else {
		rightVelocity = LoadVelocities(baseCoord+uint3(1,0,0));
	}

	float vel00 = -centerVelocity.x;
	float vel10 = -centerVelocity.y;
	float vel20 = -centerVelocity.z;
	float vel01 = -centerVelocity.w;
	float vel21 = rightVelocity.w;
	float vel02 = bottomLeftVelocity.z;
	float vel12 = bottomVelocity.y;
	float vel22 = bottomRightVelocity.x;

#endif

		//	If the velocities along the edges, we should get close to volume conservation (ignoring floating point creep).
		//	Ignoring surface height here... Possible to end up with water below the ground surface
	float deltaHeight = DeltaTime * (vel00 + vel10 + vel20 + vel01 + vel21 + vel02 + vel12 + vel22);

	#if defined(WRITING_HEIGHTS)
		const float RainRate = 1.f;

		float centerSurfaceHeight = LoadSurfaceHeight(baseCoord.xy);
		WaterHeights[baseCoord] = max(centerSurfaceHeight, WaterHeights[baseCoord] + deltaHeight + RainRate * DeltaTime);

		// WaterHeights[baseCoord] += deltaHeight;
	#endif
}
