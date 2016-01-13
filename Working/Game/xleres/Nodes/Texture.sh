// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(NODES_TEXTURE_SH)
#define NODES_TEXTURE_SH

#include "../CommonResources.h"
#include "../MainGeometry.h"
#include "../Surface.h"
#include "../Utility/perlinnoise.h"

Texture2D SecondaryNormalMap;

float4 LoadAnisotropic(Texture2D inputTexture, float2 texCoord : TEXCOORD0)
{
    return inputTexture.Sample(MaybeAnisotropicSampler, texCoord);
}

void GetDiffuseTexture(VSOutput geo, out float3 rgb, out float alpha)
{
    float4 diffuseTextureSample = 1.0.xxxx;
    #if (OUTPUT_TEXCOORD==1) && (RES_HAS_DiffuseTexture!=0)
        #if (USE_CLAMPING_SAMPLER_FOR_DIFFUSE==1)
            diffuseTextureSample = DiffuseTexture.Sample(ClampingSampler, geo.texCoord);
        #else
            diffuseTextureSample = DiffuseTexture.Sample(MaybeAnisotropicSampler, geo.texCoord);
        #endif
    #endif
    rgb = diffuseTextureSample.rgb;
    alpha = diffuseTextureSample.a;
}

float3 LoadNormalMap(
  Texture2D normalMap,
  float2 texCoord : TEXCOORD0,
  VSOutput geo)
{
    const bool dxtNormalMap = false;
    float3 normalTextureSample = SampleNormalMap(normalMap, DefaultSampler, dxtNormalMap, texCoord);
    return TransformNormalMapToWorld(normalTextureSample, geo);
}

float3 BlendNormals(float3 normalA, float3 normalB, float alpha)
{
    return normalize(normalA * (1.0f - alpha) + normalB * alpha);
}

float3 NoiseTexture3(float3 position, float scale, float detail, float distortion, out float fac)
{
    // This function made to emulate the "Noise" noise in Blender
    //      -- this makes it easier for testing

    float3 p = position * scale;
    if (distortion != 0.f) {
        const float distortionConstant = 13.5f;
        p.x += (0.5f + 0.5f * PerlinNoise3D(position + distortionConstant.xxx)) * distortion;
        p.y += (0.5f + 0.5f * PerlinNoise3D(position)) * distortion;
        p.z += (0.5f + 0.5f * PerlinNoise3D(position - distortionConstant.xxx)) * distortion;
    }

    int octaves = int(detail);
    float normFactor = ((float)(1 << octaves) / (float)((1 << (octaves + 1)) - 1));
    fac = fbmNoise3DZeroToOne(p, 1.f, .5f, 2.1042, octaves) * normFactor;
    return float3(
        fac,
        fbmNoise3DZeroToOne(float3(p[1], p[0], p[2]), 1.f, .5f, 2.1042, octaves) * normFactor,
        fbmNoise3DZeroToOne(float3(p[1], p[2], p[0]), 1.f, .5f, 2.1042, octaves) * normFactor);
}

#endif
