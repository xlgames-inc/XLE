// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "particle.hlsl"
#include "../Lighting/Atmosphere.hlsl"

ParticleVStoGS main(VSInput input)
{
	ParticleVStoGS output;
	output.position = input.position;

	#if GEO_HAS_COLOR==1
		output.colour 		= VSIn_GetColour(input);
	#endif

	#if GEO_HAS_TEXCOORD==1
		output.texCoord 	= VSIn_GetTexCoord(input);
	#endif

	#if OUTPUT_FOG_COLOR == 1
		output.fogColor = CalculateFog(input.position.z, WorldSpaceView - input.position, NegativeDominantLightDirection);
	#endif

	output.texCoordScale = input.texCoordScale;
	output.screenRot = input.screenRot;
	return output;
}

VSOutput nogs(VSInput input)
{
	VSOutput output;

	float3 cameraRight	= float3(CameraBasis[0].x, CameraBasis[1].x, CameraBasis[2].x);
	float3 cameraUp		= float3(CameraBasis[0].y, CameraBasis[1].y, CameraBasis[2].y);
	float3 rotatedRight = cameraRight * input.screenRot.x + cameraUp * input.screenRot.y;
	float3 rotatedUp	= cameraRight * input.screenRot.z + cameraUp * input.screenRot.w;

	float2 expXY = input.texCoordScale.zy * 2.f - 1.f;

	float3 worldPosition
		= input.position
		+ rotatedRight * expXY.x
		+ rotatedUp * expXY.y
		;
	output.position = mul(WorldToClip, float4(worldPosition,1));

	#if OUTPUT_COLOUR==1
		output.colour 		= input.colour;
	#endif

	#if OUTPUT_TEXCOORD==1
		output.texCoord 	= input.texCoord;
	#endif

	#if (OUTPUT_TANGENT_FRAME==1)
		// build the correct tangent frame for the vertex, assuming
		// texturing over the full surface, as normal
		output.tangent = normalize(rotatedRight);
		output.bitangent = normalize(rotatedUp);
		output.normal = normalize(cross(output.tangent, output.bitangent));
	#endif

	#if OUTPUT_WORLD_VIEW_VECTOR==1
		output.worldViewVector = WorldSpaceView.xyz - worldPosition.xyz;
	#endif

	#if (OUTPUT_BLEND_TEXCOORD==1)
		output.blendTexCoord = input.blendTexCoord.xyz;
	#endif

    #if (OUTPUT_FOG_COLOR==1)
        output.fogColor = CalculateFog(worldPosition.z, WorldSpaceView - worldPosition, NegativeDominantLightDirection);
    #endif

	return output;
}
