#include "../Framework/MainGeometry.hlsl"
#include "../Framework/DeformVertex.hlsl"
#include "DeformVertex_Standard.vertex.hlsl"
#include "../Core/DefaultIllum.vertex.hlsl"
#include "../../Nodes/Templates.sh"

VSOUT frameworkEntry(VSIN input)
{
	DeformedVertex deformedVertex = DeformedVertex_Initialize(input);
	return DefaultIllumVertex(deformedVertex, input);
}

VSOUT frameworkEntryWithDeformVertex(VSIN input)
{
	DeformedVertex deformedVertex = DeformedVertex_Initialize(input);
	deformedVertex = DeformVertex(deformedVertex, input);
	return DefaultIllumVertex(deformedVertex, input);
}
