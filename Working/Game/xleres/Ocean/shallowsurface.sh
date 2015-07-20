// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Ocean.h"
#include "OceanShallow.h"
#include "../MainGeometry.h"
#include "../Transform.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

uint ShallowWaterPage;
Texture2DArray<float>	ShallowWaterHeights		: register(t3);

VSOutput vs_main(uint vertexId : SV_VertexId)
{
    VSOutput output;

    uint2 p = uint2(
        vertexId % (SHALLOW_WATER_TILE_DIMENSION+1),
        vertexId / (SHALLOW_WATER_TILE_DIMENSION+1));

    float3 localPosition = float3(
        p.x / float(SHALLOW_WATER_TILE_DIMENSION),
        p.y / float(SHALLOW_WATER_TILE_DIMENSION),
        0.f);

    localPosition.z = ShallowWaterHeights.Load(uint4(p, ShallowWaterPage, 0));

    #if GEO_HAS_INSTANCE_ID==1
        float3 worldPosition = InstanceWorldPosition(input, objectCentreWorld);
    #else
        float3 worldPosition = mul(LocalToWorld, float4(localPosition,1)).xyz;
    #endif

    output.position = mul(WorldToClip, float4(worldPosition,1));

    return output;
}
