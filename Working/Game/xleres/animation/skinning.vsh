// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)


/////////////////////////////////// cbuffer method ///////////////////////////////////

cbuffer JointTransforms : register(b5)
{
	row_major float3x4 JointTransforms[200];
}

float3 CalculateSkinnedVertexPosition( float3 inputPosition, float4 weights, uint4 jointIndices, uint influenceCount)
{
	float3 outputPosition = 0.0.xxx;
	[unroll] for (uint c=0; c<influenceCount; ++c) {
		outputPosition += weights[c] * mul(JointTransforms[jointIndices[c]], float4(inputPosition,1)).xyz;
	}

	return outputPosition;
}

float3 CalculateSkinnedVertexNormal( float3 inputNormal, float4 weights, uint4 jointIndices, uint influenceCount)
{
	float3 outputNormal = 0.0.xxx;
	[unroll] for (uint c=0; c<influenceCount; ++c) {
		float3x4 jointTransform = JointTransforms[jointIndices[c]];
		float3x3 rotationPart = float3x3(jointTransform[0].xyz, jointTransform[1].xyz, jointTransform[2].xyz);
		outputNormal += weights[c] * mul(rotationPart, inputNormal);
	}

		//	Note --		not renormalizing the normal here.
		//				We usually re-normalize in the pixel shader. So the effects of normalizing
		//				in the vertex shader are a little unclear. Of course, it will affect the
		//				interpolation across the triangle.
	return outputNormal;
}


///////////////////////////////////   E N T R Y   P O I N T S   ///////////////////////////////////

float3 P4(	float3 inputPosition	: POSITION0,
			float4 weights			: WEIGHTS0,
			uint4 jointIndices		: JOINTINDICES0 ) : POSITION0
{
		//
		//		Take the input position, and transform it through the 
		//		skinning calculations required for the given weights
		//		and joint indicies
		//

	return CalculateSkinnedVertexPosition(inputPosition, weights, jointIndices, 4);
}

float3 P2(	float3 inputPosition	: POSITION0,
			float4 weights			: WEIGHTS0,
			uint4 jointIndices		: JOINTINDICES0 ) : POSITION0
{
	return CalculateSkinnedVertexPosition(inputPosition, weights, jointIndices, 2);
}

float3 P1(	float3 inputPosition	: POSITION0,
			float4 weights			: WEIGHTS0,
			uint4 jointIndices		: JOINTINDICES0 ) : POSITION0
{
	return CalculateSkinnedVertexPosition(inputPosition, weights, jointIndices, 1);
}

float3 P0(	float3 inputPosition	: POSITION0,
			float4 weights			: WEIGHTS0,
			uint4 jointIndices		: JOINTINDICES0 ) : POSITION0
{
	return inputPosition;
}




float3 PN4(	float3 inputPosition	: POSITION0,
			float3 inputNormal		: NORMAL0,
			float4 weights			: WEIGHTS0,
			uint4 jointIndices		: JOINTINDICES0,
			out float3 oNormal		: NORMAL0 ) : POSITION0
{
		//
		//		Take the input position, and transform it through the 
		//		skinning calculations required for the given weights
		//		and joint indicies
		//

	oNormal = CalculateSkinnedVertexNormal(inputNormal, weights, jointIndices, 4);
	return CalculateSkinnedVertexPosition(inputPosition, weights, jointIndices, 4);
}

float3 PN2(	float3 inputPosition	: POSITION0,
			float3 inputNormal		: NORMAL0,
			float4 weights			: WEIGHTS0,
			uint4 jointIndices		: JOINTINDICES0,
			out float3 oNormal		: NORMAL0 ) : POSITION0
{
	oNormal = CalculateSkinnedVertexNormal(inputNormal, weights, jointIndices, 2);
	return CalculateSkinnedVertexPosition(inputPosition, weights, jointIndices, 2);
}

float3 PN1(	float3 inputPosition	: POSITION0,
			float3 inputNormal		: NORMAL0,
			float4 weights			: WEIGHTS0,
			uint4 jointIndices		: JOINTINDICES0,
			out float3 oNormal		: NORMAL0 ) : POSITION0
{
	oNormal = CalculateSkinnedVertexNormal(inputNormal, weights, jointIndices, 1);
	return CalculateSkinnedVertexPosition(inputPosition, weights, jointIndices, 1);
}

float3 PN0(	float3 inputPosition	: POSITION0,
			float3 inputNormal		: NORMAL0,
			float4 weights			: WEIGHTS0,
			uint4 jointIndices		: JOINTINDICES0,
			out float3 oNormal		: NORMAL0 ) : POSITION0
{
	oNormal = inputNormal;
	return inputPosition;
}

