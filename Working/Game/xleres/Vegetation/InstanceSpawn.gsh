// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define GEO_HAS_TEXCOORD 1

#include "../Transform.h"
#include "../MainGeometry.h"
#include "../Utility/perlinnoise.h"
#include "../Utility/MathConstants.h"

#include "../Terrain.h"
#include "../Utility/perlinnoise.h"
#include "../Forward/TerrainGenerator.h"

struct WorkingTriangle
{
	float3 pts[3];
};

struct GSOutput
{
	float4 position		: INSTANCEPOS;		// world space position & rotation value
	uint instanceParam	: INSTANCEPARAM;	// instance type & shadowing
};

cbuffer InstanceSpawn : register(b5)
{
	row_major float4x4 WorldToCullFrustum;
	float GridSpacing;	// 0.75
}

static const uint MaxOutputVertices = 112;

void Swap(inout float3 A, inout float3 B)
{
	float3 x = A;
	A = B;
	B = x;
}

static const float ShiftAmount[6] = 
{
	3.f, 3.f, .0f, .5f, 3.f, 3.f
};

static const float MaxDrawDistanceSq[6] = 
{
	75.f * 75.f, 100.f * 100.f, 
	20.f * 20.f, 40.f * 40.f, 
	50.f * 50.f, 60.f * 60.f
};

void WriteInstance(	float3 instancePosition, float2 tc, float rotationValue, 
					inout uint outputVertices, uint outputStreamIndex, inout PointStream<GSOutput> outputStream)
{
	if ((outputVertices+1)<=MaxOutputVertices) {
		GSOutput output;

		float noiseValue = fbmNoise2D(instancePosition.xy, 9.632f, .85f, 2.0192f, 3);
		uint instanceType = uint(6*frac(8.f * abs(noiseValue)));

		float3 camOffset = instancePosition - WorldSpaceView;
		float distSq = dot(camOffset, camOffset);
		if (distSq < MaxDrawDistanceSq[instanceType]) {
			const float shiftAmount = ShiftAmount[instanceType];
			instancePosition.x += shiftAmount * frac(noiseValue * 4.5f);
			instancePosition.y += shiftAmount * frac(noiseValue * -3.3f);
			output.position = float4(instancePosition, rotationValue);

				//	using the texture coordinate value, we can look up
				//	the shadowing in the terrain shadowing map
				//	This produces a single shadow sample per object. It's not perfectly
				//	accurate for large objects (particularlly tall plants, etc). But it should
				//	work well for grass and small things. And it should be much more efficient
				//	than doing extra shadowing work per-vertex or per-pixel.
			float2 finalTexCoord = lerp(TexCoordMins.xy, TexCoordMaxs.xy, tc.xy);
			float2 shadowSample = CoverageTileSet.SampleLevel(DefaultSampler, float3(finalTexCoord.xy, CoverageOrigin.z), 0).rg;
			float shadowing = saturate(ShadowSoftness * (SunAngle + shadowSample.r)) * saturate(ShadowSoftness * (shadowSample.g - SunAngle));

			output.instanceParam = ((instanceType+1) & 0xffff) | ((uint(float(0xffff) * shadowing)) << 16);

			outputStream.Append(output);
			++outputVertices;
		}
	}
}

void RasterizeBetweenEdges(	float3 e00, float3 e01, float3 e10, float3 e11, 
							float2 tc00, float2 tc01, float2 tc10, float2 tc11,
							inout uint outputVertices, float spacing,
							inout PointStream<GSOutput> outputStream)
{
	float y = ceil(e00.y/spacing)*spacing;
	for (;y<e01.y; y+=spacing) {
		float e0a = (y - e00.y) / (e01.y - e00.y);
		float e2a = (y - e10.y) / (e11.y - e10.y);

		float spanx0 = lerp(e00.x, e01.x, e0a);
		float spanx1 = lerp(e10.x, e11.x, e2a);
		float spanz0 = lerp(e00.z, e01.z, e0a);
		float spanz1 = lerp(e10.z, e11.z, e2a);

		float2 spantc0 = lerp(tc00, tc01, e0a);
		float2 spantc1 = lerp(tc10, tc11, e2a);

		float spanMinX = min(spanx0, spanx1);
		float spanMaxX = max(spanx0, spanx1);

		float x = ceil(spanMinX/spacing)*spacing;
		for (;x<spanMaxX; x+=spacing) {

			float ax = (x - spanx0) / (spanx1 - spanx0);
			float  z = lerp(spanz0, spanz1, ax);
			const float rotationValue	 = 0.f;
			const uint outputStreamIndex = 0;	// based on type

			float2 tc = lerp(spantc0, spantc1, ax);

			WriteInstance(
				float3(x, y, z), tc,
				rotationValue, outputVertices, 
				outputStreamIndex, outputStream);

		}
	}
}

float EstimatePointCount(WorkingTriangle tri, float spacing)
{
	float minY = tri.pts[0].y;
	float maxY = tri.pts[2].y;
	float minX = min(min(tri.pts[0].x, tri.pts[1].x), tri.pts[2].x);
	float maxX = max(max(tri.pts[0].x, tri.pts[1].x), tri.pts[2].x);
	float ceilMinX = ceil(minX/spacing)*spacing;
	float ceilMinY = ceil(minY/spacing)*spacing;

		//	Calculation is not exact... But it's quick.
	return ceil((maxX - ceilMinX)/spacing) * ceil((maxY - ceilMinY)/spacing) / 2;
}

bool PtInFrustum(float4 pt)
{
	float3 p = pt.xyz/pt.w;
	float3 q = max(float3(p.x, p.y, p.z), float3(-p.x, -p.y, 1-pt.z));
	float m = max(max(q.x, q.y), q.z);
	return m <= 1.f;
}

int CountTrue(bool3 input)
{
	return dot(true.xxx, input);
}

bool TriInFrustum(float4 pt0, float4 pt1, float4 pt2)
{
	float3 xs = float3(pt0.x, pt1.x, pt2.x);
	float3 ys = float3(pt0.y, pt1.y, pt2.y);
	float3 zs = float3(pt0.z, pt1.z, pt2.z);
	float3 ws = abs(float3(pt0.w, pt1.w, pt2.w));

	int l  = CountTrue(xs < -ws);
	int r  = CountTrue(xs >  ws);
	int t  = CountTrue(ys < -ws);
	int b  = CountTrue(ys >  ws);
	int f  = CountTrue(zs < 0.f);
	int bk = CountTrue(zs >  ws);

	return max(max(max(max(max(l, r), t), b), f), bk) < 3;
}

bool CullTriangle(WorkingTriangle tri)
{
		//	culling an imaginary triangle along the top of the grass
		//	objects provides a better result
	float4 p0 = mul(WorldToCullFrustum, float4(tri.pts[0] + float3(0,0,1),1));
	float4 p1 = mul(WorldToCullFrustum, float4(tri.pts[1] + float3(0,0,1),1));
	float4 p2 = mul(WorldToCullFrustum, float4(tri.pts[2] + float3(0,0,1),1));

	const bool accurateClip = true;
	if (!accurateClip) {
		// not exactly correct for large triangles -- each point could be outside, but part of the triangles could still be inside
		bool3 inFrustum = bool3(PtInFrustum(p0), PtInFrustum(p1), PtInFrustum(p2));
		return dot(inFrustum, bool3(true,true,true))!=0;
	} else {
		return TriInFrustum(p0, p1, p2);
	}
}

[maxvertexcount(112)]
	void main(triangle VSOutput input[3], inout PointStream<GSOutput> outputStream)
{

		//
		//		Find regular points on the input geometry
		//		and output instances of new geometry.
		//
		//		This is a rasterisation operation...
		//			... we could also use a compute shader for this. Read one vertex buffer
		//			and write another.
		//

	WorkingTriangle tri;
	#if (OUTPUT_WORLD_POSITION==1)
		tri.pts[0] = input[0].worldPosition.xyz;
		tri.pts[1] = input[1].worldPosition.xyz;
		tri.pts[2] = input[2].worldPosition.xyz;
	#else
		tri.pts[0] = input[0].position.xyz;
		tri.pts[1] = input[1].position.xyz;
		tri.pts[2] = input[2].position.xyz;
	#endif

	float2 tc0 = 0.0.xx, tc1 = 0.0.xx, tc2 = 0.0.xx;
	#if (OUTPUT_TEXCOORD==1)
		tc0 = input[0].texCoord.xy;
		tc1 = input[1].texCoord.xy;
		tc2 = input[2].texCoord.xy;
	#endif

		//	Cull offscreen and distant triangles...
	bool inFrustum = CullTriangle(tri);
	[branch] if (inFrustum) {

			//		Sort min y to max y

		if (tri.pts[0].y > tri.pts[1].y) {
			Swap(tri.pts[0], tri.pts[1]);
		}
		if (tri.pts[1].y > tri.pts[2].y) {
			Swap(tri.pts[1], tri.pts[2]);
			if (tri.pts[0].y > tri.pts[1].y) {
				Swap(tri.pts[0], tri.pts[1]);
			}
		}

			//		Sometimes the triangle can be too big.. It would result
			//		in too many output vertices. In these cases, try to
			//		distribute the available geometry evenly over the polygon
			//		Every step reduces the instance count to a quarter

		float gridSpacing	= GridSpacing;
		uint ptCount		= EstimatePointCount(tri, gridSpacing);
		uint maxPtCount		= MaxOutputVertices/4;
		while (ptCount > maxPtCount) {
			gridSpacing *= 2.f;
			ptCount /= 4.f;
		}

			//		Edges always point down, and go from top to
			//		bottom

		uint outputVertices = 0;
		RasterizeBetweenEdges(tri.pts[0], tri.pts[1], tri.pts[0], tri.pts[2], tc0, tc1, tc0, tc2, outputVertices, gridSpacing, outputStream);
		RasterizeBetweenEdges(tri.pts[1], tri.pts[2], tri.pts[0], tri.pts[2], tc1, tc2, tc0, tc2, outputVertices, gridSpacing, outputStream);

	}
}


struct GSOutputWire
{

		//		Note -- need as few output components as possible...
		//				fewer output components mean more output
		//				vertices can fit in the geometry shader output
		//				buffer

	float4 position : SV_Position;
	float3 color	: COLOR0;
	float2 texCoord : TEXCOORD0;
};

void RasterizeLineBetweenEdges(	float3 e00, float3 e01, float3 e10, float3 e11, 
								inout uint outputVertices,
								float spacing,
								inout LineStream<GSOutputWire> outputStream)
{
	float y = ceil(e00.y/spacing)*spacing;
	for (;y<e01.y; y+=spacing) {
		float e0a = (y - e00.y) / (e01.y - e00.y);
		float e2a = (y - e10.y) / (e11.y - e10.y);

		float spanx0 = lerp(e00.x, e01.x, e0a);
		float spanx1 = lerp(e10.x, e11.x, e2a);
		float spanz0 = lerp(e00.z, e01.z, e0a);
		float spanz1 = lerp(e10.z, e11.z, e2a);

		GSOutputWire output;
		output.color = 1.0.xxx;
		output.texCoord = 0.0.xx;

		if ((outputVertices+2)<=70) {
			float3 start	= float3(spanx0, y, spanz0);
			float3 end		= float3(spanx1, y, spanz1);

			output.position = mul(WorldToClip, float4(start,1));
			outputStream.Append(output);

			output.position = mul(WorldToClip, float4(end,1));
			outputStream.Append(output);
			outputStream.RestartStrip();
			outputVertices+= 2;
		}
	}
}

[maxvertexcount(113)]
	void wireframe(triangle VSOutput input[3], inout LineStream<GSOutputWire> outputStream)
{
	GSOutputWire output;
	output.color = 1.0.xxx;
	output.texCoord = 0.0.xx;
	
	output.position = mul(WorldToClip, float4(input[0].position.xyz,1));
	outputStream.Append(output);
	
	output.position = mul(WorldToClip, float4(input[1].position.xyz,1));
	outputStream.Append(output);
	
	output.position = mul(WorldToClip, float4(input[2].position.xyz,1));
	outputStream.Append(output);
	outputStream.RestartStrip();

	WorkingTriangle tri;
	tri.pts[0] = input[0].position.xyz;
	tri.pts[1] = input[1].position.xyz;
	tri.pts[2] = input[2].position.xyz;

		//	Sort min y to max y
	if (tri.pts[0].y > tri.pts[1].y) {
		Swap(tri.pts[0], tri.pts[1]);
	}
	if (tri.pts[1].y > tri.pts[2].y) {
		Swap(tri.pts[1], tri.pts[2]);
		if (tri.pts[0].y > tri.pts[1].y) {
			Swap(tri.pts[0], tri.pts[1]);
		}
	}

	float gridSpacing = GridSpacing;
	uint ptCount = EstimatePointCount(tri, gridSpacing);
	uint maxPtCount = MaxOutputVertices/4;
	while (ptCount > maxPtCount) {
		gridSpacing *= 2.f;
		ptCount /= 2.f;
	}

	uint outputVertices = 3;
	RasterizeLineBetweenEdges(tri.pts[0], tri.pts[1], tri.pts[0], tri.pts[2], outputVertices, gridSpacing, outputStream);
	RasterizeLineBetweenEdges(tri.pts[1], tri.pts[2], tri.pts[0], tri.pts[2], outputVertices, gridSpacing, outputStream);
}

