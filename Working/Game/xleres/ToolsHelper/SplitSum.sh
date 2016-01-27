// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define FORCE_GGX_REF

#include "../Lighting/SpecularMethods.h"
#include "../Lighting/ImageBased.h"

static const uint PassSampleCount = 4;      // 256

float4 main(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
    float NdotV = texCoord.x;
    float roughness = 1.f-texCoord.y;
    const uint sampleCount = 64 * 1024;
    return float4(GenerateSplitTerm(NdotV, roughness, sampleCount), 0, 1);
}

float4 main_trans(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
    float NdotV = texCoord.x;
    float roughness = 1.f-texCoord.y;
    const uint sampleCount = 64 * 1024;
    return float4(GenerateSplitTermTrans(NdotV, roughness, sampleCount), 0, 1);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

cbuffer SubResourceId
{
    uint ArrayIndex, MipIndex;
    uint PassIndex, PassCount;
}

Texture2D Input;

float3 SampleInputTexture(float3 direction)
{
    // return Input.SampleLevel(DefaultSampler, AdjSkyCubeMapCoords(L), 0).rgb;
    direction = float3(direction.y, direction.x, direction.z);
    float2 coord = EquirectangularMappingCoord(direction);
    coord.x = -coord.x; // hack to match IBL Baker
    return Input.SampleLevel(DefaultSampler, coord, 0).rgb;
}

// See DirectX documentation:
// https://msdn.microsoft.com/en-us/library/windows/desktop/bb204881(v=vs.85).aspx
static const float3 CubeMapPanels_DX[6][3] =
{
        // +X, -X
    { float3(0,0,-1), float3(0,-1,0), float3(1,0,0) },
    { float3(0,0,1), float3(0,-1,0), float3(-1,0,0) },

        // +Y, -Y
    { float3(1,0,0), float3(0,0,1), float3(0,1,0) },
    { float3(1,0,0), float3(0,0,-1), float3(0,-1,0) },

        // +Z, -Z
    { float3(1,0,0), float3(0,-1,0), float3(0,0,1) },
    { float3(-1,0,0), float3(0,-1,0), float3(0,0,-1) }
};

float3 CalculateCubeMapDirection(uint panelIndex, float2 texCoord)
{
    float3 plusX  = CubeMapPanels_DX[panelIndex][0];
    float3 plusY  = CubeMapPanels_DX[panelIndex][1];
    float3 center = CubeMapPanels_DX[panelIndex][2];
    return normalize(
          center
        + plusX * (2.f * texCoord.x - 1.f)
        + plusY * (2.f * texCoord.y - 1.f));
}

static const float SpecularIBLMipMapCount = 9.f;

float MipmapToRoughness(uint mipIndex)
{
    // We can adjust the mapping between roughness and the mipmaps as needed...
    // Each successive mipmap is smaller, so we loose resolution linearly against
    // roughness (even though the blurring amount is not actually linear against roughness)
    // We could use the inverse of the GGX function to calculate something that is
    // more linear against the sample cone size, perhaps...?
    // Does it make sense to offset by .5 to get a value in the middle of the range? We
    // will be using trilinear filtering to get a value between 2 mipmaps.
    // Arguably a roughness of "0.0" is not very interesting -- but we commit our
    // highest resolution mipmap to that.
    return 0.05f + saturate(MipIndex / float(SpecularIBLMipMapCount));
}

float4 EquiRectFilterGlossySpecular(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
    // This is the second term of the "split-term" solution for IBL glossy specular
    // Here, we prefilter the reflection texture in such a way that the blur matches
    // the GGX equation.
    //
    // This is very similar to calculating the full IBL reflections. However, we're
    // making some simplifications to make it practical to precalculate it.
    // We can choose to use an importance-sampling approach. This will limit the number
    // of samples to some fixed amount. Alternatively, we can try to sample the texture
    // it some regular way (ie, by sampling every texel instead of just the ones suggested
    // by importance sampling).
    //
    // If we sample every pixel we need to weight by the solid angle of the texel we're
    // reading from. But if we're just using the importance sampling approach, we can skip
    // this step (it's just taken care of by the probability density function weighting)

    float3 cubeMapDirection = CalculateCubeMapDirection(ArrayIndex, texCoord);

        // undoing AdjSkyCubeMapCoords
    cubeMapDirection = float3(cubeMapDirection.x, -cubeMapDirection.z, cubeMapDirection.y);

        // have to add another flip to get the same result as the rest of the pipeline
        // it feels like there's ome confusion coming in from going through CubeMapGen (which
        // natively uses an OpenGL cubemap format). Plus all these other conversion and adjustments!!
    cubeMapDirection = float3(-cubeMapDirection.x, -cubeMapDirection.y, cubeMapDirection.z);

    // return float4(cubeMapDirection, 1.f);
    // return float4(SampleInputTexture(cubeMapDirection), 1);

    // Here is the key simplification -- we assume that the normal and view direction are
    // the same. This means that some distortion on extreme angles is lost. This might be
    // incorrect, but it will probably only be noticeable when compared to a ray traced result.
    float3 normal = cubeMapDirection;
    float3 viewDirection = cubeMapDirection;

    float roughness = MipmapToRoughness(MipIndex);
    SpecularParameters specParam = SpecularParameters_RoughF0(roughness, 1.0.xxx);

    float alphag = RoughnessToGAlpha(specParam.roughness);
    float alphad = RoughnessToDAlpha(specParam.roughness);

    float3 result = 0.0.xxx;
    float totalWeight = 0.f;

        // We need a huge number of samples for a smooth result
        // Perhaps we should use double precision math?
        // Anyway, we need to split it up into multiple passes, otherwise
        // the GPU gets locked up in the single draw call for too long.
    const uint totalSampleCount = PassSampleCount * PassCount;
    [loop] for (uint s=0; s<PassSampleCount; ++s) {
            // We could build a distribution of "H" vectors here,
            // or "L" vectors. It makes sense to use H vectors
        precise float3 H = BuildSampleHalfVectorGGX(s+PassIndex*PassSampleCount, totalSampleCount, normal, alphad);
        precise float3 L = 2.f * dot(viewDirection, H) * H - viewDirection;

            // Now we can light as if the point on the reflection map
            // is a directonal light.
            // Note that we could estimate the average distance between
            // samples in this step, and compare that to the input texture
            // size. In this way, we could select a mipmap from the input
            // texture. This would help avoid sampling artifacts, and we would
            // end up with slightly more blurry result.

        float3 lightColor = SampleInputTexture(L);

            // Unreal course notes say the probability distribution function is
            //      D * NdotH / (4 * VdotH)
            // We need to apply the inverse of this to weight the sample correctly.
            // For the most part, it just factors out of the equation...
        float NdotH = saturate(dot(normal, H));
        float VdotH = saturate(dot(viewDirection, H));

        // precise float3 brdf = CalculateSpecular(normal, viewDirection, L, H, specParam); // (also contains NdotL term)
        // precise float D = TrowReitzD(NdotH, specParam.roughness * specParam.roughness);
        // float pdfWeight = (4.f * VdotH) / (D * NdotH);
        // result += lightColor * brdf * pdfWeight;

        float NdotV = 1.f; // saturate(dot(normal, viewDirection));
        float NdotL = saturate(dot(normal, L));
        precise float G = SmithG(NdotL, alphag); // * SmithG(NdotV, alphag);
        float scale = G * VdotH / (NdotH * NdotV);

            // We're getting nan values here sometimes...?
            // This occurs in the high roughness values when we have a very large
            // number of samples. This might be a result of the bit hacks we're
            // doing in the hammersly calculation?

            // note --  seems like the weighting here could be simplified down to just
            //          NdotL?
        #if 0
            if (!isnan(scale))
                result += lightColor * scale;
        #endif

        result += lightColor * NdotL;
        totalWeight += NdotL;
    }

    // Might be more accurate to divide by "PassSampleCount" here, and then later on divide
    // by PassCount...?
    // return float4(result / float(totalSampleCount), 1.f);
    return float4(result / totalWeight / PassCount, 1.f);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

float3 CalculateFilteredTextureTrans(float3 cubeMapDirection, float roughness)
{
    float3 normal = cubeMapDirection;
    float3 viewDirection = cubeMapDirection;

    const float3 ot = viewDirection;
    const float iorIncident = 1.f;
    const float iorOutgoing = SpecularTransmissionIndexOfRefraction;

    float alphag = RoughnessToGAlpha(roughness);
    float alphad = RoughnessToDAlpha(roughness);

    float totalWeight = 0.f;
    float3 result = 0.0.xxx;
    const uint totalSampleCount = PassSampleCount * PassCount;
    [loop] for (uint s=0; s<PassSampleCount; ++s) {
        precise float3 H = BuildSampleHalfVectorGGX(
            s+PassIndex*PassSampleCount, totalSampleCount,
            -normal, alphad);

        float3 i;
        if (!CalculateTransmissionIncident(i, ot, H, iorIncident, iorOutgoing))
            continue;

        float3 lightColor = SampleInputTexture(i);
        float weight = saturate(dot(-normal, i));  // best weighting here is not clear
        result += lightColor * weight;
        totalWeight += weight;
#if 0
        // As per the reflection case, our probability distribution function is
        //      D * NdotH / (4 * VdotH)
        // However, it doesn't factor out like it does in the reflection case.
        // So, we have to do the full calculation, and then apply the inverse of
        // the pdf afterwards.

        float NdotH = saturate(dot(normal, -H));
        float VdotH = saturate(dot(viewDirection, -H));

        precise float D = TrowReitzD(NdotH, alphad);
        float pdfWeight = (4.f * VdotH) / (D * NdotH);

        float transmitted;
        GGXTransmission(        // todo -- fresnel calculation is going to get in the way
            roughness, 0.f, iorIncident, iorOutgoing,
            i, ot, -normal,
            transmitted);

        float scale = transmitted * pdfWeight;
        if (!isnan(scale))
            result += lightColor * scale;
#endif
    }

    // Might be more accurate to divide by "PassSampleCount" here, and then later on divide
    // by PassCount...?
    return result / totalWeight / PassCount;
}

float4 EquiRectFilterGlossySpecularTrans(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
    // Following the simplifications we use for split-sum specular reflections, here
    // is the equivalent sampling for specular transmission
    float3 cubeMapDirection = CalculateCubeMapDirection(ArrayIndex, texCoord);

        // undoing AdjSkyCubeMapCoords
    cubeMapDirection = float3(cubeMapDirection.x, -cubeMapDirection.z, cubeMapDirection.y);
    cubeMapDirection = float3(-cubeMapDirection.x, -cubeMapDirection.y, cubeMapDirection.z);

    float roughness = MipmapToRoughness(MipIndex);
    return float4(CalculateFilteredTextureTrans(cubeMapDirection, roughness), 1.f));
}
