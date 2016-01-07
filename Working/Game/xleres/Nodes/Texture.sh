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

Texture2D SecondaryNormalMap;

float4 SampleAnisotropic(Texture2D inputTexture, float2 texCoord : TEXCOORD0)
{
    return inputTexture.Sample(MaybeAnisotropicSampler, texCoord);
}

float4 GetDiffuseTexture(VSOutput geo)
{
    float4 diffuseTextureSample = 1.0.xxxx;
    #if (OUTPUT_TEXCOORD==1) && (RES_HAS_DiffuseTexture!=0)
        #if (USE_CLAMPING_SAMPLER_FOR_DIFFUSE==1)
            diffuseTextureSample = DiffuseTexture.Sample(ClampingSampler, geo.texCoord);
        #else
            diffuseTextureSample = DiffuseTexture.Sample(MaybeAnisotropicSampler, geo.texCoord);
        #endif
    #endif
    return diffuseTextureSample;
}

float3 LoadNormalMap(
  Texture2D normalMap,
  float2 texCoord : TEXCOORD0,
  VSOutput geo)
{
    const bool dxtNormalMap = false;
    float3 normalTextureSample = SampleNormalMap(normalMap, DefaultSampler, dxtNormalMap, texCoord);
    return NormalMapToWorld(normalTextureSample, geo);
}

float3 BlendNormals(float3 normalA, float3 normalB, float alpha)
{
    return normalize(normalA * (1.0f - alpha) + normalB * alpha);
}

#endif
