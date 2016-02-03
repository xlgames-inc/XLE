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
#include "IBL/IBLAlgorithm.h"
#include "IBL/IBLRef.h"
#include "../Utility/Misc.h"        // for DitherPatternInt
#include "../Transform.h"           // for GlobalSamplingPassCount, GlobalSamplingPassIndex

TextureCube DiffuseIBL  : register(t19);
TextureCube SpecularIBL : register(t20);
Texture2D<float2> GlossLUT : register(t21);        // this is the look up table used in the split-sum IBL glossy reflections
Texture2D<float> GlossTransLUT : register(t22);

TextureCube SpecularTransIBL : register(t30);

#define RECALC_SPLIT_TERM
#define RECALC_FILTERED_TEXTURE
#define REF_IBL

float3 IBLPrecalc_SampleInputTexture(float3 direction)
{
    return ReadSkyReflectionTexture(InvAdjSkyCubeMapCoords(direction), 0.f);
}

#include "IBL/IBLPrecalc.h"

float3 SampleDiffuseIBL(float3 worldSpaceNormal, LightScreenDest lsd)
{
        // note -- in the same way that we apply the specular BRDF when for
        //      the gloss reflections, we could also apply the diffuse BRDF
        //      to the diffuse reflections (including the view dependent part)
        //      Currently we'll just get lambert response...
    #if defined(REF_IBL)
        uint dither = DitherPatternInt(lsd.pixelCoords)&0xf;
        dither = dither*GlobalSamplingPassCount+GlobalSamplingPassIndex;
        const uint sampleCount = 64;
        return SampleDiffuseIBL_Ref(worldSpaceNormal, SkyReflectionTexture, sampleCount, dither, 16*GlobalSamplingPassCount);
    #elif HAS_DIFFUSE_IBL==1
        float3 result = DiffuseIBL.SampleLevel(DefaultSampler, AdjSkyCubeMapCoords(worldSpaceNormal), 0).rgb;
        // note -- our pipeline doesn't currently factor the 1.0f / pi normalization factor into
        // the texture. We could burn this, rather than having to multiply here
        const float normalizationFactor = 1.0f / pi;
        result *= normalizationFactor;
        return result;
    #else
        return 0.0.xxx;
    #endif
}

float3 SplitSumIBL_PrefilterEnvMap(float roughness, float3 reflection, uint dither)
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
    #if !defined(RECALC_FILTERED_TEXTURE)
        return SpecularIBL.SampleLevel(
            DefaultSampler, AdjSkyCubeMapCoords(reflection),
            RoughnessToMipmap(roughness)).rgb;
    #else
        const uint sampleCount = 64;
        return GenerateFilteredSpecular(
            AdjSkyCubeMapCoords(reflection), roughness, sampleCount, dither&0xf, 16);
    #endif
}

float2 SplitSumIBL_IntegrateBRDF(float roughness, float NdotV, uint dither)
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
    #if !defined(RECALC_SPLIT_TERM)
        return GlossLUT.SampleLevel(ClampingSampler, float2(NdotV, 1.f - roughness), 0).xy;
    #else
        const uint sampleCount = 64;
        return GenerateSplitTerm(saturate(NdotV), saturate(roughness), sampleCount, dither&0xf, 16);
    #endif
}

float3 SampleSpecularIBL_SplitSum(float3 normal, float3 viewDirection, SpecularParameters specParam, uint dither)
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
    float3 prefilteredColor = SplitSumIBL_PrefilterEnvMap(specParam.roughness, R, dither);
    float2 envBRDF = SplitSumIBL_IntegrateBRDF(specParam.roughness, NdotV, dither);
    return prefilteredColor * (specParam.F0 * envBRDF.x + envBRDF.y);
}

float3 SampleSpecularIBL(float3 normal, float3 viewDirection, SpecularParameters specParam, LightScreenDest lsd)
{
    uint dither = DitherPatternInt(lsd.pixelCoords);
    #if defined(REF_IBL)
        const uint sampleCount = 64;
        return SampleSpecularIBL_Ref(normal, viewDirection, specParam, SkyReflectionTexture, sampleCount, dither&0xf, 16);
    #else
        return SampleSpecularIBL_SplitSum(normal, viewDirection, specParam, dither);
    #endif
}

/////////////////// T R A N S M I T T E D   S P E C U L A R ///////////////////

float SplitSumIBLTrans_IntegrateBRDF(float roughness, float NdotV, float iorIncident, float iorOutgoing, uint dither)
{
    #if !defined(RECALC_SPLIT_TERM)
        return GlossTransLUT.SampleLevel(ClampingSampler, float2(NdotV, 1.f - roughness), 0);
    #else
        const uint sampleCount = 64;
        return GenerateSplitTermTrans(NdotV, roughness, iorIncident, iorOutgoing, sampleCount, dither&0xf, 16);
    #endif
}

float3 SplitSumIBLTrans_PrefilterEnvMap(
    float roughness, float3 dir,
    float iorIncident, float iorOutgoing, uint dither)
{
    #if !defined(RECALC_FILTERED_TEXTURE)
        return SpecularTransIBL.SampleLevel(
            DefaultSampler, AdjSkyCubeMapCoords(dir),
            RoughnessToMipmap(roughness)).rgb;
    #else
        const uint sampleCount = 64;
        return CalculateFilteredTextureTrans(
            AdjSkyCubeMapCoords(dir), roughness,
            iorIncident, iorOutgoing,
            sampleCount, dither&0xf, 16);
    #endif
}

float3 SampleSpecularIBLTrans_SplitSum(
    float3 normal, float3 viewDirection,
    SpecularParameters specParam,
    float iorIncident, float iorOutgoing, uint dither)
{
    float NdotV = saturate(dot(normal, viewDirection));

    // This is a little tricky because we are performing refraction based on
    // the surface normal here, and we will consider refraction based on microfacet
    // detail later.
    // Our microfacet simulation will take into account focusing of light and
    // brighten the refraction appropriately. But we're not simulating that same
    // focusing when it comes to the surface normal refraction.
    // It's a little strange because we're doing 2 separate refraction simulations.
    // Also, the index of refraction we use here will vary depending on the material,
    // while the refraction we do on the microfacet level uses a fixed ior.

    // float3 i = refract(-viewDirection, normal, 1.f/ior); // iorOutgoing/iorIncident);
    // if (dot(i, i) <= 0.f) return 0.0.xxx;
    // float3 i = CalculateTransmissionOutgoing(viewDirection, -normal, iorIncident, iorOutgoing);

    float3 i;
    if (!CalculateTransmissionIncident(i, viewDirection, normal, iorIncident, iorOutgoing))
        return 0.0.xxx;

    float3 prefilteredColor = SplitSumIBLTrans_PrefilterEnvMap(
        specParam.roughness, i,
        iorIncident, iorOutgoing, dither);

    float envBRDF = SplitSumIBLTrans_IntegrateBRDF(specParam.roughness, NdotV, iorIncident, iorOutgoing, dither);
    return specParam.transmission * prefilteredColor * ((1.f - specParam.F0.g) * envBRDF);
}

float3 SampleSpecularIBLTrans(
    float3 normal, float3 viewDirection,
    SpecularParameters specParam, LightScreenDest lsd)
{
    // In our scheme, the camera is always within the substance with ior=iorOutgoing
    // Also, the normal should point towards the substance with the lower ior (usually air)
    // So, depending on the type of refraction we want to achieve, we must sometimes
    // flip the normal.
    float surfaceIOR = F0ToRefractiveIndex(specParam.F0.g);
    float iorIncident, iorOutgoing;
    if (dot(normal, viewDirection) > 0) {
        // viewer is outside of the object, incident light is coming from behind the interface
        iorIncident = surfaceIOR;
        iorOutgoing = 1.f;
    } else {
        // viewer is inside the object
        iorIncident = 1.f;
        iorOutgoing = surfaceIOR;
    }

    uint dither = DitherPatternInt(lsd.pixelCoords);
    #if defined(REF_IBL)
        const uint sampleCount = 64;
        return specParam.transmission * SampleTransmittedSpecularIBL_Ref(
            normal, viewDirection, specParam,
            iorIncident, iorOutgoing,
            SkyReflectionTexture, sampleCount, dither&0xf, 16);
    #else
        return SampleSpecularIBLTrans_SplitSum(normal, viewDirection, specParam, iorIncident, iorOutgoing, dither);
    #endif
}

#endif
