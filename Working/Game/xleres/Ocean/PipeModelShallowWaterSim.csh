// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Ocean.h"
#include "OceanShallow.h"

	//
	//		Inspired by "Dynamic Simulation of Splashing Fluids"
	//			James F. O�Brien and Jessica K. Hodgins	//	//		This is actually a really simple simulation of the water	//		surface (more or less ignoring the bottom of the water	//		volume). It uses the difference in heights of adjacent	//		to cells calculate a pressure differential, which becomes	//		an acceleration value. We maintain velocity values from	//		frame to frame, and update iteratively. It's very cheap!	//	//		But interaction with the shore line might not be correct,	//		because the term that takes into account the water depth	//		seems a bit rough.	//

RWTexture2DArray<float>		WaterHeights	: register(u0);		// final result will be written here
RWTexture2DArray<float>		Velocities0		: register(u1);
RWTexture2DArray<float>		Velocities1		: register(u2);
RWTexture2DArray<float>		Velocities2		: register(u3);
RWTexture2DArray<float>		Velocities3		: register(u4);
Texture2D<uint>				LookupTable		: register(t3);

cbuffer Constants : register(b2)
{
	int2	SimulatingIndex;
	uint	ArrayIndex;
	int		buffer;
}

static const float g = 9.8f;
static const float waterDensity = 999.97;		// (kg/m�)
static const float DeltaTime = 1.f / 60.f;
static const float VelResistance = .97f;

///////////////////////////////////////////////////
	//   c o m p r e s s i o n   //
///////////////////////////////////////////////////

cbuffer CompressionConstants
{
	float3 CompressionMidPoint;
	float CompressionRadius;
}

float2 WorldPositionFromGridIndex(int2 gridIndex)
{
	float2 worldPosition =
		float2(	(SimulatingIndex.x + float(gridIndex.x) / float(SHALLOW_WATER_TILE_DIMENSION)) * ShallowGridPhysicalDimension,
				(SimulatingIndex.y + float(gridIndex.y) / float(SHALLOW_WATER_TILE_DIMENSION)) * ShallowGridPhysicalDimension);
	return worldPosition;
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

		// we can factor out "waterDensity" if there is no external pressure
	const float pressureScalar = 10000.f;
	return ((g * pressureScalar * (h0 - h1)) / waterDepth + (externalPressure1 - externalPressure0) / (waterDensity * waterDepth));
}

groupshared float CachedHeights[2][SHALLOW_WATER_TILE_DIMENSION+2];

float4 CalculateDeltaVelocities(uint cellIndex, float surfaceHeight, float2 worldPosition)
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

	float cellHeight  = CachedHeights[1][1 + cellIndex];
	float cellHeight0 = CachedHeights[0][cellIndex];		// cell (-1, -1)
	float cellHeight1 = CachedHeights[0][1 + cellIndex];	// cell ( 0, -1)
	float cellHeight2 = CachedHeights[0][2 + cellIndex];	// cell (+1, -1)
	float cellHeight3 = CachedHeights[1][cellIndex];		// cell (-1,  0)

		// external pressure from objects pressing on the water (eg, ships, etc)
	float ep  = CalculateExternalPressure(worldPosition);
	float ep0 = CalculateExternalPressure(worldPosition - float2(-1.f, -1.f) * ShallowGridPhysicalDimension / float(SHALLOW_WATER_TILE_DIMENSION));
	float ep1 = CalculateExternalPressure(worldPosition - float2( 0.f, -1.f) * ShallowGridPhysicalDimension / float(SHALLOW_WATER_TILE_DIMENSION));
	float ep2 = CalculateExternalPressure(worldPosition - float2( 1.f, -1.f) * ShallowGridPhysicalDimension / float(SHALLOW_WATER_TILE_DIMENSION));
	float ep3 = CalculateExternalPressure(worldPosition - float2(-1.f,  0.f) * ShallowGridPhysicalDimension / float(SHALLOW_WATER_TILE_DIMENSION));

		// water depth simulation isn't working effectively. It has too large an effect
		// ... just keeping it constant for now.
	float waterDepth = 50.f; // max(1.f, cellHeight - surfaceHeight);

	float a0 = AccelerationFromPressure(cellHeight, cellHeight0, waterDepth, ep, ep0);
	float a1 = AccelerationFromPressure(cellHeight, cellHeight1, waterDepth, ep, ep1);
	float a2 = AccelerationFromPressure(cellHeight, cellHeight2, waterDepth, ep, ep2);
	float a3 = AccelerationFromPressure(cellHeight, cellHeight3, waterDepth, ep, ep3);

		// note -- 	if there's a barrier, we can simulate it by setting the
		//			velocity to zero here, the cell heights are set to very
		//			low numbers for boundaries
	if (cellHeight0<-1000.f) a0 = 0.f;
	if (cellHeight1<-1000.f) a1 = 0.f;
	if (cellHeight2<-1000.f) a2 = 0.f;
	if (cellHeight3<-1000.f) a3 = 0.f;

	return DeltaTime * float4(a0, a1, a2, a3);
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

float4 LoadVelocities(uint3 coord)
{
	return float4(Velocities0[coord], Velocities1[coord], Velocities2[coord], Velocities3[coord]);
}

void StoreVelocities(uint3 coord, float4 vel)
{
	Velocities0[coord] = vel.x;
	Velocities1[coord] = vel.y;
	Velocities2[coord] = vel.z;
	Velocities3[coord] = vel.w;
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
	CachedHeights[1][1+x] = WaterHeights[baseCoord];
	if (x==0) {
		CachedHeights[1][0] = CalculateBoundingWaterHeight(baseCoord.xy, int2(-1,0));
		CachedHeights[1][SHALLOW_WATER_TILE_DIMENSION+1] = CalculateBoundingWaterHeight(baseCoord.xy, int2(1,0));
	}

		// We must also write a cache from immediately above. Except for the first row, it's easy
	if (dispatchThreadId.y==0) {
		CachedHeights[0][1+x] = CalculateBoundingWaterHeight(baseCoord.xy, int2(0,-1));
		if (x==0) {
			CachedHeights[0][0] = CalculateBoundingWaterHeight(baseCoord.xy, int2(-1,-1));
			CachedHeights[0][SHALLOW_WATER_TILE_DIMENSION+1] = CalculateBoundingWaterHeight(baseCoord.xy, int2(1,-1));
		}
	} else {
		CachedHeights[0][1+x] = WaterHeights[baseCoord+int3(0,-1,0)];
		if (x==0) {
			CachedHeights[0][0] = CalculateBoundingWaterHeight(baseCoord.xy+int2(0,-1), int2(-1, 0));
			CachedHeights[0][SHALLOW_WATER_TILE_DIMENSION+1] = CalculateBoundingWaterHeight(baseCoord.xy+int2(0,-1), int2(1, 0));
		}
	}

	float centerSurfaceHeight = LoadSurfaceHeight(baseCoord.xy);
	float2 worldPosition = WorldPositionFromGridIndex(baseCoord.xy);

	GroupMemoryBarrierWithGroupSync();

	float4 deltaV = CalculateDeltaVelocities(x, centerSurfaceHeight, worldPosition);
	StoreVelocities(baseCoord, LoadVelocities(baseCoord) * VelResistance + DeltaTime * deltaV);
}

////////////////////////////////////////////////////////////////////////////////////////////////

static const float4 EdgeVelocity = 0.0.xxxx;

float4 CalculateRightVelocity(int2 address)
{
	uint rightSimulatingGrid = CalculateShallowWaterArrayIndex(LookupTable, SimulatingIndex + int2(1, 0));
	if (rightSimulatingGrid < 128) {
		uint3 coords = uint3(0, address.y, rightSimulatingGrid);
		return LoadVelocities(coords);
	}

	return EdgeVelocity;
}

float4 CalculateLeftVelocity(int2 address)
{
	uint leftSimulatingGrid = CalculateShallowWaterArrayIndex(LookupTable, SimulatingIndex + int2(-1, 0));
	if (leftSimulatingGrid < 128) {
		uint3 coords = uint3(SHALLOW_WATER_TILE_DIMENSION-1, address.y, leftSimulatingGrid);
		return LoadVelocities(coords);
	}

	return EdgeVelocity;
}

float4 CalculateBottomVelocity(int2 address)
{
	uint bottomSimulatingGrid = CalculateShallowWaterArrayIndex(LookupTable, SimulatingIndex + int2(0, 1));
	if (bottomSimulatingGrid < 128) {
		uint3 coords = uint3(address.x, 0, bottomSimulatingGrid);
		return LoadVelocities(coords);
	}

	return EdgeVelocity;
}

float4 CalculateBottomLeftVelocity(int2 address)
{
	uint bottomLeftSimulatingGrid = CalculateShallowWaterArrayIndex(LookupTable, SimulatingIndex + int2(-1, 1));
	if (bottomLeftSimulatingGrid < 128) {
		uint3 coords = uint3(SHALLOW_WATER_TILE_DIMENSION-1, 0, bottomLeftSimulatingGrid);
		return LoadVelocities(coords);
	}

	return EdgeVelocity;
}

float4 CalculateBottomRightVelocity(int2 address)
{
	uint bottomRightSimulatingGrid = CalculateShallowWaterArrayIndex(LookupTable, SimulatingIndex + int2(1, 1));
	if (bottomRightSimulatingGrid < 128) {
		uint3 coords = uint3(0, 0, bottomRightSimulatingGrid);
		return LoadVelocities(coords);
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

	float4 centerVelocity = LoadVelocities(baseCoord);
	float4 rightVelocity, bottomVelocity, bottomRightVelocity, bottomLeftVelocity;

	if (dispatchThreadId.y == SHALLOW_WATER_TILE_DIMENSION-1) {
		bottomVelocity = CalculateBottomVelocity(dispatchThreadId.xy);
		if (dispatchThreadId.x == SHALLOW_WATER_TILE_DIMENSION-1) {
			bottomRightVelocity = CalculateBottomRightVelocity(dispatchThreadId.xy);
		} else {
			bottomRightVelocity = CalculateBottomVelocity(dispatchThreadId.xy + uint2(1,0));
		}
		if (dispatchThreadId.x == 0) {
			bottomLeftVelocity = CalculateBottomLeftVelocity(dispatchThreadId.xy);
		} else {
			bottomLeftVelocity = CalculateBottomVelocity(dispatchThreadId.xy+int2(-1,1));
		}
	} else {
		bottomVelocity = LoadVelocities(baseCoord+uint3(0,1,0));
		if (dispatchThreadId.x == SHALLOW_WATER_TILE_DIMENSION-1) {
			bottomRightVelocity = CalculateRightVelocity(dispatchThreadId.xy+uint2(0,1));
		} else {
			bottomRightVelocity = LoadVelocities(baseCoord+uint3(1,1,0));
		}
		if (dispatchThreadId.x == 0) {
			bottomLeftVelocity = CalculateLeftVelocity(dispatchThreadId.xy+uint2(0,1));
		} else {
			bottomLeftVelocity = LoadVelocities(baseCoord+int3(-1,1,0));
		}
	}

	if (dispatchThreadId.x == SHALLOW_WATER_TILE_DIMENSION-1) {
		rightVelocity = CalculateRightVelocity(dispatchThreadId.xy);
	} else {
		rightVelocity = LoadVelocities(baseCoord+uint3(1,0,0));
	}

		// calculate the velocities in each direction
		//		00    10    20
		//		01          21
		//      02    12    22

	float vel00 = -centerVelocity.x;
	float vel10 = -centerVelocity.y;
	float vel20 = -centerVelocity.z;
	float vel01 = -centerVelocity.w;
	float vel21 = rightVelocity.w;
	float vel02 = bottomLeftVelocity.z;
	float vel12 = bottomVelocity.y;
	float vel22 = bottomRightVelocity.x;

		//	If the velocities along the edges, we should get close to volume conservation (ignoring floating point creep).
		//	Ignoring surface height here... Possible to end up with water below the ground surface
	float deltaHeight = DeltaTime * (vel00 + vel10 + vel20 + vel01 + vel21 + vel02 + vel12 + vel22);

	// float centerSurfaceHeight = LoadSurfaceHeight(baseCoord.xy);
	// WaterHeights[baseCoord] = max(centerSurfaceHeight, WaterHeights[baseCoord] + deltaHeight);

	WaterHeights[baseCoord] += deltaHeight;
}
