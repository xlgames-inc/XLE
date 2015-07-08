// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(TERRAIN_GENERATOR_H)
#define TERRAIN_GENERATOR_H

#include "../../Utility/perlinnoise.h"

cbuffer TerrainLighting : register(b6)
{
	float SunAngle;				// should be a value between -1.f and 1.f representing angles between -.5 * pi and .5 * pi
	float ShadowSoftness;		// (around 50.f)
}

static const uint MaxCoverageLayers = 5;

cbuffer TileBuffer : register(b7)
{
	row_major float4x4 LocalToCell;
	int3	HeightMapOrigin;
	int		TileDimensionsInVertices;
    int4    NeighbourLodDiffs;

	float4	CoverageCoordMins[MaxCoverageLayers];
	float4	CoverageCoordMaxs[MaxCoverageLayers];
	int4	CoverageOrigin[MaxCoverageLayers];
}

#if defined(COVERAGE_FMT_0)
	Texture2DArray<COVERAGE_FMT_0> CoverageTileSet0 : register(t1);
#endif
#if defined(COVERAGE_FMT_1)
	Texture2DArray<COVERAGE_FMT_1> CoverageTileSet1 : register(t2);
#endif
#if defined(COVERAGE_FMT_2)
	Texture2DArray<COVERAGE_FMT_2> CoverageTileSet2 : register(t3);
#endif
#if defined(COVERAGE_FMT_3)
	Texture2DArray<COVERAGE_FMT_3> CoverageTileSet3 : register(t4);
#endif
#if defined(COVERAGE_FMT_4)
	Texture2DArray<COVERAGE_FMT_4> CoverageTileSet4 : register(t5);
#endif

float3 AddNoise(float3 worldPosition)
{
	#if (DO_ADD_NOISE==1)
		worldPosition.z += 5.f * fbmNoise2D(worldPosition.xy, 20.f, .5f, 2.1042, 6);
	#endif
	return worldPosition;
}

#define MakeCoverageTileSet(index) CoverageTileSet ## index

#endif
