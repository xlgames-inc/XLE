// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(PREFIX_H)
#define PREFIX_H

	//
	//		Basic functions required for all shaders
	//		This file will be included as the first include
	//		in all shaders!
	//

float 	DefaultValue_float()	{ return 0.f; }
float2 	DefaultValue_float2() 	{ return 0.0.xx; }
float3 	DefaultValue_float3() 	{ return 0.0.xxx; }
float4 	DefaultValue_float4() 	{ return float4(0.0.xxx, 1.0f); }

uint 	DefaultValue_uint()		{ return 0; }
uint2 	DefaultValue_uint2() 	{ return uint2(0,0); }
uint3 	DefaultValue_uint3() 	{ return uint3(0,0,0); }
uint4 	DefaultValue_uint4() 	{ return uint4(0,0,0,1); }

int 	DefaultValue_int()		{ return 0; }
int2 	DefaultValue_int2() 	{ return int2(0,0); }
int3 	DefaultValue_int3() 	{ return int3(0,0,0); }
int4 	DefaultValue_int4() 	{ return int4(0,0,0,1); }

float4 	Cast_float_to_float4(float input)		{ return float4(input.xxx, 1); }
float4 	Cast_float2_to_float4(float2 input)		{ return float4(input.xy, 0, 1); }
float4 	Cast_float3_to_float4(float3 input)		{ return float4(input.xyz, 1); }

float3 	Cast_float_to_float3(float input)		{ return float3(input.xxx); }
float3 	Cast_float2_to_float3(float2 input)		{ return float3(input.xy, 0); }
float3 	Cast_float3_to_float3(float3 input)		{ return float3(input.xyz); }

float3 	Cast_float4_to_float3(float4 input)		{ return input.xyz; }
float4 	Cast_float3x3_to_float4(float3x3 input)		{ return float4(input[0].xyz, 1); }

struct DefaultOutput1T0 { float  target : SV_Target0; };
struct DefaultOutput2T0 { float2 target : SV_Target0; };
struct DefaultOutput3T0 { float3 target : SV_Target0; };
struct DefaultOutput4T0 { float4 target : SV_Target0; };

struct DefaultOutput1T1 { float  target : SV_Target1; };
struct DefaultOutput2T1 { float2 target : SV_Target1; };
struct DefaultOutput3T1 { float3 target : SV_Target1; };
struct DefaultOutput4T1 { float4 target : SV_Target1; };

struct DefaultOutput1T2 { float  target : SV_Target2; };
struct DefaultOutput2T2 { float2 target : SV_Target2; };
struct DefaultOutput3T2 { float3 target : SV_Target2; };
struct DefaultOutput4T2 { float4 target : SV_Target2; };

#include "../TextureAlgorithm.h"

#if SHADER_NODE_EDITOR==1
    const int2 SI_OutputDimensions;
	int2 NodeEditor_GetOutputDimensions() { return SI_OutputDimensions; }
#else
	int2 NodeEditor_GetOutputDimensions() { return int2(128, 128); }
#endif

float4 BackgroundPattern(float4 position)
{
	int2 c = int2(position.xy / 16.f);
	float4 colours[] = { float4(0.125f, 0.125f, 0.125f, 1.f), float4(0.025f, 0.025f, 0.025f, 1.f) };
	return colours[(c.x+c.y)%2];
}

float4 FilledGraphPattern(float4 position)
{
	int2 c = int2(position.xy / 8.f);
	float4 colours[] = { float4(0.35f, 0.35f, 0.35f, 1.f), float4(0.65f, 0.65f, 0.65f, 1.f) };
	return colours[(c.x+c.y)%2];
}


//////////////////////////////////////////////////////////////////
		// Graphs //

float NodeEditor_GraphEdgeFactor(float value)
{
	value = abs(value);
    float d = fwidth(value);
    return 1.f - smoothstep(0.0.xxx, 2.f * d, value);
}

float NodeEditor_IsGraphEdge(float functionResult, float comparisonValue)
{
	float distance = functionResult - comparisonValue;
	return NodeEditor_GraphEdgeFactor(distance);
}

float4 NodeEditor_GraphEdgeColour(int index)
{
	float4 colours[] = { float4(1, 0, 0, 1), float4(0, 1, 0, 1), float4(0, 0, 1, 1), float4(0, 1, 1, 1) };
	return colours[min(3, index)];
}

struct NodeEditor_GraphOutput
{
	float4 output : SV_Target0;
};

#endif
