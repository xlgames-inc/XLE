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

cbuffer TileBuffer : register(b8)
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

float SampleCoverageTileSet_Int(Texture2DArray<float> tileSet, uint tileSetIndex, float2 texCoord)
{
	float2 finalTexCoord = lerp(CoverageCoordMins[tileSetIndex].xy, CoverageCoordMaxs[tileSetIndex].xy, texCoord);

	float2 tcf = floor(finalTexCoord);
	float2 A = finalTexCoord - tcf;
	const float w[4] =
	{
		(1.f - A.x) * A.y,
		A.x * A.y,
		A.x * (1.f - A.y),
		(1.f - A.x) * (1.f - A.y)
	};

	float samples[4];
	samples[0] = tileSet.Load(
		uint4(uint2(tcf) + uint2(0,1), CoverageOrigin[tileSetIndex].z, 0));
	samples[1] = tileSet.Load(
		uint4(uint2(tcf) + uint2(1,1), CoverageOrigin[tileSetIndex].z, 0));
	samples[2] = tileSet.Load(
		uint4(uint2(tcf) + uint2(1,0), CoverageOrigin[tileSetIndex].z, 0));
	samples[3] = tileSet.Load(
		uint4(tcf, CoverageOrigin[tileSetIndex].z, 0));

	return
		( samples[0] * w[0] + samples[1] * w[1]
		+ samples[2] * w[2] + samples[3] * w[3] );
}

float SampleCoverageTileSet_Int(Texture2DArray<uint> tileSet, uint tileSetIndex, float2 texCoord)
{
	float2 finalTexCoord = lerp(CoverageCoordMins[tileSetIndex].xy, CoverageCoordMaxs[tileSetIndex].xy, texCoord);

	float2 tcf = floor(finalTexCoord);
	float2 A = finalTexCoord - tcf;
	const float w[4] =
	{
		(1.f - A.x) * A.y,
		A.x * A.y,
		A.x * (1.f - A.y),
		(1.f - A.x) * (1.f - A.y)
	};

	uint samples[4];
	samples[0] = tileSet.Load(
		uint4(uint2(tcf) + uint2(0,1), CoverageOrigin[tileSetIndex].z, 0));
	samples[1] = tileSet.Load(
		uint4(uint2(tcf) + uint2(1,1), CoverageOrigin[tileSetIndex].z, 0));
	samples[2] = tileSet.Load(
		uint4(uint2(tcf) + uint2(1,0), CoverageOrigin[tileSetIndex].z, 0));
	samples[3] = tileSet.Load(
		uint4(tcf, CoverageOrigin[tileSetIndex].z, 0));

	return
		( samples[0] * w[0] + samples[1] * w[1]
		+ samples[2] * w[2] + samples[3] * w[3] );
}

#define SampleCoverageTileSet(tileSetIndex, texCoord)											\
	SampleCoverageTileSet_Int(MakeCoverageTileSet(tileSetIndex), tileSetIndex, texCoord)		\
	/**/

#endif
