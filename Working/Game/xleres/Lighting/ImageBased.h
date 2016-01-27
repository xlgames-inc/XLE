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

TextureCube DiffuseIBL  : register(t19);
TextureCube SpecularIBL : register(t20);
Texture2D<float2> GlossLUT : register(t21);        // this is the look up table used in the split-sum IBL glossy reflections
Texture2D<float2> GlossTransLUT : register(t22);

TextureCube SpecularTransIBL : register(t30);

float3 SampleDiffuseIBL(float3 worldSpaceNormal)
{
        // note -- in the same way that we apply the specular BRDF when for
        //      the gloss reflections, we could also apply the diffuse BRDF
        //      to the diffuse reflections (including the view dependent part)
        //      Currently we'll just get lambert response...
    #if HAS_DIFFUSE_IBL==1
        float3 result = DiffuseIBL.SampleLevel(DefaultSampler, AdjSkyCubeMapCoords(worldSpaceNormal), 0).rgb;
            //  When using textures made with IBL Baker, we must multiply in the
            //  normalization factor here... But with textures build from ModifiedCubeMapGen, it is
            //  already incorporated
            // const float normalizationFactor = 1.0f / pi;
            // result *= normalizationFactor;
        return result;
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

static const float MinSamplingAlpha = 0.001f;

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

float3 SampleSpecularIBL_Ref(float3 normal, float3 viewDirection, SpecularParameters specParam, TextureCube tex)
{
        // hack -- currently problems when roughness is 0!
    if (specParam.roughness == 0.f) return 0.0.xxx;

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
    const uint sampleCount = 512;
    for (uint s=0; s<sampleCount; ++s) {
            // We could build a distribution of "H" vectors here,
            // or "L" vectors. It makes sense to use H vectors
        precise float3 H = BuildSampleHalfVectorGGX(s, sampleCount, normal, alphad);
        float3 L = 2.f * dot(viewDirection, H) * H - viewDirection;

            // Now we can light as if the point on the reflection map
            // is a directonal light

        float3 lightColor = tex.SampleLevel(DefaultSampler, AdjSkyCubeMapCoords(L), 0).rgb;
        precise float3 brdf = CalculateSpecular(normal, viewDirection, L, H, specParam); // (also contains NdotL term)

            // Unreal course notes say the probability distribution function is
            //      D * NdotH / (4 * VdotH)
            // We need to apply the inverse of this to weight the sample correctly.
            // A better solution is to factor the terms out of the microfacet specular
            // equation. But since this is for a reference implementation, let's do
            // it the long way.
        float NdotH = saturate(dot(normal, H));
        float VdotH = saturate(dot(viewDirection, H));
        precise float D = TrowReitzD(NdotH, alphad);
        float pdfWeight = (4.f * VdotH) / (D * NdotH);

        result += lightColor * brdf * pdfWeight;
    }

    return result / float(sampleCount);
}

float3 SplitSumIBL_PrefilterEnvMap(float roughness, float3 reflection)
{
        // Roughness is mapped on to a linear progression through
        // the mipmaps. Because of the way SampleLevel works, we
        // need to know how many mipmaps there are in the texture!
        // For a cubemap with 512x512 faces, there should be 9 mipmaps.
        // Ideally this value would not be hard-coded...!
        // We should also use a trilinear sampler, so that we get
        // interpolation through the mipmap levels.
        // Also note that the tool that filters the texture must
        // use the same mapping for roughness that we do -- otherwise
        // we need to apply the mapping there.
    const float specularIBLMipMapCount = 9.f + 1.f;     // (actually getting a much closer result with the +1.f here)
    return SpecularIBL.SampleLevel(
        DefaultSampler, AdjSkyCubeMapCoords(reflection),
        roughness*specularIBLMipMapCount).rgb;
}

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

float2 SplitSumIBL_IntegrateBRDF(float roughness, float NdotV)
{
    // Note that we can also use an analytical approximation for
    // this term. See:
    // https://knarkowicz.wordpress.com/2014/12/27/analytical-dfg-term-for-ibl/
    // That would avoid using a lookup table, and replace it with
    // just a few shader instructions.
    // This lookup table should have at least 16 bit of precision, and
    // the values are all in the range [0, 1] (so we can use UNORM format)
    // note -- it may be ok to use PointClampSampler here...? is it better to
    //          use a small texture and bilinear filtering, or a large texture
    //          and no filtering?
    return GlossLUT.SampleLevel(ClampingSampler, float2(NdotV, 1.f - roughness), 0).xy;
    // const uint sampleCount = 64;
    // return GenerateSplitTerm(saturate(NdotV), saturate(roughness), sampleCount);
}

float3 SampleSpecularIBL_SplitSum(float3 normal, float3 viewDirection, SpecularParameters specParam)
{
    // This is the split-sum approximation for glossy specular reflections from
    // Brian Karis for Unreal.
    //
    // This is a useful method for us, because we can use some existing tools for
    // generating the input textures for this.
    // For example, see IBL Baker:
    //      https://github.com/derkreature/IBLBaker
    //
    // See http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
    // for more details.
    //
    // Note that when we prefilter the environment map, we assume that N = V = R.
    // This removes the distortion to the GGX specular shape that should occur at grazing
    // angles. But Karis suggests that the error is minor. The advantage is the blurring
    // we apply to the reflection cubemap now only depends on roughness.

    float NdotV = saturate(dot(normal, viewDirection));
    float3 R = 2.f * dot(viewDirection, normal) * normal - viewDirection;   // reflection vector
    float3 prefilteredColor = SplitSumIBL_PrefilterEnvMap(specParam.roughness, R);
    float2 envBRDF = SplitSumIBL_IntegrateBRDF(specParam.roughness, NdotV);
    return prefilteredColor * (specParam.F0 * envBRDF.x + envBRDF.y);
}

float3 SampleSpecularIBL(float3 normal, float3 viewDirection, SpecularParameters specParam)
{
    return SampleSpecularIBL_SplitSum(normal, viewDirection, specParam);
}

/////////////////// T R A N S M I T T E D   S P E C U L A R ///////////////////

float3 SampleTransmittedSpecularIBL_Ref(
    float3 normal, float3 viewDirection,
    SpecularParameters specParam, TextureCube tex)
{
        // hack -- currently problems when roughness is 0!
    if (specParam.roughness == 0.f) return 0.0.xxx;

    float iorIncident = 1.f;
    float iorOutgoing = SpecularTransmissionIndexOfRefraction;
    float3 ot = viewDirection;

        // This is a reference implementation of transmitted IBL specular.
        // We're going to follow the same method and microfacet distribution as
        // SampleSpecularIBL_Ref
    float alphad = RoughnessToDAlpha(specParam.roughness);
    float3 result = 0.0.xxx;
    const uint sampleCount = 128;
    for (uint s=0; s<sampleCount; ++s) {
        // using the same distribution of half-vectors that we use for reflection
        // (except we flip the normal because of the way the equation is built)
        precise float3 H = BuildSampleHalfVectorGGX(s, sampleCount, -normal, alphad);

        // following Walter07 here, we need to build "i", the incoming direction.
        // Actually Walter builds the outgoing direction -- but we need to reverse the
        // equation and get the incoming direction.
        float3 i;
        if (!CalculateTransmissionIncident(i, ot, H, iorIncident, iorOutgoing))
            continue;

        #if 0
            if (isinf(i.x) || isnan(i.x)) return float3(1,0,0);
            if (abs(length(i) - 1.f) > 0.01f) return float3(1,0,0);
            float3 H2 = CalculateHt(i, ot, iorIncident, iorOutgoing);
            if (length(H2 - H) > 0.01f) return float3(1,0,0);
        #endif

        // ok, we've got our incoming vector. We can do the cube map lookup
        // Note that when we call "CalculateSpecular", it's going to recalculate
        // the transmission half vector and come out with the same result.
        float3 lightColor = tex.SampleLevel(DefaultSampler, AdjSkyCubeMapCoords(i), 0).rgb;
        precise float3 brdf = CalculateSpecular(normal, viewDirection, i, H, specParam); // (also contains NdotL term)

        // We have to apply the distribution weight. Since our half vectors are distributed
        // in the same fashion as the reflection case, we should have the same weight.
        // (But we need to consider the flipping that occurs)
        // float NdotH = saturate(dot(normal, -H));
        // float VdotH = saturate(dot(viewDirection, -H));
        // precise float D = TrowReitzD(NdotH, alphad);
        // float pdfWeight = (4.f * VdotH) / (D * NdotH);
        float pdfWeight = SamplingPDFWeight(H, -normal, viewDirection, alphad);

        result += lightColor * brdf * pdfWeight;
    }

    return result / float(sampleCount);
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

float2 SplitSumIBLTrans_IntegrateBRDF(float roughness, float NdotV)
{
    const uint sampleCount = 64;
    return GenerateSplitTermTrans(NdotV, roughness, sampleCount);
    // return GlossTransLUT.SampleLevel(ClampingSampler, float2(NdotV, 1.f - roughness), 0).xy;
}

float3 SplitSumIBLTrans_PrefilterEnvMap(float roughness, float3 ot)
{
    const float specularIBLMipMapCount = 9.f + 1.f;     // (actually getting a much closer result with the +1.f here)
    return SpecularTransIBL.SampleLevel(
        DefaultSampler, AdjSkyCubeMapCoords(ot),
        roughness*specularIBLMipMapCount).rgb;
}

float3 SampleSpecularIBLTrans_SplitSum(float3 normal, float3 viewDirection, SpecularParameters specParam)
{
    float NdotV = saturate(dot(normal, viewDirection));
    float3 prefilteredColor = SplitSumIBLTrans_PrefilterEnvMap(specParam.roughness, viewDirection);
    float2 envBRDF = SplitSumIBLTrans_IntegrateBRDF(specParam.roughness, NdotV);
    return specParam.transmission * prefilteredColor * (1.f - specParam.F0) * envBRDF.x;
}

float3 SampleSpecularIBLTrans(float3 normal, float3 viewDirection, SpecularParameters specParam)
{
    return SampleSpecularIBLTrans_SplitSum(normal, viewDirection, specParam);
}

// note --
//      see the project "https://github.com/derkreature/IBLBaker"
//      for an interesting tool to filter the input textures

#endif
