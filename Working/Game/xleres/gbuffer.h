// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(GBUFFER_H)
#define GBUFFER_H

#include "CommonResources.h"

float ManualInterpolate(Texture2D tex, float2 texCoords)
{
    uint2 dims; tex.GetDimensions(dims.x, dims.y);
    float2 exploded = texCoords*float2(dims.xy);
    float result00 = tex[uint2(exploded) + uint2(0,0)].a;
    float result10 = tex[uint2(exploded) + uint2(1,0)].a;
    float result01 = tex[uint2(exploded) + uint2(0,1)].a;
    float result11 = tex[uint2(exploded) + uint2(1,1)].a;
    float2 fracPart = frac(exploded);
    return
          result00 * (1.0f-fracPart.x) * (1.0f-fracPart.y)
        + result10 * (     fracPart.x) * (1.0f-fracPart.y)
        + result01 * (1.0f-fracPart.x) * (     fracPart.y)
        + result11 * (     fracPart.x) * (     fracPart.y)
        ;
}

float3 GBuffer_CalculateBestFitNormal(float3 inputNormal)
{
	//
	//		Calculate the normal value that will 
	//		quantize down to 8 bit best and still
	//		best represent this normal...
	//
	//		This requires a little more calculation
	//		in our shader, but it means we can write
	//		to a R8G8B8A8 output normal buffer, instead
	//		of a 16 bit floating point buffer.
	//
	//		see:
	//			http://advances.realtimerendering.com/s2010/Kaplanyan-CryEngine3(SIGGRAPH%202010%20Advanced%20RealTime%20Rendering%20Course).pdf
	//

		//	(assuming input normal is already normalized)
	
	// get unsigned normal for the cubemap lookup
	float3 vNormalUns = abs(inputNormal.rgb);
	// get the main axis for cubemap lookup
	float maxNAbs = max(vNormalUns.z, max(vNormalUns.x, vNormalUns.y));
	// get the texture coordinates in a collapsed cubemap
	float2 vTexCoord = vNormalUns.z<maxNAbs?(vNormalUns.y<maxNAbs?vNormalUns.yz:vNormalUns.xz):vNormalUns.xy;
	vTexCoord = vTexCoord.x < vTexCoord.y ? vTexCoord.yx : vTexCoord.xy;
	vTexCoord.y /= vTexCoord.x;
	// fit normal into the edge of the unit cube
	float3 result = inputNormal.xyz / maxNAbs;
	// look-up fitting length and scale the normal to get the best fit
    #if defined(_CS)
        const bool interpolateBetweenSamples = false;       // non interpolating version seems accurate enough
        float fFittingScale;
        if (interpolateBetweenSamples) {
            fFittingScale = ManualInterpolate(NormalsFittingTexture, vTexCoord);
        } else {
            uint2 dims; NormalsFittingTexture.GetDimensions(dims.x, dims.y);
            fFittingScale = NormalsFittingTexture[uint2(saturate(vTexCoord)*dims.xy + 0.5.xx)%dims].a;
            // fFittingScale = NormalsFittingTexture[uint2(vTexCoord*dims.xy)].a;
        }
    #else
	    float fFittingScale = NormalsFittingTexture.Sample(ClampingSampler, vTexCoord).a;
    #endif
	// scale the normal to get the best fit
	result.rgb *= fFittingScale;
	// wrap to [0;1] unsigned form
	result.rgb = result.rgb * .5f + .5f;
	return result;
}

float4 CompressGBufferNormal(float3 inputNormal)
{
	return float4(GBuffer_CalculateBestFitNormal(inputNormal),0);
}

float3 DecompressGBufferNormal(float4 gBufferNormalSample)
{
    float3 rangeAdj = -1.0.xxx + 2.f * gBufferNormalSample.xyz;
    float lengthSq = dot(rangeAdj, rangeAdj);
    // if (lengthSq < 0.001f)
    //     return rangeAdj;

    return rangeAdj * rsqrt(lengthSq);
}

#define HAS_PROPERTIES_BUFFER 1

struct GBufferEncoded
{
    float4 diffuseBuffer    : SV_Target0;
    float4 normalBuffer     : SV_Target1;
    #if HAS_PROPERTIES_BUFFER == 1
        float4 propertiesBuffer : SV_Target2;
    #endif
};

struct GBufferValues
{
    float3  diffuseAlbedo;
    float3  worldSpaceNormal;
    float   blendingAlpha;
    float   reflectivity;
    float   normalMapAccuracy;
    float   cookedAmbientOcclusion;
};

GBufferValues GBufferValues_Default()
{
    GBufferValues result;
    result.diffuseAlbedo     = 1.0.xxx;
    result.worldSpaceNormal  = 0.0.xxx;
    result.blendingAlpha     = 1.f;
    result.reflectivity      = 0.f;
    result.normalMapAccuracy = 1.f;
    result.cookedAmbientOcclusion = 1.f;
    return result;
}


GBufferEncoded Encode(GBufferValues values)
{
        //
        //      Take the raw gbuffer input values and
        //      generate the encoded values
        //
    GBufferEncoded result;
    result.diffuseBuffer = float4(values.diffuseAlbedo, values.blendingAlpha);
    result.normalBuffer.xyz = CompressGBufferNormal(values.worldSpaceNormal.xyz).xyz;
    result.normalBuffer.a = values.blendingAlpha;

        // todo -- use log scale for reflectivity?
    #if HAS_PROPERTIES_BUFFER == 1
        result.propertiesBuffer.r = values.reflectivity;
        result.propertiesBuffer.g = values.cookedAmbientOcclusion;
        result.propertiesBuffer.b = 0.f;
        result.propertiesBuffer.a = values.blendingAlpha;
    #else
        result.normalBuffer.a = reflectivity;
    #endif

    return result;
}

GBufferValues Decode(GBufferEncoded values)
{
    GBufferValues result = GBufferValues_Default();
	result.diffuseAlbedo = values.diffuseBuffer.rgb;
	result.worldSpaceNormal  = DecompressGBufferNormal(values.normalBuffer);
	result.blendingAlpha = values.diffuseBuffer.a;
	result.reflectivity = 1.f;
	result.normalMapAccuracy = 1.f;

	#if HAS_PROPERTIES_BUFFER==1
		result.cookedAmbientOcclusion = values.propertiesBuffer.g;
		result.reflectivity = values.propertiesBuffer.r;
	#else
		result.reflectivity = values.normalBuffer.a;
	#endif
	return result;
}

GBufferValues GetSystemStruct_GBufferValues()
{
        //  called by the shader nodes editor to get the 
        //  gbuffer values for preview
    return GBufferValues_Default();
}

#endif


