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

#if (USE_LOOKUP_TABLE==1)
    Texture2D<uint> CellIndexLookupTable : register(t4);
#endif

cbuffer ShallowWaterCellConstants : register(b2)
{
    int2	SimulatingIndex;
    uint	ArrayIndex;
    float2  WorldSpaceOffset;

    //  -X, -Y
    //  -------------------------------
    //  TopLeft,    Top,    TopRight
    //  Left,               Right
    //  BottomLeft, Bottom, BottomRight
    //  -------------------------------
    //                           +X, +Y
    int    TopLeftGrid, TopGrid, TopRightGrid;
    int    LeftGrid, RightGrid;
    int    BottomLeftGrid, BottomGrid, BottomRightGrid;
}

cbuffer SimulatingConstants : register(b3)
{
    float	RainQuantityPerFrame;		// 0.001f
    float   EvaporationConstant;
    float   PressureConstant;           // 150.f (for pipe model)

    float3  CompressionMidPoint;
    float   CompressionRadius;
}

#if (USE_LOOKUP_TABLE==1)
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
#endif

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

#if 0
    uint2 Coord10(uint2 a, uint2 dims) { return uint2(min(a.x+1, dims.x-1), a.y); }
    uint2 Coord01(uint2 a, uint2 dims) { return uint2(a.x, min(a.y+1, dims.y-1)); }
    uint2 Coord11(uint2 a, uint2 dims) { return uint2(min(a.x+1, dims.x-1), min(a.y+1, dims.y-1)); }
#else
    uint2 Coord10(uint2 a, uint2 dims) { return uint2(a.x+1, a.y); }
    uint2 Coord01(uint2 a, uint2 dims) { return uint2(a.x, a.y+1); }
    uint2 Coord11(uint2 a, uint2 dims) { return uint2(a.x+1, a.y+1); }
#endif

float ManualInterpolateSurfaceHeight_Exploded(Texture2DArray<uint> tex, float2 exploded, uint arrayIndex)
{
		// note that wrapping/borders aren't handled
		//	.. it means there can be problems if we try to read from
		//	outside the normal texture area
        //  (but when reading from terrain textures, we should have some extra "overlap" pixels out our normal range)
    uint3 dims; tex.GetDimensions(dims.x, dims.y, dims.z);
    uint2 base = uint2(exploded);
    // base.x = min(base.x, dims.x-1);
    // base.y = min(base.y, dims.y-1);
	float result00 = tex[uint3(  uint2(base), arrayIndex)] & 0x3fff;
	float result10 = tex[uint3(Coord10(base, dims.xy), arrayIndex)] & 0x3fff;
	float result01 = tex[uint3(Coord01(base, dims.xy), arrayIndex)] & 0x3fff;
	float result11 = tex[uint3(Coord11(base, dims.xy), arrayIndex)] & 0x3fff;
	float2 fracPart = frac(exploded);
	return
	      result00 * (1.0f-fracPart.x) * (1.0f-fracPart.y)
	    + result10 * (     fracPart.x) * (1.0f-fracPart.y)
	    + result01 * (1.0f-fracPart.x) * (     fracPart.y)
	    + result11 * (     fracPart.x) * (     fracPart.y)
	    ;
}

SamplerState LinearClampingSampler : register(s1);

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
		float2 tcInput = saturate(coord.xy / float2(SHALLOW_WATER_TILE_DIMENSION, SHALLOW_WATER_TILE_DIMENSION));
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

#if (USE_LOOKUP_TABLE==1)
    int3 NormalizeGridCoord(int2 coord)
    {
        int2 tile = float2(coord.xy) / float(SHALLOW_WATER_TILE_DIMENSION);
        uint gridIndex = CalculateShallowWaterArrayIndex(CellIndexLookupTable, tile);
        if (gridIndex < 128) {
            return int3(coord.xy - tile * SHALLOW_WATER_TILE_DIMENSION, gridIndex);
        }

        return int3(0,0,-1);	// off the edge of the simulation area
    }
#endif

int3 NormalizeRelativeGridCoord(int2 relCoord)
{
    #if (USE_LOOKUP_TABLE==1)
        return NormalizeGridCoord(SimulatingIndex * SHALLOW_WATER_TILE_DIMENSION + relCoord);
    #else
        const int tileDim = SHALLOW_WATER_TILE_DIMENSION;
        if (relCoord.y < 0) {
            if (relCoord.x < 0) {
                return int3(relCoord + int2(tileDim, tileDim), TopLeftGrid);
            } else if (relCoord.x >= tileDim) {
                return int3(relCoord + int2(-tileDim, tileDim), TopRightGrid);
            } else {
                return int3(relCoord + int2(0, tileDim), TopGrid);
            }
        } else if (relCoord.y >= tileDim) {
            if (relCoord.x < 0) {
                return int3(relCoord + int2(tileDim, -tileDim), BottomLeftGrid);
            } else if (relCoord.x >= tileDim) {
                return int3(relCoord + int2(-tileDim, -tileDim), BottomRightGrid);
            } else {
                return int3(relCoord + int2(0, -tileDim), BottomGrid);
            }
        } else {
            if (relCoord.x < 0) {
                return int3(relCoord + int2(tileDim, 0), LeftGrid);
            } else if (relCoord.x >= tileDim) {
                return int3(relCoord + int2(-tileDim, 0), RightGrid);
            } else {
                return int3(relCoord, ArrayIndex);
            }
        }

    #endif
}

    ///////////////////////////////////////////////////
        //   c o m p r e s s i o n   //
    ///////////////////////////////////////////////////

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
    float radiusSq = CompressionRadius * CompressionRadius;
    float d = max(0.f, 1.0f - (distance2DSq / radiusSq));
    return 5e7f * d;
}

#endif
