// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(IMAGE_BASED_H)
#define IMAGE_BASED_H

#if !defined(SPECULAR_METHOD)
    #define SPECULAR_METHOD 1
#endif

#include "SpecularMethods.h"
#include "../CommonResources.h"

TextureCube DiffuseIBL : register(t19);

float3 SampleDiffuseIBL(float3 worldSpaceNormal)
{
    #if HAS_DIFFUSE_IBL==1
        return DiffuseIBL.SampleLevel(DefaultSampler, worldSpaceNormal, 0).rgb;
    #else
        return 0.0.xxx;
    #endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////
    //  Reference specular reflections
///////////////////////////////////////////////////////////////////////////////////////////////////

float VanderCorputRadicalInverse(uint bits)
{
    // This is the "Van der Corput radical inverse" function
    // see source:
    //      http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
 }

float2 HammersleyPt(uint i, uint N)
{
    return float2(float(i)/float(N), VanderCorputRadicalInverse(i));
}

float3 BuildSampleHalfVectorGGX(uint i, uint sampleCount, float3 normal, float roughness)
{
        // Very similar to the unreal course notes implementation here
    float2 xi = HammersleyPt(i, sampleCount);

    float a = roughness * roughness;

    // The following will attempt to select points that are
    // well distributed for the GGX highlight
    // See a version of this equation in
    //  http://igorsklyar.com/system/documents/papers/28/Schlick94.pdf
    //  under the Monte Carlo Techniques heading
    // Note that we may consider remapping 'a' here to take into account
    // the remapping we do for the G and D terms in our GGX implementation...?
    //
    // Also note because we're not taking a even distribution of points, we
    // have to be careful to make sure the final equation is properly normalized
    // (in other words, if we had a full dome of incoming white light,
    // we don't want changes to the roughness to change the result).
    // See http://http.developer.nvidia.com/GPUGems3/gpugems3_ch20.html
    // for more information about normalized "PDFs" in this context
    //
    float cosTheta = sqrt((1.f - xi.y) / (1.f + (a*a - 1.f) * xi.y));
    float sinTheta = sqrt(1.f - cosTheta * cosTheta);
    float phi = 2.f * pi * xi.x;

    float3 H;
    H.x = sinTheta * cos(phi);
    H.y = sinTheta * sin(phi);
    H.z = cosTheta;

    // we're just building a tangent frame to give meaning to theta and phi...
    float3 up = abs(normal.z) < 0.999f ? float3(0,0,1) : float3(1,0,0);
    float3 tangentX = normalize(cross(up, normal));
    float3 tangentY = cross(normal, tangentX);
    return tangentX * H.x + tangentY * H.y + normal * H.z;
}

float3 SampleSpecularIBL(float3 normal, float3 viewDirection, SpecularParameters specParam, TextureCube tex)
{
    // This is a reference implementation of glossy specular reflections from a
    // cube map texture. It's too inefficient to run at real-time. But it provides
    // a visual reference.
    // In the typical final-result, we must use precalculated lookup tables to reduce
    // the number of samples down to 1.
    // We will sample a fixed number of directions across the peak area of the GGX
    // specular highlight. We return the mean of those sample directions.
    // As standard, we use a 2D Hammersley distribution to generate sample directions.
    //
    // For reference, see
    // GPU Gems article
    //      http://http.developer.nvidia.com/GPUGems3/gpugems3_ch20.html
    // Unreal course notes
    //      http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf

    float3 result = 0.0.xxx;
    const uint sampleCount = 64;
    for (uint s=0; s<sampleCount; ++s) {
            // We could build a distribution of "H" vectors here,
            // or "L" vectors. It makes sense to use H vectors
        float3 H = BuildSampleHalfVectorGGX(s, sampleCount, normal, specParam.roughness);
        float3 L = 2.f * dot(viewDirection, H) * H - viewDirection;

            // Now we can light as if the point on the reflection map
            // is a directonal light
        float3 lightColor = tex.SampleLevel(DefaultSampler, L, 0).rgb;
        float3 brdf = CalculateSpecular(normal, viewDirection, L, H, specParam); // (also contains NdotL term)

            // Unreal course notes say the probability distribution function is
            //      D * NdotH / (4 * VdotH)
            // We need to apply the inverse of this to weight the sample correctly.
            // A better solution is to factor the terms out of the microfacet specular
            // equation. But since this is for a reference implementation, let's do
            // it the long way.
        float NdotH = saturate(dot(normal, H));
        float VdotH = saturate(dot(viewDirection, H));
        float D = TrowReitzD(NdotH, specParam.roughness * specParam.roughness);
        float pdfWeight = (4.f * VdotH) / (D * NdotH);

        result += lightColor * brdf * pdfWeight;
    }

    return result / float(sampleCount);
}

// note --
//      see the project "https://github.com/derkreature/IBLBaker"
//      for an interesting tool to filter the input textures

#endif
