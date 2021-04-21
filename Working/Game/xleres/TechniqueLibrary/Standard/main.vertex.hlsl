#include "../Framework/VSIN.hlsl"
#include "../Framework/VSOUT.hlsl"
#include "../Framework/DeformVertex.hlsl"
#include "DeformVertex_Standard.vertex.hlsl"
#include "../Core/BuildVSOUT.vertex.hlsl"
#include "../../Nodes/Templates.vertex.sh"

VSOUT frameworkEntry(VSIN input)
{
	DeformedVertex deformedVertex = DeformedVertex_Initialize(input);
	return BuildVSOUT(deformedVertex, input);
}

VSOUT frameworkEntryWithDeformVertex(VSIN input)
{
	DeformedVertex deformedVertex = DeformedVertex_Initialize(input);
	deformedVertex = DeformVertex(deformedVertex, input);
	return BuildVSOUT(deformedVertex, input);
}
