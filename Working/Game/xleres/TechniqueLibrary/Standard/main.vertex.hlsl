#include "../Framework/MainGeometry.hlsl"
#include "../Framework/DeformVertex.hlsl"
#include "DeformVertex_Standard.vertex.hlsl"
#include "../Core/DefaultIllum.vertex.hlsl"
#include "../../Nodes/Templates.sh"

VSOUT frameworkEntry(VSIN input)
{
	DeformedVertex deformedVertex = DeformedVertex_Initialize(input);
	deformedVertex = DeformVertex_Standard(deformedVertex, input);
	return DefaultIllumVertex(deformedVertex, input);
}

/*
	Note -- we don't currently support customizing this with patches, in the
		way that we do pixel shaders. If we did, it might look something like this:

VSOUT frameworkEntryWithDeformVertex(VSOUT geo)
{
	DeformedVertex deformedVertex = DeformedVertex_Initialize(input);
	deformedVertex = DeformVertex(deformedVertex, input);
	return DefaultIllumVertex(deformedVertex, result);
}
*/
