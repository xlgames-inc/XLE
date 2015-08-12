// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(BASIC2D_VSH)
#define BASIC2D_VSH

#include "Transform.h"

struct ViewFrustumInterpolator
{
	float3 oViewFrustumVector : VIEWFRUSTUMVECTOR;
};

ViewFrustumInterpolator BuildInterpolator_ViewFrustumInterpolator(uint vertexId)
{
	ViewFrustumInterpolator vfi;
	vfi.oViewFrustumVector = FrustumCorners[vertexId].xyz;
	return vfi;
}

void fullscreen(uint vertexId : SV_VertexID, out float4 oPosition : SV_Position, out float2 oTexCoord : TEXCOORD0)
{
	float2 coord = float2((float)(vertexId / 2), (float)(vertexId % 2));
	oTexCoord = coord;
	oPosition = float4(2.f * coord.x - 1.f, -2.f * coord.y + 1.f, 0.f, 1.f);
}

void fullscreen_viewfrustumvector(uint vertexId : SV_VertexID, out float4 oPosition : SV_Position, out float2 oTexCoord : TEXCOORD0, out ViewFrustumInterpolator vfi)
{
	float2 coord = float2((float)(vertexId / 2), (float)(vertexId % 2));
	oTexCoord = coord;
	vfi.oViewFrustumVector = FrustumCorners[vertexId].xyz;
	oPosition = float4(2.f * coord.x - 1.f, -2.f * coord.y + 1.f, 0.f, 1.f);
}

void fullscreen_viewfrustumvector_deep(uint vertexId : SV_VertexID, out float4 oPosition : SV_Position, out float2 oTexCoord : TEXCOORD0, out ViewFrustumInterpolator vfi)
{
	float2 coord = float2((float)(vertexId / 2), (float)(vertexId % 2));
	oTexCoord = coord;
	vfi.oViewFrustumVector = FrustumCorners[vertexId].xyz;
	oPosition = float4(2.f * coord.x - 1.f, -2.f * coord.y + 1.f, 1.f, 1.f);
}

void fullscreen_flip(uint vertexId : SV_VertexID, out float4 oPosition : SV_Position, out float2 oTexCoord : TEXCOORD0)
{
	vertexId ^= 1;		// xor bit 1 to flip Y coord
	float2 coord = float2((float)(vertexId / 2), (float)(vertexId % 2));
	oTexCoord = coord;
	oPosition = float4(2.f * coord.x - 1.f, -2.f * coord.y + 1.f, 0.f, 1.f);
}

void fullscreen_flip_viewfrustumvector(uint vertexId : SV_VertexID, out float4 oPosition : SV_Position, out float2 oTexCoord : TEXCOORD0, out ViewFrustumInterpolator vfi)
{
	vertexId ^= 1;		// xor bit 1 to flip Y coord
	float2 coord = float2((float)(vertexId / 2), (float)(vertexId % 2));
	oTexCoord = coord;
	vfi.oViewFrustumVector = FrustumCorners[vertexId].xyz;
	oPosition = float4(2.f * coord.x - 1.f, -2.f * coord.y + 1.f, 0.f, 1.f);
}



//////////////

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

float4 PixelCoordToSVPosition(float2 pixelCoord)
{
	return float4(	pixelCoord.x * ReciprocalViewportDimensions.x  *  2.f - 1.f,
					pixelCoord.y * ReciprocalViewportDimensions.y * -2.f + 1.f,
					0.f, 1.f);
}

float4 P2C(		float2 iPosition : POSITION0,
				float4 iColor	 : COLOR0,
				out float4 oColor : COLOR0 ) : SV_POSITION
{
	oColor	 = iColor;
	return PixelCoordToSVPosition(iPosition);
}

float4 P2CR(	float2 iPosition  : POSITION0,
				float4 iColor	  : COLOR0,
				float iRadius	  : RADIUS,
				out float4 oColor : COLOR0,
				out float oRadius : RADIUS ) : SV_POSITION
{
	oColor	 = iColor;
	oRadius  = iRadius;
	return PixelCoordToSVPosition(iPosition);
}

PSInput_Basic P2CT(	float2 iPosition : POSITION0,
				float4 iColor	 : COLOR0,
				float2 iTexCoord : TEXCOORD0 )
{
	PSInput_Basic output;
	output._position = PixelCoordToSVPosition(iPosition);
	output._color	 = iColor;
	output._texCoord = iTexCoord;
	return output;
}

void P2T(	float2 iPosition : POSITION0,
			float2 iTexCoord : TEXCOORD0,
			out float4 oPosition	: SV_Position,
			out float2 oTexCoord0	: TEXCOORD0 )
{
	oPosition = PixelCoordToSVPosition(iPosition);
	oTexCoord0 = iTexCoord;
}

void P2CTT(	float2 iPosition	: POSITION0,
			float4 iColor		: COLOR0,
			float2 iTexCoord0	: TEXCOORD0,
			float2 iTexCoord1	: TEXCOORD1,

			out float4 oPosition	: SV_Position,
			out float4 oColor		: COLOR0,
			out float2 oTexCoord0	: TEXCOORD0,
			out float2 oTexCoord1	: TEXCOORD1,
			nointerpolation out float2 oOutputDimensions : OUTPUTDIMENSIONS )
{
	oPosition	= PixelCoordToSVPosition(iPosition);
	oColor		= iColor;
	oTexCoord0	= iTexCoord0;
	oTexCoord1	= iTexCoord1;
	oOutputDimensions = 1.0f / ReciprocalViewportDimensions.xy;
}

#endif
