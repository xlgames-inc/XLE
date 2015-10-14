// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define OUTPUT_TEXCOORD 1
#include "../MainGeometry.h"
#include "../Surface.h"
#include "../Transform.h"
#include "../TransformAlgorithm.h"
#include "../gbuffer.h"

VSOutput vs_main(VSInput input)
{
    VSOutput output;
    float3 localPosition	= GetLocalPosition(input);
    float3 worldPosition = mul(LocalToWorld, float4(localPosition,1)).xyz;

    #if OUTPUT_TEXCOORD==1
        output.texCoord = GetTexCoord(input);
    #endif

    output.position = mul(WorldToClip, float4(worldPosition,1));

    #if OUTPUT_LOCAL_VIEW_VECTOR==1
        output.localViewVector = LocalSpaceView.xyz - localPosition.xyz;
    #endif

    #if OUTPUT_WORLD_VIEW_VECTOR==1
        output.worldViewVector = WorldSpaceView.xyz - worldPosition.xyz;
    #endif

    #if OUTPUT_WORLD_POSITION==1
        output.worldPosition = worldPosition.xyz;
    #endif

    #if OUTPUT_FOG_COLOR == 1
        // output.fogColor = CalculateFog(worldPosition.z, WorldSpaceView - worldPosition, NegativeDominantLightDirection);
        output.fogColor = float4(0.0.xxx, 1.f);
    #endif

    return output;
}

void ps_depthonly(float4 pos : SV_Position) {}

#if !(MAT_ALPHA_TEST==1)
    [earlydepthstencil]
#endif
float4 main(VSOutput geo) : SV_Target0
{
    return 1.0.xxxx;
}

#if !(MAT_ALPHA_TEST==1)
    [earlydepthstencil]
#endif
GBufferEncoded ps_deferred(VSOutput geo)
{
    float3 color0 = float3(1.0f, 0.f, 0.f);
    float3 color1 = float3(0.0f, 0.f, 1.f);
    uint flag = (uint(geo.position.x/4.f) + uint(geo.position.y/4.f))&1;
    GBufferValues result = GBufferValues_Default();
    result.diffuseAlbedo = flag?color0:color1;
    return Encode(result);
}
