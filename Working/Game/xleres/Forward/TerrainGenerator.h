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

cbuffer TileBuffer : register(b7)
{
	row_major float4x4 LocalToCell;
	int3	HeightMapOrigin;
	float4	TexCoordMins;
	float4	TexCoordMaxs;
	int3	CoverageOrigin;
	int		TileDimensionsInVertices;
    int4    NeighbourLodDiffs;
}

Texture2DArray CoverageTileSet : register(t0);

#endif

