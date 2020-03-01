// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(LIGHTING_FORWARD_H)
#define LIGHTING_FORWARD_H

#include "LightDesc.hlsl"
#include "DirectionalResolve.hlsl"
#include "AmbientResolve.hlsl"
#include "BasicLightingEnvironment.hlsl"
#include "ShadowsResolve.hlsl"
#include "CascadeResolve.hlsl"

float3 ResolveLitColor(
	GBufferValues sample, float3 directionToEye,
	float3 worldPosition,
	LightScreenDest screenDest)
{
	float3 result = 0.0.xxx;

	#if (OUTPUT_NORMAL==1)

			// Calculate the shadowing of light sources (where we can)

		float shadowing[BASIC_LIGHT_COUNT];
		{
			[unroll] for (uint c=0; c<BASIC_LIGHT_COUNT; ++c)
				shadowing[c] = 1.f;

			#if SHADOW_CASCADE_MODE!=0 && (VULKAN!=1)
				bool enableNearCascade = false;
				#if SHADOW_ENABLE_NEAR_CASCADE != 0
					enableNearCascade = true;
				#endif

				CascadeAddress cascade = ResolveCascade_FromWorldPosition(worldPosition, GetShadowCascadeMode(), enableNearCascade);
				if (cascade.cascadeIndex >= 0) {
					shadowing[0] = ResolveShadows_Cascade(
						cascade.cascadeIndex, cascade.frustumCoordinates, cascade.miniProjection,
						screenDest.pixelCoords, screenDest.sampleIndex, ShadowResolveConfig_NoFilter());
				}
			#endif
		}

			// note -- 	This loop must be unrolled when using GGX specular...
			//			This is because there are texture look-ups in the GGX
			//			implementation, and those can cause won't function
			//			correctly inside a dynamic loop

		[unroll] for (uint c=0; c<BASIC_LIGHT_COUNT; ++c) {
			result += shadowing[c] * LightResolve_Diffuse(
				sample, directionToEye,
				BasicLight[c].Position, BasicLight[c]);

			result += shadowing[c] * LightResolve_Specular(
				sample, directionToEye,
				BasicLight[c].Position, BasicLight[c]);
		}
	#endif

	result += LightResolve_Ambient(
		sample, directionToEye, BasicAmbient, screenDest,
		AmbientResolveHelpers_Default());

	return result;
}

#endif
