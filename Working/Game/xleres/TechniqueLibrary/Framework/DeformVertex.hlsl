
#if !defined(DEFORM_VERTEX_HLSL)
#define DEFORM_VERTEX_HLSL

#if !defined(VSIN_H)
	#error Include TechniqueLibrary/Framework/VSIN.hlsl before including this file
#endif

#include "../Math/SurfaceAlgorithm.hlsl"

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

