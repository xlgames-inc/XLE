// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(OCEAN_SHALLOW_H)
#define OCEAN_SHALLOW_H

#define SWB_GLOBALWAVES     1
#define SWB_SURFACE         2
#define SWB_BASEHEIGHT      3

#if !defined(SHALLOW_WATER_BOUNDARY)
    #define SHALLOW_WATER_BOUNDARY  SWB_GLOBALWAVES
#endif

#if !defined(SHALLOW_WATER_TILE_DIMENSION)
    #define SHALLOW_WATER_TILE_DIMENSION 128
#endif

Texture2D<uint> CellIndexLookupTable : register(t4);

cbuffer ShallowWaterUpdateConstants : register(b2)
{
    int2	SimulatingIndex;
    uint	ArrayIndex;
    float	RainQuantityPerFrame;		// 0.001f
    float2  WorldSpaceOffset;
}

uint CalculateShallowWaterArrayIndex(Texture2D<uint> CellIndexLookupTable, int2 gridCoord)
{
	uint2 dims;
	CellIndexLookupTable.GetDimensions(dims.x, dims.y);
	uint result;
	uint2 a = gridCoord + int2(256,256);
    if (a.x < dims.x && a.y < dims.y) {
		result = CellIndexLookupTable.Load(uint3(a, 0));
	} else {
		result = 0xffffffff;
	}
	return result;
}

#if SURFACE_HEIGHTS_FLOAT==1
    Texture2D<float>        SurfaceHeightsTexture : register(t0);
#else
    Texture2DArray<uint>    SurfaceHeightsTexture : register(t0);
#endif

cbuffer SurfaceHeightAddressing : register(b1)
{
	int3	SurfaceHeightBaseCoord;
	uint2	SurfaceHeightTextureMin;
	uint2	SurfaceHeightTextureMax;
	float	SurfaceHeightScale, SurfaceHeightOffset;
}

float ManualInterpolateSurfaceHeight_Exploded(Texture2DArray<uint> tex, float2 exploded, uint arrayIndex)
{
		// note that wrapping/borders aren't handled
		//	.. it means there can be problems if we try to read from
		//	outside the normal texture area
	float result00 = tex[uint3(uint2(exploded) + uint2(0,0), arrayIndex)];
	float result10 = tex[uint3(uint2(exploded) + uint2(1,0), arrayIndex)];
	float result01 = tex[uint3(uint2(exploded) + uint2(0,1), arrayIndex)];
	float result11 = tex[uint3(uint2(exploded) + uint2(1,1), arrayIndex)];
	float2 fracPart = frac(exploded);
	return
	      result00 * (1.0f-fracPart.x) * (1.0f-fracPart.y)
	    + result10 * (     fracPart.x) * (1.0f-fracPart.y)
	    + result01 * (1.0f-fracPart.x) * (     fracPart.y)
	    + result11 * (     fracPart.x) * (     fracPart.y)
	    ;
}

SamplerState LinearClampingSampler : register(s0);

float LoadSurfaceHeight(int2 coord)
{
		//	the coordinate system for the water may not exactly match the coordinates
		//	for the surface height texture. We need to convert coordinate systems, and
		//	potentially to bilinear filtering between sample points

	const bool oneToOne = false;
	if (oneToOne) {
		float rawHeight = (float)SurfaceHeightsTexture.Load(int4(SurfaceHeightBaseCoord + int3(coord.xy, 0), 0));
		return SurfaceHeightOffset + rawHeight * SurfaceHeightScale;
	} else {
		float2 tcInput = coord.xy / float2(SHALLOW_WATER_TILE_DIMENSION, SHALLOW_WATER_TILE_DIMENSION);
		float2 surfaceHeightCoord = lerp(float2(SurfaceHeightTextureMin), float2(SurfaceHeightTextureMax), tcInput);

			// "surfaceHeightCoord" is the "exploded" coordinates on this texture
			//		-- that is integer values correspond with pixel top-left corners
        #if SURFACE_HEIGHTS_FLOAT==1
            int2 dims;
            SurfaceHeightsTexture.GetDimensions(dims.x, dims.y);
            float rawHeight =
                SurfaceHeightsTexture.SampleLevel(
                    LinearClampingSampler,
                    (float2(SurfaceHeightBaseCoord.xy) + surfaceHeightCoord) / float2(dims), 0);
        #else
            float rawHeight = ManualInterpolateSurfaceHeight_Exploded(
                SurfaceHeightsTexture, float2(SurfaceHeightBaseCoord.xy) + surfaceHeightCoord, SurfaceHeightBaseCoord.z);
        #endif
		return SurfaceHeightOffset + rawHeight * SurfaceHeightScale;
	}
}

int3 NormalizeGridCoord(int2 coord)
{
    int2 tile = float2(coord.xy) / float(SHALLOW_WATER_TILE_DIMENSION);
    uint gridIndex = CalculateShallowWaterArrayIndex(CellIndexLookupTable, tile);
    if (gridIndex < 128) {
        return int3(coord.xy - tile * SHALLOW_WATER_TILE_DIMENSION, gridIndex);
    }

    return int3(0,0,-1);	// off the edge of the simulation area
}

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
        // shifting half an element back seems to match the terrain rendering
        // much better -- but it's not clear why
    return WorldSpaceOffset + float2(SimulatingIndex + (eleIndex - 0.5.xx) / float(SHALLOW_WATER_TILE_DIMENSION)) * ShallowGridPhysicalDimension;
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

#endif
