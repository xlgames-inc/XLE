// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Ocean.h"
#include "OceanShallow.h"
#include "ShallowFlux.h"

	//
	//		Inspired by "Dynamic Simulation of Splashing Fluids"
	//			James F. O'Brien and Jessica K. Hodgins	//	//		This is actually a really simple simulation of the water	//		surface (more or less ignoring the bottom of the water	//		volume). It uses the difference in heights of adjacent	//		to cells calculate a pressure differential, which becomes	//		an acceleration value. We maintain velocity values from	//		frame to frame, and update iteratively. It's very cheap!	//	//		But interaction with the shore line might not be correct,	//		because the term that takes into account the water depth	//		seems a bit rough.	//

#if !defined(WRITING_VELOCITIES) //////////////////////////
	RWTexture2DArray<float>	WaterHeights : register(u0);
#else /////////////////////////////////////////////////////
	Texture2DArray<float>	WaterHeights : register(t5);
#endif ////////////////////////////////////////////////////

static const float g 			 = 9.8f;
static const float WaterDensity  = 999.97;		// (kg/m^2) (important for dynamic compression)
static const float DeltaTime     = 1.f / 60.f;
static const float VelResistance = .99f; // .97f;
static const float4 EdgeVelocity = 0.0.xxxx;
static const float EdgeHeight 	 = -10000.f;	// WaterBaseHeight

	//		Here, "PressureConstant" is particularly important for this model
	//		it determines the rate of movement of the water. The size of
	//		the water grid can be factored in by scaling this value.

///////////////////////////////////////////////////////////////////////////////////////////////////
	//   m a i n   s i m u l a t i o n   //
///////////////////////////////////////////////////////////////////////////////////////////////////

float AccelerationFromPressure(float h0, float h1, float waterDepth, float ep0, float ep1)
{
		//		Here we imagine the volume volume as a lot columns (or pipes)
		//		that are applying pressure on each other. The pressure applied
		//		depends on the surface area between the pipes (hense the water
		//		depth term). Just be eye, however, it looks like water depth
		//		might have too large an effect at small depths?
	const float externalPressure0 = ep0, externalPressure1 = ep1;

		// we can factor out "WaterDensity" if there is no external pressure
	return max(0, ((g * PressureConstant * (h0 - h1)) / waterDepth + (externalPressure1 - externalPressure0) / (WaterDensity * waterDepth)));
	// return max(0, h0 - h1);
}

#if defined(DUPLEX_VEL)
	groupshared float CachedHeights[3][SHALLOW_WATER_TILE_DIMENSION+2];
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
		cellHeight[4] = CachedHeights[1][2 + cellIndex];	// cell (+1,  0)
		cellHeight[5] = CachedHeights[2][cellIndex];		// cell (-1, +1)
		cellHeight[6] = CachedHeights[2][1 + cellIndex];	// cell ( 0, +1)
		cellHeight[7] = CachedHeights[2][2 + cellIndex];	// cell (+1, +1)
	#endif

		// external pressure from objects pressing on the water (eg, ships, etc)
	const float wsScale = ShallowGridPhysicalDimension / float(SHALLOW_WATER_TILE_DIMENSION);
	float centerEp = CalculateExternalPressure(worldPosition);

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

///////////////////////////////////////////////////////////////////////////////////////////////////
	//   l o a d   a n d   s t o r e   //
///////////////////////////////////////////////////////////////////////////////////////////////////

void StoreVelocities(float velocities[AdjCellCount], uint3 coord)
{
#if defined(WRITING_VELOCITIES)
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

void LoadVelocities_BoundaryCheck(out float velocities[AdjCellCount], int3 coord)
{
	if (coord.x >= 0 && coord.y >= 0 && coord.x < SHALLOW_WATER_TILE_DIMENSION && coord.y < SHALLOW_WATER_TILE_DIMENSION) {
		LoadVelocities(velocities, coord);
	} else {
		int3 normalized = NormalizeGridCoord(SimulatingIndex * SHALLOW_WATER_TILE_DIMENSION + coord.xy);
		if (normalized.z >= 0) {
			LoadVelocities(velocities, normalized);
		} else {
			[unroll] for (uint c=0; c<AdjCellCount; ++c)
				velocities[c] = EdgeVelocity;
		}
	}
}

float LoadWaterHeight(int3 pos)
{
	return WaterHeights[pos] + RainQuantityPerFrame;
}

float LoadWaterHeight_BoundaryCheck(int2 absPosition)
{
	int3 normalized = NormalizeGridCoord(absPosition + SimulatingIndex * SHALLOW_WATER_TILE_DIMENSION);
	if (normalized.z < 0)
		return EdgeHeight;

	return LoadWaterHeight(normalized);
}

float GetWaterDepth(float waterHeight, float surfaceHeight)
{
	return max(1e-2f, waterHeight - surfaceHeight);
}

float InitCachedHeights(int3 baseCoord)
{

		//		In each thread, we're going to calculate the the new height value for a single
		//		cell. First thing we need to load is build the cached heights. We can distribute
		//		this across the thread group easily...
		//
		//		Each tile is updated one after the other. We need to read from some adjacent tiles,
		//		but we can only read from tiles that haven't been updated this frame yet. This means
		//		we can only read top and left (and we need to make sure the tiles are updated in
		//		the right order)

	const uint tileDim = SHALLOW_WATER_TILE_DIMENSION;
	uint x = baseCoord.x;
	float centerWaterHeight = LoadWaterHeight(baseCoord);
	CachedHeights[1][1+x] = centerWaterHeight;

		// We must also write a cache from immediately above. Except for the first row, it's easy
	if (baseCoord.y==0) {
		CachedHeights[0][1+x] = LoadWaterHeight_BoundaryCheck(baseCoord.xy + int2(0,-1));
	} else {
		CachedHeights[0][1+x] = LoadWaterHeight(baseCoord+int3(0,-1,0));
	}

	#if defined(DUPLEX_VEL)
			// Also need to cache the height immediately below
		if (baseCoord.y==(tileDim-1)) {
			CachedHeights[2][1+x] = LoadWaterHeight_BoundaryCheck(baseCoord.xy + int2(0,1));
		} else {
			CachedHeights[2][1+x] = LoadWaterHeight(baseCoord+int3(0,1,0));
		}
	#endif

		// special work in thread zero -- here we load the extreme left and right parts
		// other threads will probably be idle in this part
	if (x==0) {
		CachedHeights[1][0] = LoadWaterHeight_BoundaryCheck(baseCoord.xy + int2(-1,0));
		CachedHeights[1][tileDim+1] = LoadWaterHeight_BoundaryCheck(baseCoord.xy + int2(tileDim,0));

		CachedHeights[0][0] = LoadWaterHeight_BoundaryCheck(baseCoord.xy + int2(-1,-1));
		CachedHeights[0][tileDim+1] = LoadWaterHeight_BoundaryCheck(baseCoord.xy + int2(tileDim,-1));

		#if defined(DUPLEX_VEL)
			CachedHeights[2][0] = LoadWaterHeight_BoundaryCheck(baseCoord.xy+int2(-1,1));
			CachedHeights[2][tileDim+1] = LoadWaterHeight_BoundaryCheck(baseCoord.xy+int2(tileDim,1));
		#endif
	}

	return centerWaterHeight;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
	//   e n t r y   p o i n t s   //
///////////////////////////////////////////////////////////////////////////////////////////////////

[numthreads(SHALLOW_WATER_TILE_DIMENSION, 1, 1)]
	void		UpdateVelocities(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	int3 baseCoord = int3(dispatchThreadId.xy, ArrayIndex);
	float centerWaterHeight = InitCachedHeights(baseCoord);
	float centerSurfaceHeight = LoadSurfaceHeight(baseCoord.xy);
	float2 worldPosition = WorldPositionFromElementIndex(baseCoord.xy);
	float waterDepth = GetWaterDepth(centerWaterHeight, centerSurfaceHeight);

	GroupMemoryBarrierWithGroupSync();

	float acceleration[AdjCellCount], velocities[AdjCellCount];
	CalculateAcceleration(acceleration, baseCoord.x, waterDepth, worldPosition);
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

///////////////////////////////////////////////////////////////////////////////////////////////////

float4 LoadVelocities4D(uint3 coord)
{
	float vel[AdjCellCount];
	LoadVelocities(vel, coord);
	return float4(vel[0], vel[1], vel[2], vel[3]);
}

float4 GetRightVelocity(int2 address)
{
	uint rightSimulatingGrid = CalculateShallowWaterArrayIndex(CellIndexLookupTable, SimulatingIndex + int2(1, 0));
	if (rightSimulatingGrid < 128) {
		uint3 coords = uint3(0, address.y, rightSimulatingGrid);
		return LoadVelocities4D(coords);
	}

	return EdgeVelocity;
}

float4 GetLeftVelocity(int2 address)
{
	uint leftSimulatingGrid = CalculateShallowWaterArrayIndex(CellIndexLookupTable, SimulatingIndex + int2(-1, 0));
	if (leftSimulatingGrid < 128) {
		uint3 coords = uint3(SHALLOW_WATER_TILE_DIMENSION-1, address.y, leftSimulatingGrid);
		return LoadVelocities4D(coords);
	}

	return EdgeVelocity;
}

float4 GetBottomVelocity(int2 address)
{
	uint bottomSimulatingGrid = CalculateShallowWaterArrayIndex(CellIndexLookupTable, SimulatingIndex + int2(0, 1));
	if (bottomSimulatingGrid < 128) {
		uint3 coords = uint3(address.x, 0, bottomSimulatingGrid);
		return LoadVelocities4D(coords);
	}

	return EdgeVelocity;
}

float4 GetBottomLeftVelocity(int2 address)
{
	uint bottomLeftSimulatingGrid = CalculateShallowWaterArrayIndex(CellIndexLookupTable, SimulatingIndex + int2(-1, 1));
	if (bottomLeftSimulatingGrid < 128) {
		uint3 coords = uint3(SHALLOW_WATER_TILE_DIMENSION-1, 0, bottomLeftSimulatingGrid);
		return LoadVelocities4D(coords);
	}

	return EdgeVelocity;
}

float4 GetBottomRightVelocity(int2 address)
{
	uint bottomRightSimulatingGrid = CalculateShallowWaterArrayIndex(CellIndexLookupTable, SimulatingIndex + int2(1, 1));
	if (bottomRightSimulatingGrid < 128) {
		uint3 coords = uint3(0, 0, bottomRightSimulatingGrid);
		return LoadVelocities4D(coords);
	}

	return EdgeVelocity;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

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

	[unroll] for (uint c=0; c<AdjCellCount; ++c) {
		float temp[AdjCellCount];
		LoadVelocities_BoundaryCheck(temp, int3(baseCoord) + int3(AdjCellDir[c],0));
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

		//		In the "simplex" model, we store a velocity/flux value in only one
		//		direction. This only works correctly if the calculation for the
		//		flux is calculated identically for both cells (in both direction).
		//		In the common pipe model, this isn't true. But we can make some
		//		simplifications to make it true. In this simplex model, we store
		//		less temporary data, and reduce the shader calculations quite a bit.

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

		//		If the velocities along the edges, we should get close to volume conservation (ignoring floating point creep).
		//		Ignoring surface height here... Possible to end up with water below the ground surface
	float deltaHeight = DeltaTime * (vel00 + vel10 + vel20 + vel01 + vel21 + vel02 + vel12 + vel22);

	#if !defined(WRITING_VELOCITIES)
		float centerSurfaceHeight = LoadSurfaceHeight(baseCoord.xy);
		float depth = max(0, LoadWaterHeight(baseCoord) + deltaHeight - centerSurfaceHeight);
		depth *= EvaporationConstant;
		WaterHeights[baseCoord] = centerSurfaceHeight + depth;
	#endif
}
