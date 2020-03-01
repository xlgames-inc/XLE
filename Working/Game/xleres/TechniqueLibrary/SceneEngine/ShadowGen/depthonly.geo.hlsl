// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "GeometryConfiguration.hlsl"
#include "../Lighting/ShadowProjection.hlsl"
#include "../../Framework/Transform.hlsl"
#include "../../Framework/MainGeometry.hlsl"
#include "../../Math/ProjectionMath.hlsl"

	// support up to 6 output frustums (for cube map point light source projections)
[maxvertexcount(18)]
	void main(	triangle VSShadowOutput input[3],
				uint primitiveId : SV_PrimitiveID,
				inout TriangleStream<VSOutput> outputStream)
{
		// todo -- perhaps do backface removal here to
		//		quickly reject half of the triangles
		//		-- but we have to enable/disable it for double sided geometry
	//if (BackfaceSign(float4(input[0].position,1), float4(input[1].position,1), float4(input[2].position,1)) > 0)
	//	return;

	uint count = min(GetShadowSubProjectionCount(GetShadowCascadeMode()), OUTPUT_SHADOW_PROJECTION_COUNT);
	for (uint c=0; c<count; ++c) {
		#if defined(FRUSTUM_FILTER)
			if ((FRUSTUM_FILTER & (1<<c)) == 0) {
				continue;
			}
		#endif

		uint shadowFrustumFlags0 = (input[0].shadowFrustumFlags >> (c*4)) & 0xf;
		uint shadowFrustumFlags1 = (input[1].shadowFrustumFlags >> (c*4)) & 0xf;
		uint shadowFrustumFlags2 = (input[2].shadowFrustumFlags >> (c*4)) & 0xf;

			//	do some basic culling
			//	-- left, right, top, bottom (but not front/back)
		if (((shadowFrustumFlags0 & shadowFrustumFlags1) & shadowFrustumFlags2) != 0) {
			continue;
		}

		#if SHADOW_CASCADE_MODE==SHADOW_CASCADE_MODE_ARBITRARY
			float4 p0 = input[0].shadowPosition[c];
			float4 p1 = input[1].shadowPosition[c];
			float4 p2 = input[2].shadowPosition[c];
		#elif SHADOW_CASCADE_MODE==SHADOW_CASCADE_MODE_ORTHOGONAL
			float4 p0 = float4(AdjustForOrthoCascade(input[0].position.xyz, c), 1.f);
			float4 p1 = float4(AdjustForOrthoCascade(input[1].position.xyz, c), 1.f);
			float4 p2 = float4(AdjustForOrthoCascade(input[2].position.xyz, c), 1.f);
		#else
			float4 p0 = 0.0.xxxx;
			float4 p1 = 0.0.xxxx;
			float4 p2 = 0.0.xxxx;
		#endif

			//
			//		Try to offset the final depth buffer
			//		value by half the depth buffer precision.
			//		This seems to get a better result... suggesting
			//		maybe a floor takes place? It's unclear. Actually,
			//		subtracting half a pixel seems to give the most
			//		balanced result. When there is no bias, this approximately
			//		half of pixels shadow themselves, and half don't.
			//		Still some investigation required to find out why that is...
			//
		const float halfDepthBufferPrecision = .5f/65536.f;
		p0.z -= halfDepthBufferPrecision * p0.w;
		p1.z -= halfDepthBufferPrecision * p1.w;
		p2.z -= halfDepthBufferPrecision * p2.w;

		VSOutput output;
		output.position = p0;
		#if OUTPUT_TEXCOORD==1
			output.texCoord = input[0].texCoord;
		#endif
		#if OUTPUT_COLOUR==1
			output.colour = input[0].colour;
		#endif
		output.renderTargetIndex = c;
		#if (OUTPUT_PRIMITIVE_ID==1)
			output.primitiveId = primitiveId;
		#endif
		outputStream.Append(output);

		output.position = p1;
		#if OUTPUT_TEXCOORD==1
			output.texCoord = input[1].texCoord;
		#endif
		#if OUTPUT_COLOUR>=1
			output.colour = input[1].colour;
		#endif
		output.renderTargetIndex = c;
		#if (OUTPUT_PRIMITIVE_ID==1)
			output.primitiveId = primitiveId;
		#endif
		outputStream.Append(output);

		output.position = p2;
		#if OUTPUT_TEXCOORD==1
			output.texCoord = input[2].texCoord;
		#endif
		#if OUTPUT_COLOUR>=1
			output.colour = input[2].colour;
		#endif
		output.renderTargetIndex = c;
		#if (OUTPUT_PRIMITIVE_ID==1)
			output.primitiveId = primitiveId;
		#endif
		outputStream.Append(output);
		outputStream.RestartStrip();

			// if the triangle is entirely within this frustum, then skip other frustums
			// note -- maybe this condition doesn't help enough to cover it's cost?
		if (((shadowFrustumFlags0 | shadowFrustumFlags1) | shadowFrustumFlags2) == 0) {
			break;
		}
	}

	#if (SHADOW_CASCADE_MODE==SHADOW_CASCADE_MODE_ORTHOGONAL) && (SHADOW_ENABLE_NEAR_CASCADE==1)
		// If we have the near shadows enabled, we must write out an additional shadow projection
		// into the final render target.
		// Note that the clip test here is going to be expensive! We ideally enable this only when
		// required.

		uint nearCascadeIndex = GetShadowSubProjectionCount(GetShadowCascadeMode());

		float4 p0 = float4(mul(OrthoNearCascade, float4(input[0].position.xyz, 1.f)), 1.f);
		float4 p1 = float4(mul(OrthoNearCascade, float4(input[1].position.xyz, 1.f)), 1.f);
		float4 p2 = float4(mul(OrthoNearCascade, float4(input[2].position.xyz, 1.f)), 1.f);

		if (TriInFrustum(p0, p1, p2)) {
			const float halfDepthBufferPrecision = .5f/65536.f;
			p0.z -= halfDepthBufferPrecision * p0.w;
			p1.z -= halfDepthBufferPrecision * p1.w;
			p2.z -= halfDepthBufferPrecision * p2.w;

			VSOutput output;
			output.position = p0;
			#if OUTPUT_TEXCOORD==1
				output.texCoord = input[0].texCoord;
			#endif
			#if OUTPUT_COLOUR==1
				output.colour = input[0].colour;
			#endif
			output.renderTargetIndex = nearCascadeIndex;
			#if (OUTPUT_PRIMITIVE_ID==1)
				output.primitiveId = primitiveId;
			#endif
			outputStream.Append(output);

			output.position = p1;
			#if OUTPUT_TEXCOORD==1
				output.texCoord = input[1].texCoord;
			#endif
			#if OUTPUT_COLOUR==1
				output.colour = input[1].colour;
			#endif
			output.renderTargetIndex = nearCascadeIndex;
			#if (OUTPUT_PRIMITIVE_ID==1)
				output.primitiveId = primitiveId;
			#endif
			outputStream.Append(output);

			output.position = p2;
			#if OUTPUT_TEXCOORD==1
				output.texCoord = input[2].texCoord;
			#endif
			#if OUTPUT_COLOUR==1
				output.colour = input[2].colour;
			#endif
			output.renderTargetIndex = nearCascadeIndex;
			#if (OUTPUT_PRIMITIVE_ID==1)
				output.primitiveId = primitiveId;
			#endif
			outputStream.Append(output);
			outputStream.RestartStrip();
		}

	#endif

}
