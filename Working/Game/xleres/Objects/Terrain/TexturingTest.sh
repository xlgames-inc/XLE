// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainTexturing.sh"
#include "../Utility/EdgeDetection.h"
#include "../Utility/DistinctColors.h"
#include "HeightsSample.h"

class TestMaterial : IProceduralTexture
{
    ProceduralTextureOutput Calculate(float3 worldPosition, float slopeFactor, float2 textureCoord);
};

float GetHeight(int2 coord)
{
    uint rawHeightValue = HeightsTileSet.Load(int4(HeightMapOrigin.xy + coord, HeightMapOrigin.z, 0));
    return float(rawHeightValue) * LocalToCell[2][2] + LocalToCell[2][3];
}

float2 CalculateDHDXY(int2 coord)
{
    float centerHeight = GetHeight(coord);

    float2 dhdp = 0.0.xx;

#if 0
    for (uint y=0; y<5; ++y) {
        for (uint x=0; x<5; ++x) {
            float heightDiff = GetHeight(coord + int2(x,y) - int2(2,2)) - centerHeight;
            dhdp.x += SharrHoriz5x5[x][y] * heightDiff;
            dhdp.y += SharrVert5x5[x][y] * heightDiff;
        }
    }
#else
    for (uint y=0; y<3; ++y) {
        for (uint x=0; x<3; ++x) {
            float heightDiff = GetHeight(coord + int2(x,y) - int2(1,1)) - centerHeight;
            dhdp.x += SharrHoriz3x3[x][y] * heightDiff;
            dhdp.y += SharrVert3x3[x][y] * heightDiff;
        }
    }
#endif

    return dhdp;
}

static const int2 Offsets[9] =
{
    int2(-1, -1), int2( 0, -1), int2( 1, -1),
    int2(-1,  0), int2( 0,  0), int2( 1,  0),
    int2(-1,  1), int2( 0,  1), int2( 1,  1)
};

int CalculateCenterType(int2 baseTC)
{
///////////////////////////////////////////////////////////////////////////////////////////////////

        //  0  1  2
        //  3  4  5
        //  6  7  8

    float heights[9];
    float2 dhdxy[9];
    float heightDiff[9];
    for (uint c=0; c<9; c++) {
        heights[c] = GetHeight(baseTC + Offsets[c]);
        dhdxy[c] = CalculateDHDXY(baseTC + Offsets[c]);
    }
    for (uint c2=0; c2<9; c2++) {
        heightDiff[c2] = heights[c2] - heights[4];
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    float slopeThreshold = 1.5f;
    bool centerIsSlope = max(abs(dhdxy[4].x), abs(dhdxy[4].y)) > slopeThreshold;
    // bool topBottomTrans = dot(normalize(dhdxy[1]), normalize(dhdxy[8])) < .2f;
    // bool leftRightTrans = dot(normalize(dhdxy[3]), normalize(dhdxy[5])) < .2f;
    float3 b = float3( 0.f, -2.f, heightDiff[1]);
    float3 t = float3( 0.f,  2.f, heightDiff[8]);
    float3 l = float3(-2.f,  0.f, heightDiff[3]);
    float3 r = float3( 2.f,  0.f, heightDiff[5]);
    bool topBottomTrans = dot(normalize(b), -normalize(t)) < .4f;
    bool leftRightTrans = dot(normalize(l), -normalize(r)) < .4f;

    // good for finding flat places:
    // dot(dhdxy[1], dhdxy[8]) >= 1.f;
    // dot(dhdxy[3], dhdxy[5]) >= 1.f;

    if (topBottomTrans || leftRightTrans) return 2;
    return int(centerIsSlope);
}

ProceduralTextureOutput TestMaterial::Calculate(
    float3 worldPosition, float slopeFactor,
    float2 textureCoord)
{
    float2 explodedTC = textureCoord * (TileDimensionsInVertices-HeightsOverlap);
    int2 baseTC = floor(explodedTC + 0.5.xx);

    int centerType = CalculateCenterType(baseTC);

    int blockRegion;
    const float A = 0.15f;
    float2 C = explodedTC - baseTC;

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

    ProceduralTextureOutput result;
    result.diffuseAlbedo = 1.0.xxx;
    result.tangentSpaceNormal = float3(0,0,1);
    result.specularity = 1.f;

    float3 colors[6] =
    {
        float3(0,0,1),
        float3(1,1,1),
        float3(1,0,0),

        float3(0,0,.5),
        float3(.5,.5,.5),
        float3(.5,0,0)
    };

    int blendType = centerType;
    if (blockRegion != 4) {
        int connectType = CalculateCenterType(baseTC + Offsets[blockRegion]);
        blendType = connectType; // min(centerType, connectType);
    }
    float blendX = (1.f - (abs(C.x) - A) / (.5f - A));
    float blendY = (1.f - (abs(C.y) - A) / (.5f - A));
    float blendAlpha = saturate(min(blendX, blendY));
    blendAlpha = lerp(0.5f, 1.f, blendAlpha);

    // result.diffuseAlbedo = colors[centerType];

    const uint strataIndex = 1;
    float2 tcTex0 = worldPosition.xy * TextureFrequency[strataIndex][centerType];
    float3 sample0 = StrataDiffuseSample(tcTex0, strataIndex, centerType);

    if (blendType != centerType) {
        float2 tcTex1 = worldPosition.xy * TextureFrequency[0][blendType];
        float3 sample1 = StrataDiffuseSample(tcTex1, 0, blendType);

        result.diffuseAlbedo = lerp(sample1, sample0, blendAlpha);
    } else {
        result.diffuseAlbedo = sample0;
    }

    // result.diffuseAlbedo = 0.5.xxx + blendAlpha.xxx;
    // result.diffuseAlbedo = (blockRegion != 4) ? colors[3+blendType] : colors[centerType];

    return result;
}
