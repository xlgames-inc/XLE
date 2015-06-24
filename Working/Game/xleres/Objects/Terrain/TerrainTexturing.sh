// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define OUTPUT_TEXCOORD 1
#define VSOUTPUT_EXTRA float2 dhdxy : DHDXY;

#include "TerrainGenerator.h"
#include "../../CommonResources.h"
#include "../../Utility/perlinnoise.h"
#include "../../Utility/DistinctColors.h"
#include "../../gbuffer.h"
#include "../../Colour.h"

#define GBUFFER_TYPE 1	// hack -- (not being set by the client code currently)

struct PSInput
{
    float4 position : SV_Position;

    #if (DRAW_WIREFRAME==1)
        float3 barycentricCoords : BARYCENTRIC;
    #endif

    float2 texCoord : TEXCOORD0;
    float2 dhdxy : DHDXY;

    #if (OUTPUT_WORLD_POSITION==1)
        float3 worldPosition : WORLDPOSITION;
    #endif
};

float edgeFactor( float3 barycentricCoords )
{
    float3 d = fwidth(barycentricCoords);
    float3 a3 = smoothstep(0.0.xxx, d * 1.5, barycentricCoords);
    return min(min(a3.x, a3.y), a3.z);
}

float edgeFactor2( float2 coords, float width )
{
    float2 d = fwidth(coords);
    float2 a3 = smoothstep(0.0.xx, d*width, coords);
    return min(a3.x, a3.y);
}

    //	Big stack of terrain texture -- including normal maps and specularity textures at each level
    //	For each strata:
    //		0: texture 0
    //		1: texture 1
    //		2: slopes
    //
    //	Each has entries in the diffuse albedo, normals and specularity textures
    //	We separate diffuse, normals and specularity into differnt arrays so they can have different
    //	pixel formats and dimensions. But this storage mode does mean that every texture of the same
    //	type must agree on both pixel format and dimensions. We can get around that by converting the
    //	array into some kind of large texture atlas (like a mega-texture type thing). That would also
    //	work better for streaming mip maps.
Texture2DArray StrataDiffuse		: register( t8);
Texture2DArray StrataNormals		: register( t9);
Texture2DArray StrataSpecularity	: register(t10);

struct ProceduralTextureOutput
{
    float3 diffuseAlbedo;
    float3 tangentSpaceNormal;
    float specularity;
};

interface IProceduralTexture
{
    ProceduralTextureOutput Calculate(float3 worldPosition, float slopeFacor, float2 texCoord);
};

#if !defined(COVERAGE_1000)
    #undef PROCEDURAL_TEXTURE_COUNT
    #define PROCEDURAL_TEXTURE_COUNT 1
#else
    #if !defined(PROCEDURAL_TEXTURE_COUNT)
        #define PROCEDURAL_TEXTURE_COUNT 1
    #endif
#endif

IProceduralTexture ProceduralTextures[PROCEDURAL_TEXTURE_COUNT];

#if STRATA_COUNT==0
    ProceduralTextureOutput GetTextureForStrata(
        uint strataIndex, float3 worldPosition,
        float slopeFactor, float2 textureCoord, float noiseValue0)
    {
        ProceduralTextureOutput result;
        result.diffuseAlbedo = 1.0.xxx;
        result.tangentSpaceNormal = float3(0.5,0.5,1);
        result.specularity = 1.0.xxx;
        return result;
    }
#else
    static const uint StrataCount = STRATA_COUNT;

    cbuffer TexturingParameters : register(b5)
    {
        float4 StrataEndHeights[StrataCount];
        float4 TextureFrequency[StrataCount];
    }

    float3 AsStackCoord(float2 tc, uint strata, uint textureType)
    {
        return float3(tc, strata*3+textureType);
    }

    float3 StrataDiffuseSample(float2 tc, uint strata, uint textureType)
    {
        float3 result = StrataDiffuse.Sample(MaybeAnisotropicSampler, AsStackCoord(tc, strata, textureType)).rgb;
            // Compenstate for wierd SRGB behaviour in archeage assets
            // ideally we should configure the texture samplers to do this automatically,
            // rather than with an explicit call here
        result = SRGBToLinear(result);
        return result;
    }

    float StrataSpecularSample(float2 tc, uint strata, uint textureType)
    {
        return SRGBLuminance(StrataSpecularity.Sample(MaybeAnisotropicSampler, AsStackCoord(tc, strata, textureType)).rgb);
    }

    static const bool UseStrataNormals = false;
    static const bool UseStrataSpecular = false;

    ProceduralTextureOutput GetTextureForStrata(
        uint strataIndex, float3 worldPosition,
        float slopeFactor, float2 textureCoord, float noiseValue0)
    {
        float2 tc0 = worldPosition.xy * TextureFrequency[strataIndex].xx;
        float2 tc1 = worldPosition.xy * TextureFrequency[strataIndex].yy;

        float alpha = saturate(.5f + .7f * noiseValue0);
        // alpha = min(1.f, exp(32.f * (alpha-.5f)));
        // alpha = lerp(.25f, 0.75f, alpha);

        ProceduralTextureOutput result;
        float3 A = StrataDiffuseSample(tc0, strataIndex, 0);
        float3 B = StrataDiffuseSample(tc0, strataIndex, 1);
        result.diffuseAlbedo = lerp(A, B, alpha);

        if (UseStrataNormals) {
            float3 An = StrataNormals.Sample(MaybeAnisotropicSampler, float3(tc0, strataIndex*3+0)).rgb;
            float3 Bn = StrataNormals.Sample(MaybeAnisotropicSampler, float3(tc1, strataIndex*3+1)).rgb;
            result.tangentSpaceNormal = lerp(An, Bn, alpha);
        } else {
            result.tangentSpaceNormal = float3(0.5,0.5,1);
        }

        if (UseStrataSpecular) {
            float As = StrataSpecularSample(tc0, strataIndex, 0);
            float Bs = StrataSpecularSample(tc0, strataIndex, 1);
            result.specularity = lerp(As, Bs, alpha);
        } else {
            result.specularity = 1;
        }

        const float slopeStart = .55f;
        const float slopeSoftness = 3.f;
        const float slopeDarkness = 1.f; // .75f;

        float a = fwidth(worldPosition.x);
        float b = fwidth(worldPosition.y);

        float slopeAlpha = pow(min(1.f,slopeFactor/slopeStart), slopeSoftness);
        if (slopeAlpha > 0.05f) {	// can't branch here because of the texture lookups below... We would need to do 2 passes

                //		slope texture coordinates should be based on worldPosition x or y,
                //		depending on which is changing most quickly in screen space
                //		This is causing some artefacts!
            float2 tcS0 = worldPosition.xz * TextureFrequency[strataIndex].zz;
            // if (fwidth(worldPosition.y) > fwidth(worldPosition.x))

                // soft darkening based on slope give a curiously effective approximation of ambient occlusion
            float arrayIdx = strataIndex*3+2;
            float3 S = StrataDiffuseSample(tcS0, strataIndex, 2);
            float3 Sn = StrataNormals.Sample(MaybeAnisotropicSampler, float3(tcS0, arrayIdx)).rgb;
            float Ss = StrataSpecularSample(tcS0, strataIndex, 2);

            tcS0.x = worldPosition.y * TextureFrequency[strataIndex].z;
            float3 S2 = StrataDiffuseSample(tcS0, strataIndex, 2);
            float3 Sn2 = StrataNormals.Sample(MaybeAnisotropicSampler, float3(tcS0, arrayIdx)).rgb;
            float Ss2 = StrataSpecularSample(tcS0, strataIndex, 2);

            float A = a / (a+b);
            float B = b / (a+b);
            S = S * A + S2 * B;
            Sn = Sn * A + Sn2 * B;
            Ss = Ss * A + Ss2 * B;

            result.diffuseAlbedo = lerp(result.diffuseAlbedo, slopeDarkness * S, slopeAlpha);
            if (UseStrataNormals)
                result.tangentSpaceNormal = lerp(result.tangentSpaceNormal, Sn, slopeAlpha);

            if (UseStrataSpecular)
                result.specularity = lerp(result.specularity, Ss, slopeAlpha);
        }

        return result;
    }
#endif

class StrataMaterial : IProceduralTexture
{
    ProceduralTextureOutput Calculate(float3 worldPosition, float slopeFactor, float2 textureCoord);
};

ProceduralTextureOutput StrataMaterial::Calculate(float3 worldPosition, float slopeFactor, float2 textureCoord)
{
        //	Build a texturing value by blending together multiple input textures
        //	Select the input textures from some of the input parameters, like slope and height

        //	This mostly just a place-holder!
        //	The noise calculation is too expensive, and there is very little configurability!
        //	But, I need a terrain that looks roughly terrain-like just to get things working

    float noiseValue0 = fbmNoise2D(worldPosition.xy, 225.f, .65f, 2.1042, 4);
    noiseValue0 += .5f * fbmNoise2D(worldPosition.xy, 33.7f, .75f, 2.1042, 4);
    noiseValue0 = clamp(noiseValue0, -1.f, 1.f);

    float strataAlpha = 0.f;
    #if STRATA_COUNT!=0
        float worldSpaceHeight = worldPosition.z - 25.f * noiseValue0; //  - 18.23f * noiseValue1;
        uint strataBase0 = StrataCount-1, strataBase1 = StrataCount-1;
        [unroll] for (uint c=0; c<(StrataCount-1); ++c) {
            if (worldSpaceHeight < StrataEndHeights[c+1].x) {
                strataBase0 = c;
                strataBase1 = c+1;
                strataAlpha = (worldSpaceHeight - StrataEndHeights[c].x) / (StrataEndHeights[c+1].x - StrataEndHeights[c].x);
                break;
            }
        }
    #else
        uint strataBase0 = 0, strataBase1 = 0;
    #endif

    strataAlpha = exp(16.f * (strataAlpha-1.f));	// only blend in the last little bit

    ProceduralTextureOutput value0 = GetTextureForStrata(strataBase0, worldPosition, slopeFactor, textureCoord, noiseValue0);
    ProceduralTextureOutput value1 = GetTextureForStrata(strataBase1, worldPosition, slopeFactor, textureCoord, noiseValue0);

    ProceduralTextureOutput result;
    result.diffuseAlbedo = lerp(value0.diffuseAlbedo, value1.diffuseAlbedo, strataAlpha);
    result.tangentSpaceNormal = lerp(value0.tangentSpaceNormal, value1.tangentSpaceNormal, strataAlpha);
    result.specularity = lerp(value0.specularity, value1.specularity, strataAlpha);

    float2 nxy = 2.f * result.tangentSpaceNormal.xy - 1.0.xx;
    result.tangentSpaceNormal = float3(nxy, sqrt(saturate(1.f + dot(nxy, -nxy))));

    return result;
}

class DummyMaterial : IProceduralTexture
{
    ProceduralTextureOutput Calculate(float3 worldPosition, float slopeFactor, float2 textureCoord);
};

ProceduralTextureOutput DummyMaterial::Calculate(float3 worldPosition, float slopeFactor, float2 textureCoord)
{
    ProceduralTextureOutput result;
    result.diffuseAlbedo = 1.0.xxx;
    result.tangentSpaceNormal = float3(0, 0, 1);
    result.specularity = 1.f;
    return result;
}

struct TerrainPixel
{
    float3 diffuseAlbedo;
    float3 worldSpaceNormal;
    float specularity;
    float cookedAmbientOcclusion;
};

uint GetEdgeIndex(float2 texCoord)
{
    float2 t = 2.f * texCoord - 1.0.xx;
    if (abs(t.x) > abs(t.y))	{ return (t.x < 0.f) ? 3 : 1; }
    else						{ return (t.y < 0.f) ? 0 : 2; }
}

TerrainPixel CalculateTexturing(PSInput geo)
{
    float4 result = 1.0.xxxx;
    float2 finalTexCoord = 0.0.xx;
    float shadowing = 1.f;

    #if (OUTPUT_TEXCOORD==1) && (SOLIDWIREFRAME_TEXCOORD==1)

            // "COVERAGE_2" is the angle based shadows layer.
            // if it exists, we will have this define
        #if defined(COVERAGE_2)
                // todo -- we need special interpolation to avoid wrapping into neighbour coverage tiles
            finalTexCoord = lerp(CoverageCoordMins[COVERAGE_2].xy, CoverageCoordMaxs[COVERAGE_2].xy, geo.texCoord.xy);

            uint3 dims;
            MakeCoverageTileSet(COVERAGE_2).GetDimensions(dims.x, dims.y, dims.z);

            float2 shadowSample = MakeCoverageTileSet(COVERAGE_2).SampleLevel(DefaultSampler, float3(finalTexCoord.xy / float2(dims.xy), CoverageOrigin[COVERAGE_2].z), 0).rg;
            shadowing = saturate(ShadowSoftness * (SunAngle + shadowSample.r)) * saturate(ShadowSoftness * (shadowSample.g - SunAngle));
        #endif

    #endif

    float slopeFactor = max(abs(geo.dhdxy.x), abs(geo.dhdxy.y));
    float3 worldPosition = 0.0.xxx;
    #if SOLIDWIREFRAME_WORLDPOSITION==1
        worldPosition = geo.worldPosition;
    #endif

    uint procTextureIndex = 0;

    #if defined(COVERAGE_1000)
        {
            uint2 finalTexCoord = lerp(CoverageCoordMins[COVERAGE_1000].xy, CoverageCoordMaxs[COVERAGE_1000].xy, geo.texCoord.xy);
            procTextureIndex = MakeCoverageTileSet(COVERAGE_1000).Load(uint4(finalTexCoord, CoverageOrigin[COVERAGE_1000].z, 0)).r;
        }
    #endif

    procTextureIndex = min(procTextureIndex, PROCEDURAL_TEXTURE_COUNT);
    ProceduralTextureOutput procTexture;
    procTexture = ProceduralTextures[0].Calculate(worldPosition, slopeFactor, geo.texCoord);
    result.rgb = procTexture.diffuseAlbedo.rgb;

    #if DRAW_WIREFRAME==1
        float patchEdge = 1.0f - edgeFactor2(frac(geo.texCoord), 5.f);
        uint edgeIndex = GetEdgeIndex(geo.texCoord);
        float3 lineColour = lerp(1.0.xxx, (NeighbourLodDiffs[edgeIndex]==0?float3(0,1,0):float3(1,0,0)), patchEdge);
        result.rgb = lerp(lineColour, result.rgb, edgeFactor(geo.barycentricCoords));
    #endif

        //	calculate the normal from the input derivatives
        //	because of the nature of terrain, we can make
        //	some simplifying assumptions.
    float3 uaxis = float3(1.0f, 0.f, geo.dhdxy.x);
    float3 vaxis = float3(0.0f, 1.f, geo.dhdxy.y);
    float3 normal = normalize(cross(uaxis, vaxis));	// because of all of the constant values, this cross product should be simplified in the optimiser

    float3 deformedNormal = normalize(
          procTexture.tangentSpaceNormal.x * uaxis
        + procTexture.tangentSpaceNormal.y * vaxis
        + procTexture.tangentSpaceNormal.z * normal);

    float emulatedAmbientOcclusion = 1.f; // lerp(0.5f, 1.f, SRGBLuminance(result.rgb));

    TerrainPixel output;
    output.diffuseAlbedo = result.rgb;
    output.worldSpaceNormal = deformedNormal;
    output.specularity = .25f * procTexture.specularity;
    output.cookedAmbientOcclusion = shadowing * emulatedAmbientOcclusion;

    #if (OUTPUT_TEXCOORD==1) && defined(VISUALIZE_COVERAGE)
        float2 coverageTC = lerp(CoverageCoordMins[VISUALIZE_COVERAGE].xy, CoverageCoordMaxs[VISUALIZE_COVERAGE].xy, geo.texCoord.xy);
        uint sample = MakeCoverageTileSet(VISUALIZE_COVERAGE).Load(uint4(uint2(coverageTC.xy), CoverageOrigin[VISUALIZE_COVERAGE].z, 0));
        output.diffuseAlbedo = GetDistinctFloatColour(sample);
    #endif

    return output;
}

[earlydepthstencil] GBufferEncoded ps_main(PSInput geo)
{
    TerrainPixel p = CalculateTexturing(geo);
    GBufferValues output = GBufferValues_Default();
    output.diffuseAlbedo = p.diffuseAlbedo;
    output.worldSpaceNormal = p.worldSpaceNormal;
    output.material.specular = p.specularity;
    output.material.roughness = 0.85f;
    output.material.metal = 0.f;
    output.cookedAmbientOcclusion = p.cookedAmbientOcclusion;
    return Encode(output);
}

[earlydepthstencil] float4 ps_main_forward(PSInput geo) : SV_Target0
{
    TerrainPixel p = CalculateTexturing(geo);
    return float4(LightingScale * p.diffuseAlbedo * p.cookedAmbientOcclusion, 1.f);
}
