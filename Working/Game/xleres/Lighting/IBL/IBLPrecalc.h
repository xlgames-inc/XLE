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

float2 GenerateSplitTerm(
    float NdotV, float roughness,
    uint passSampleCount, uint passIndex, uint passCount)
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
    [loop] for (uint s=0; s<passSampleCount; ++s) {
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
            L = BuildSampleHalfVectorGGX(s*passCount+passIndex, passSampleCount*passCount, reflect(-V, normal), alphad);
            H = normalize(L + V);
        } else {
            H = BuildSampleHalfVectorGGX(s*passCount+passIndex, passSampleCount*passCount, normal, alphad);
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
            #if !defined(OLD_M_DISTRIBUTION_FN)
                precise float normalizedSpecular = G / (4.f * NdotL * NdotV * NdotH);  // (excluding F term)
            #else
                // This factors out the D term, and introduces some other terms.
                //      pdf inverse = 4.f * VdotH / (D * NdotH)
                //      specular eq = D*G*F / (4*NdotL*NdotV)
                precise float normalizedSpecular = G * VdotH / (NdotH * NdotV);  // (excluding F term)
            #endif

            A += (1.f - F) * normalizedSpecular;
            B += F * normalizedSpecular;
        }
    }

    return float2(A, B) / float(passSampleCount).xx;
}

float GenerateSplitTermTrans(
    float NdotV, float roughness,
    uint passSampleCount, uint passIndex, uint passCount)
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

    float A = 0.f;
    [loop] for (uint s=0; s<passSampleCount; ++s) {

        precise float3 H = BuildSampleHalfVectorGGX(
            s*passCount+passIndex, passSampleCount*passCount, -normal, alphad);

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

        float pdfWeight = InversePDFWeight(H, -normal, V, alphad);

        float VdotH = abs(dot(V, H));
        precise float F = 1.f - SchlickFresnelCore(VdotH);

        float normalizedSpecular = transmitted * pdfWeight;
        A += F * normalizedSpecular;
    }

    return A / float(passSampleCount);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
    //  Filtered textures
///////////////////////////////////////////////////////////////////////////////////////////////////

float3 GenerateFilteredSpecular(
    float3 cubeMapDirection, float roughness,
    uint passSampleCount, uint passIndex, uint passCount)
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
    [loop] for (uint s=0; s<passSampleCount; ++s) {
            // We could build a distribution of "H" vectors here,
            // or "L" vectors. It makes sense to use H vectors
        precise float3 H = BuildSampleHalfVectorGGX(s*passCount+passIndex, passSampleCount*passCount, normal, alphad);
        precise float3 L = 2.f * dot(viewDirection, H) * H - viewDirection;

            // Now we can light as if the point on the reflection map
            // is a directonal light.
            // Note that we could estimate the average distance between
            // samples in this step, and compare that to the input texture
            // size. In this way, we could select a mipmap from the input
            // texture. This would help avoid sampling artifacts, and we would
            // end up with slightly more blurry result.

        float3 lightColor = IBLPrecalc_SampleInputTexture(L);

        const uint Method_Unreal = 0;
        const uint Method_Flat = 1;
        const uint Method_PDF = 2;
        const uint Method_Constant = 3;
        const uint Method_Balanced = 4;
        const uint Method_NdotH = 5;
        const uint method = Method_NdotH;
        float weight;
        if (method == Method_Unreal) {
            // This is the method from the unreal course notes
            // It seems to distort the shape of the GGX equation
            // slightly.
            weight = saturate(dot(normal, L));
        } else if (method == Method_Flat) {
            lightColor *= InversePDFWeight(H, normal, viewDirection, alphad);
            weight = 1.f;
        } else if (method == Method_PDF) {
            // This method weights the each point by the GGX sampling pdf associated
            // with this direction. This may be a little odd, because it's in effect
            // just squaring the effect of "constant" weighting. However it does
            // look like, and sort of highlights the GGX feel. It also de-emphasises
            // the lower probability directions.
            weight = 1.f/InversePDFWeight(H, normal, viewDirection, alphad);
        } else if (method == Method_Balanced) {
            // In this method, we want to de-emphasise the lowest probability samples.
            // the "pdf" represents the probability of getting a certain direction
            // in our sampling distribution.
            // Any probabilities higher than 33%, we're going to leave constant
            // -- because that should give us a shape that is most true to GGX
            // For lower probability samples, we'll quiet them down a bit...
            // Note "lower probability samples" really just means H directions
            // that are far from "normal" (ie, "theta" in BuildSampleHalfVectorGGX is high)
            float pdf = 1.f/InversePDFWeight(H, normal, viewDirection, alphad);
            weight = saturate(pow(pdf * 3.f, 3.f));
        } else if (method == Method_Constant) {
            // Constant should just give us an even distribution, true to the
            // probably distribution in BuildSampleHalfVectorGGX
            weight = 1.f;
        } else if (method == Method_NdotH) {
            // seems simple and logical, and produces a good result
            weight = abs(dot(normal, H));
        }
        result += lightColor * weight;
        totalWeight += weight;
    }

    // Might be more accurate to divide by "PassSampleCount" here, and then later on divide
    // by PassCount...?
    return result / totalWeight;
}

float3 CalculateFilteredTextureTrans(
    float3 cubeMapDirection, float roughness,
    uint passSampleCount, uint passIndex, uint passCount)
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
    [loop] for (uint s=0; s<passSampleCount; ++s) {
        precise float3 H = BuildSampleHalfVectorGGX(
            s*passCount+passIndex, passSampleCount*passCount,
            -normal, alphad);

        float3 i;
        if (!CalculateTransmissionIncident(i, ot, H, iorIncident, iorOutgoing))
            continue;

        float3 lightColor = IBLPrecalc_SampleInputTexture(i);

            // See GenerateFilteredSpecular for more options
            // for filtering... This method just helps to take
            // to take the edge off less accurate samples.
        float weight = abs(dot(normal, H));
        result += lightColor * weight;
        totalWeight += weight;
    }

    return result / totalWeight;
}


#endif
