// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(TERRAIN_GENERATOR_H)
#define TERRAIN_GENERATOR_H

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

Texture2DArray<uint> CoverageTileSet0 : register(t0);
Texture2DArray<uint> CoverageTileSet1 : register(t1);
Texture2DArray<uint> CoverageTileSet2 : register(t2);
Texture2DArray<uint> CoverageTileSet3 : register(t3);
Texture2DArray<uint> CoverageTileSet4 : register(t4);

#endif
