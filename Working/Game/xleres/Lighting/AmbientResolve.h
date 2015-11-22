// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(AMBIENT_RESOLVE_H)
#define AMBIENT_RESOLVE_H

#include "LightDesc.h"
#include "MaterialQuery.h"
#include "Constants.h"
#include "ImageBased.h"
#include "../TextureAlgorithm.h"
#include "../Colour.h"	                 // for LightingScale

#if CALCULATE_SCREENSPACE_REFLECTIONS==1
    #include "../System/LoadGBuffer.h"
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(TILED_LIGHTS_RESOLVE_MS)
    Texture2D_MaybeMS<float4>	TiledLightsResolve	 	: register(t5);
#else
    Texture2D<float4>			TiledLightsResolve	 	: register(t5);
#endif

Texture2D<float>				AmbientOcclusion		: register(t6);

#if SKY_PROJECTION==5
    TextureCube						SkyReflectionTexture : register(t11);
#else
    Texture2D						SkyReflectionTexture[3] : register(t11);
#endif

#if CALCULATE_SCREENSPACE_REFLECTIONS==1
    Texture2D<float4>			ScreenSpaceReflResult	: register(t7);
    Texture2D<float4>			LightBufferCopy			: register(t8);
#endif

#if !defined(SKY_PROJECTION)
    #define SKY_PROJECTION 3
#endif

float3 ReadSkyReflectionTexture(float3 reflectionVector, float roughness, float blurriness)
{

        // note --  Ideally we want to be using importance sampling (or similar Monte Carlo
        //          method) to find the correct level of blurriness for the reflections.
        //          But it's a little expensive.
        //          We can get an efficient version by building the importance sampled
        //          blurred textures into the mipmaps...
        //          Until then, just adding a bias to the mipmap level gives us a
        //          rough approximation.
    float mipMap = blurriness + saturate(roughness) * ReflectionBlurrinessFromRoughness;

    #if SKY_PROJECTION == 1

        uint2 reflectionTextureDims;
        SkyReflectionTexture[0].GetDimensions(reflectionTextureDims.x, reflectionTextureDims.y);

        return ReadReflectionHemiBox(
            reflectionVector,
            SkyReflectionTexture[0], SkyReflectionTexture[1], SkyReflectionTexture[2],
            reflectionTextureDims, uint(mipMap));

    #elif (SKY_PROJECTION == 3) || (SKY_PROJECTION == 4)

        uint2 reflectionTextureDims;
        SkyReflectionTexture[0].GetDimensions(reflectionTextureDims.x, reflectionTextureDims.y);

        #if (SKY_PROJECTION == 3)
            float2 skyReflectionCoord = EquirectangularMappingCoord(reflectionVector);
        #else
            float2 skyReflectionCoord = HemiEquirectangularMappingCoord(reflectionVector);
        #endif
        mipMap = max(mipMap, CalculateMipmapLevel(skyReflectionCoord.xy, reflectionTextureDims));

        // #define DEBUG_REFLECTION_MIPMAPS 1
        #if DEBUG_REFLECTION_MIPMAPS==1
            if (uint(mipMap)==0) return float3(1, 0, 0);
            if (uint(mipMap)==1) return float3(0, 1, 0);
            if (uint(mipMap)==2) return float3(0, 0, 1);
            if (uint(mipMap)==3) return float3(1, 1, 0);
            if (uint(mipMap)==4) return float3(1, 0, 1);
            if (uint(mipMap)==5) return float3(0, 1, 1);
            if (uint(mipMap)==6) return float3(0, .5, 1);
            if (uint(mipMap)==7) return float3(1, 0, .5);
        #endif

        return SkyReflectionTexture[0].SampleLevel(ClampingSampler, skyReflectionCoord.xy, mipMap).rgb;

    #elif SKY_PROJECTION==5

            // todo -- we need to calculate the correct cubemap mipmap here, and then
            //          clamp against "mipMap"
        return SkyReflectionTexture.SampleLevel(DefaultSampler, reflectionVector, mipMap).rgb;
    #else
        return 0.0.xxx;
    #endif
}

float3 CalculateSkyReflectionFresnel(float3 F0, GBufferValues sample, float3 viewDirection, bool mirrorReflection)
{
        // Note that it might be worth considering a slight hack to
        // the fresnel calculation here... Imagine a reflective sphere --
        // around microfacet shadowing should start to play a bigger effect.
        // That should give us an excuse to ramp down the fresnel a little
        // bit around the edge. This could be handy because very bright halos
        // around objects can tend to highlight inaccuracies in the lighting.
    if (!mirrorReflection) {
        float3 worldSpaceReflection = reflect(-viewDirection, sample.worldSpaceNormal);
        float3 halfVector = normalize(worldSpaceReflection + viewDirection);
        return SchlickFresnelF0(viewDirection, halfVector, F0);
    } else {
        return SchlickFresnelF0(viewDirection, sample.worldSpaceNormal, F0);
    }
}

float3 CalculateSkyReflections(GBufferValues sample, float3 viewDirection, float3 fresnel, float blurriness)
{
    float3 worldSpaceReflection = reflect(-viewDirection, sample.worldSpaceNormal);
    float roughness = Material_GetRoughness(sample);
    return fresnel * ReadSkyReflectionTexture(worldSpaceReflection, roughness, blurriness);
}

/////////////////////////////////////////

float3 CalcBasicAmbient(int2 pixelCoords, uint sampleIndex, GBufferValues sample, float3 ambientColor)
{
    #if CALCULATE_AMBIENT_OCCLUSION==1
        float ambientOcclusionSample = LoadFloat1(AmbientOcclusion, pixelCoords, sampleIndex);
    #else
        float ambientOcclusionSample = 1.f;
    #endif

    ambientOcclusionSample *= sample.cookedAmbientOcclusion;

    #if CALCULATE_TILED_LIGHTS==1
        #if defined(TILED_LIGHTS_RESOLVE_MS)
            float3 tiledLights	= LoadFloat4(TiledLightsResolve, pixelCoords, sampleIndex).xyz;
        #else
            float3 tiledLights	= TiledLightsResolve.Load(int3(pixelCoords, 0)).xyz;
        #endif
    #else
        float3 tiledLights = 0.0.xxx;
    #endif

    float3 diffuseIBL = SampleDiffuseIBL(sample.worldSpaceNormal);

    float metal = Material_GetMetal(sample);
    return ((1.0f - metal)*ambientOcclusionSample)*(ambientColor + tiledLights + diffuseIBL)*sample.diffuseAlbedo.rgb;
}

/////////////////////////////////////////

#if CALCULATE_SCREENSPACE_REFLECTIONS==1
    float4 CalculateScreenSpaceReflections(float2 texCoord, float fresnel, float ambientColor)
    {
            //	the screen space refl sampler should contain a texture coordinate
            //	and an alpha value. We need a copy of the "lighting buffer"
            //	render target for this step -- because we want to read from a texture
            //	that can the direct lighting already calculated.
        float4 reflectionValue = ScreenSpaceReflResult.SampleLevel(ClampingSampler, texCoord, 0);

        float intersectionQuality = reflectionValue.z;
        float pixelReflectivity = reflectionValue.a;
        float2 reflectionSourceCoords = reflectionValue.xy;
        float3 litSample = LightBufferCopy.SampleLevel(ClampingSampler, reflectionSourceCoords, 0).rgb;

            //	we need to predict the ambient light on this point, also (because integration occurs
            //	in the ambient shader, the amient integrating hasn't happened)
        uint2 dims; LightBufferCopy.GetDimensions(dims.x, dims.y);
        float4 samplePosition = float4(reflectionSourceCoords * float2(dims), 0.f, 1.f);
        GBufferValues sample = LoadGBuffer(samplePosition, SystemInputs_Default());
        litSample += CalcBasicAmbient(int2(samplePosition.xy), SystemInputs_Default(), sample, ambientColor);

        return float4(
            litSample.rgb * (fresnel / LightingScale),
            min(1.f, intersectionQuality * pixelReflectivity));
    }
#endif

float3 LightResolve_Ambient(
    GBufferValues sample,
    float3 directionToEye,
    AmbientDesc ambient,
    int2 pixelCoords,
    uint sampleIndex,
    bool mirrorReflection = false)
{
    float3 result = CalcBasicAmbient(pixelCoords, sampleIndex, sample, ambient.Colour);

        // In our metal model, we store F0 values per wavelength in sample.diffuseAlbedo.
        // This gives us fantasic freedom to control the metallic reflections.
        // See some good reference here:
        //  https://seblagarde.wordpress.com/2011/08/17/feeding-a-physical-based-lighting-mode/
        // Note that we can calculate the correct F0 values for real-world materials using
        // the methods described on that page.
        // Also consider sRGB/Linear wierdness in this step...
    float3 F0 = lerp(Material_GetF0_0(sample).xxx, sample.diffuseAlbedo, Material_GetMetal(sample));

    float3 fresnel = CalculateSkyReflectionFresnel(F0, sample, directionToEye, mirrorReflection);
    float blurriness = ambient.SkyReflectionBlurriness;

    #if SKY_PROJECTION==5
        float3 skyReflections = SampleSpecularIBL(
            sample.worldSpaceNormal, directionToEye,
            SpecularParameters_RoughF0(sample.material.roughness, F0),
            SkyReflectionTexture);
    #else
        float3 skyReflections = CalculateSkyReflections(sample, directionToEye, fresnel, blurriness);
    #endif
    skyReflections *= ambient.SkyReflectionScale;

    #if CALCULATE_SCREENSPACE_REFLECTIONS==1
            //	The "screen space" reflections block out the sky reflections.
            //	If we get a collision with the screen space reflections, then we need
            //	to blend from the sky reflection colour into that colour.
            // note... if we want fogging on the reflections, we need to perform the fog calculations here, on the
            // reflected pixel
        float4 dynamicReflections = CalculateScreenSpaceReflections(texCoord, fresnel, ambient.Color);
        float3 finalReflection = lerp(skyReflections, dynamicReflections.rgb, dynamicReflections.a);
    #else
        float3 finalReflection = skyReflections;
    #endif

        // This is just a hack to stand in for better specular occlusion
        // we need some kind of specular occlusion here, otherwise it becomes too
        // difficult to balance
    #define SPECULAR_OCCLUSION_EXPERIMENT
    #if defined(SPECULAR_OCCLUSION_EXPERIMENT)
        finalReflection *= saturate(dot(viewDirection, sample.worldSpaceNormal));
    #endif

    finalReflection *= sample.cookedAmbientOcclusion;
    result += Material_GetReflectionScale(sample) * finalReflection;

    return result;
}

#endif
