// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(TRANSFORM_H)
#define TRANSFORM_H

cbuffer GlobalTransform : register(b0)
{
	row_major float4x4 WorldToClip;
	float4 FrustumCorners[4];
	float3 WorldSpaceView;
	float NearClip;
	float FarClip;
	float2 DepthProjRatio;
    row_major float4x4 CameraBasis;
}

cbuffer LocalTransform : register(b1)
{
	row_major float3x4 LocalToWorld;
	float3 LocalSpaceView;
	float3 LocalNegativeLightDirection;
}

cbuffer GlobalState : register(b4)
{
	float3 NegativeDominantLightDirection;
	float Time;
}

float3x3 GetLocalToWorldUniformScale()
{
	return (float3x3)LocalToWorld;
}


#define SKIN_TRANSFORMS_MATRICES	0
#define SKIN_TRANSFORMS_QT			1
#define SKIN_TRANSFORMS_DQ			2

static const uint BoneCountMax = 256;

cbuffer BoneTransforms : register(b6)
{
		//		We have multiple options for storing bone transformations:
		//			1. float3x4 transforms	--			shader math for vector by matrix is simplier
		//												and more efficient than quaternions. So this 
		//												method creates more efficient vertex shaders than
		//												method 2.
		//												Also, scaling is supported
		//
		//			2. Quaternion + translation --		Uses 2/3rds of the shader constants of method 1.
		//												So, this method means less CPU-GPU transfers, but
		//												more expensive shaders.
		//
		//			3. Dual-Quaternion --				Cheapest math for vertices with many bone weights.
		//												But the art must be specifically made for this method.
		//
		//			4. Compressed Matrix?? --			It seems like we might be able to remove some elements
		//												from the matrix representation. If we assume there is
		//												no scale, some elements can be implied by the other 
		//												elements. This would reduce the constant usage, but
		//												increase the shader instructions. It might be more efficient
		//												than the Q+T method -- but it might not matter that
		//												much in D3D11. We can do big cbuffer and tbuffer loads
		//												efficiently now.
		//
#if SKIN_TRANSFORMS==SKIN_TRANSFORMS_MATRICES

	row_major float3x4 BoneTransforms[BoneCountMax];

#elif SKIN_TRANSFORMS==SKIN_TRANSFORMS_QT

	float2x4 BoneTransforms[BoneCountMax];

#elif SKIN_TRANSFORMS==SKIN_TRANSFORMS_DQ

		// not implemented

#endif
}

#endif
