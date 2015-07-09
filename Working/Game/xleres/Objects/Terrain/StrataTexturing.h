// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ITerrainTexturing.h"
#include "../../CommonResources.h"
#include "../../Utility/perlinnoise.h"
#include "../../Colour.h"

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

///////////////////////////////////////////////////////////////////////////////////////////////////

#if STRATA_COUNT==0
    TerrainTextureOutput GetTextureForStrata(
        uint strataIndex, float3 worldPosition,
        float slopeFactor, float2 textureCoord, float noiseValue0)
    {
        TerrainTextureOutput result;
        result.diffuseAlbedo = 1.0.xxx;
        result.tangentSpaceNormal = float3(0.5,0.5,1);
        result.specularity = 1.0f;
        return result;
    }
#else
    static const uint StrataCount = STRATA_COUNT;

    cbuffer TexturingParameters : register(b5)
    {
        float4 StrataEndHeights[StrataCount];
        float4 TextureFrequency[StrataCount];
    }

    float3 AsAtlasCoord(float2 tc, uint strata, uint textureType)
    {
        return float3(tc, strata*3+textureType);
    }

    float3 DiffuseAtlasSample(float2 tc, uint strata, uint textureType)
    {
        return DiffuseAtlas.Sample(MaybeAnisotropicSampler, AsAtlasCoord(tc, strata, textureType)).rgb;
    }

    float StrataSpecularSample(float2 tc, uint strata, uint textureType)
    {
        return SRGBLuminance(SpecularityAtlas.Sample(MaybeAnisotropicSampler, AsAtlasCoord(tc, strata, textureType)).rgb);
    }

    static const bool UseNoramlsAtlas = false;
    static const bool UseStrataSpecular = false;

    TerrainTextureOutput GetTextureForStrata(
        uint strataIndex, float3 worldPosition,
        float slopeFactor, float2 textureCoord, float noiseValue0)
    {
        float2 tc0 = worldPosition.xy * TextureFrequency[strataIndex].xx;
        float2 tc1 = worldPosition.xy * TextureFrequency[strataIndex].yy;

        float alpha = saturate(.5f + .7f * noiseValue0);
        // alpha = min(1.f, exp(32.f * (alpha-.5f)));
        // alpha = lerp(.25f, 0.75f, alpha);

        TerrainTextureOutput result;
        float3 A = DiffuseAtlasSample(tc0, strataIndex, 0);
        float3 B = DiffuseAtlasSample(tc1, strataIndex, 1);
        result.diffuseAlbedo = lerp(A, B, alpha);

        if (UseNoramlsAtlas) {
            float3 An = NoramlsAtlas.Sample(MaybeAnisotropicSampler, float3(tc0, strataIndex*3+0)).rgb;
            float3 Bn = NoramlsAtlas.Sample(MaybeAnisotropicSampler, float3(tc1, strataIndex*3+1)).rgb;
            result.tangentSpaceNormal = lerp(An, Bn, alpha);
        } else {
            result.tangentSpaceNormal = float3(0.5,0.5,1);
        }

        if (UseStrataSpecular) {
            float As = StrataSpecularSample(tc0, strataIndex, 0);
            float Bs = StrataSpecularSample(tc1, strataIndex, 1);
            result.specularity = lerp(As, Bs, alpha);
        } else {
            result.specularity = 1;
        }

        const float slopeStart = .7f; // .55f;
        const float slopeSoftness = 7.f; // 3.f;
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
            float3 S = DiffuseAtlasSample(tcS0, strataIndex, 2);
            float3 Sn = NoramlsAtlas.Sample(MaybeAnisotropicSampler, float3(tcS0, arrayIdx)).rgb;
            float Ss = StrataSpecularSample(tcS0, strataIndex, 2);

            tcS0.x = worldPosition.y * TextureFrequency[strataIndex].z;
            float3 S2 = DiffuseAtlasSample(tcS0, strataIndex, 2);
            float3 Sn2 = NoramlsAtlas.Sample(MaybeAnisotropicSampler, float3(tcS0, arrayIdx)).rgb;
            float Ss2 = StrataSpecularSample(tcS0, strataIndex, 2);

            float A = a / (a+b);
            float B = b / (a+b);
            S = S * A + S2 * B;
            Sn = Sn * A + Sn2 * B;
            Ss = Ss * A + Ss2 * B;

            result.diffuseAlbedo = lerp(result.diffuseAlbedo, slopeDarkness * S, slopeAlpha);
            if (UseNoramlsAtlas)
                result.tangentSpaceNormal = lerp(result.tangentSpaceNormal, Sn, slopeAlpha);

            if (UseStrataSpecular)
                result.specularity = lerp(result.specularity, Ss, slopeAlpha);
        }

        return result;
    }
#endif

class StrataMaterial : ITerrainTexturing
{
    TerrainTextureOutput Calculate(float3 worldPosition, float2 dhdxy, uint materialId, float2 textureCoord);
};

TerrainTextureOutput StrataMaterial::Calculate(float3 worldPosition, float2 dhdxy, uint materialId, float2 textureCoord)
{
        //	Build a texturing value by blending together multiple input textures
        //	Select the input textures from some of the input parameters, like slope and height

        //	This mostly just a place-holder!
        //	The noise calculation is too expensive, and there is very little configurability!
        //	But, I need a terrain that looks roughly terrain-like just to get things working

    float noiseValue0 = fbmNoise2D(worldPosition.xy, 225.f, .65f, 2.1042, 4);
    noiseValue0 += .5f * fbmNoise2D(worldPosition.xy, 33.7f, .75f, 2.1042, 4);
    noiseValue0 = clamp(noiseValue0, -1.f, 1.f);

    float slopeFactor = max(abs(dhdxy.x), abs(dhdxy.y));

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

    TerrainTextureOutput value0 = GetTextureForStrata(strataBase0, worldPosition, slopeFactor, textureCoord, noiseValue0);
    TerrainTextureOutput value1 = GetTextureForStrata(strataBase1, worldPosition, slopeFactor, textureCoord, noiseValue0);

    TerrainTextureOutput result;
    result.diffuseAlbedo = lerp(value0.diffuseAlbedo, value1.diffuseAlbedo, strataAlpha);
    result.tangentSpaceNormal = lerp(value0.tangentSpaceNormal, value1.tangentSpaceNormal, strataAlpha);
    result.specularity = lerp(value0.specularity, value1.specularity, strataAlpha);

    float2 nxy = 2.f * result.tangentSpaceNormal.xy - 1.0.xx;
    result.tangentSpaceNormal = float3(nxy, sqrt(saturate(1.f + dot(nxy, -nxy))));

    return result;
}
