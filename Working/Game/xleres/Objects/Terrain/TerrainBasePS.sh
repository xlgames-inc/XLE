// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define OUTPUT_TEXCOORD 1
#define VSOUTPUT_EXTRA float2 dhdxy : DHDXY;

#define GBUFFER_TYPE 1	// hack -- (not being set by the client code currently)

#include "StrataTexturing.h"
#include "GradFlagTexturing.h"

#include "TerrainGenerator.h"
#include "ITerrainTexturing.h"

#include "../../gbuffer.h"
#include "../../Utility/DistinctColors.h"

#define DO_DEFORM_NORMAL 1

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

#if DRAW_WIREFRAME==1
    float3 BlendWireframe(PSInput geo, float3 baseColour);
#endif

struct TerrainPixel
{
    float3 diffuseAlbedo;
    float3 worldSpaceNormal;
    float specularity;
    float cookedAmbientOcclusion;
};

float2 LoadInterpolatedShadows(Texture2DArray<float2> tex, float2 texCoord, int arrayIndex)
{
    float2 floored = floor(texCoord);
    float2 ceiled = floored + 1.0.xx;
    float2 alpha = texCoord - floored;
    float2 A = tex.Load(int4(floored.x, floored.y, arrayIndex, 0));
    float2 B = tex.Load(int4(ceiled.x, floored.y, arrayIndex, 0));
    float2 C = tex.Load(int4(floored.x, ceiled.y, arrayIndex, 0));
    float2 D = tex.Load(int4(ceiled.x, ceiled.y, arrayIndex, 0));
    return
          (1.f - alpha.x) * (1.f - alpha.y) * A
        + (alpha.x) * (1.f - alpha.y) * B
        + (1.f - alpha.x) * (alpha.y) * C
        + (alpha.x) * (alpha.y) * D
        ;
}

TerrainPixel CalculateTexturing(PSInput geo)
{
    float2 finalTexCoord = 0.0.xx;
    float shadowing = 1.f;

    #if (OUTPUT_TEXCOORD==1) && (SOLIDWIREFRAME_TEXCOORD==1)

            // "COVERAGE_2" is the angle based shadows layer.
            // if it exists, we will have this define
        #if defined(COVERAGE_2)
                // todo -- we need special interpolation to avoid wrapping into neighbour coverage tiles
            finalTexCoord = lerp(CoverageCoordMins[COVERAGE_2].xy, CoverageCoordMaxs[COVERAGE_2].xy, geo.texCoord.xy);
            float2 shadowSample = LoadInterpolatedShadows(MakeCoverageTileSet(COVERAGE_2), finalTexCoord, CoverageOrigin[COVERAGE_2].z);

            shadowing
                = saturate(ShadowSoftness * (SunAngle + shadowSample.r))
                * saturate(ShadowSoftness * (shadowSample.g - SunAngle));
        #endif

    #endif

    float3 worldPosition = 0.0.xxx;
    #if OUTPUT_WORLD_POSITION==1
        worldPosition = geo.worldPosition;
    #endif

    TerrainTextureOutput procTexture;

    #if defined(COVERAGE_1000)
        {
            float2 matIdTC = lerp(
                CoverageCoordMins[COVERAGE_1000].xy,
                CoverageCoordMaxs[COVERAGE_1000].xy, geo.texCoord.xy);
            float2 matIdTCB = floor(matIdTC);
            float2 A = matIdTC - matIdTCB;
            const float w[4] =
            {
                (1.f - A.x) * A.y,
                A.x * A.y,
                A.x * (1.f - A.y),
                (1.f - A.x) * (1.f - A.y)
            };

            uint materialId[4];
            materialId[0] = MakeCoverageTileSet(COVERAGE_1000).Load(
                uint4(uint2(matIdTCB) + uint2(0,1), CoverageOrigin[COVERAGE_1000].z, 0));
            materialId[1] = MakeCoverageTileSet(COVERAGE_1000).Load(
                uint4(uint2(matIdTCB) + uint2(1,1), CoverageOrigin[COVERAGE_1000].z, 0));
            materialId[2] = MakeCoverageTileSet(COVERAGE_1000).Load(
                uint4(uint2(matIdTCB) + uint2(1,0), CoverageOrigin[COVERAGE_1000].z, 0));
            materialId[3] = MakeCoverageTileSet(COVERAGE_1000).Load(
                uint4(matIdTCB, CoverageOrigin[COVERAGE_1000].z, 0));

            float2 m = 1.0f / float2(CoverageCoordMaxs[COVERAGE_1000].xy - CoverageCoordMins[COVERAGE_1000].xy);
            const float2 tcOffset[4] =
            {
                float2(0.f, m.y),
                float2(m.x, m.y),
                float2(m.x, 0.f),
                0.0.xx
            };

                // note that when we do this, we don't need to do blending inside
                // of the ITerrainTexturing object! If we blend inside of that object
                // we will end up with 16 taps... We only need 4, so long as coverage
                // samples evenly fit inside of the gradient flag samples (which they
                // should always do)
            procTexture = TerrainTextureOutput_Blank();
            [unroll] for (uint c=0; c<4; c++) {
                float2 texCoord = geo.texCoord + tcOffset[c];
                TerrainTextureOutput sample = MainTexturing.Calculate(worldPosition, geo.dhdxy, materialId[c], texCoord);
                procTexture = AddWeighted(procTexture, sample, w[c]);
            }
        }
    #else
        procTexture = MainTexturing.Calculate(worldPosition, geo.dhdxy, 0, geo.texCoord);
    #endif

    float3 resultDiffuse = procTexture.diffuseAlbedo.rgb;

    #if DRAW_WIREFRAME==1
        resultDiffuse = BlendWireframe(geo, resultDiffuse);
    #endif

        //	How to calculate the tangent frame? If we do
        //  not normalize uaxis and vaxis before the cross
        //  product, it should allow the optimizer to simplify
        //  the cross product operation... But we will end up
        //  with a tangent frame with 3 unnormalized vectors.
        //
        //  Alternatively, we could normalize uaxis and vaxis
        //  first...?
    float3 uaxis = float3(1.0f, 0.f, geo.dhdxy.x);
    float3 vaxis = float3(0.0f, 1.f, geo.dhdxy.y);
    float3 normal = cross(uaxis, vaxis);

    #if DO_DEFORM_NORMAL==1
            // because of all of the blending, tangentSpaceNormal
            // will be unnormalized. We could normalize it here, before
            // we pass it through the tangent frame.
            // But the tangent frame is already unnormalized. So we already
            // have some accuracy problems. Well, we can avoid normalizes
            // until the very end, and just accept some accuracy problems.
        float3 deformedNormal = normalize(
             procTexture.tangentSpaceNormal.x * uaxis
           + procTexture.tangentSpaceNormal.y * vaxis
           + procTexture.tangentSpaceNormal.z * normal);
    #else
        float3 deformedNormal = normalize(normal);
    #endif

    #if defined(COVERAGE_3)
        finalTexCoord = lerp(CoverageCoordMins[COVERAGE_3].xy, CoverageCoordMaxs[COVERAGE_3].xy, geo.texCoord.xy);

        float aoSample;
        {
            float2 tcf = floor(finalTexCoord);
            float2 A = finalTexCoord - tcf;
            const float w[4] =
            {
                (1.f - A.x) * A.y,
                A.x * A.y,
                A.x * (1.f - A.y),
                (1.f - A.x) * (1.f - A.y)
            };

            uint aoSamples[4];
            aoSamples[0] = MakeCoverageTileSet(COVERAGE_3).Load(
                uint4(uint2(tcf) + uint2(0,1), CoverageOrigin[COVERAGE_3].z, 0));
            aoSamples[1] = MakeCoverageTileSet(COVERAGE_3).Load(
                uint4(uint2(tcf) + uint2(1,1), CoverageOrigin[COVERAGE_3].z, 0));
            aoSamples[2] = MakeCoverageTileSet(COVERAGE_3).Load(
                uint4(uint2(tcf) + uint2(1,0), CoverageOrigin[COVERAGE_3].z, 0));
            aoSamples[3] = MakeCoverageTileSet(COVERAGE_3).Load(
                uint4(tcf, CoverageOrigin[COVERAGE_3].z, 0));

            aoSample =
                ( aoSamples[0] * w[0] + aoSamples[1] * w[1]
                + aoSamples[2] * w[2] + aoSamples[3] * w[3] ) / float(0xff);
        }

        // resultDiffuse = lerp(float3(0,0,1), float3(1,0,0), aoSample);
        shadowing *= aoSample;
    #endif

    TerrainPixel output;
    output.diffuseAlbedo = resultDiffuse;
    output.worldSpaceNormal = deformedNormal;
    output.specularity = .25f * procTexture.specularity;
    output.cookedAmbientOcclusion = shadowing;

    #if (OUTPUT_TEXCOORD==1) && defined(VISUALIZE_COVERAGE)
        if ((dot(uint2(geo.position.xy), uint2(1,1))/4)%4 == 0) {
            float2 coverageTC = lerp(CoverageCoordMins[VISUALIZE_COVERAGE].xy, CoverageCoordMaxs[VISUALIZE_COVERAGE].xy, geo.texCoord.xy);
            uint sample = MakeCoverageTileSet(VISUALIZE_COVERAGE).Load(uint4(uint2(coverageTC.xy), CoverageOrigin[VISUALIZE_COVERAGE].z, 0));
            output.diffuseAlbedo = GetDistinctFloatColour(sample);
        }
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

#if DRAW_WIREFRAME==1

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

    uint GetEdgeIndex(float2 texCoord)
    {
        float2 t = 2.f * texCoord - 1.0.xx;
        if (abs(t.x) > abs(t.y))	{ return (t.x < 0.f) ? 3 : 1; }
        else						{ return (t.y < 0.f) ? 0 : 2; }
    }

    float3 BlendWireframe(PSInput geo, float3 baseColour)
    {
        float patchEdge = 1.0f - edgeFactor2(frac(geo.texCoord), 5.f);
        uint edgeIndex = GetEdgeIndex(geo.texCoord);
        float3 lineColour = lerp(1.0.xxx, (NeighbourLodDiffs[edgeIndex]==0?float3(0,1,0):float3(1,0,0)), patchEdge);
        float ef = 1.f - edgeFactor(geo.barycentricCoords);
        ef *= int(512.0f * (geo.texCoord.x + geo.texCoord.y)) & 1;      // dotted line (causes horrible aliasing in the distance)
        return lerp(baseColour.rgb, lineColour, ef);
    }

#endif
