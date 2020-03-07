
#if !defined(DEFORM_VERTEX_HLSL)
#define DEFORM_VERTEX_HLSL

#if !defined(MAIN_GEOMETRY_H)
	#error Include TechniqueLibrary/Framework/MainGeometry.hlsl before including this file
#endif

#include "Surface.hlsl"

struct DeformedVertex
{
	float3 					position;
	CompressedTangentFrame 	tangentFrame;
	uint					coordinateSpace;
};

DeformedVertex DeformedVertex_Initialize(VSIN input)
{
	DeformedVertex deformedVertex;
	deformedVertex.position = VSIN_GetLocalPosition(input);
	deformedVertex.tangentFrame = VSIN_GetCompressedTangentFrame(input);
	deformedVertex.coordinateSpace = 0;
	return deformedVertex;
}

#endif

