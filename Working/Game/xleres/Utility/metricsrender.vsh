// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Metrics.h"

struct VSOutput
{
	float2	topLeft : TOPLEFT;
	float2	bottomRight : BOTTOMRIGHT;
	float	value : VALUE;
};

StructuredBuffer<MetricsStructure> MetricsObject;

const uint2 ScreenDimensions;

uint TileCount()
{
	return ((ScreenDimensions.x + 15) / 16) * ((ScreenDimensions.y + 15) / 16);
}

VSOutput main(uint vertexId : SV_VertexId)
{

		//		Get the value we want to write from the metrics
		//		buffer, and 

	const float lineHeight = 64.f/2.f;
	VSOutput output;
	output.topLeft = float2(64.f, lineHeight*float(4+vertexId));
	output.bottomRight = float2(256.f+64.f, lineHeight*float(5+vertexId));
	// output.bottomRight = topLeft + float2(256.f, lineHeight);		(for some reason this line causes the compiler to crash!)
	
	if (vertexId == 0) {
		output.value = MetricsObject[0].TranslucentSampleCount;
	} else if (vertexId == 1) {
		output.value = MetricsObject[0].PixelsWithTranslucentSamples;
	} else if (vertexId == 2) {
		output.value = MetricsObject[0].TranslucentSampleCount / float(MetricsObject[0].PixelsWithTranslucentSamples);
	} else if (vertexId == 3) {
		output.value = MetricsObject[0].MaxTranslucentSampleCount;
	} else if (vertexId == 4) {
		output.value = MetricsObject[0].TotalTileCount;
	} else if (vertexId == 5) {
		output.value = MetricsObject[0].TotalClusterCount / float(MetricsObject[0].TotalTileCount);
	} else if (vertexId == 6) {
		output.value = MetricsObject[0].LightCullCount;
	} else if (vertexId == 7) {
		output.value = MetricsObject[0].LightCalculateCount;
	} else if (vertexId == 8) {
		output.value = MetricsObject[0].ClusterErrorCount;
	} else {
		output.value = 0.f;
	}

	return output;
}


