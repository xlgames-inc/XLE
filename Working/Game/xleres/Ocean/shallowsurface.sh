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

float3 CalculateLocalPosition(uint vertexId)
{
    uint2 p = uint2(
        vertexId % (SHALLOW_WATER_TILE_DIMENSION+1),
        vertexId / (SHALLOW_WATER_TILE_DIMENSION+1));

    return float3(
        p.x / float(SHALLOW_WATER_TILE_DIMENSION),
        p.y / float(SHALLOW_WATER_TILE_DIMENSION),
        0.f);
}

VSOutput vs_main(uint vertexId : SV_VertexId)
{
    VSOutput output;
    float3 localPosition = CalculateLocalPosition(vertexId);

    #if GEO_HAS_INSTANCE_ID==1
        float3 worldPosition = InstanceWorldPosition(input, objectCentreWorld);
    #else
        float3 worldPosition = mul(LocalToWorld, float4(localPosition,1)).xyz;
    #endif

    output.position = mul(WorldToClip, float4(worldPosition,1));

    return output;
}
