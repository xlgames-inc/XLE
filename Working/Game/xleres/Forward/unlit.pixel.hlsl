// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../TechniqueLibrary/Framework/MainGeometry.hlsl"
#include "../TechniqueLibrary/System/Binding.hlsl"
#include "../TechniqueLibrary/Math/TextureAlgorithm.hlsl"
#include "../TechniqueLibrary/Framework/CommonResources.hlsl"

Texture2D		DiffuseTexture          BIND_MAT_T0;

    //  This cbuffer contains basic constants are used frequently enough that
    //  we can add support for them in most shaders.
cbuffer BasicMaterialConstants BIND_MAT_B0
{
	float3  MaterialDiffuse;
    float   Opacity;
}

#if !((OUTPUT_TEXCOORD==1) && (MAT_ALPHA_TEST==1)) && (VULKAN!=1)
	[earlydepthstencil]
#endif
float4 main(VSOutput geo, SystemInputs sys) : SV_Target0
{
	float4 result = float4(MaterialDiffuse.rgb, Opacity);

    #if (OUTPUT_TEXCOORD==1) && (RES_HAS_DiffuseTexture!=0)
        result *= DiffuseTexture.Sample(MaybeAnisotropicSampler, geo.texCoord);
    #endif

    #if (OUTPUT_COLOUR==1) && MAT_MODULATE_VERTEX_ALPHA
        result.a *= geo.colour.a;
    #endif

    #if (OUTPUT_COLOUR>=1)
        result.rgb *= geo.colour.rgb;
    #endif

    return result;
}
