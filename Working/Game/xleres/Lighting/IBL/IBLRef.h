// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(IBL_REF_H)
#define IBL_REF_H

#include "IBLAlgorithm.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
    //  Reference specular reflections
///////////////////////////////////////////////////////////////////////////////////////////////////

float3 SampleSpecularIBL_Ref(
    float3 normal, float3 viewDirection,
    SpecularParameters specParam, TextureCube tex,
    uint passSampleCount, uint passIndex, uint passCount)
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

    float alphad = RoughnessToDAlpha(specParam.roughness);
    float3 result = 0.0.xxx;
    for (uint s=0; s<passSampleCount; ++s) {
            // We could build a distribution of "H" vectors here,
            // or "L" vectors. It makes sense to use H vectors
        precise float3 H = SampleMicrofacetNormalGGX(s*passCount+passIndex, passSampleCount*passCount, normal, alphad);
        float3 L = 2.f * dot(viewDirection, H) * H - viewDirection;

            // Now we can light as if the point on the reflection map
            // is a directonal light

            // note -- "CalculateSpecular" has NdotL term built-in
        float3 lightColor = tex.SampleLevel(DefaultSampler, AdjSkyCubeMapCoords(L), 0).rgb;
        precise float3 brdf = CalculateSpecular(normal, viewDirection, L, H, specParam); // (also contains NdotL term)
        float pdfWeight = InversePDFWeight(H, normal, viewDirection, alphad);
        result += lightColor * brdf * pdfWeight;
    }

    return result / float(passSampleCount);
}

float3 SampleTransmittedSpecularIBL_Ref(
    float3 normal, float3 viewDirection,
    SpecularParameters specParam, TextureCube tex,
    uint passSampleCount, uint passIndex, uint passCount)
{
    float iorIncident = 1.f;
    float iorOutgoing = SpecularTransmissionIndexOfRefraction;
    float3 ot = viewDirection;

        // This is a reference implementation of transmitted IBL specular.
        // We're going to follow the same method and microfacet distribution as
        // SampleSpecularIBL_Ref
    float alphad = RoughnessToDAlpha(specParam.roughness);
    float3 result = 0.0.xxx;
    for (uint s=0; s<passSampleCount; ++s) {
        // using the same distribution of half-vectors that we use for reflection
        // (except we flip the normal because of the way the equation is built)
        precise float3 H = SampleMicrofacetNormalGGX(s*passCount+passIndex, passSampleCount*passCount, -normal, alphad);

        // following Walter07 here, we need to build "i", the incoming direction.
        // Actually Walter builds the outgoing direction -- but we need to reverse the
        // equation and get the incoming direction.
        float3 i;
        if (!CalculateTransmissionIncident(i, ot, H, iorIncident, iorOutgoing))
            continue;

        // ok, we've got our incoming vector. We can do the cube map lookup
        // Note that when we call "CalculateSpecular", it's going to recalculate
        // the transmission half vector and come out with the same result.
        float3 lightColor = tex.SampleLevel(DefaultSampler, AdjSkyCubeMapCoords(i), 0).rgb;
        // precise float3 brdf = CalculateSpecular(normal, viewDirection, i, H, specParam); // (also contains NdotL term)

        float bsdf;
        //GGXTransmission(
        //    iorIncident, iorOutgoing, specParam.roughness,
        //    i, viewDirection, -normal,        // (note flipping normal)
        //    bsdf);

        bsdf = 1.f;
        bsdf *= SmithG(abs(dot( i,  normal)), RoughnessToGAlpha(specParam.roughness));
        bsdf *= SmithG(abs(dot(ot,  normal)), RoughnessToGAlpha(specParam.roughness));
        bsdf *= TrowReitzD(abs(dot( H, normal)), alphad);

        // One term from the Walter07 paper is causing problems... This is the
        // only brightness term that uses the index of refraction. So it may be intended
        // to to take into account focusing of light. Sure enough, without this term the
        // refraction tends to match the brightness of the incoming light closely.
        //
        // However, with it, it produces numbers that are quite high (> 100.f) and it's
        // making our solution just too bright... For large iorOutgoing values, it seems fine
        //  -- but it's just causing problems when iorOutgoing approaches 1.f)
        // It's not clear at the moment what the best solution is for dealing with this term.
        // I want to know what the derivation is, and why it gets so bright as iorOutgoing
        // approaches zero.
        //
        // My feeling is that this term is intended to actually be the term below (with a sign
        // reversed). This new equation equals "4" when iorIncident == iorOutgoing, and I think
        // that 4 might have been factored out with the "4" in the denominator in the microfacet
        // equation.
        #if 0
            bsdf *= Sq(iorOutgoing) / Sq(iorIncident * dot(i, H) + iorOutgoing * dot(ot, H));
        #else
            bsdf *= Sq(iorOutgoing) / Sq(iorIncident * dot(i, H) - iorOutgoing * dot(ot, H));
        #endif

        bsdf *= abs(dot(i, H)) * abs(dot(ot, H));
        bsdf /= abs(dot(i, normal)) * abs(dot(ot, normal));

        bsdf *= GGXTransmissionFresnel(
            i, viewDirection, specParam.F0.g,
            iorIncident, iorOutgoing);

        bsdf *= -dot(i, normal);

        // We have to apply the distribution weight. Since our half vectors are distributed
        // in the same fashion as the reflection case, we should have the same weight.
        // (But we need to consider the flipping that occurs)
        float pdfWeight = InversePDFWeight(H, -normal, viewDirection, alphad);

        result += lightColor * bsdf * pdfWeight;
    }

    return result / float(passSampleCount);
}

#endif
