// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ITerrainTexturing.h"
#include "../Utility/MathConstants.h"
#include "HeightsSample.h"

class GradFlagTexturing : ITerrainTexturing
{
    TerrainTextureOutput Calculate(float3 worldPosition, float2 dhdxy, uint materialId, float2 texCoord);
};

uint DynamicGradientFlag(int2 coord);

int CalculateCenterType(int2 baseTC)
{
    return LoadEncodedGradientFlags(int3(HeightMapOrigin.xy + baseTC, HeightMapOrigin.z));
    // return DynamicGradientFlag(baseTC);
}

static const uint MaxMaterialIds = 64;
cbuffer TexturingParameters : register(b5)
{
    float4 MappingParameters[MaxMaterialIds * 5];
}

TerrainTextureOutput LoadTexSample(float3 worldPosition, uint materialId, uint texType)
{
    const uint strataIndex = 0;
    float2 tc = worldPosition.xy * MappingParameters[materialId*5+texType][0];
    uint remappedIndex = asuint(MappingParameters[materialId*5+texType][3]);

    float3 coord = float3(tc, remappedIndex);
    TerrainTextureOutput result;
    result.diffuseAlbedo = DiffuseAtlas.Sample(MaybeAnisotropicSampler, coord).rgb;
    result.tangentSpaceNormal = float3(0,0,1);
    result.specularity = 1.f;
    return result;
}

TerrainTextureOutput Blend(TerrainTextureOutput zero, TerrainTextureOutput one, float alpha)
{
    TerrainTextureOutput result;
    result.diffuseAlbedo = lerp(zero.diffuseAlbedo, one.diffuseAlbedo, alpha);
    result.tangentSpaceNormal = float3(0,0,1);
    result.specularity = 1.f;
    return result;
}

TerrainTextureOutput AddWeighted(TerrainTextureOutput zero, TerrainTextureOutput one, float weight)
{
    TerrainTextureOutput result;
    result.diffuseAlbedo = zero.diffuseAlbedo + one.diffuseAlbedo * weight;
    result.tangentSpaceNormal = float3(0,0,1);
    result.specularity = 1.f;
    return result;
}

TerrainTextureOutput GradFlagTexturing::Calculate(
    float3 worldPosition, float2 dhdxy, uint materialId, float2 textureCoord)
{
    TerrainTextureOutput result;
    result.diffuseAlbedo = 0.0.xxx;
    result.tangentSpaceNormal = float3(0,0,1);
    result.specularity = 1.f;

    float3 debugColors[4] =
    {
        float3(0, 0, 1),
        float3(1, 1, 1),
        float3(1, 0, 0),
        float3(0, 1, 1)
    };

    const uint TexType_Flat = 0;
    const uint TexType_Slope = 1;
    const uint TexType_RoughFlat = 2;
    const uint TexType_SlopeFlat = 3;
    const uint TexType_Blending = 4;

    const float A = 0.15f;  // grace area

    const bool highQualityBlend = true;
    const bool useDebugColors = false;
    if (highQualityBlend) {

            // This is a basic bilinear style blend
            // it requires 4 texture lookups per pixel
        float2 explodedTC = textureCoord * (TileDimensionsInVertices-HeightsOverlap);
        int2 baseTC = floor(explodedTC);
        float2 C = explodedTC - baseTC;

        C = saturate((C - A.xx) / (1.f - 2 * A));

        int s[4];
        s[0] = CalculateCenterType(baseTC);
        s[1] = CalculateCenterType(baseTC + int2(1, 0));
        s[2] = CalculateCenterType(baseTC + int2(0, 1));
        s[3] = CalculateCenterType(baseTC + int2(1, 1));

        float w[4];
        w[0] = (1.f - C.x) * (1.f - C.y);
        w[1] = (C.x) * (1.f - C.y);
        w[2] = (1.f - C.x) * (C.y);
        w[3] = (C.x) * (C.y);

        float typeWeights[2];
        typeWeights[0] = typeWeights[1] = 0;

        [unroll] for (uint c=0; c<4; ++c) {
            if (useDebugColors) result.diffuseAlbedo += debugColors[s[c]&1] * w[c];
            else {
                result = AddWeighted(
                    result,
                    LoadTexSample(worldPosition, materialId, s[c]&1),
                    w[c]);
            }

            typeWeights[s[c]&1] += w[c];
        }

        float2 B = explodedTC - baseTC;
        float roughAlpha = 0.f;
        roughAlpha += ((s[0] & 2)!=0) * (1.f - B.x) * (1.f - B.y);
        roughAlpha += ((s[1] & 2)!=0) * (B.x) * (1.f - B.y);
        roughAlpha += ((s[2] & 2)!=0) * (1.f - B.x) * (B.y);
        roughAlpha += ((s[3] & 2)!=0) * (B.x) * (B.y);
        roughAlpha = 4.f * saturate(roughAlpha - 0.8f);
        // roughAlpha *= roughAlpha; roughAlpha *= roughAlpha;
        // roughAlpha *= roughAlpha; roughAlpha *= roughAlpha;
        result = Blend(result, LoadTexSample(worldPosition, materialId, TexType_RoughFlat), roughAlpha);

            // we can find the edging like this --
            //      This allows blending in a transition texture between the repeating textures
        float trans = 0.0f;
        [unroll] for (uint c2=0; c2<2; ++c2)
            trans += .5f - abs(0.5f - typeWeights[c2]);
        trans *= trans;
        result = Blend(result, LoadTexSample(worldPosition, materialId, TexType_Blending), trans);

    } else {

            // this blending method limits the number of texture lookups to
            // only 2. But the blending is not perfect. Sometimes it produces
            // sharp edges.

        float2 explodedTC = textureCoord * (TileDimensionsInVertices-HeightsOverlap);
        int2 baseTC = floor(explodedTC + 0.5.xx);
        float2 C = explodedTC - baseTC;

        int centerType = CalculateCenterType(baseTC);

        int blockRegion;

    #if 0
            // this gives diagonals in the corners
        if (C.x <= -A) {
            if (C.y <= -A) blockRegion = 0;
            else if (C.y >= A) blockRegion = 6;
            else blockRegion = 3;
        } else if (C.x >= A) {
            if (C.y <= -A) blockRegion = 2;
            else if (C.y >= A) blockRegion = 8;
            else blockRegion = 5;
        } else {
            if (C.y <= -A) blockRegion = 1;
            else if (C.y >= A) blockRegion = 7;
            else blockRegion = 4;
        }
    #else
            // this has no diagonals
        if (abs(C.x) < A && abs(C.y) < A) {
            blockRegion = 4;
        } else if (C.x > C.y) {
            if (-C.y > C.x) blockRegion = 1;
            else blockRegion = 5;
        } else {
            if (-C.x > C.y) blockRegion = 3;
            else blockRegion = 7;
        }
    #endif

        const int2 Offsets[9] =
        {
            int2(-1, -1), int2( 0, -1), int2( 1, -1),
            int2(-1,  0), int2( 0,  0), int2( 1,  0),
            int2(-1,  1), int2( 0,  1), int2( 1,  1)
        };

        int blendType = centerType;
        if (blockRegion != 4) {
            int connectType = CalculateCenterType(baseTC + Offsets[blockRegion]);
            blendType = connectType; // min(centerType, connectType);
        }

        float2 blendXY = (abs(C) - A.xx) / (.5f - A);
        float blendAlpha = max(blendXY.x, blendXY.y);

        int s[2];
        s[0] = centerType;
        s[1] = blendType;
        float w[2];
        w[0] = 1.f - 0.5f * blendAlpha;
        w[1] = 0.5f * blendAlpha;

        [unroll] for (uint c=0; c<2; ++c) {
            if (useDebugColors) result.diffuseAlbedo += debugColors[s[c]] * w[c];
            else {
                result = AddWeighted(result, LoadTexSample(worldPosition, materialId, s[c]), w[c]);
            }
        }
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

float GetHeight(int2 coord)
{
    uint rawHeightValue = LoadRawHeightValue(int3(HeightMapOrigin.xy + coord, HeightMapOrigin.z));
    return float(rawHeightValue) * LocalToCell[2][2] + LocalToCell[2][3];
}

bool CoordIsValid(int2 coord) { return true; }

#include "GradientFlags.h"

uint DynamicGradientFlag(int2 coord)
{
    return CalculateRawGradientFlags(coord, 2.f, SlopeThresholdDefault, TransThresholdDefault);
}
