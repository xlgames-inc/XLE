// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

struct VertexType_P
{
	float3 position : POSITION0;
};

[maxvertexcount(4)]
	void P(point VertexType_P input[1], inout PointStream<VertexType_P> outputStream)
{
	outputStream.Append(input[0]);
}



struct VertexType_PN
{
	float3 position : POSITION0;
	float3 normal : NORMAL0;
};

[maxvertexcount(4)]
	void PN(point VertexType_PN input[1], inout PointStream<VertexType_PN> outputStream)
{
	outputStream.Append(input[0]);
}


