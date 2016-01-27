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

TextureCube DiffuseIBL  : register(t19);
TextureCube SpecularIBL : register(t20);
Texture2D<float2> GlossLUT : register(t21);        // this is the look up table used in the split-sum IBL glossy reflections
Texture2D<float2> GlossTransLUT : register(t22);

TextureCube SpecularTransIBL : register(t30);

#define RECALC_SPLIT_TERM
#define RECALC_FILTERED_TEXTURE

float3 IBLPrecalc_SampleInputTexture(float3 direction)
{
    return  SkyReflectionTexture.SampleLevel(DefaultSampler, direction, 0).rgb;
}

#include "IBL/IBLPrecalc.h"

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
    #if !defined(RECALC_FILTERED_TEXTURE)
        return SpecularIBL.SampleLevel(
            DefaultSampler, AdjSkyCubeMapCoords(reflection),
            RoughnessToMipmap(roughness)).rgb;
    #else
        const uint sampleCount = 64;
        return GenerateFilteredSpecular(
            AdjSkyCubeMapCoords(reflection), roughness, sampleCount, 0, sampleCount);
    #endif
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
    #if !defined(RECALC_SPLIT_TERM)
        return GlossLUT.SampleLevel(ClampingSampler, float2(NdotV, 1.f - roughness), 0).xy;
    #else
        const uint sampleCount = 64;
        return GenerateSplitTerm(saturate(NdotV), saturate(roughness), sampleCount);
    #endif
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
    // return SampleSpecularIBL_Ref(normal, viewDirection, specParam, SkyReflectionTexture);
    return SampleSpecularIBL_SplitSum(normal, viewDirection, specParam);
}

/////////////////// T R A N S M I T T E D   S P E C U L A R ///////////////////

float2 SplitSumIBLTrans_IntegrateBRDF(float roughness, float NdotV)
{
    #if !defined(RECALC_SPLIT_TERM)
        return GlossTransLUT.SampleLevel(ClampingSampler, float2(NdotV, 1.f - roughness), 0).xy;
    #else
        const uint sampleCount = 64;
        return GenerateSplitTermTrans(NdotV, roughness, sampleCount);
    #endif
}

float3 SplitSumIBLTrans_PrefilterEnvMap(float roughness, float3 ot)
{
    #if !defined(RECALC_FILTERED_TEXTURE)
        return SpecularTransIBL.SampleLevel(
            DefaultSampler, AdjSkyCubeMapCoords(ot),
            RoughnessToMipmap(roughness)).rgb;
    #else
        const uint sampleCount = 64;
        return CalculateFilteredTextureTrans(
            AdjSkyCubeMapCoords(ot), roughness, sampleCount, 0, sampleCount);
    #endif
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

#endif
