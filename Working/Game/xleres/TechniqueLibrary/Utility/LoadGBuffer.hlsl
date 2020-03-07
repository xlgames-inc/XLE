// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(LOAD_GBUFFER_H)
#define LOAD_GBUFFER_H

#include "../Framework/gbuffer.hlsl"
#include "../Math/TextureAlgorithm.hlsl"

Texture2D_MaybeMS<float4>		GBuffer_Diffuse		BIND_NUMERIC_T0;
Texture2D_MaybeMS<float4>		GBuffer_Normals		BIND_NUMERIC_T1;
#if HAS_PROPERTIES_BUFFER==1
	Texture2D_MaybeMS<float4>	GBuffer_Parameters	BIND_NUMERIC_T2;
#endif

GBufferValues LoadGBuffer(float2 position, SystemInputs sys)
{
	int2 pixelCoord = position.xy;

    #if SHADER_NODE_EDITOR==1
            //      In the shader node editor, the gbuffer textures might not be
            //      the same dimensions as the output viewport. So we need to resample.
            //      Just do basic point resampling (otherwise normal filtering, etc,
            //      could get really complex)
        uint2 textureDims;
        GBuffer_Diffuse.GetDimensions(textureDims.x, textureDims.y);
        pixelCoord = textureDims * pixelCoord / NodeEditor_GetOutputDimensions();
    #endif

	GBufferEncoded encoded;
	encoded.diffuseBuffer = LoadFloat4(GBuffer_Diffuse, pixelCoord, GetSampleIndex(sys));
	encoded.normalBuffer = LoadFloat4(GBuffer_Normals, pixelCoord, GetSampleIndex(sys));
	#if HAS_PROPERTIES_BUFFER==1
		encoded.propertiesBuffer = LoadFloat4(GBuffer_Parameters, pixelCoord, GetSampleIndex(sys));
	#endif
	return Decode(encoded);
}

#endif
