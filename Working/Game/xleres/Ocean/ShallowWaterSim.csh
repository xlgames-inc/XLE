// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Ocean.h"
#include "OceanShallow.h"

	//
	//		Following "Rapid, Stable Fluid Dynamics for Computer Graphics"
	//			by Michael Kass and Gavin Miller.
	//
	//		Here's an interesting water simulation that will take into account
	//		the water depth. So waves should bend into the shape of the shore
	//		line as they approach a beach.
	//
	//		(deep ocean should be fine with a simplier dynamic simulation)
	//
	//		Each group should be one full row or column (this way we can store
	//		some temporary information in "groupshared" arrays)... Is this best,
	//		or should we have a temporary texture for the matrix elements?
	//
	//		note -- we could consider using float16 or uint16 elements for these
	//				textures.
	//

RWTexture2DArray<float>	WaterHeights;		// final result will be written here
Texture2DArray<float>	WaterHeightsN1 : register(t1);		// "N-1" iteration
Texture2DArray<float>	WaterHeightsN2 : register(t2);		// "N-2" iteration
Texture2D<uint>			LookupTable    : register(t3);

groupshared float	WorkingHeights[2][SHALLOW_WATER_TILE_DIMENSION+2];
groupshared int		DisplacedWater				 = 0;
groupshared int		DisplacedSincCount			 = 0;
groupshared int		InitialTotalVolume			 = 0;
groupshared int		FinalTotalVolume			 = 0;
groupshared int		TotalVolumeForDistribution	 = 0;
groupshared int		ElementCountAboveSurface	 = 0;

static const float	ThresholdAboveSurface		 = 0.f; // 0.00001f;	// "ThresholdAboveSurface" should be small, and
static const float	MultiplierForVolumeCalc		 = 10000.f;				//		"MultiplierForVolumeCalc" should be large for when there are small amounts of rain moving over the surface
static const int	JacobiIterationCount		 = 26;					// good values: 6, 13, 26

static const float g		 = 9.8f;
static const float DeltaTime = 1.f / 60.f;

///////////////////////////////////////////////////
	//   c o m p r e s s i o n   //
///////////////////////////////////////////////////

float CalculateCompression(uint x, int3 c0)
{
	float2 worldPosition =
		float2(	(float(SimulatingIndex.x) + float(c0.x) / float(SHALLOW_WATER_TILE_DIMENSION)) * ShallowGridPhysicalDimension,
				(float(SimulatingIndex.y) + float(c0.y) / float(SHALLOW_WATER_TILE_DIMENSION)) * ShallowGridPhysicalDimension);
	float2 off = worldPosition - CompressionMidPoint.xy;

	float minCompression = LoadSurfaceHeight(c0);

	float distance2DSq = dot(off, off);
	float radiusSq = CompressionRadius * CompressionRadius;
	if (distance2DSq < radiusSq) {
		float x = 0.75f * min(1.f - distance2DSq / radiusSq, 0.5f);
		// return max(minCompression, lerp(WaterBaseHeight, CompressionMidPoint.z, x));
		return WaterHeightsN1[c0] - x;
	}

	return 5000.f;
}

float SimulateCompression(uint x, int3 cm1, int3 c0, int3 cp1, inout float workingHeight, int bufferIndex=2)
{
	return 1000000.f;

	float compressionX = CalculateCompression(x, c0);
	float compressionXm1 = CalculateCompression(x-1, cm1);
	float compressionXp1 = CalculateCompression(x+1, cp1);

	const float differenceThreshold = 0.001f;
	float diffm1, diff0, diffp1;
	if (bufferIndex >= 2) {
		diff0  = compressionX   - WaterHeightsN1[c0];
		diffm1 = compressionXm1 - WaterHeightsN1[cm1];
		diffp1 = compressionXp1 - WaterHeightsN1[cp1];
	} else {
		diffm1 = compressionXm1 - WorkingHeights[bufferIndex][1+x-1];
		diff0  = compressionX   - WorkingHeights[bufferIndex][1+x];
		diffp1 = compressionXp1 - WorkingHeights[bufferIndex][1+x+1];
	}

	bool cp0 = diff0 < differenceThreshold;
	bool recievesDisplacedWater = false;
	if (!cp0) {
		bool cpm1 = diffm1 < differenceThreshold;
		bool cpp1 = diffp1 < differenceThreshold;
		//if (cpm1 || cpp1) {		 We have to distribute displaced water across all nearby grid elements... If we try to put it just on the edges, it just builds up too much there
			InterlockedAdd(DisplacedSincCount, 1);
			recievesDisplacedWater = true;
		//}
	} else {
			//	it's compressed... limit the water height and
			//	add the displaced amount to nearby displaced water sincs
		InterlockedAdd(DisplacedWater, int(-diff0 * 1000.f));
	}

	GroupMemoryBarrierWithGroupSync();

	if (recievesDisplacedWater) {
			// if we recieve displaced water, then take our share...
		workingHeight += float(DisplacedWater) / (1000.f*float(DisplacedSincCount));
	}

	workingHeight = min(workingHeight, compressionX);
	return compressionX;
}

#define COMPRESS_BEFORE_SIMULATION 0		// just seem to get a better result if we do the compression after the simulation

///////////////////////////////////////////////////
	//   m a i n   s i m u l a t i o n   //
///////////////////////////////////////////////////

float4 BuildCoefficients(uint x, int3 cm1, int3 c0, int3 cp1, float centerSurfaceHeight)
{
		// "distanceExaggerate" is a hack to change the XY coordinate system (to try to get
		//	finer detail waves)
	const float distanceExaggerate = 1.f;
	const float deltaX		 = distanceExaggerate * ShallowGridPhysicalDimension / SHALLOW_WATER_TILE_DIMENSION;
	const float commonFactor = g * DeltaTime * DeltaTime / (2.0f * deltaX * deltaX);

	float d0  = WorkingHeights[0][1+x]   - centerSurfaceHeight;
	float dm1 = WorkingHeights[0][1+x-1] - LoadSurfaceHeight(cm1);
	float dp1 = WorkingHeights[0][1+x+1] - LoadSurfaceHeight(cp1);
	dm1 = max(dm1, .0f);
	d0  = max(d0,  .0f);
	dp1 = max(dp1, .0f);

	float e0  = 1.f + commonFactor * (dm1 + 2.f * d0 + dp1);
	float f0  = -commonFactor * (d0 + dp1);
	float fm1 = -commonFactor * (dm1 + d0);

	float hn1 = max(WaterHeightsN1[c0], centerSurfaceHeight);
	float hn2 = max(WaterHeightsN2[c0], centerSurfaceHeight);
	float rhs = 2.f * hn1 - hn2 + RainQuantityPerFrame;	// we need to pretend that the rain was there in previous frames, also... otherwise the simulation doesn't react well
	return float4(fm1, e0, f0, rhs);
}

void		RunSimulation(uint x, int3 cm1, int3 c0, int3 cp1)
{
		//		First, calculate the elements of the linear matrix.
		//		Each thread calculates a single element.
		//		We do this only once -- using the water height values from
		//		the previous frame. Maybe this is a little non-ideal...
		//		We could recalculate the matrix after certain jacobi iterations...?

	float initialWorkingHeight = WaterHeightsN1[c0];

		//		Deal with displacement / compression pattern
		//		Water must be constrained to the height given by the
		//		compression pattern. We prevent the water from rising
		//		above this height. In some places, extra water must be
		//		displaced to adjacent points. Any removed water will be
		//		equally divided amongst the points on the edge of
		//		the compression

	#if COMPRESS_BEFORE_SIMULATION == 1
		float compressionX = SimulateCompression(x, cm1, c0, cp1, initialWorkingHeight, 2);
	#endif

	float centerSurfaceHeight = LoadSurfaceHeight(c0);

	initialWorkingHeight += RainQuantityPerFrame;

	WorkingHeights[0][1+x] = initialWorkingHeight;
	InterlockedAdd(InitialTotalVolume, int((max(0, initialWorkingHeight - centerSurfaceHeight)) * MultiplierForVolumeCalc));

	// WorkingHeights[0][1+x] += RainQuantityPerFrame;		// (add after InitialTotalVolume -- so the total volume will readjust)

	GroupMemoryBarrierWithGroupSync();

	float4 coefficients = BuildCoefficients(x, cm1, c0, cp1, centerSurfaceHeight);

		//		With the jacobi method we attempt solve the linear
		//		equations by iteratively calculating approximates
		//		with constant values for other parts of the matrix.
		//		It's convenient for a highly multithreaded environment
		//		(whereas a CPU solution might attempt to invert the
		//		matrix)
	int finalReadBufferIndex;
	const bool simpleSimulation = false;
	if (!simpleSimulation) {
		for (int i=0; i<JacobiIterationCount; ++i) {
			const int readBufferIndex = i&1;
			const int writeBufferIndex = (i+1)&1;

			float hm1		 = WorkingHeights[readBufferIndex][1+x-1];
			float hp1		 = WorkingHeights[readBufferIndex][1+x+1];
			float sum		 = hm1 * coefficients.r + hp1 * coefficients.b;
			float newValue	 = (coefficients.a - sum) / coefficients.g;
			#if COMPRESS_BEFORE_SIMULATION == 1
				newValue = min(newValue, compressionX);		// keep compression limitation, no matter what
			#endif
			WorkingHeights[writeBufferIndex][1+x] = newValue;

			GroupMemoryBarrierWithGroupSync();		// (sync each iteration, as we swap the buffers)
		}
		finalReadBufferIndex = JacobiIterationCount&1;
	} else {
		const int readBufferIndex = 0;
		const int writeBufferIndex = 1;
		WorkingHeights[writeBufferIndex][1+x] =
			.333f * (WorkingHeights[readBufferIndex][1+x-1] + WorkingHeights[readBufferIndex][1+x] + WorkingHeights[readBufferIndex][1+x+1]);
		finalReadBufferIndex = writeBufferIndex;
	}

		//		There's some cleanup work... If the surface of the
		//		water has gone below the fixed surface height, we
		//		need to clamp it. We also need to check for lost
		//		water volume, and correct as needed

	float finalHeight = max(WorkingHeights[finalReadBufferIndex][1+x], centerSurfaceHeight);

	#if COMPRESS_BEFORE_SIMULATION == 0
		float compressionX = SimulateCompression(x, cm1, c0, cp1, finalHeight, finalReadBufferIndex);
	#endif

	float finalHeightAboveSurface = finalHeight - centerSurfaceHeight;

#if 1
		//	Distribute lost volume over total surface. Distribution should be related
		//	to the depth of the water -- deapest points should get the most distribution.
		//		This might cause some wierdness in areas with pits in the surface heights...?
		//		But it's to try to avoid problems adding re-distributed water into areas that
		//		just have a small surface trickle of water
		//
		//	We loose a lot of volume every frame... but most of it is just flowing
		//	to neighbouring cells. We need to be absolutely sure that we don't
		//	redistribute volume to surface-water places however -- because that will
		//	just create a constant stream of water dripping down from high places
	int volumeRatioCalc = int(clamp(finalHeightAboveSurface, 0, 10) * MultiplierForVolumeCalc);
	if (finalHeightAboveSurface > ThresholdAboveSurface) {
		InterlockedAdd(FinalTotalVolume, int(finalHeightAboveSurface * MultiplierForVolumeCalc));
		InterlockedAdd(TotalVolumeForDistribution, volumeRatioCalc);
		InterlockedAdd(ElementCountAboveSurface, 1);
	} else {
		finalHeight = centerSurfaceHeight;
	}
	GroupMemoryBarrierWithGroupSync();

	if (finalHeightAboveSurface > ThresholdAboveSurface) {
		float initialTotalVolume = InitialTotalVolume / MultiplierForVolumeCalc;
		float finalTotalVolume = FinalTotalVolume / MultiplierForVolumeCalc;

		const bool useSmartDistribution = true;
		if (useSmartDistribution) {
			if (TotalVolumeForDistribution > 0) {
				float ratio = volumeRatioCalc / float(TotalVolumeForDistribution);
				finalHeight += (initialTotalVolume - finalTotalVolume) * ratio;
			}
		} else {
			finalHeight += (initialTotalVolume - finalTotalVolume) / float(ElementCountAboveSurface);
		}
		finalHeight = min(finalHeight, compressionX);					// keep compression limitation, no matter what
	}
#else
	finalHeight = max(finalHeight, centerSurfaceHeight);
#endif

	WaterHeights[c0] = finalHeight;
	// WaterHeights[c0] = centerSurfaceHeight;
}

////////////////////////////////////////////////////////////////////////////////////////////////
	//   a d j a c e n t   g r i d s   //
////////////////////////////////////////////////////////////////////////////////////////////////
Texture2D<float> GlobalWavesHeightsTexture : register(t4);

float LookupGlobalWaves(float2 worldCoords)
{
		//	because of the XY movement of the global water grid,
		//	we can't know exactly the water height at given world
		//	coords... but we can know an estimate...

	uint2 texDim;
	GlobalWavesHeightsTexture.GetDimensions(texDim.x, texDim.y);

	float2 heightsTextureAddress = worldCoords / float2(PhysicalWidth, PhysicalHeight);
	return WaterBaseHeight + StrengthConstantZ * StrengthConstantMultiplier * OceanTextureCustomInterpolate(
		GlobalWavesHeightsTexture, texDim, heightsTextureAddress);
}

float CombineLocalAndGlobalWaves(float localWaves, float globalWaves)
{
	return localWaves;
	// return localWaves + 0.25f * (globalWaves - localWaves);

	if (abs(globalWaves - WaterBaseHeight) > abs(localWaves - WaterBaseHeight)) {
		return globalWaves;
	} else {
		return localWaves;
	}
}

float CalculateBoundingWaterHeight(int2 address, int2 direction)
{
		//	When calling this, address should be a coordinate on the very boundary of the tile,
		//	and direction should shift us into the next tile
		//		eg, address = int2(SHALLOW_WATER_TILE_DIMENSION-1, 5); direction = (1,0)

	float2 worldCoords = (float2(SimulatingIndex.xy) + float2(address.xy + direction.xy) / float(SHALLOW_WATER_TILE_DIMENSION)) * ShallowGridPhysicalDimension;
	float globalWaves = LookupGlobalWaves(worldCoords);

	uint adjacentSimulatingGrid = CalculateShallowWaterArrayIndex(LookupTable, SimulatingIndex + direction);
	if (adjacentSimulatingGrid < 128) {
		int2 dims = int2(SHALLOW_WATER_TILE_DIMENSION, SHALLOW_WATER_TILE_DIMENSION);
		uint3 coords = uint3((address.xy + direction.xy + dims) % dims, adjacentSimulatingGrid);
		return CombineLocalAndGlobalWaves(WaterHeightsN1[coords], globalWaves);
	}

	#if SHALLOW_WATER_BOUNDARY == SWB_SURFACE
		return LoadSurfaceHeight(int3(address + direction, 0));
	#elif SHALLOW_WATER_BOUNDARY == SWB_GLOBALWAVES
		return globalWaves;
	#elif SHALLOW_WATER_BOUNDARY == SWB_BASEHEIGHT
		return WaterBaseHeight;
	#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////
	//   e n t r y   p o i n t s   //
////////////////////////////////////////////////////////////////////////////////////////////////
[numthreads(SHALLOW_WATER_TILE_DIMENSION, 1, 1)]
	void		RunSimulationH(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	int3 baseCoord = int3(dispatchThreadId.x, dispatchThreadId.y, ArrayIndex);

	if (dispatchThreadId.x==0) {
		WorkingHeights[1][0] = WorkingHeights[0][0] = CalculateBoundingWaterHeight(baseCoord.xy, int2(-1, 0));
	}

	if (dispatchThreadId.x==SHALLOW_WATER_TILE_DIMENSION-1) {
		WorkingHeights[1][1+SHALLOW_WATER_TILE_DIMENSION] = WorkingHeights[0][1+SHALLOW_WATER_TILE_DIMENSION] =
			CalculateBoundingWaterHeight(baseCoord.xy, int2(1,0));
	}

	RunSimulation(dispatchThreadId.x, baseCoord + int3(-1,0,0), baseCoord, baseCoord + int3(1,0,0));
}

[numthreads(SHALLOW_WATER_TILE_DIMENSION, 1, 1)]
	void		RunSimulationV(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	int3 baseCoord = int3(dispatchThreadId.y, dispatchThreadId.x, ArrayIndex);

	if (dispatchThreadId.x==0) {
		WorkingHeights[1][0] = WorkingHeights[0][0] = CalculateBoundingWaterHeight(baseCoord.xy, int2(0, -1));
	}

	if (dispatchThreadId.x==SHALLOW_WATER_TILE_DIMENSION-1) {
		WorkingHeights[1][1+SHALLOW_WATER_TILE_DIMENSION] = WorkingHeights[0][1+SHALLOW_WATER_TILE_DIMENSION] =
			CalculateBoundingWaterHeight(baseCoord.xy, int2(0, 1));
	}

	RunSimulation(dispatchThreadId.x, baseCoord + int3(0,-1,0), baseCoord, baseCoord + int3(0,1,0));
}


////////////////////////////////////////////////////////////////////////////////////////////////
	//   v e l o c i t i e s   //
////////////////////////////////////////////////////////////////////////////////////////////////

RWTexture2DArray<float>		Velocities0		: register(u1);
RWTexture2DArray<float>		Velocities1		: register(u2);
RWTexture2DArray<float>		Velocities2		: register(u3);
RWTexture2DArray<float>		Velocities3		: register(u4);

RWTexture2DArray<float>		SobelSlopesX	: register(u5);
RWTexture2DArray<float>		SobelSlopesY	: register(u6);

int3 NormalizeGridCoord(int3 coord)
{
	int3 dims;
	WaterHeights.GetDimensions(dims.x, dims.y, dims.z);
	if (	coord.x >= 0		&& coord.y >= 0
		&&	coord.x < dims.x	&& coord.y < dims.y) {
		return coord;
	}

	int2 tile = ((SimulatingIndex * SHALLOW_WATER_TILE_DIMENSION) + coord.xy) / SHALLOW_WATER_TILE_DIMENSION;
	uint adjacentSimulatingGrid = CalculateShallowWaterArrayIndex(LookupTable, tile);
	if (adjacentSimulatingGrid < 128) {
		return int3(coord.xy - tile * SHALLOW_WATER_TILE_DIMENSION, adjacentSimulatingGrid);
	}

	return int3(0,0,-1);	// off the edge of the simulation area
}

float LoadHeightDifference(int3 coord)
{
	coord = NormalizeGridCoord(coord);
	if (coord.z < 0) {
		return 0.f;
	}

	return WaterHeights[coord] - WaterHeightsN1[coord];
}

float LoadHeight(int3 coord)
{
	coord = NormalizeGridCoord(coord);
	if (coord.z < 0) {
		return 0.f;
	}

	return WaterHeights[coord];
}

[numthreads(SHALLOW_WATER_TILE_DIMENSION, 1, 1)]
	void UpdateVelocities0(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	int3 baseCoord = int3(dispatchThreadId.x, dispatchThreadId.y, ArrayIndex);

		//	Try to estimate the velocity by using a Sobel filter to find
		//	the slopes of the change in height. This theory, this might
		//	not be directly related to the velocity of the water movement.
		//	But it should produce a vector field that is similar to velocity.

	float centerHeight = LoadHeight(baseCoord);

#if 0

	const float weight = 1.f/68.f;
	float horizontalFilter[5][5] = {
		{ -2.f, -1.f,  0.f,  1.f,  2.f },
		{ -2.f, -4.f,  0.f,  4.f,  2.f },
		{ -2.f, -4.f,  0.f,  4.f,  2.f },
		{ -2.f, -4.f,  0.f,  4.f,  2.f },
		{ -2.f, -1.f,  0.f,  1.f,  2.f }
	};
	float verticalFilter[5][5] = {
		{ -2.f, -2.f, -2.f, -2.f, -2.f },
		{ -1.f, -4.f, -4.f, -4.f, -1.f },
		{  0.f,  0.f,  0.f,  0.f,  0.f },
		{  1.f,  4.f,  4.f,  4.f,  1.f },
		{  2.f,  2.f,  2.f,  2.f,  2.f }
	};

	float filteredX, filteredY;
	for (int y=0; y<5; ++y) {
		for (int x=0; x<5; ++x) {

			float otherHeight = LoadHeightDifference(baseCoord + int3(x-2, y-2, 0));
			// if (otherHeight < centerHeight) otherHeight = .25f * (otherHeight - centerHeight);
			// else							otherHeight = otherHeight - centerHeight;

			filteredX += horizontalFilter[y][x] * otherHeight;
			filteredY +=   verticalFilter[y][x] * otherHeight;
		}
	}

	filteredX *= weight;
	filteredY *= weight;

#else

	float weightScale = 0.f;

	float filteredX = 0.f, filteredY = 0.f;
	const uint filterSize = 11;

	if (LoadHeightDifference(baseCoord)<0) {

		for (int y=0; y<filterSize; ++y) {
			for (int x=0; x<filterSize; ++x) {

				int2 offset = int2(x-2, y-2);
				float thisWeight = exp(-length(float2(offset)));
				weightScale += thisWeight;

				float otherHeight = max(0, LoadHeightDifference(baseCoord + int3(offset, 0)));
				filteredX += thisWeight * sign(float(offset.x)) * otherHeight;
				filteredY += thisWeight * sign(float(offset.y)) * otherHeight;

			}
		}

	} else {

		for (int y=0; y<filterSize; ++y) {
			for (int x=0; x<filterSize; ++x) {

				int2 offset = int2(x-2, y-2);
				float thisWeight = exp(-length(float2(offset)));
				weightScale += thisWeight;

				float otherHeight = min(0, LoadHeightDifference(baseCoord + int3(offset, 0)));
				filteredX += thisWeight * sign(float(offset.x)) * otherHeight;
				filteredY += thisWeight * sign(float(offset.y)) * otherHeight;

			}
		}

	}

	filteredX /= weightScale;
	filteredY /= weightScale;

#endif

	SobelSlopesX[baseCoord] = filteredX;
	SobelSlopesY[baseCoord] = filteredY;
}

void StoreVelocities(uint3 coord, float vel[4])
{
	Velocities0[coord] = lerp(Velocities0[coord], vel[0], 0.025f);
	Velocities1[coord] = lerp(Velocities1[coord], vel[1], 0.025f);
	Velocities2[coord] = lerp(Velocities2[coord], vel[2], 0.025f);
	Velocities3[coord] = lerp(Velocities3[coord], vel[3], 0.025f);
}

[numthreads(SHALLOW_WATER_TILE_DIMENSION, 1, 1)]
	void UpdateVelocities1(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	int3 baseCoord = int3(dispatchThreadId.x, dispatchThreadId.y, ArrayIndex);

		//	We know the slopes of the changes in height. This tells us a little
		//	about the direction water is flowing in. We can use this an as estimate
		//	of the velocity.
		//
		//	But what we really want to know is the amount of water that is
		//	transfered from one grid element to another. We can use the dot product
		//	with the velocity to get this. Perhaps we should use the velocities in both
		//	grid elements to calculate this movement of water.

	float2 baseVel = float2(SobelSlopesX[baseCoord], SobelSlopesY[baseCoord]);

	float result[4];
	int2 offsets[4] = { int2(-1, -1), int2(0, -1), int2(1, -1), int2(-1, 0) };

	[unroll] for (uint c=0; c<4; ++c) {
		int3 otherCoord = NormalizeGridCoord(baseCoord + int3(offsets[c], 0));
		float2 otherVel = float2(SobelSlopesX[otherCoord], SobelSlopesY[otherCoord]);

		float2 dir = normalize(float2(offsets[c]));	// out of the grid
		float flow0 = dot(baseVel, dir);
		float flow1 = dot(otherVel, dir);

		result[c] = (flow0 + flow1) * 0.5f;
	}

	StoreVelocities(baseCoord, result);
}
