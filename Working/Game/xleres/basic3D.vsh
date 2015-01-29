// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(BASIC3D_VSH)
#define BASIC3D_VSH

#include "Transform.h"

struct PSInput_Basic
{
	float4 _position : SV_Position;
	float4 _color	 : COLOR0;
	float2 _texCoord : TEXCOORD0;
};

cbuffer ReciprocalViewportDimensions
{
	float2 ReciprocalViewportDimensions;
}

float4 TransformPosition(float3 localPosition)
{
	float3 worldPosition = localPosition; // mul(LocalToWorld, float4(localPosition,1)).xyz;
	return mul(WorldToClip, float4(worldPosition,1));
}

float4 PC(		float3 iPosition : POSITION0,
				float4 iColor	 : COLOR0,
				out float4 oColor : COLOR0 ) : SV_POSITION
{
	oColor	 = iColor;
	return TransformPosition(iPosition);
}

float4 PCR(	float3 iPosition  : POSITION0,
				float4 iColor	  : COLOR0,
				float iRadius	  : RADIUS,
				out float4 oColor : COLOR0,
				out float oRadius : RADIUS ) : SV_POSITION
{
	oColor	 = iColor;
	oRadius  = iRadius;
	return TransformPosition(iPosition);
}

PSInput_Basic PCT(	float3 iPosition : POSITION0,
				float4 iColor	 : COLOR0,
				float2 iTexCoord : TEXCOORD0 )
{
	PSInput_Basic output;
	output._position = TransformPosition(iPosition);
	output._color	 = iColor;
	output._texCoord = iTexCoord;
	return output;
}

void PT(	float3 iPosition : POSITION0,
			float2 iTexCoord : TEXCOORD0,
			out float4 oPosition	: SV_Position,
			out float2 oTexCoord0	: TEXCOORD0 )
{
	oPosition = TransformPosition(iPosition);
	oTexCoord0 = iTexCoord;
}

void PCTT(	float3 iPosition	: POSITION0,
			float4 iColor		: COLOR0,
			float2 iTexCoord0	: TEXCOORD0,
			float2 iTexCoord1	: TEXCOORD1,

			out float4 oPosition	: SV_Position,
			out float4 oColor		: COLOR0,
			out float2 oTexCoord0	: TEXCOORD0,
			out float2 oTexCoord1	: TEXCOORD1,
			nointerpolation out float2 oOutputDimensions : OUTPUTDIMENSIONS )
{
	oPosition	= TransformPosition(iPosition);
	oColor		= iColor;
	oTexCoord0	= iTexCoord0;
	oTexCoord1	= iTexCoord1;
	oOutputDimensions = 1.0f / ReciprocalViewportDimensions.xy;
}

#endif

