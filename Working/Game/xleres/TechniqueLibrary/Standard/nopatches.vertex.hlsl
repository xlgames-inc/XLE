#include "../Framework/MainGeometry.hlsl"
#include "../Framework/DeformVertex.hlsl"
#include "DeformVertex_Standard.vertex.hlsl"
#include "../Core/DefaultIllum.vertex.hlsl"
#include "../../Nodes/Templates.sh"

VSOUT main(VSIN input)
{
	DeformedVertex deformedVertex = DeformedVertex_Initialize(input);
	deformedVertex = DeformVertex_Standard(deformedVertex, input);
	return DefaultIllumVertex(deformedVertex, input);
}
