// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define GEO_HAS_TEXCOORD 1

#include "../TechniqueLibrary/Framework/SystemUniforms.hlsl"
#include "../TechniqueLibrary/Framework/MainGeometry.hlsl"

VSOUT main(VSIN input)
{
	VSOUT output;
	float3 localPosition	= float3(input.position.xy, input.texCoord.x);
	float3 worldPosition 	= mul(SysUniform_GetLocalToWorld(), float4(localPosition,1));
	output.position = float4(worldPosition, 1);
	output.texCoord = 0.0.xx;
	return output;
}

