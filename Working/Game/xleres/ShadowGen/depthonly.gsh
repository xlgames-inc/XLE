// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "GeometryConfiguration.h"
#include "../Transform.h"
#include "../MainGeometry.h"
#include "../ShadowProjection.h"

	// support up to 6 output frustums (for cube map point light source projections)
[maxvertexcount(18)]
	void main(triangle VSShadowOutput input[3], inout TriangleStream<VSOutput> outputStream)
{
	for (uint c=0; c<ProjectionCount; ++c) {
		#if defined(FRUSTUM_FILTER)
			if ((FRUSTUM_FILTER & (1<<c)) == 0) {
				continue;
			}
		#endif
		
		float4 p0 = input[0].shadowPosition[c];
		float4 p1 = input[1].shadowPosition[c];
		float4 p2 = input[2].shadowPosition[c];

		uint shadowFrustumFlags0 = (input[0].shadowFrustumFlags >> (c*4)) & 0xf;
		uint shadowFrustumFlags1 = (input[1].shadowFrustumFlags >> (c*4)) & 0xf;
		uint shadowFrustumFlags2 = (input[2].shadowFrustumFlags >> (c*4)) & 0xf;

			//	do some basic culling
			//	-- left, right, top, bottom (but not front/back)
		if (((shadowFrustumFlags0 & shadowFrustumFlags1) & shadowFrustumFlags2) != 0) {
			continue;
		}

			//
			//		Try to offset the final depth buffer
			//		value by half the depth buffer precision.
			//		This seems to get a better result... suggesting
			//		maybe a floor takes place.
			//
		const float halfDepthBufferPrecision = .5f/65536.f;
		p0.z += halfDepthBufferPrecision * p0.w;
		p1.z += halfDepthBufferPrecision * p1.w;
		p2.z += halfDepthBufferPrecision * p2.w;

		VSOutput output;
		output.position = p0;
		#if OUTPUT_TEXCOORD==1
			output.texCoord = input[0].texCoord;
		#endif
		#if OUTPUT_COLOUR==1
			output.colour = input[0].colour;
		#endif
		output.renderTargetIndex = c;
		outputStream.Append(output);
		
		output.position = p1;
		#if OUTPUT_TEXCOORD==1
			output.texCoord = input[1].texCoord;
		#endif
		#if OUTPUT_COLOUR==1
			output.colour = input[1].colour;
		#endif
		output.renderTargetIndex = c;
		outputStream.Append(output);
		
		output.position = p2;
		#if OUTPUT_TEXCOORD==1
			output.texCoord = input[2].texCoord;
		#endif
		#if OUTPUT_COLOUR==1
			output.colour = input[2].colour;
		#endif
		output.renderTargetIndex = c;
		outputStream.Append(output);
		outputStream.RestartStrip();

			// if the triangle is entirely within this frustum, then skip other frustums
		if (((shadowFrustumFlags0 | shadowFrustumFlags1) | shadowFrustumFlags2) == 0) {
			break;
		}
	}
}
