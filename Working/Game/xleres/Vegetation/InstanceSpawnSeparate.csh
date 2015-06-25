// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Transform.h"

ByteAddressBuffer	InstancePositions;
ByteAddressBuffer	InstanceTypes;

struct InstanceDef
{
	float4 posAndShadowing;
	float2 sinCosTheta;
};

AppendStructuredBuffer<InstanceDef> OutputBuffer[INSTANCE_BIN_COUNT];

cbuffer SpawnParams : register(b1)
{
	uint BinThresholds[8];
	float DrawDistanceSq[8];
}

bool CheckAppend(uint index, uint typeValue, InstanceDef def)
{
	[branch] if (index < INSTANCE_BIN_COUNT && typeValue <= BinThresholds[index]) {
		float3 viewOffset = def.posAndShadowing.xyz - WorldSpaceView;
		if (dot(viewOffset, viewOffset) < DrawDistanceSq[index])
			OutputBuffer[index].Append(def);
		return true;
	}
	return false;
}

[numthreads(256, 1, 1)]
	void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	uint params = InstanceTypes.Load(4*dispatchThreadId.x);
	uint type = params & 0xffff;
	uint sh0 = params >> 16;

	InstanceDef def;
	def.posAndShadowing = float4(
		asfloat(InstancePositions.Load(dispatchThreadId.x*16+ 0)),
		asfloat(InstancePositions.Load(dispatchThreadId.x*16+ 4)),
		asfloat(InstancePositions.Load(dispatchThreadId.x*16+ 8)),
		sh0 / float(0xffff));

	float rotationAngle = asfloat(InstancePositions.Load(dispatchThreadId.x*16+12));
	sincos(rotationAngle, def.sinCosTheta.x, def.sinCosTheta.y);

	[unroll] for (uint c=0; c<8; ++c)
		if (CheckAppend(c, type, def)) break;
}
