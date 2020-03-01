// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define OUTPUT_TEXCOORD 1

#include "TerrainGenerator.h"
#include "../../TechniqueLibrary/Framework/MainGeometry.hlsl"
#include "../../TechniqueLibrary/Framework/Transform.hlsl"

VSOutput vs_basic(uint vertexIndex : SV_VertexId)
{
    VSOutput output;

    int x = vertexIndex % TileDimensionsInVertices;
    int y = vertexIndex / TileDimensionsInVertices;
    uint rawHeightValue = LoadRawHeightValue(int3(HeightMapOrigin.xy + int2(x,y), HeightMapOrigin.z));

    float3 localPosition;
    localPosition.x		 = float(x) / float(TileDimensionsInVertices-1);
    localPosition.y		 = float(y) / float(TileDimensionsInVertices-1);
    localPosition.z		 = float(rawHeightValue);

    float3 cellPosition	 = mul( LocalToCell, float4(localPosition, 1)).xyz;
    float3 worldPosition = mul(LocalToWorld, float4( cellPosition,1));
    worldPosition        = AddNoise(worldPosition);
    output.position		 = mul( WorldToClip, float4(worldPosition,1));

    #if (OUTPUT_WORLD_POSITION==1)
        output.worldPosition = worldPosition;
    #endif

    #if (OUTPUT_TEXCOORD==1)
        output.texCoord = 0.0.xx;
    #endif

    return output;
}
