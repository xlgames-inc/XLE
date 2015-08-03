// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ITerrainTexturing.h"
#include "../Utility/MathConstants.h"
#include "../Utility/perlinnoise.h"
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

static const uint MaxMaterialIds = 32;
static const uint MaxProcTextures = 8;
cbuffer TexturingParameters : register(b5)
{
    float4 MappingParameters[MaxMaterialIds * 5];
}

cbuffer ProcTextureParameters : register(b7)
{
    uint4 ProcTextureInputs[MaxProcTextures];
}

TerrainTextureOutput LoadRawTexSample(uint remappedTexIndex, float2 coord, float2 duvdx, float2 duvdy)
{
    TerrainTextureOutput result;
    result.diffuseAlbedo = DiffuseAtlas.SampleGrad(MaybeAnisotropicSampler, float3(coord, remappedTexIndex), duvdx, duvdy).rgb;
    result.tangentSpaceNormal.xy = NormalsAtlas.SampleGrad(MaybeAnisotropicSampler, float3(coord, remappedTexIndex), duvdx, duvdy).xy;
    result.specularity = 1.f;

    result.tangentSpaceNormal.xy = 2.f * result.tangentSpaceNormal.xy - 1.0.xx;
    result.tangentSpaceNormal =
        float3(
            result.tangentSpaceNormal.xy,
            sqrt(saturate(1.f + dot(result.tangentSpaceNormal.xy, -result.tangentSpaceNormal.xy))));

    return result;
}

TerrainTextureOutput SampleProceduralTexture(float3 worldPosition, float2 coord, float2 duvdx, float2 duvdy, uint procTextureType)
{
    TerrainTextureOutput A = LoadRawTexSample(ProcTextureInputs[procTextureType][0], coord, duvdx, duvdy);
    TerrainTextureOutput B = LoadRawTexSample(ProcTextureInputs[procTextureType][1], coord, duvdx, duvdy);

    float hgrid = asfloat(ProcTextureInputs[procTextureType][2]);
    float gain = asfloat(ProcTextureInputs[procTextureType][3]);

        // the noise calculation here is very expensive! it should really be replaced
        // with a precalculated texture (or maybe even just cache a noise field around the camera?)
    float noiseValue0 = fbmNoise2D(worldPosition.xy, hgrid, gain, 2.1042, 4);
#if 1
    return Blend(A, B, saturate(0.5f + noiseValue0));
#else
    TerrainTextureOutput result = TerrainTextureOutput_Blank();
    result.tangentSpaceNormal = float3(0,0,1);
    result.specularity = 1.f;
    result.diffuseAlbedo = saturate(0.5f + 0.9f * noiseValue0).xxx;
    return result;
#endif
}

TerrainTextureOutput LoadTexSample(float3 worldPosition, uint materialId, uint texType)
{
    const uint strataIndex = 0;
    float4 param = MappingParameters[materialId*5+texType];
    float2 tc = worldPosition.xy * param[0];
    uint remappedIndex = asuint(param[3]);
    uint textureType = asuint(param[2]);

        // note --  Potentially expensive branch here, were we must choose
        //          whether or not to use procedural texturing path. We should
        //          get a lot of coherency though.
    float2 duvdx = ddx(tc), duvdy = ddy(tc);
    [branch] if (textureType > 0) {
        return SampleProceduralTexture(worldPosition, tc, duvdx, duvdy, remappedIndex);
    } else {
        return LoadRawTexSample(remappedIndex, tc, duvdx, duvdy);
    }
}

float GetBlendingWeightBias(TerrainTextureOutput texSample)
{
        // Bias blending based on brightness.
        //  Gives a slighly nicer edge to blending
        //  But this would ideally be using values in the alpha channel of the texture
    // float weight = 2.f * (SRGBLuminance(texSample.diffuseAlbedo.rgb) - 0.5f);
    // weight *= weight;
    // return 5.f + 5.f * weight;
    return 10.f * SRGBLuminance(texSample.diffuseAlbedo.rgb);
}

TerrainTextureOutput GradFlagTexturing::Calculate(
    float3 worldPosition, float2 dhdxy, uint materialId, float2 textureCoord)
{
    TerrainTextureOutput result = TerrainTextureOutput_Blank();

    float3 debugColors[4] =
    {
        float3(1, 1, 1),
        float3(0, 0, 1),
        float3(0, 1, 0),
        float3(1, 0, 0)
    };

    const uint TexType_Flat = 0;
    const uint TexType_Slope = 1;
    const uint TexType_RoughFlat = 2;
    const uint TexType_SlopeFlat = 3;
    const uint TexType_Blending = 4;

    const float A = 0.15f;  // grace area

    #if defined(COVERAGE_1000)
        const bool doBlending = false;
    #else
        const bool doBlending = true;
    #endif
    if (!doBlending) {
        float2 explodedTC = textureCoord * (TileDimensionsInVertices-HeightsOverlap);
        int s = CalculateCenterType(floor(explodedTC));
        return LoadTexSample(worldPosition, materialId, s);
    } else {
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
            w[0] = (1.f - C.x)  * (1.f - C.y);
            w[1] = (C.x)        * (1.f - C.y);
            w[2] = (1.f - C.x)  * (C.y);
            w[3] = (C.x)        * (C.y);

            float typeWeights[4];
            typeWeights[0] = typeWeights[1] = typeWeights[2] = typeWeights[3] = 0;

            float weightTotal = 0.f;
            [unroll] for (uint c=0; c<4; ++c) {
                if (useDebugColors) {
                    result.diffuseAlbedo += debugColors[s[c]] * w[c];
                    weightTotal += .25f;
                } else {
                    TerrainTextureOutput texSample = LoadTexSample(worldPosition, materialId, s[c]);
                    float weightBias = GetBlendingWeightBias(texSample);
                    float weight = weightBias * w[c] + w[c];
                    result = AddWeighted(result, texSample, weight);
                    weightTotal += weight;
                }

                typeWeights[s[c]] += w[c];
            }

            result.diffuseAlbedo /= max(1e-5f, weightTotal);
            result.specularity /= max(1e-5f, weightTotal);

                // we can find the edging like this --
                //      This allows blending in a transition texture between the repeating textures
            const bool blendingTexture = false;
            if (blendingTexture) {
                float trans = 0.0f;
                [unroll] for (uint c2=0; c2<2; ++c2)
                    trans += .5f - abs(0.5f - typeWeights[c2]);
                trans *= trans;
                result = Blend(result, LoadTexSample(worldPosition, materialId, TexType_Blending), trans);
            }

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
    return CalculateRawGradientFlags(coord, 2.f);
}
