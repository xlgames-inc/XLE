// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../TechniqueLibrary/Framework/SystemUniforms.hlsl"
#include "../TechniqueLibrary/Framework/MainGeometry.hlsl"
#include "../TechniqueLibrary/Math/TransformAlgorithm.hlsl"
#include "../TechniqueLibrary/Framework/Surface.hlsl"

VSOUT main(VSIN input)
{
	VSOUT output;
	float3 worldPosition = mul(SysUniform_GetLocalToWorld(), float4(VSIN_GetLocalPosition(input),1));
	output.position		 = mul(SysUniform_GetWorldToClip(), float4(worldPosition,1));

	#if VSOUT_HAS_TEXCOORD==1
		output.texCoord = VSIN_GetTexCoord0(input);
	#endif

	return output;
}
