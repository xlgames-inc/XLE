// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define OUTPUT_TEXCOORD 1
#define VSOUTPUT_EXTRA float2 dhdxy : DHDXY;

#define GBUFFER_TYPE 1	// hack -- (not being set by the client code currently)

//
//	This is the basic LOD behaviour for terrain in XLE
//		-- note that it's still in a very initial stage, and it's a bit experimental!
//

#include "TerrainGenerator.h"
#include "../Transform.h"
#include "../MainGeometry.h"
#include "../CommonResources.h"
#include "../Utility/perlinnoise.h"
#include "../gbuffer.h"
#include "../Colour.h"

static const uint OverlapTexels = 2;

Texture2DArray<uint> HeightsTileSet : register(t0);

float3 AddNoise(float3 worldPosition)
{
	#if (DO_ADD_NOISE==1)
		worldPosition.z += 33.f * fbmNoise2D(worldPosition.xy, 75.f, .5f, 2.1042, 6);
	#endif
	return worldPosition;
}

VSOutput main(uint vertexIndex : SV_VertexId)
{
	VSOutput output;

	int x = vertexIndex % TileDimensionsInVertices;
	int y = vertexIndex / TileDimensionsInVertices;
	uint rawHeightValue = HeightsTileSet.Load(
		int4(HeightMapOrigin.xy + int2(x,y), HeightMapOrigin.z, 0));

	float3 localPosition;
	localPosition.x		 = float(x) / float(TileDimensionsInVertices-1);
	localPosition.y		 = float(y) / float(TileDimensionsInVertices-1);
	localPosition.z		 = float(rawHeightValue);

	float3 cellPosition	 = mul( LocalToCell, float4(localPosition, 1)).xyz;
	float3 worldPosition = mul(LocalToWorld, float4( cellPosition,1));
	worldPosition = AddNoise(worldPosition);
	output.position		 = mul( WorldToClip, float4(worldPosition,1));

	#if (OUTPUT_WORLD_POSITION==1)
		output.worldPosition = worldPosition;
	#endif

	return output;
}

///////////////////////////////////////////////////////////////////////////

struct PatchInputControlPoint
{
	float3 worldPosition : POSITION;
};

PatchInputControlPoint vs_dyntess_main(uint vertexIndex : SV_VertexId)
{
	int x = vertexIndex % 2;
	int y = vertexIndex / 2;
	uint rawHeightValue = HeightsTileSet.Load(
		int4(HeightMapOrigin.xy + int2(x,y) * (TileDimensionsInVertices-OverlapTexels), HeightMapOrigin.z, 0));

	float3 localPosition;
	localPosition.x		 = float(x);
	localPosition.y		 = float(y);
	localPosition.z		 = float(rawHeightValue);

	float3 cellPosition	 = mul( LocalToCell, float4(localPosition, 1)).xyz;
	float3 worldPosition = mul(LocalToWorld, float4( cellPosition, 1));
	worldPosition = AddNoise(worldPosition);

	PatchInputControlPoint output;
	output.worldPosition = worldPosition;
	return output;
}

///////////////////////////////////////////////////////////////////////////

struct HS_ConstantOutput
{
    float Edges[4]        : SV_TessFactor;
    float Inside[2]       : SV_InsideTessFactor;
};

#define ControlPointCount		4

static const int InterpolationQuality = (DO_EXTRA_SMOOTHING==1)?2:1;
#define MaxTessellation 32
#define MinTessellation 4

float CalculateScreenSpaceEdgeLength(float3 e0, float3 e1)
{
	float4 p0 = mul(WorldToClip, float4(e0, 1));
	float4 p1 = mul(WorldToClip, float4(e1, 1));

	float2 viewportDims = float2(1280.f, 720.f);
	float2 s0 = (p0.xy / p0.w) * .5f * viewportDims.xy;
	float2 s1 = (p1.xy / p1.w) * .5f * viewportDims.xy;
	return length(s0 - s1);
}

float CalculateDoubleScreenSpaceEdgeLength(float3 e0, float3 e1)
{
	return CalculateScreenSpaceEdgeLength(e0, lerp(e0, e1, 2.f));
	// return CalculateScreenSpaceEdgeLength(lerp(e1, e0, 2.f), e1);
}

uint RemapEdgeIndex(uint hsEdgeIndex)
{
	if (hsEdgeIndex == 0) { return 3; }
	if (hsEdgeIndex == 1) { return 0; }
	if (hsEdgeIndex == 2) { return 1; }
	return 2;
}

	//
	//		PatchConstantFunction
	//			-- this is run once per patch. It calculates values that are constant
	//				over the entire patch
	//
HS_ConstantOutput PatchConstantFunction(
    InputPatch<PatchInputControlPoint, ControlPointCount> ip,
    uint PatchID : SV_PrimitiveID)
{
    HS_ConstantOutput output;

	float2 halfViewport = float2(512, 400);
	const float edgeThreshold = 384.f;
	float mult = MaxTessellation / edgeThreshold;

	float2 screenPts[4];
	for (uint c2=0; c2<4; ++c2) {
		float4 clip = mul(WorldToClip, float4(ip[c2].worldPosition, 1));
		screenPts[c2] = clip.xy / clip.w * halfViewport;
	}

		// Edges:
		//  0: u == 0 (pt0 -> pt2)
		//	1: v == 0 (pt0 -> pt1)
		//	2: u == 1 (pt3 -> pt1)
		//	3: v == 1 (pt3 -> pt2)
	uint edgeStartPts[4]	= { 0, 0, 3, 3 };
	uint edgeEndPts[4]		= { 2, 1, 1, 2 };

	for (uint c=0; c<4; ++c) {
			//	Here, we calculate the amount of tessellation for the terrain edge
			//	This is the most important algorithm for terrain.
			//
			//	The current method is just a simple solution. Most of time we might
			//	need something more sophisticated.
			//
			//	In particular, we want to try to detect edges that are most likely
			//	to make up the siloette of details. Often terrain has smooth areas
			//	that don't need a lot of detail... But another area might have rocky
			//	detail with sharp edges -- that type of geometry needs much more detail.
			//
			//	Note that this method is currently producing the wrong results for
			//	tiles that straddle the near clip plane! This can make geometry near
			//	the camera swim around a bit.
		float2 startS	= screenPts[edgeStartPts[c]];
		float2 endS		= screenPts[edgeEndPts[c]];

			//	The "extra-smoothing" boosts the maximum tessellation to twice it's
			//	normal value, and enables higher quality interpolation. This way
			//	distant geometry should be the same quality, but we can add extra
			//	vertices in near geometry when we need it.
		float screenSpaceLength = length(startS - endS);
		output.Edges[c] = clamp(
			screenSpaceLength * mult,
			MinTessellation, (DO_EXTRA_SMOOTHING==1)?(2*MaxTessellation):(MaxTessellation));

			// On the LOD interface boundaries, we need to lock the tessellation
			// amounts to something predictable
		const float lodBoundaryTess = MaxTessellation;
		if (NeighbourLodDiffs[RemapEdgeIndex(c)] > 0) {
			output.Edges[c] = lodBoundaryTess;
		} else if (NeighbourLodDiffs[RemapEdgeIndex(c)] < 0) {
			output.Edges[c] = lodBoundaryTess/2;
		}
	}

		//	Could use min, max or average edge
		//	Note that when there are large variations between edge tessellation and
		//	inside tessellation, it can cause some wierd artefacts. We need to be
		//	careful about that.
	// output.Inside[0] = min(output.Edges[1], output.Edges[3]);	// v==0 && v==1 edges
	// output.Inside[1] = min(output.Edges[0], output.Edges[2]);	// u==0 && u==1 edges

	output.Inside[0] = lerp(output.Edges[1], output.Edges[3], 0.5f);	// v==0 && v==1 edges
	output.Inside[1] = lerp(output.Edges[0], output.Edges[2], 0.5f);	// u==0 && u==1 edges

	return output;
}

///////////////////////////////////////////////////////////////////////////

struct PatchOutputControlPoint
{
	float3 worldPosition : POSITION;
};

[domain("quad")]
[partitioning("fractional_even")]
[outputtopology("triangle_cw")]
[patchconstantfunc("PatchConstantFunction")]
[outputcontrolpoints(4)]
[maxtessfactor(MaxTessellation)]
PatchOutputControlPoint hs_main(
    InputPatch<PatchInputControlPoint, ControlPointCount> ip,
    uint i : SV_OutputControlPointID,
    uint PatchID : SV_PrimitiveID )
{
		//	DirectX11 samples suggest that just copying the control points
		//	will activate a "fast pass through mode"
    PatchOutputControlPoint output;
	output.worldPosition = ip[i].worldPosition;
    return output;
}

///////////////////////////////////////////////////////////////////////////

int2  T0(int2 input) { return int2(max(input.x-1, HeightMapOrigin.x),								max(input.y-1, HeightMapOrigin.y)); }
int2  T1(int2 input) { return int2(    input.x+0,													max(input.y-1, HeightMapOrigin.y)); }
int2  T2(int2 input) { return int2(    input.x+1,													max(input.y-1, HeightMapOrigin.y)); }
int2  T3(int2 input) { return int2(min(input.x+2, HeightMapOrigin.x+TileDimensionsInVertices-1),	max(input.y-1, HeightMapOrigin.y)); }

int2  T4(int2 input) { return int2(max(input.x-1, HeightMapOrigin.x),								input.y); }
int2  T5(int2 input) { return int2(min(input.x+2, HeightMapOrigin.x+TileDimensionsInVertices-1),	input.y); }

int2  T6(int2 input) { return int2(max(input.x-1, HeightMapOrigin.x),								input.y+1); }
int2  T7(int2 input) { return int2(min(input.x+2, HeightMapOrigin.x+TileDimensionsInVertices-1),	input.y+1); }

int2  T8(int2 input) { return int2(max(input.x-1, HeightMapOrigin.x),								min(input.y+2, HeightMapOrigin.y+TileDimensionsInVertices-1)); }
int2  T9(int2 input) { return int2(    input.x+0,													min(input.y+2, HeightMapOrigin.y+TileDimensionsInVertices-1)); }
int2 T10(int2 input) { return int2(    input.x+1,													min(input.y+2, HeightMapOrigin.y+TileDimensionsInVertices-1)); }
int2 T11(int2 input) { return int2(min(input.x+2, HeightMapOrigin.x+TileDimensionsInVertices-1),	min(input.y+2, HeightMapOrigin.y+TileDimensionsInVertices-1)); }

float EvaluateCubicCurve(float pm0, float p0, float p1, float p2, float t)
{
		//	evaluate a basic catmul rom curve through the given points
	float t2 = t*t;
	float t3 = t2*t;

	float m0 = .5f * (p1 - pm0);		// catmull rom tangent values
	float m1 = .5f * (p2 - p0);
	return
		  p0 * (1 - 3.f * t2 + 2.f * t3)
		+ p1 * (3.f * t2 - 2.f * t3)
		+ m0 * (t - 2.f * t2 + t3)
		+ m1 * (-t2 + t3);
}

float CustomSample(float2 UV, int interpolationQuality)
{
		// todo -- consider doing height interpolation in world space (rather than in
		//			0 - 65535 height map space). This may result in more accurate floating
		//			point numbers.

		//	Note that this high quality interpolation is only really useful when we
		//	tessellation to higher levels than the input texture (ie, if the input
		//	texture is 32x32, but we want to tessellate up to 64x64). So maybe we
		//	can disable it for lower levels of tessellation;

	if (interpolationQuality==1) {

			//	Do our own custom bilinear interpolation across the heights texture
			//	Minimum quality for patches actively changing LOD.

		float2 texelCoords = HeightMapOrigin.xy + UV * float(TileDimensionsInVertices-OverlapTexels);
		int2 minTexelCorner = int2(texelCoords); // round down
		float2 filter = texelCoords - minTexelCorner;

		int A = HeightsTileSet.Load(int4(minTexelCorner + int2(0,0), HeightMapOrigin.z, 0));
		int B = HeightsTileSet.Load(int4(minTexelCorner + int2(1,0), HeightMapOrigin.z, 0));
		int C = HeightsTileSet.Load(int4(minTexelCorner + int2(0,1), HeightMapOrigin.z, 0));
		int D = HeightsTileSet.Load(int4(minTexelCorner + int2(1,1), HeightMapOrigin.z, 0));

		float w0 = (1.0f - filter.x) * (1.0f - filter.y);
		float w1 = (       filter.x) * (1.0f - filter.y);
		float w2 = (1.0f - filter.x) * (       filter.y);
		float w3 = (       filter.x) * (       filter.y);

		return	  float(A) * w0
				+ float(B) * w1
				+ float(C) * w2
				+ float(D) * w3
				;

	} else if (interpolationQuality==2) {

			//	Do bicubic interpolation, to pick up implied curves between sample points.
			//	We can improve the performance by storing tangents at height map point.

		float2 texelCoords = HeightMapOrigin.xy + UV * float(TileDimensionsInVertices-OverlapTexels);
		int2 minTexelCorner = int2(texelCoords); // round down
		float2 filter = texelCoords - minTexelCorner;

			//	Let's try this method:
			//		build 4 horizontal catmull-rom curves, and evaluate these at the
			//		UV.x location.
			//		that defines 4 control points
			//		- make a new vertical curve along those control points
			//		evaluate that curve at the UV.y position

		float A   = (float)HeightsTileSet.Load(int4(minTexelCorner + int2(0,0), HeightMapOrigin.z, 0));
		float B   = (float)HeightsTileSet.Load(int4(minTexelCorner + int2(1,0), HeightMapOrigin.z, 0));
		float C   = (float)HeightsTileSet.Load(int4(minTexelCorner + int2(0,1), HeightMapOrigin.z, 0));
		float D   = (float)HeightsTileSet.Load(int4(minTexelCorner + int2(1,1), HeightMapOrigin.z, 0));

		float t0  = (float)HeightsTileSet.Load(int4( T0(minTexelCorner), HeightMapOrigin.z, 0));
		float t1  = (float)HeightsTileSet.Load(int4( T1(minTexelCorner), HeightMapOrigin.z, 0));
		float t2  = (float)HeightsTileSet.Load(int4( T2(minTexelCorner), HeightMapOrigin.z, 0));
		float t3  = (float)HeightsTileSet.Load(int4( T3(minTexelCorner), HeightMapOrigin.z, 0));
		float t4  = (float)HeightsTileSet.Load(int4( T4(minTexelCorner), HeightMapOrigin.z, 0));
		float t5  = (float)HeightsTileSet.Load(int4( T5(minTexelCorner), HeightMapOrigin.z, 0));
		float t6  = (float)HeightsTileSet.Load(int4( T6(minTexelCorner), HeightMapOrigin.z, 0));
		float t7  = (float)HeightsTileSet.Load(int4( T7(minTexelCorner), HeightMapOrigin.z, 0));
		float t8  = (float)HeightsTileSet.Load(int4( T8(minTexelCorner), HeightMapOrigin.z, 0));
		float t9  = (float)HeightsTileSet.Load(int4( T9(minTexelCorner), HeightMapOrigin.z, 0));
		float t10 = (float)HeightsTileSet.Load(int4(T10(minTexelCorner), HeightMapOrigin.z, 0));
		float t11 = (float)HeightsTileSet.Load(int4(T11(minTexelCorner), HeightMapOrigin.z, 0));

		float q0  = EvaluateCubicCurve(t0, t1, t2, t3, filter.x);
		float q1  = EvaluateCubicCurve(t4,  A,  B, t5, filter.x);
		float q2  = EvaluateCubicCurve(t6,  C,  D, t7, filter.x);
		float q3  = EvaluateCubicCurve(t8, t9,t10,t11, filter.x);

		return EvaluateCubicCurve(q0, q1, q2, q3, filter.y);

	} else {

			//	Just do point sampling. This is not really accurate enough when the tessellation is
			//	changing -- points will jump from height to height and create wierd wrinkles.
			//	It should be ok for patches that are fixed at the lowest LOD, however.

		float2 texelCoords = HeightMapOrigin.xy + UV * float(TileDimensionsInVertices-OverlapTexels);
		int2 minTexelCorner = int2(floor(texelCoords)); // round down
		return (float)HeightsTileSet.Load(int4(minTexelCorner + int2(0,0), HeightMapOrigin.z, 0));

	}
}

///////////////////////////////////////////////////////////////////////////

[domain("quad")]
	VSOutput ds_main(	HS_ConstantOutput input, float2 UV : SV_DomainLocation,
						const OutputPatch<PatchInputControlPoint, ControlPointCount> inputPatch)
{
		//	After the hardware tessellator has run, let's
		//	calculate the positions of the final points. That means finding the
		//	correct location on the patch surface, and reading the height values from
		//	the texture. Let's just go back to patch local coords again.

	float rawHeightValue = CustomSample(UV.xy, InterpolationQuality);

		// quick hack to get normal values for the terrain
		//		-- find height values from the source height map
		//			and extract dhdx and dhdy from that
		//		Inside CustomSample, there are many extra interpolation
		//		steps -- that makes it a little inefficient
		//
		//		Note that this is not very accurate around the tile edges.
		//		we need an extra row & column of height values to correctly
		//		calculate the normal values for the edges. This is also needed
		//		to make cubic interpolation more accurate, btw!
	float A = 1.0f/(TileDimensionsInVertices);
	float heightDX = CustomSample(float2(UV.x + A, UV.y), InterpolationQuality);
	float heightDY = CustomSample(float2(UV.x, UV.y + A), InterpolationQuality);

		//	heightDX is the interpolated height change over the distance of a single height map element.
		//	We really want to convert this to world coordinates.
		//		we can simplify this by making assumptions about LocalToCell and LocalToWorld...
		//		let's assume that they both contain scale and translation, but no rotation or skew
		//		This is most likely the case (also they probably contain uniform scale)
	float conversionToWorldUnitsX = 1.0f/(TileDimensionsInVertices-OverlapTexels) * LocalToCell[0][0] * LocalToWorld[0][0];
	float conversionToWorldUnitsY = 1.0f/(TileDimensionsInVertices-OverlapTexels) * LocalToCell[1][1] * LocalToWorld[1][1];
	float conversionToWorldUnitsZ = LocalToCell[2][2] * LocalToWorld[2][2];
	float dhdx = (heightDX - rawHeightValue) * conversionToWorldUnitsZ / conversionToWorldUnitsX;
	float dhdy = (heightDY - rawHeightValue) * conversionToWorldUnitsZ / conversionToWorldUnitsY;

	float3 localPosition;
	localPosition.x		 = UV.x;
	localPosition.y		 = UV.y;
	localPosition.z		 = float(rawHeightValue);

	float3 cellPosition	 = mul( LocalToCell, float4(localPosition, 1)).xyz;
	float3 worldPosition = mul(LocalToWorld, float4( cellPosition, 1)).xyz;
	worldPosition = AddNoise(worldPosition);

	const bool showRawTessellationPatch = false;
	if (showRawTessellationPatch) {
		float u0w = (1.f - UV.x) * (1.f - UV.y);
		float u1w = (      UV.x) * (1.f - UV.y);
		float u2w = (1.f - UV.x) * (      UV.y);
		float u3w = (      UV.x) * (      UV.y);

		worldPosition =
			  u0w * inputPatch[0].worldPosition
			+ u1w * inputPatch[1].worldPosition
			+ u2w * inputPatch[2].worldPosition
			+ u3w * inputPatch[3].worldPosition
			;
	}

	float4 clipPosition  = mul( WorldToClip, float4(worldPosition, 1));

	VSOutput output;
	output.position = clipPosition;
	output.texCoord = UV.xy;
	#if (OUTPUT_WORLD_POSITION==1)
		output.worldPosition = worldPosition;
	#endif
		// output height derivatives from domain shader (instead of normals
		//		-- because they will go through the linear interpolators
		//		much better than normals)
	output.dhdxy = float2(dhdx, dhdy);
	return output;
}

///////////////////////////////////////////////////////////////////////////

struct SW_GStoPS
{
	float4 position				 : SV_Position;
	float3 barycentricCoords	 : BARYCENTRIC;
	#if SOLIDWIREFRAME_TEXCOORD==1
		float2 texCoord : TEXCOORD0;
		float2 dhdxy : DHDXY;
	#endif
	#if SOLIDWIREFRAME_WORLDPOSITION==1
		float3 worldPosition : WORLDPOSITION;
	#endif
};

float edgeFactor( float3 barycentricCoords )
{
    float3 d = fwidth(barycentricCoords);
    float3 a3 = smoothstep(0.0.xxx, d * 1.5, barycentricCoords);
    return min(min(a3.x, a3.y), a3.z);
}

float edgeFactor2( float2 coords, float width )
{
    float2 d = fwidth(coords);
    float2 a3 = smoothstep(0.0.xx, d*width, coords);
    return min(a3.x, a3.y);
}

	//	Big stack of terrain texture -- including normal maps and specularity textures at each level
	//	For each strata:
	//		0: texture 0
	//		1: texture 1
	//		2: slopes
	//
	//	Each has entries in the diffuse albedo, normals and specularity textures
	//	We separate diffuse, normals and specularity into differnt arrays so they can have different
	//	pixel formats and dimensions. But this storage mode does mean that every texture of the same
	//	type must agree on both pixel format and dimensions. We can get around that by converting the
	//	array into some kind of large texture atlas (like a mega-texture type thing). That would also
	//	work better for streaming mip maps.
Texture2DArray StrataDiffuse		: register( t8);
Texture2DArray StrataNormals		: register( t9);
Texture2DArray StrataSpecularity	: register(t10);

struct ProceduralTextureOutput
{
	float3 diffuseAlbedo;
	float3 tangentSpaceNormal;
	float specularity;
};

#if STRATA_COUNT==0
	ProceduralTextureOutput GetTextureForStrata(uint strataIndex, float3 worldPosition, float slopeFactor, float2 textureCoord, float noiseValue0)
	{
		ProceduralTextureOutput result;
		result.diffuseAlbedo = 1.0.xxx;
		result.tangentSpaceNormal = 0.0.xxx;
		result.specularity = 0.0.xxx;
		return result;
	}
#else
	static const uint StrataCount = STRATA_COUNT;

	cbuffer TexturingParameters : register(b5)
	{
		float4 StrataEndHeights[StrataCount];
		float4 TextureFrequency[StrataCount];
	}

	ProceduralTextureOutput GetTextureForStrata(uint strataIndex, float3 worldPosition, float slopeFactor, float2 textureCoord, float noiseValue0)
	{
		float2 tc0 = worldPosition.xy * TextureFrequency[strataIndex].xx;
		float2 tc1 = worldPosition.xy * TextureFrequency[strataIndex].yy;

		float3 A = StrataDiffuse.Sample(MaybeAnisotropicSampler, float3(tc0, strataIndex*3+0)).rgb;
		float3 An = StrataNormals.Sample(MaybeAnisotropicSampler, float3(tc0, strataIndex*3+0)).rgb;
		float As = SRGBLuminance(StrataSpecularity.Sample(MaybeAnisotropicSampler, float3(tc0, strataIndex*3+0)).rgb);

		float3 B = StrataDiffuse.Sample(MaybeAnisotropicSampler, float3(tc1, strataIndex*3+1)).rgb;
		float3 Bn = StrataNormals.Sample(MaybeAnisotropicSampler, float3(tc1, strataIndex*3+1)).rgb;
		float Bs = SRGBLuminance(StrataSpecularity.Sample(MaybeAnisotropicSampler, float3(tc1, strataIndex*3+1)).rgb);

		float alpha = saturate(.5f + .7f * noiseValue0);
		// alpha = min(1.f, exp(32.f * (alpha-.5f)));
		// alpha = lerp(.25f, 0.75f, alpha);

		float3 X = lerp(A, B, alpha);
		float3 Xn = lerp(An, Bn, alpha);
		float Xs = lerp(As, Bs, alpha);

		ProceduralTextureOutput result;
		result.diffuseAlbedo = X;
		result.tangentSpaceNormal = Xn;
		result.specularity = Xs;

		const float slopeStart = .35f;
		const float slopeSoftness = 3.f;
		const float slopeDarkness = .75f;

		float slopeAlpha = pow(min(1.f,slopeFactor/slopeStart), slopeSoftness);
		if (slopeAlpha > 0.05f) {	// can't branch here because of the texture lookups below... We would need to do 2 passes

				//		slope texture coordinates should be based on worldPosition x or y,
				//		depending on which is changing most quickly in screen space
				//		This is causing some artefacts!
			float2 tcS0 = worldPosition.xz * TextureFrequency[strataIndex].zz;
			// if (fwidth(worldPosition.y) > fwidth(worldPosition.x))

				// soft darkening based on slope give a curiously effective approximation of ambient occlusion
			float arrayIdx = strataIndex*3+2;
			float3 S = StrataDiffuse.Sample(MaybeAnisotropicSampler, float3(tcS0, arrayIdx)).rgb;
			float3 Sn = StrataNormals.Sample(MaybeAnisotropicSampler, float3(tcS0, arrayIdx)).rgb;
			float3 Ss = SRGBLuminance(StrataSpecularity.Sample(MaybeAnisotropicSampler, float3(tcS0, arrayIdx)).rgb);

			tcS0.x = worldPosition.y * TextureFrequency[strataIndex].z;
			float3 S2 = StrataDiffuse.Sample(MaybeAnisotropicSampler, float3(tcS0, arrayIdx)).rgb;
			float3 Sn2 = StrataNormals.Sample(MaybeAnisotropicSampler, float3(tcS0, arrayIdx)).rgb;
			float3 Ss2 = SRGBLuminance(StrataSpecularity.Sample(MaybeAnisotropicSampler, float3(tcS0, arrayIdx)).rgb);

			result.diffuseAlbedo = lerp(result.diffuseAlbedo, slopeDarkness * .5f * (S+S2), slopeAlpha);
			result.tangentSpaceNormal = lerp(result.tangentSpaceNormal, .5f * (Sn + Sn2), slopeAlpha);
			result.specularity = lerp(result.specularity, .5f * (Ss + Ss2), slopeAlpha);
		}

		return result;
	}
#endif

ProceduralTextureOutput BuildProceduralTextureValue(float3 worldPosition, float slopeFactor, float2 textureCoord)
{
		//	Build a texturing value by blending together multiple input textures
		//	Select the input textures from some of the input parameters, like slope and height

		//	This mostly just a place-holder!
		//	The noise calculation is too expensive, and there is very little configurability!
		//	But, I need a terrain that looks roughly terrain-like just to get things working

	float noiseValue0 = fbmNoise2D(worldPosition.xy, 225.f, .65f, 2.1042, 4);
	noiseValue0 += .5f * fbmNoise2D(worldPosition.xy, 33.7f, .75f, 2.1042, 4);
	noiseValue0 = clamp(noiseValue0, -1.f, 1.f);

	float strataAlpha = 0.f;
	#if STRATA_COUNT!=0
		float worldSpaceHeight = worldPosition.z - 25.f * noiseValue0; //  - 18.23f * noiseValue1;
		uint strataBase0 = StrataCount-1, strataBase1 = StrataCount-1;
		[unroll] for (uint c=0; c<(StrataCount-1); ++c) {
			if (worldSpaceHeight < StrataEndHeights[c+1].x) {
				strataBase0 = c;
				strataBase1 = c+1;
				strataAlpha = (worldSpaceHeight - StrataEndHeights[c].x) / (StrataEndHeights[c+1].x - StrataEndHeights[c].x);
				break;
			}
		}
	#else
		uint strataBase0 = 0, strataBase1 = 0;
	#endif

	strataAlpha = exp(16.f * (strataAlpha-1.f));	// only blend in the last little bit

	ProceduralTextureOutput value0 = GetTextureForStrata(strataBase0, worldPosition, slopeFactor, textureCoord, noiseValue0);
	ProceduralTextureOutput value1 = GetTextureForStrata(strataBase1, worldPosition, slopeFactor, textureCoord, noiseValue0);

	ProceduralTextureOutput result;
	result.diffuseAlbedo = lerp(value0.diffuseAlbedo, value1.diffuseAlbedo, strataAlpha);
	result.tangentSpaceNormal = lerp(value0.tangentSpaceNormal, value1.tangentSpaceNormal, strataAlpha);
	result.specularity = lerp(value0.specularity, value1.specularity, strataAlpha);

	result.diffuseAlbedo = pow(result.diffuseAlbedo, 2.2);

	float2 nxy = 2.f * result.tangentSpaceNormal.xy - 1.0.xx;
	nxy *= 0.5f;	// reduce normal map slightly -- make things slightly softer and cleaner
	result.tangentSpaceNormal = float3(nxy, sqrt(saturate(1.f + dot(nxy, -nxy))));

	return result;
}

// struct PSOutput { float4 diffuse : SV_Target0; float4 normal : SV_target1; };

struct TerrainPixel
{
	float3 diffuseAlbedo;
	float3 worldSpaceNormal;
	float specularity;
	float cookedAmbientOcclusion;
};

uint GetEdgeIndex(float2 texCoord)
{
	float2 t = 2.f * texCoord - 1.0.xx;
	if (abs(t.x) > abs(t.y))	{ return (t.x < 0.f) ? 3 : 1; }
	else						{ return (t.y < 0.f) ? 0 : 2; }
}

TerrainPixel CalculateTerrain(SW_GStoPS geo)
{
	float4 result = 1.0.xxxx;
	float2 finalTexCoord = 0.0.xx;
	float shadowing = 0.f;
	#if (OUTPUT_TEXCOORD==1) && (SOLIDWIREFRAME_TEXCOORD==1)
			// todo -- we need special interpolation to avoid wrapping into neighbour coverage tiles
		finalTexCoord = lerp(TexCoordMins.xy, TexCoordMaxs.xy, geo.texCoord.xy);
		// result.rgb = MassageTerrainBaseColour(CoverageTileSet.Sample(MaybeAnisotropicSampler, float3(finalTexCoord.xy, CoverageOrigin.z)).rgb);

		float2 shadowSample = CoverageTileSet.SampleLevel(DefaultSampler, float3(finalTexCoord.xy, CoverageOrigin.z), 0).rg;
		shadowing = saturate(ShadowSoftness * (SunAngle + shadowSample.r)) * saturate(ShadowSoftness * (shadowSample.g - SunAngle));
	#endif

	float slopeFactor = max(abs(geo.dhdxy.x), abs(geo.dhdxy.y));
	float3 worldPosition = 0.0.xxx;
	#if SOLIDWIREFRAME_WORLDPOSITION==1
		worldPosition = geo.worldPosition;
	#endif
	ProceduralTextureOutput procTexture = BuildProceduralTextureValue(worldPosition, slopeFactor, finalTexCoord);
	result.rgb = procTexture.diffuseAlbedo.rgb;

	#if DRAW_WIREFRAME==1
		float patchEdge = 1.0f - edgeFactor2(frac(geo.texCoord), 5.f).xxx;
		uint edgeIndex = GetEdgeIndex(geo.texCoord);
		float3 lineColour = lerp(1.0.xxx, (NeighbourLodDiffs[edgeIndex]==0?float3(0,1,0):float3(1,0,0)), patchEdge);
		result.rgb = lerp(lineColour, result.rgb, edgeFactor(geo.barycentricCoords));
	#endif

		//	calculate the normal from the input derivatives
		//	because of the nature of terrain, we can make
		//	some simplifying assumptions.
	float3 uaxis = float3(1.0f, 0.f, geo.dhdxy.x);
	float3 vaxis = float3(0.0f, 1.f, geo.dhdxy.y);
	float3 normal = normalize(cross(uaxis, vaxis));	// because of all of the constant values, this cross product should be simplified in the optimiser

	float3 deformedNormal = normalize(
		  procTexture.tangentSpaceNormal.x * uaxis
		+ procTexture.tangentSpaceNormal.y * vaxis
		+ procTexture.tangentSpaceNormal.z * normal);

	float emulatedAmbientOcclusion = lerp(0.5f, 1.f, SRGBLuminance(result.rgb));

	TerrainPixel output;
	output.diffuseAlbedo = result;
	output.worldSpaceNormal = deformedNormal;
	output.specularity = .125f * procTexture.specularity;
	output.cookedAmbientOcclusion = shadowing * emulatedAmbientOcclusion;
	return output;
}

[earlydepthstencil]
GBufferEncoded ps_main(SW_GStoPS geo)
{
	TerrainPixel p = CalculateTerrain(geo);
	GBufferValues output = GBufferValues_Default();
	output.diffuseAlbedo = p.diffuseAlbedo;
	output.worldSpaceNormal = p.worldSpaceNormal;
	output.material.specular = p.specularity;
	output.material.roughness = 0.8f;
	output.cookedAmbientOcclusion = p.cookedAmbientOcclusion;
	return Encode(output);
}

[earlydepthstencil]
float4 ps_main_forward(SW_GStoPS geo) : SV_Target0
{
	TerrainPixel p = CalculateTerrain(geo);
	return float4(LightingScale * p.diffuseAlbedo * p.cookedAmbientOcclusion, 1.f);
}


///////////////////////////////////////////////////////////////////////////

struct IntersectionResult
{
	float4	_intersectionDistanceAndCoords : INTERSECTION;
};

cbuffer IntersectionTestParameters
{
	float3 RayStart;
	float3 RayEnd;
}

float RayVsTriangle(float3 rayStart, float3 rayEnd, float3 inputTriangle[3], out float3 barycentricResult)
{
		//
		//		Find the plane of the triangle, and find the intersection point
		//		between the ray and that plane. Then test to see if the intersection
		//		point is within the triangle (or on an edge).
		//

	float3 triangleNormal = normalize(cross(inputTriangle[1] - inputTriangle[0], inputTriangle[2] - inputTriangle[0]));
	float planeW = dot(inputTriangle[0], triangleNormal);

	float A = dot(rayStart, triangleNormal) - planeW;
	float B = dot(rayEnd, triangleNormal) - planeW;
	float alpha = A / (A-B);
	if (alpha < 0.f || alpha > 1.f)
		return -1.f;

	float3 intersectionPt = lerp(rayStart, rayEnd, alpha);

		//	look to see if this point is contained within the triangle
		//	we'll use barycentric coordinates (because that's useful for
		//	the caller later)
	float3 v0 = inputTriangle[1] - inputTriangle[0], v1 = inputTriangle[2] - inputTriangle[0], v2 = intersectionPt - inputTriangle[0];
    float d00 = dot(v0, v0);
    float d01 = dot(v0, v1);
    float d11 = dot(v1, v1);
    float d20 = dot(v2, v0);
    float d21 = dot(v2, v1);
    float denom = d00 * d11 - d01 * d01;
    float v = (d11 * d20 - d01 * d21) / denom;
    float w = (d00 * d21 - d01 * d20) / denom;
    float u = 1.0f - v - w;

	if (u >= 0.f && u <= 1.f && v >= 0.f && v <= 1.f && w >= 0.f && w <= 1.f) {
		barycentricResult = float3(u, v, w);
		return alpha;
	}

	return -1.f;
}

[maxvertexcount(1)]
	void gs_intersectiontest(triangle VSOutput input[3], inout PointStream<IntersectionResult> intersectionHits)
{
		//
		//		Given the input triangle, look for an intersection with the ray we're currently
		//		testing. If we find an intersection, let's append it to the IntersectionHits buffer.
		//
		//		We going to do the test in world space. But there's an important advantage to doing
		//		this in the geometry shader (instead of compute shader, or on the CPU) -- it uses
		//		the same LOD calculations as normal rendering. So we end up testing against the same
		//		triangles that get rendered (which means that the result seems consistant to the user)
		//

	#if (OUTPUT_WORLD_POSITION==1)
		float3 testingTriangle[3];
		testingTriangle[0] = input[0].worldPosition;
		testingTriangle[1] = input[1].worldPosition;
		testingTriangle[2] = input[2].worldPosition;

		float3 barycentric;
		float intersection = RayVsTriangle(RayStart, RayEnd, testingTriangle, barycentric);
		if (intersection >= 0.f && intersection < 1.f) {
			float2 surfaceCoordinates =
				  barycentric.x * input[0].texCoord
				+ barycentric.y * input[1].texCoord
				+ barycentric.z * input[2].texCoord
				;

			IntersectionResult intersectionResult;
			intersectionResult._intersectionDistanceAndCoords = float4(
				intersection, surfaceCoordinates.x, surfaceCoordinates.y, 1.f);

				//	can we write to an append buffer from a geometry shader?
				//	it would be a lot more convenient than having to use stream output
				//		(which can be a bit messy on the CPU side)
			intersectionHits.Append(intersectionResult);
		}

		// IntersectionResult temp;
		// temp._intersectionDistanceAndCoords = 1.0.xxxx;
		// intersectionHits.Append(temp);
	#endif
}
