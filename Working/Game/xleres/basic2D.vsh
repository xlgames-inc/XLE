// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(BASIC2D_VSH)
#define BASIC2D_VSH

#include "Transform.h"

#define NDC_POSITIVE 1
#define NDC_POSITIVE_RIGHT_HANDED 2

#if VULKAN
	#define NDC NDC_POSITIVE_RIGHT_HANDED
#else
	#define NDC NDC_POSITIVE
#endif

struct ViewFrustumInterpolator
{
	float3 oViewFrustumVector : VIEWFRUSTUMVECTOR;
};

struct FullscreenCorner
{
	float2 coord;
	float4 position;
	float2 texCoord;
	ViewFrustumInterpolator vfi;
};

FullscreenCorner MakeFullscreenCorner(uint vertexId)
{
	FullscreenCorner result;

	result.coord = float2((float)(vertexId >> 1), (float)(vertexId & 1));
	#if NDC == NDC_POSITIVE_RIGHT_HANDED
		result.position = float4(2.f * result.coord.x - 1.f, 2.f * result.coord.y - 1.f, 0.f, 1.f);
	#else
		result.position = float4(2.f * result.coord.x - 1.f, -2.f * result.coord.y + 1.f, 0.f, 1.f);
	#endif
	result.texCoord = result.coord;
	result.vfi.oViewFrustumVector = FrustumCorners[vertexId].xyz;

	return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void fullscreen(uint vertexId : SV_VertexID, out float4 oPosition : SV_Position, out float2 oTexCoord : TEXCOORD0)
{
	FullscreenCorner corner = MakeFullscreenCorner(vertexId);
	oTexCoord = corner.texCoord;
	oPosition = corner.position;
}

void fullscreen_viewfrustumvector(uint vertexId : SV_VertexID, out float4 oPosition : SV_Position, out float2 oTexCoord : TEXCOORD0, out ViewFrustumInterpolator vfi)
{
	FullscreenCorner corner = MakeFullscreenCorner(vertexId);
	oTexCoord = corner.texCoord;
	vfi = corner.vfi;
	oPosition = corner.position;
}

void fullscreen_viewfrustumvector_deep(uint vertexId : SV_VertexID, out float4 oPosition : SV_Position, out float2 oTexCoord : TEXCOORD0, out ViewFrustumInterpolator vfi)
{
	FullscreenCorner corner = MakeFullscreenCorner(vertexId);
	oTexCoord = corner.texCoord;
	vfi = corner.vfi;
	oPosition = float4(corner.position.xy, 1.f, 1.f);
}

void fullscreen_flip(uint vertexId : SV_VertexID, out float4 oPosition : SV_Position, out float2 oTexCoord : TEXCOORD0)
{
	vertexId ^= 1;		// xor bit 1 to flip Y coord
	FullscreenCorner corner = MakeFullscreenCorner(vertexId);
	oTexCoord = corner.texCoord;
	oPosition = corner.position;
}

void fullscreen_flip_viewfrustumvector(uint vertexId : SV_VertexID, out float4 oPosition : SV_Position, out float2 oTexCoord : TEXCOORD0, out ViewFrustumInterpolator vfi)
{
	vertexId ^= 1;		// xor bit 1 to flip Y coord
	FullscreenCorner corner = MakeFullscreenCorner(vertexId);
	oTexCoord = corner.texCoord;
	vfi = corner.vfi;
	oPosition = corner.position;
}

cbuffer ScreenSpaceOutput
{
	float2 OutputMin, OutputMax;
	float2 InputMin, InputMax;
	float2 OutputDimensions;
}

void screenspacerect(
	uint vertexId : SV_VertexID,
	out float4 oPosition : SV_Position,
	out float2 oTexCoord0 : TEXCOORD0)
{
	FullscreenCorner corner = MakeFullscreenCorner(vertexId);
	oTexCoord0 = lerp(InputMin, InputMax, corner.texCoord);
	float2 coord = lerp(OutputMin, OutputMax, corner.coord);
	oPosition = float4(
		 2.f * coord.x / OutputDimensions.x - 1.f,
		-2.f * coord.y / OutputDimensions.y + 1.f,
		 0.f, 1.f);
}


//////////////

struct PSInput_Basic
{
	float4 _position : SV_Position;
	float4 _color	 : COLOR0;
	float2 _texCoord : TEXCOORD0;
};

cbuffer ReciprocalViewportDimensionsCB
{
	float2 ReciprocalViewportDimensions;
}

float4 PixelCoordToSVPosition(float2 pixelCoord)
{
	// This is a kind of viewport transform -- unfortunately it needs to
	// be customized for vulkan because of the different NDC space
#if NDC == NDC_POSITIVE_RIGHT_HANDED
	return float4(	pixelCoord.x * ReciprocalViewportDimensions.x *  2.f - 1.f,
					pixelCoord.y * ReciprocalViewportDimensions.y *  2.f - 1.f,
					0.f, 1.f);
#else
	return float4(	pixelCoord.x * ReciprocalViewportDimensions.x *  2.f - 1.f,
					pixelCoord.y * ReciprocalViewportDimensions.y * -2.f + 1.f,
					0.f, 1.f);
#endif
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

void P2CCTT(float2 iPosition	: POSITION0,
			float4 iColor0		: COLOR0,
			float4 iColor1		: COLOR1,
			float2 iTexCoord0	: TEXCOORD0,
			float2 iTexCoord1	: TEXCOORD1,

			out float4 oPosition	: SV_Position,
			out float4 oColor0		: COLOR0,
			out float2 oTexCoord0	: TEXCOORD0,
			out float4 oColor1		: COLOR1,		// note the flip in ordering here -- makes it easier when using a PCT pixel shader
			out float2 oTexCoord1	: TEXCOORD1,
			nointerpolation out float2 oOutputDimensions : OUTPUTDIMENSIONS )
{
	oPosition	= PixelCoordToSVPosition(iPosition);
	oColor0		= iColor0;
	oColor1		= iColor1;
	oTexCoord0	= iTexCoord0;
	oTexCoord1	= iTexCoord1;
	oOutputDimensions = 1.0f / ReciprocalViewportDimensions.xy;
}

#endif
