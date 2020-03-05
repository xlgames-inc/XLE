// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define GEO_HAS_TEXCOORD 1

#include "../Terrain/Terrain.hlsl"
#include "../../Framework/SystemUniforms.hlsl"
#include "../../Framework/MainGeometry.hlsl"
#include "../../Math/perlinnoise.hlsl"
#include "../../Math/MathConstants.hlsl"

Texture2D TerrainBaseTexture : register(t3);

struct WorkingTriangle
{
	float3 pts[3];
};

struct GSOutput
{
		//		Note -- need as few output components as possible...
		//				fewer output components mean more output
		//				vertices can fit in the geometry shader output
		//				buffer
	float4 position : SV_Position;
	float3 color : COLOR0;
	float2 texCoord : TEXCOORD0;
};

static const uint MaxOutputVertices = 112;

static const float DefaultGridSpacing = 0.75f;

float2 CalculateWind(float3 worldPosition)
{
	float2 timeOffset = float2(-0.132f, 0.0823f) * 5.f * SysUniform_GetGlobalTime();
	float noiseValue = PerlinNoise2D(0.0187f*worldPosition.xy+timeOffset);

	float2 timeOffset2 = float2(1.132f, 2.0823f) * 2.f * SysUniform_GetGlobalTime();
	float noiseStrength = PerlinNoise2D(0.01137f*worldPosition.xy + timeOffset);
	noiseStrength *= noiseStrength;
	noiseStrength = .5f + .5f*noiseStrength;

	float windDirection = noiseValue * 2.f * pi;
	float2 sineCosine;
	sincos(windDirection, sineCosine.x, sineCosine.y);

	return noiseStrength * sineCosine.yx;
}

void WriteInstance(float3 instancePosition, inout uint outputVertices, inout TriangleStream<GSOutput> outputStream)
{
	// #define TOP_DOWN_VIEW
	#if !defined(TOP_DOWN_VIEW)
		float3 offsets[4] =
		{
			float3(-0.5f, 0.f, 1.0f),
			float3( 0.5f, 0.f, 1.0f),
			float3(-0.5f, 0.f, 0.0f),
			float3( 0.5f, 0.f, 0.0f)
		};
	#else
		float3 offsets[4] =
		{
			float3(-0.5f,  0.5f, 0.1f),
			float3( 0.5f,  0.5f, 0.1f),
			float3(-0.5f, -0.5f, 0.1f),
			float3( 0.5f, -0.5f, 0.1f)
		};
	#endif
	float2 tcs[4] =
	{
		float2(0.f, 0.f),
		float2(1.f, 0.f),
		float2(0.f, 1.f),
		float2(1.f, 1.f)
	};

	if ((outputVertices+4)<=MaxOutputVertices) {

		const float noiseLookupScale = 0.338f;
		float noiseValue = PerlinNoise2D(noiseLookupScale*instancePosition.xy);

		float4 images[] =
		{
			float4(0.f, 5.f/1024.f,				568.f/1024.f, 340.f/1024.f),
			float4(568.f/1024.f, 0.f,			1024.f/1024.f, 340.f/1024.f),
			float4(0.f, 340.f/1024.f,			646.f/1024.f, (340.f+289.f)/1024.f),
			float4(646.f/1024.f, 340.f/1024.f,	(646.f+247.f)/1024.f, (340.f+289.f)/1024.f),
			float4(0.f, 646.f/1024.f,			489.f/1024.f, 1.f),
			float4(523.f/1024.f, 626.f/1024.f,	1.f, 1.f)
		};
		bool useTerrainColour[] =
		{
			true, false, true, false, true, true
		};
		float scaleParameter[] =
		{
			0.85f, 1.f, 1.f, 1.f, 1.f, 1.f
		};

		uint imageArrayIndex = uint(noiseValue * 73.f) % 6;
		float2 minTC = images[imageArrayIndex].xy;
		float2 maxTC = images[imageArrayIndex].zw;

			//	Calculate grass properties (size, color, image index, etc)...

			//		\todo -- lookup sin/cos, size, color from large random texture

		float ratio = (images[imageArrayIndex].w-images[imageArrayIndex].y) / (images[imageArrayIndex].z-images[imageArrayIndex].x);
		float width = lerp(0.75f, 2.f, frac(noiseValue * 32.917f));
		width *= scaleParameter[imageArrayIndex];
		float ratioScale = lerp(1.f, 2.f, frac(noiseValue * 11.327f));
		float height = width * ratio * ratioScale;

		float rotation = noiseValue * 17.4f * pi;
		float sinRotation, cosRotation;
		sincos(rotation, sinRotation, cosRotation);

		// float3 normal = float3(0,0,1); // float3(sinRotation*sqrtHalf, cosRotation*sqrtHalf, 1.5f*sqrtHalf);

		float noiseValue2 = PerlinNoise2D(1.167f*instancePosition.xy);
		float baseOffsetAngle = noiseValue2 * 3.14159f;
		float2 sineCosine;
		sincos(baseOffsetAngle, sineCosine.x, sineCosine.y);
		float baseOffsetMagnitude = noiseValue * 2.5f;
		float2 baseOffset = baseOffsetMagnitude * sineCosine.yx;
		instancePosition.xy += baseOffset;

			//	Some grass objects read color from the terrain texture
			//	(others don't)

		float3 color = .2.xxx;
		if (useTerrainColour[imageArrayIndex]) {
			float2 baseTexCoord = BuildBaseTexCoord(instancePosition.xyz);
			color = TerrainBaseTexture.SampleLevel(DefaultSampler, baseTexCoord, 0.f).rgb;
		}

		float colorScale = noiseValue2;
		color *= lerp(0.75f, 1.5f, colorScale);

			//	Calculate wind movement
		float2 wind = CalculateWind(instancePosition);

		#if defined(TOP_DOWN_VIEW)
			{
				float2 timeOffset2 = float2(1.132f, 2.0823f) * 2.f * SysUniform_GetGlobalTime();
				float noiseStrength = PerlinNoise2D(0.01137f*instancePosition.xy + timeOffset2);
				noiseStrength *= noiseStrength;
				noiseStrength = .5f + .5f*noiseStrength;
				color = lerp(float3(1,0,0), float3(0,0,1), noiseStrength);
			}
		#endif

		GSOutput output;
		for (uint c=0; c<4; c++) {

			float3 offset = offsets[c];
			offset.z *= height;
			#if !defined(TOP_DOWN_VIEW)
				offset.xy = float2(cosRotation * offset.x, sinRotation * offset.x);
			#endif
			offset.xy *= width;

				//	Move grass from side to side with the wind
				//	(note, z isn't effected... it might be better if it was a true rotate)
			offset.xy += offsets[c].z * wind.xy;

			float3 worldPosition = instancePosition + offset;
			output.position = mul(SysUniform_GetWorldToClip(), float4(worldPosition,1));
			output.texCoord.x = lerp(minTC.x, maxTC.x, tcs[c].x);
			output.texCoord.y = lerp(minTC.y, maxTC.y, tcs[c].y);
			output.color = color;
			outputStream.Append(output);

		}
		outputStream.RestartStrip();
		outputVertices += 4;

	}
}

void Swap(inout float3 A, inout float3 B)
{
	float3 x = A;
	A = B;
	B = x;
}

void RasterizeBetweenEdges(	float3 e00, float3 e01, float3 e10, float3 e11,
							inout uint outputVertices,
							float spacing,
							inout TriangleStream<GSOutput> outputStream)
{
	float y = ceil(e00.y/spacing)*spacing;
	for (;y<e01.y; y+=spacing) {
		float e0a = (y - e00.y) / (e01.y - e00.y);
		float e2a = (y - e10.y) / (e11.y - e10.y);

		float spanx0 = lerp(e00.x, e01.x, e0a);
		float spanx1 = lerp(e10.x, e11.x, e2a);
		float spanz0 = lerp(e00.z, e01.z, e0a);
		float spanz1 = lerp(e10.z, e11.z, e2a);

		float spanMinX = min(spanx0, spanx1);
		float spanMaxX = max(spanx0, spanx1);

		float x = ceil(spanMinX/spacing)*spacing;
		for (;x<spanMaxX; x+=spacing) {
			float ax = (x - spanx0) / (spanx1 - spanx0);
			float  z = lerp(spanz0, spanz1, ax);
			WriteInstance(float3(x, y, z), outputVertices, outputStream);
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

row_major float4x4 WorldToCullFrustum;

bool PtInFrustum(float4 pt)
{
	float3 p = pt.xyz/pt.w;
	float3 q = max(	float3( p.x,  p.y, p.z),
					float3(-p.x, -p.y, 1-pt.z));
	float m = max(max(q.x, q.y), q.z);
	return m <= 1.f;
}

bool CullTriangle(WorkingTriangle tri)
{
		//	culling an imaginary triangle along the top of the grass
		//	objects provides a better result
	float4 p0 = mul(WorldToCullFrustum, float4(tri.pts[0] + float3(0,0,1),1));
	float4 p1 = mul(WorldToCullFrustum, float4(tri.pts[1] + float3(0,0,1),1));
	float4 p2 = mul(WorldToCullFrustum, float4(tri.pts[2] + float3(0,0,1),1));

	bool3 inFrustum = bool3(PtInFrustum(p0), PtInFrustum(p1), PtInFrustum(p2));
	return dot(inFrustum, bool3(true,true,true))!=0;
}

[maxvertexcount(112)]
	void main(triangle VSOUT input[3], inout TriangleStream<GSOutput> outputStream)
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
	tri.pts[0] = input[0].position.xyz;
	tri.pts[1] = input[1].position.xyz;
	tri.pts[2] = input[2].position.xyz;

		//	Cull offscreen and distant triangles...
	bool inFrustum = CullTriangle(tri);
	[branch] if (inFrustum) {

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

			//	Sometimes the triangle can be too big.. It would result
			//	in too many output vertices. In these cases, try to
			//	distribute the available geometry evenly over the polygon
			//	Every step reduces the instance count to a quarter
		float gridSpacing = DefaultGridSpacing;
		uint ptCount = EstimatePointCount(tri, gridSpacing);
		uint maxPtCount = MaxOutputVertices/4;
		while (ptCount > maxPtCount) {
			gridSpacing *= 2.f;
			ptCount /= 4.f;
		}

			//
			//		Edges always point down, and go from top to
			//		bottom
			//

		uint outputVertices = 0;
		RasterizeBetweenEdges(tri.pts[0], tri.pts[1], tri.pts[0], tri.pts[2], outputVertices, gridSpacing, outputStream);
		RasterizeBetweenEdges(tri.pts[1], tri.pts[2], tri.pts[0], tri.pts[2], outputVertices, gridSpacing, outputStream);

	}
}



void RasterizeLineBetweenEdges(	float3 e00, float3 e01, float3 e10, float3 e11,
								inout uint outputVertices,
								float spacing,
								inout LineStream<GSOutput> outputStream)
{
	float y = ceil(e00.y/spacing)*spacing;
	for (;y<e01.y; y+=spacing) {
		float e0a = (y - e00.y) / (e01.y - e00.y);
		float e2a = (y - e10.y) / (e11.y - e10.y);

		float spanx0 = lerp(e00.x, e01.x, e0a);
		float spanx1 = lerp(e10.x, e11.x, e2a);
		float spanz0 = lerp(e00.z, e01.z, e0a);
		float spanz1 = lerp(e10.z, e11.z, e2a);

		GSOutput output;
		output.color = 1.0.xxx;
		output.texCoord = 0.0.xx;

		if ((outputVertices+2)<=70) {
			float3 start	= float3(spanx0, y, spanz0);
			float3 end		= float3(spanx1, y, spanz1);

			output.position = mul(SysUniform_GetWorldToClip(), float4(start,1));
			outputStream.Append(output);

			output.position = mul(SysUniform_GetWorldToClip(), float4(end,1));
			outputStream.Append(output);
			outputStream.RestartStrip();
			outputVertices+= 2;
		}
	}
}

[maxvertexcount(113)]
	void wireframe(triangle VSOUT input[3], inout LineStream<GSOutput> outputStream)
{
	GSOutput output;
	output.color = 1.0.xxx;
	output.texCoord = 0.0.xx;

	output.position = mul(SysUniform_GetWorldToClip(), float4(input[0].position.xyz,1));
	outputStream.Append(output);

	output.position = mul(SysUniform_GetWorldToClip(), float4(input[1].position.xyz,1));
	outputStream.Append(output);

	output.position = mul(SysUniform_GetWorldToClip(), float4(input[2].position.xyz,1));
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

	float gridSpacing = DefaultGridSpacing;
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
