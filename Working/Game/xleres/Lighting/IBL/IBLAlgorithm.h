// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(IBL_ALGORITHM_H)
#define IBL_ALGORITHM_H

#include "../LightingAlgorithm.h"

static const float MinSamplingAlpha = 0.001f;
static const float SpecularIBLMipMapCount = 9.f;

float VanderCorputRadicalInverse(uint bits)
{
    // This is the "Van der Corput radical inverse" function
    // see source:
    //      http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
    // Note -- Can't we just use the HLSL intrinsic "reversebits"? Is there a benefit?
    return float(reversebits(bits)) * 2.3283064365386963e-10f;
    // bits = (bits << 16u) | (bits >> 16u);
    // bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    // bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    // bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    // bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    // return float(bits) * 2.3283064365386963e-10f; // / 0x100000000
 }

float2 HammersleyPt(uint i, uint N)
{
    return float2(float(i)/float(N), VanderCorputRadicalInverse(i));
}

float3 BuildSampleHalfVectorGGX(uint i, uint sampleCount, float3 normal, float alphad)
{
        // Very similar to the unreal course notes implementation here
    float2 xi = HammersleyPt(i, sampleCount);

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
    // Note that I've swapped xi.x & xi.y from the Unreal implementation. Maybe
    // not a massive change.
    // float cosTheta = sqrt((1.f - xi.x) / (1.f + (alphad*alphad - 1.f) * xi.x));

    float q = TrowReitzDInverse(lerp(.31f, 1.f, xi.x), max(MinSamplingAlpha, alphad));
    float cosTheta = q;
    float sinTheta = sqrt(1.f - cosTheta * cosTheta);
    float phi = 2.f * pi * xi.y;

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

float SamplingPDFWeight(float3 H, float3 N, float3 V, float alphad)
{
    float NdotH = saturate(dot(H, N));
    precise float D = TrowReitzD(NdotH, max(MinSamplingAlpha, alphad));
    return (1.f - .31f) / D;

    // float VdotH = abs(dot(V, H));
    // return (4.f * VdotH) / (D * NdotH);
}

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
    return 0.05f + saturate(mipIndex / float(SpecularIBLMipMapCount));
}

float RoughnessToMipmap(float roughness)
{
    return saturate((roughness - 0.05f) * SpecularIBLMipMapCount);
}


#endif
