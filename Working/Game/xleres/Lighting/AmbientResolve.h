// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(AMBIENT_RESOLVE_H)
#define AMBIENT_RESOLVE_H

#include "LightDesc.h"
#include "MaterialQuery.h"
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
Texture2D						SkyReflectionTexture[3] : register(t11);

#if CALCULATE_SCREENSPACE_REFLECTIONS==1
    Texture2D<float4>			ScreenSpaceReflResult	: register(t7);
    Texture2D<float4>			LightBufferCopy			: register(t8);
#endif

#if !defined(SKY_PROJECTION)
    #define SKY_PROJECTION 3
#endif

float3 ReadSkyReflectionTexture(float3 reflectionVector, float roughness, float blurriness)
{
    uint2 reflectionTextureDims;
    SkyReflectionTexture[0].GetDimensions(reflectionTextureDims.x, reflectionTextureDims.y);
    float mipMap = blurriness + saturate(roughness) * 3.f;

    #if SKY_PROJECTION == 1

        return ReadReflectionHemiBox(
            reflectionVector,
            SkyReflectionTexture[0], SkyReflectionTexture[1], SkyReflectionTexture[2],
            reflectionTextureDims, uint(mipMap));

    #elif (SKY_PROJECTION == 3) || (SKY_PROJECTION == 4)

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

    #else
        return 0.0.xxx;
    #endif
}

float CalculateSkyReflectionFresnel(GBufferValues sample, float3 viewDirection)
{
    float F0 = Material_GetF0_0(sample);

    float3 worldSpaceReflection = reflect(-viewDirection, sample.worldSpaceNormal);
    float3 halfVector = normalize(worldSpaceReflection + viewDirection);
    float fresnel = SchlickFresnelF0(viewDirection, halfVector, F0);
    return fresnel;
}

float3 CalculateSkyReflections(GBufferValues sample, float3 viewDirection, float fresnel, float blurriness)
{
    float3 worldSpaceReflection = reflect(-viewDirection, sample.worldSpaceNormal);

    float roughness = Material_GetRoughness(sample);
    float specular = Material_GetSpecular0(sample);

    float3 reflSampl = ReadSkyReflectionTexture(worldSpaceReflection, roughness, blurriness);
    reflSampl += Material_GetReflectionBoost(sample).xxx;

    return reflSampl * (specular * fresnel);
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

    float metal = Material_GetMetal(sample);
    return ((1.0f - metal)*ambientOcclusionSample)*(ambientColor + tiledLights)*sample.diffuseAlbedo.rgb;
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
    uint sampleIndex)
{
    float3 result = CalcBasicAmbient(pixelCoords, sampleIndex, sample, ambient.Colour);

    float fresnel = CalculateSkyReflectionFresnel(sample, directionToEye);
    float blurriness = ambient.SkyReflectionBlurriness;

    float3 skyReflections = ambient.SkyReflectionScale * CalculateSkyReflections(sample, directionToEye, fresnel, blurriness);
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

    finalReflection *= lerp(sample.diffuseAlbedo, 1.0.xxx, Material_GetMetal(sample));
    finalReflection *= sample.cookedAmbientOcclusion;
    result += Material_GetReflectionScale(sample) * finalReflection;

    return result;
}

#endif
