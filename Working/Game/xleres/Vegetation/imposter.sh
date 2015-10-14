// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define OUTPUT_TEXCOORD 1
#define GEO_HAS_TEXCOORD 1
#define VSINPUT_EXTRA uint spriteIndex : SPRITEINDEX;
#define VSOUTPUT_EXTRA uint spriteIndex : SPRITEINDEX;

#include "../MainGeometry.h"
#include "../Surface.h"
#include "../Transform.h"
#include "../TransformAlgorithm.h"
#include "../gbuffer.h"
#include "../CommonResources.h"
#include "../Lighting/LightingAlgorithm.h"  // for CalculateMipmapLevel

Texture2D ImposterAltas[2] : register(t0);

VSOutput vs_main(VSInput input)
{
    VSOutput output;
    float3 localPosition	= GetLocalPosition(input);
    float3 worldPosition = localPosition; // mul(LocalToWorld, float4(localPosition,1)).xyz;

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

    output.spriteIndex = input.spriteIndex;

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

static const uint MipMapCount = 5;
cbuffer SpriteTable
{
    uint4 SpriteCoords[512];
}

float4 LoadImposterAltas(
    uint atlasIndex,
    uint4 coords,
    float2 tc)
{
    float2 ftc;
    ftc.x = lerp((float)coords[0], (float)coords[2], tc.x);
    ftc.y = lerp((float)coords[1], (float)coords[3], tc.y);
    // return ImposterAltas[atlasIndex].SampleLevel(ClampingSampler, ftc, 0);
    return ImposterAltas[atlasIndex].Load(uint3(ftc, 0));
}

#if !(MAT_ALPHA_TEST==1)
    [earlydepthstencil]
#endif
GBufferEncoded ps_deferred(VSOutput geo)
{
    if (0) {
        float3 color0 = float3(1.0f, 0.f, 0.f);
        float3 color1 = float3(0.0f, 0.f, 1.f);
        uint flag = (uint(geo.position.x/4.f) + uint(geo.position.y/4.f))&1;
        GBufferValues result = GBufferValues_Default();
        result.diffuseAlbedo = flag?color0:color1;
        return Encode(result);
    }

    uint4 coords[MipMapCount];
    for (uint c=0; c<MipMapCount; ++c)
        coords[c] = SpriteCoords[geo.spriteIndex*MipMapCount+c];

        // We must do custom interpolation, for two reasons:
        //  1) our mip-maps are scattered about the atlas
        //  2) we want to use an interior clamping region
        //      (ie, bilinear should not blur into neighbouring sprites)

        // Note that since the sprite is close to screen-aligned, the
        // mip-map index is probably fixed across the entire surface of
        // the sprite. So we could probably calculate it in the vertex shader
        // (or even in a geometry shader)
    float2 tc = geo.texCoord;
    uint mip = (uint)CalculateMipmapLevel(tc, uint2(coords[0][2] - coords[0][0], coords[0][3] - coords[0][1]));
    mip = min(mip, MipMapCount-1);

    float4 diffuse = LoadImposterAltas(0, coords[mip], tc);
    float4 normal = LoadImposterAltas(1, coords[mip], tc);

        // we can't use the normal Encode() call for the gbuffer
        // because the normal is already compressed into it's 8 bit format
    GBufferEncoded result;
    result.diffuseBuffer = diffuse;
    result.normalBuffer = normal;
    #if HAS_PROPERTIES_BUFFER == 1
        result.propertiesBuffer = 1.0.xxxx;
    #endif
    return result;
}
