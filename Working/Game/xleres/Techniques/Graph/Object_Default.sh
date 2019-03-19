
#if !defined(OBJECT_DEFAULT_SH)
#define OBJECT_DEFAULT_SH

#include "../../MainGeometry.h"
#include "../../Surface.h"
#include "../../CommonResources.h"
#include "../../gbuffer.h"

float4 DiffuseSampler(VSOutput geo)
{
    return DiffuseTexture.Sample(MaybeAnisotropicSampler, geo.texCoord);
}

float3 NormalSampler(VSOutput geo)
{
    return GetNormal(geo);
}

PerPixelMaterialParam MaterialSampler(VSOutput geo)
{
    PerPixelMaterialParam result;
	result.roughness = 0.25;
	result.specular = 0.05;
	result.metal = 0.0;
	return result;
}

GBufferValues MakeGBufferValues(float4 diffuse, float3 worldSpaceNormal, PerPixelMaterialParam material)
{
    GBufferValues result = GBufferValues_Default();
    result.diffuseAlbedo = diffuse.rgb;
    result.blendingAlpha = diffuse.a;
    result.material = material;
    result.worldSpaceNormal = worldSpaceNormal;
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////

PerPixelMaterialParam DecodeParametersTexture_RMS(float4 paramTextureSample)
{
		//	We're just storing roughness, specular & material
		//	pixel pixel in this texture. Another option is to
		//	have a per-pixel material index, and get these values
		//	from a small table of material values.
	PerPixelMaterialParam result = PerPixelMaterialParam_Default();
	result.roughness = paramTextureSample.r;
	result.specular = paramTextureSample.g;
	result.metal = paramTextureSample.b;
	return result;
}

Texture2D       ParametersTexture       BIND_MAT_T2;

PerPixelMaterialParam MaterialSampler_RMS(VSOutput geo)
{
    return DecodeParametersTexture_RMS(
        ParametersTexture.Sample(DefaultSampler, geo.texCoord));
}

#endif
