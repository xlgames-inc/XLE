// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

ByteAddressBuffer	InstancePositions;
ByteAddressBuffer	InstanceTypes;

AppendStructuredBuffer<float4> OutputBuffer0;
#if INSTANCE_BIN_COUNT >= 2
	AppendStructuredBuffer<float4> OutputBuffer1;
#endif
#if INSTANCE_BIN_COUNT >= 3
	AppendStructuredBuffer<float4> OutputBuffer2;
#endif
#if INSTANCE_BIN_COUNT >= 4
	AppendStructuredBuffer<float4> OutputBuffer3;
#endif
#if INSTANCE_BIN_COUNT >= 5
	AppendStructuredBuffer<float4> OutputBuffer4;
#endif
#if INSTANCE_BIN_COUNT >= 6
	AppendStructuredBuffer<float4> OutputBuffer5;
#endif
#if INSTANCE_BIN_COUNT >= 7
	AppendStructuredBuffer<float4> OutputBuffer6;
#endif
#if INSTANCE_BIN_COUNT >= 8
	AppendStructuredBuffer<float4> OutputBuffer7;
#endif

[numthreads(256, 1, 1)]
	void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	uint params = InstanceTypes.Load(4*dispatchThreadId.x);
	uint type = params & 0xffff;
	uint sh0 = params >> 16;

	float4 position = float4(
		asfloat(InstancePositions.Load(dispatchThreadId.x*16+ 0)),
		asfloat(InstancePositions.Load(dispatchThreadId.x*16+ 4)),
		asfloat(InstancePositions.Load(dispatchThreadId.x*16+ 8)),
		sh0 / float(0xffff));

	if (type==1) { OutputBuffer0.Append(position); }
	#if INSTANCE_BIN_COUNT >= 2
		else if (type==2) { OutputBuffer1.Append(position); }
	#endif
	#if INSTANCE_BIN_COUNT >= 3
		else if (type==3) { OutputBuffer2.Append(position); }
	#endif
	#if INSTANCE_BIN_COUNT >= 4
		else if (type==4) { OutputBuffer3.Append(position); }
	#endif
	#if INSTANCE_BIN_COUNT >= 5
		else if (type==5) { OutputBuffer4.Append(position); }
	#endif
	#if INSTANCE_BIN_COUNT >= 6
		else if (type==6) { OutputBuffer5.Append(position); }
	#endif
	#if INSTANCE_BIN_COUNT >= 7
		else if (type==7) { OutputBuffer6.Append(position); }
	#endif
	#if INSTANCE_BIN_COUNT >= 8
		else if (type==8) { OutputBuffer7.Append(position); }
	#endif
}
