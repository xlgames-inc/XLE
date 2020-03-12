#include "../Framework/MainGeometry.hlsl"
#include "../Framework/DeformVertex.hlsl"
#include "DeformVertex_Standard.vertex.hlsl"
#include "../Core/BuildVSOUT.vertex.hlsl"
#include "../../Nodes/Templates.sh"

VSOUT main(VSIN input)
{
	DeformedVertex deformedVertex = DeformedVertex_Initialize(input);
	deformedVertex = DeformVertex_Standard(deformedVertex, input);
	return BuildVSOUT(deformedVertex, input);
}
