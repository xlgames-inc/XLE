// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)


struct InputVertex_PNT
{
	float4 position		: SV_Position;
	float3 viewVector	: VIEWVECTOR;
	float3 normal		: NORMAL0;
	float2 texCoord		: TEXCOORD0;
};

struct OutputVertex_PNT
{
	float4 position		: SV_Position;
	float3 viewVector	: VIEWVECTOR;
	float2 texCoord		: TEXCOORD0;

	float2 barycentricCoords			: BARYCENTRIC;	// Barycentric interpolants
	nointerpolation float3 normals[6]	: SIXNORMS; // 6 normal positions
};

[maxvertexcount(3)]
	void PNT(triangle InputVertex_PNT input[3], inout TriangleStream<OutputVertex_PNT> outputStream)
{
	OutputVertex_PNT outVert;

	outVert.normals[0] = normalize(input[0].normal);
	outVert.normals[1] = normalize(input[0].normal + input[1].normal);
	outVert.normals[2] = normalize(input[1].normal);
	outVert.normals[3] = normalize(input[2].normal + input[0].normal);
	outVert.normals[4] = normalize(input[2].normal);
	outVert.normals[5] = normalize(input[1].normal + input[2].normal);

	outVert.position	= input[0].position;
	outVert.viewVector	= input[0].viewVector;
	outVert.texCoord	= input[0].texCoord;
	outVert.barycentricCoords = float2(0.f, 0.f);
	outputStream.Append(outVert);

	outVert.position	= input[1].position;
	outVert.viewVector	= input[1].viewVector;
	outVert.texCoord	= input[1].texCoord;
	outVert.barycentricCoords = float2(1.f, 0.f);
	outputStream.Append(outVert);

	outVert.position	= input[2].position;
	outVert.viewVector	= input[2].viewVector;
	outVert.texCoord	= input[2].texCoord;
	outVert.barycentricCoords = float2(0.f, 1.f);
	outputStream.Append(outVert);

	outputStream.RestartStrip();
}




struct InputVertex_PN
{
	float4 position		: SV_Position;
	float3 viewVector	: VIEWVECTOR;
	float3 normal		: NORMAL0;
};

struct OutputVertex_PN
{
	float4 position		: SV_Position;
	float3 viewVector	: VIEWVECTOR;

	float2 barycentricCoords			: BARYCENTRIC;	// Barycentric interpolants
	nointerpolation float3 normals[6]	: SIXNORMS; // 6 normal positions
};

[maxvertexcount(3)]
	void PN(triangle InputVertex_PN input[3], inout TriangleStream<OutputVertex_PN> outputStream)
{
	OutputVertex_PN outVert;

	outVert.normals[0] = normalize(input[0].normal);
	outVert.normals[1] = normalize(input[0].normal + input[1].normal);
	outVert.normals[2] = normalize(input[1].normal);
	outVert.normals[3] = normalize(input[2].normal + input[0].normal);
	outVert.normals[4] = normalize(input[2].normal);
	outVert.normals[5] = normalize(input[1].normal + input[2].normal);

	outVert.position	= input[0].position;
	outVert.viewVector	= input[0].viewVector;
	outVert.barycentricCoords = float2(0.f, 0.f);
	outputStream.Append(outVert);

	outVert.position	= input[1].position;
	outVert.viewVector	= input[1].viewVector;
	outVert.barycentricCoords = float2(1.f, 0.f);
	outputStream.Append(outVert);

	outVert.position	= input[2].position;
	outVert.viewVector	= input[2].viewVector;
	outVert.barycentricCoords = float2(0.f, 1.f);
	outputStream.Append(outVert);

	outputStream.RestartStrip();
}


struct InputVertex_PCR
{
	float4 position		: SV_Position;
	float4 color		: COLOR0;
	float radius		: RADIUS;
};

struct OutputVertex_PC
{
	float4 position		: SV_Position;
	float4 color		: COLOR0;
};

[maxvertexcount(3)]
	void PCR(point InputVertex_PCR input[1], inout PointStream<OutputVertex_PC> outputStream)
{
	OutputVertex_PC outVert;
	outVert.position = input[0].position;
	outVert.color = input[0].color;
	outputStream.Append(outVert);
}

