// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(IBL_PRECALC_H)
#define IBL_PRECALC_H

#include "IBLAlgorithm.h"
#include "../Lighting/LightingAlgorithm.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
    //  Split-term LUT
///////////////////////////////////////////////////////////////////////////////////////////////////

float2 GenerateSplitTerm(float NdotV, float roughness, uint sampleCount)
{
    // This generates the lookup table used by the glossy specular reflections
    // split sum approximation.
    // Based on the method presented by Unreal course notes:
    //  http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
    // While referencing the implementation in "IBL Baker":
    //  https://github.com/derkreature/IBLBaker/
    // We should maintain our own version, because we need to take into account the
    // same remapping on "roughness" that we do for run-time specular.
    //
    // For the most part, we will just be integrating the specular equation over the
    // hemisphere with white incoming light. We use importance sampling with a fixed
    // number of samples to try to make the work load manageable...

    float3 normal = float3(0.0f, 0.0f, 1.0f);
    float3 V = float3(sqrt(1.0f - NdotV * NdotV), 0.0f, NdotV);

        // Karis suggests that using our typical Disney remapping
        // for the alpha value for the G term will make IBL much too dark.
        // Indeed, it changes the output texture a lot.
        // However, it seems like a good improvement to me. It does make
        // low roughness materials slightly darker. However, it seems to
        // have the biggest effect on high roughness materials. These materials
        // get much lower reflections around the edges. This seems to be
        // good, however... Because without it, low roughness get a clear
        // halo around their edges. This doesn't happen so much with the
        // runtime specular. So it actually seems better with the this remapping.
    float alphag = RoughnessToGAlpha(roughness);
    float alphad = RoughnessToDAlpha(roughness);
    float G2 = SmithG(NdotV, alphag);

    float A = 0.f, B = 0.f;
    [loop] for (uint s=0; s<sampleCount; ++s) {
        float3 H, L;

            // Our sampling variable is the microfacet normal (which is the halfVector)
            // We will calculate a set of half vectors that are distributed in such a
            // way to reduce the sampling artifacts in the final image.
            //
            // We could consider clustering the samples around the highlight.
            // However, this changes the probability density function in confusing
            // ways. The result is similiar -- but it is as if a shadow has moved
            // across the output image.
        const bool clusterAroundHighlight = false;
        if (clusterAroundHighlight) {
            L = BuildSampleHalfVectorGGX(s, sampleCount, reflect(-V, normal), alphad);
            H = normalize(L + V);
        } else {
            H = BuildSampleHalfVectorGGX(s, sampleCount, normal, alphad);
            L = 2.f * dot(V, H) * H - V;
        }

        float NdotL = L.z;
        float NdotH = saturate(H.z);
        float VdotH = saturate(dot(V, H));

        if (NdotL > 0) {
                // using "precise" here has a massive effect...
                // Without it, there are clear errors in the result.
            precise float G = SmithG(NdotL, alphag) * G2;

            // F0 gets factored out of the equation here
            // the result we will generate is actually a scale and offset to
            // the runtime F0 value.
            precise float F = pow(1.f - VdotH, 5.f);

            // As per SampleSpecularIBL_Ref, we need to apply the inverse of the
            // propability density function to get a normalized result.
            // This factors out the D term, and introduces some other terms.
            //      pdf inverse = 4.f * VdotH / (D * NdotH)
            //      specular eq = D*G*F / (4*NdotL*NdotV)
            precise float normalizedSpecular = G * VdotH / (NdotH * NdotV);  // (excluding F term)

            A += (1.f - F) * normalizedSpecular;
            B += F * normalizedSpecular;
        }
    }

    return float2(A, B) / float(sampleCount).xx;
}

float2 GenerateSplitTermTrans(float NdotV, float roughness, uint sampleCount)
{
    // This is for generating the split-term lookup for specular transmission
    // It uses the same approximations and simplifications as for reflected
    // specular.
    // Note that we're actually assuming a constant iorOutgoing for all materials
    // This might be ok, because the effect of this term also depends on the geometry
    // arrangement (ie, how far away refracted things are). So maybe it's not a
    // critical parameter for materials (even though it should in theory be related
    // to the "specular" parameter)

    float3 normal = float3(0.0f, 0.0f, 1.0f);
    float3 V = float3(sqrt(1.0f - NdotV * NdotV), 0.0f, NdotV);

    const float iorIncident = 1.f;
    const float iorOutgoing = SpecularTransmissionIndexOfRefraction;
    float3 ot = V;

    float alphag = RoughnessToGAlpha(roughness);
    float alphad = RoughnessToDAlpha(roughness);
    float G2 = SmithG(NdotV, alphag);

    float A = 0.f, B = 0.f;
    [loop] for (uint s=0; s<sampleCount; ++s) {

        precise float3 H = BuildSampleHalfVectorGGX(
            s, sampleCount, -normal, alphad);

        float3 i;
        if (!CalculateTransmissionIncident(i, ot, H, iorIncident, iorOutgoing))
            continue;

        // As per the reflection case, our probability distribution function is
        //      D * NdotH / (4 * VdotH)
        // However, it doesn't factor out like it does in the reflection case.
        // So, we have to do the full calculation, and then apply the inverse of
        // the pdf afterwards.

        float transmitted;
        GGXTransmission(        // todo -- fresnel calculation is going to get in the way
            roughness, iorIncident, iorOutgoing,
            i, ot, -normal,
            transmitted);

        float pdfWeight = SamplingPDFWeight(H, -normal, V, alphad);

        float VdotH = abs(dot(V, H));
        precise float F = 1.f - SchlickFresnelCore(VdotH);

        float normalizedSpecular = transmitted * pdfWeight;
        A += F * normalizedSpecular;
    }

    return float2(A, B) / float(sampleCount).xx;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
    //  Filtered textures
///////////////////////////////////////////////////////////////////////////////////////////////////

float3 GenerateFilteredSpecular(float3 cubeMapDirection, float roughness, uint sampleCount, uint sampleOffset, uint totalSampleCount)
{
    // Here is the key simplification -- we assume that the normal and view direction are
    // the same. This means that some distortion on extreme angles is lost. This might be
    // incorrect, but it will probably only be noticeable when compared to a ray traced result.
    float3 normal = cubeMapDirection;
    float3 viewDirection = cubeMapDirection;

    SpecularParameters specParam = SpecularParameters_RoughF0(roughness, 1.0.xxx);

    float alphag = RoughnessToGAlpha(specParam.roughness);
    float alphad = RoughnessToDAlpha(specParam.roughness);

    float3 result = 0.0.xxx;
    float totalWeight = 0.f;

        // We need a huge number of samples for a smooth result
        // Perhaps we should use double precision math?
        // Anyway, we need to split it up into multiple passes, otherwise
        // the GPU gets locked up in the single draw call for too long.
    [loop] for (uint s=0; s<sampleCount; ++s) {
            // We could build a distribution of "H" vectors here,
            // or "L" vectors. It makes sense to use H vectors
        precise float3 H = BuildSampleHalfVectorGGX(s+sampleOffset, totalSampleCount, normal, alphad);
        precise float3 L = 2.f * dot(viewDirection, H) * H - viewDirection;

            // Now we can light as if the point on the reflection map
            // is a directonal light.
            // Note that we could estimate the average distance between
            // samples in this step, and compare that to the input texture
            // size. In this way, we could select a mipmap from the input
            // texture. This would help avoid sampling artifacts, and we would
            // end up with slightly more blurry result.

        float3 lightColor = IBLPrecalc_SampleInputTexture(L);

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
    return result / totalWeight;
}

float3 CalculateFilteredTextureTrans(float3 cubeMapDirection, float roughness, uint sampleCount, uint sampleOffset, uint totalSampleCount)
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
    [loop] for (uint s=0; s<sampleCount; ++s) {
        precise float3 H = BuildSampleHalfVectorGGX(
            s+sampleOffset, totalSampleCount,
            -normal, alphad);

        float3 i;
        if (!CalculateTransmissionIncident(i, ot, H, iorIncident, iorOutgoing))
            continue;

        float3 lightColor = IBLPrecalc_SampleInputTexture(i);
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

    return result / totalWeight;
}


#endif
