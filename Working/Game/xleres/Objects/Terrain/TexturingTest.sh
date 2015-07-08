// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainTexturing.sh"
#include "../Utility/DistinctColors.h"
#include "../Utility/MathConstants.h"
#include "HeightsSample.h"

class TestMaterial : IProceduralTexture
{
    ProceduralTextureOutput Calculate(float3 worldPosition, float slopeFactor, float2 textureCoord);
};

uint DynamicGradientFlag(int2 coord);

int CalculateCenterType(int2 baseTC)
{
    return LoadEncodedGradientFlags(int3(HeightMapOrigin.xy + baseTC, HeightMapOrigin.z));
    // return DynamicGradientFlag(baseTC);
}

float3 LoadTexSample(float3 worldPosition, uint index)
{
    const uint strataIndex = 0;
    float2 tcTex0 = worldPosition.xy * TextureFrequency[strataIndex][index];
    return StrataDiffuseSample(tcTex0, strataIndex, index);
}

ProceduralTextureOutput TestMaterial::Calculate(
    float3 worldPosition, float slopeFactor,
    float2 textureCoord)
{
    ProceduralTextureOutput result;
    result.diffuseAlbedo = 0.0.xxx;
    result.tangentSpaceNormal = float3(0,0,1);
    result.specularity = 1.f;

    float3 debugColors[3] =
    {
        float3(0,0,1),
        float3(1,1,1),
        float3(1,0,0),
    };

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

        [unroll] for (uint c=0; c<4; ++c) {
            if (useDebugColors) result.diffuseAlbedo += debugColors[s[c]] * w[c];
            else                result.diffuseAlbedo += LoadTexSample(worldPosition, s[c]) * w[c];
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
            else                result.diffuseAlbedo += LoadTexSample(worldPosition, s[c]) * w[c];
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
