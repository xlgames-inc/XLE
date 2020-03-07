#include "../Framework/MainGeometry.hlsl"
#include "../Framework/DeformVertex.hlsl"
#include "DeformVertex_Standard.vertex.hlsl"
#include "../Core/DefaultIllum.vertex.hlsl"

VSOUT main(VSIN input)
{
	DeformedVertex deformedVertex = DeformedVertex_Initialize(input);
	return DefaultIllumVertex(deformedVertex, input);
}
