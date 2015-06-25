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

static const uint MaxInstanceBinCount = 16;

AppendStructuredBuffer<InstanceDef> OutputBuffer[8];

cbuffer SpawnParams : register(b1)
{
	uint BinThresholds[MaxInstanceBinCount];
	float DrawDistanceSq[MaxInstanceBinCount];
}

#define MapOutputBuffer(x) OUTPUT_BUFFER_MAP ## x

bool CheckAppend(uint index, uint mappedIndex, uint typeValue, InstanceDef def)
{
	[branch] if (typeValue <= BinThresholds[index]) {
		float3 viewOffset = def.posAndShadowing.xyz - WorldSpaceView;
		if (dot(viewOffset, viewOffset) < DrawDistanceSq[index])
			OutputBuffer[mappedIndex].Append(def);
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
		asfloat(InstancePositions.Load(dispatchThreadId.x*16+0)),
		asfloat(InstancePositions.Load(dispatchThreadId.x*16+4)),
		asfloat(InstancePositions.Load(dispatchThreadId.x*16+8)),
		sh0 / float(0xffff));

	float rotationAngle = asfloat(InstancePositions.Load(dispatchThreadId.x*16+12));
	sincos(rotationAngle, def.sinCosTheta.x, def.sinCosTheta.y);

	if (type == 0) return;	// blanked out parts should just be zero

	if ( 0 < INSTANCE_BIN_COUNT && CheckAppend( 0, MapOutputBuffer( 0), type, def)) return;
	if ( 1 < INSTANCE_BIN_COUNT && CheckAppend( 1, MapOutputBuffer( 1), type, def)) return;
	if ( 2 < INSTANCE_BIN_COUNT && CheckAppend( 2, MapOutputBuffer( 2), type, def)) return;
	if ( 3 < INSTANCE_BIN_COUNT && CheckAppend( 3, MapOutputBuffer( 3), type, def)) return;
	if ( 4 < INSTANCE_BIN_COUNT && CheckAppend( 4, MapOutputBuffer( 4), type, def)) return;
	if ( 5 < INSTANCE_BIN_COUNT && CheckAppend( 5, MapOutputBuffer( 5), type, def)) return;
	if ( 6 < INSTANCE_BIN_COUNT && CheckAppend( 6, MapOutputBuffer( 6), type, def)) return;
	if ( 7 < INSTANCE_BIN_COUNT && CheckAppend( 7, MapOutputBuffer( 7), type, def)) return;
	if ( 8 < INSTANCE_BIN_COUNT && CheckAppend( 8, MapOutputBuffer( 8), type, def)) return;
	if ( 9 < INSTANCE_BIN_COUNT && CheckAppend( 9, MapOutputBuffer( 9), type, def)) return;
	if (10 < INSTANCE_BIN_COUNT && CheckAppend(10, MapOutputBuffer(10), type, def)) return;
	if (11 < INSTANCE_BIN_COUNT && CheckAppend(11, MapOutputBuffer(11), type, def)) return;
	if (12 < INSTANCE_BIN_COUNT && CheckAppend(12, MapOutputBuffer(12), type, def)) return;
	if (13 < INSTANCE_BIN_COUNT && CheckAppend(13, MapOutputBuffer(13), type, def)) return;
	if (14 < INSTANCE_BIN_COUNT && CheckAppend(14, MapOutputBuffer(14), type, def)) return;
	if (15 < INSTANCE_BIN_COUNT && CheckAppend(15, MapOutputBuffer(15), type, def)) return;


}
