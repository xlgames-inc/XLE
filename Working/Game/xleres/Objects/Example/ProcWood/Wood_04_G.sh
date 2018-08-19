// CompoundDocument:1
import basic = "xleres/nodes/basic.sh"
import woodrings002 = "xleres/objects/example/procwood/woodrings002.sh"
import woodplanks = "xleres/objects/example/procwood/woodplanks.sh"
import color = "xleres/nodes/color.sh"
import woodgnarlgrainstain = "xleres/objects/example/procwood/woodgnarlgrainstain.sh"

void main(out float3 color)
{
	node node_0 = [[visualNode6]]color::RGB(r:".307", g:".122", b:".042");
	node node_1 = [[visualNode9]]woodgnarlgrainstain::WoodGnarlGrainStain(coords:[[visualNode3]]woodplanks::WoodPlanks(offcenter:"5", width:".5", height:".15", shift1:".4", shift2:".1", shift3:".333", length:"2").plankCoords, gnarlStrength:".2", stain:"4", gnarlDensity:".5", grainStrength:"4", grainDensity:"100");
	color = [[visualNode0]]basic::Mix3(factor:[[visualNode2]]woodrings002::WoodRings002(coords:node_1.gnarledCoords, strength:".5", density:"50", shape:"2.5").rings, rhs:[[visualNode4]]basic::Mix3(rhs:[[visualNode5]]color::RGB(r:"1", g:".606", b:".284").result, lhs:node_0.result, factor:node_1.grain).result, lhs:[[visualNode8]]basic::Mix3(rhs:node_0.result, lhs:[[visualNode7]]color::RGB(r:".073", g:".011", b:"0").result, factor:node_1.grain).result).result;
}
attributes visualNode8(PreviewGeometry:"sphere", X:"1511.000000", Y:"682.000000", State:"Collapsed", OutputToVisualize:"");
attributes visualNode0(PreviewGeometry:"sphere", X:"1975.000000", Y:"551.000000", State:"Normal", OutputToVisualize:"");
attributes visualNode1(X:"2613.000000", Y:"682.000000", State:"Normal");
attributes visualNode2(PreviewGeometry:"plane2d", X:"1447.000000", Y:"-61.000000", State:"Normal", OutputToVisualize:"");
attributes visualNode3(PreviewGeometry:"plane2d", X:"310.000000", Y:"364.000000", State:"Normal", OutputToVisualize:"");
attributes visualNode4(PreviewGeometry:"sphere", X:"1515.000000", Y:"783.000000", State:"Collapsed", OutputToVisualize:"");
attributes visualNode5(PreviewGeometry:"sphere", X:"923.000000", Y:"815.000000", State:"Collapsed", OutputToVisualize:"");
attributes visualNode6(PreviewGeometry:"plane2d", X:"928.000000", Y:"753.000000", State:"Collapsed", OutputToVisualize:"");
attributes visualNode7(PreviewGeometry:"plane2d", X:"936.000000", Y:"699.000000", State:"Collapsed", OutputToVisualize:"");
attributes visualNode9(PreviewGeometry:"plane2d", X:"905.000000", Y:"161.000000", State:"Normal", OutputToVisualize:"result");
/* <<Chunk:NodeGraphMetaData:Wood_04_G>>--(
<?xml version="1.0" encoding="utf-8"?>
<NodeGraphMetaData xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://schemas.datacontract.org/2004/07/ShaderPatcherLayer">
	<HasTechniqueConfig>false</HasTechniqueConfig>
	<ShaderParameters xmlns:d2p1="http://schemas.microsoft.com/2003/10/Serialization/Arrays" />
	<Variables xmlns:d2p1="http://schemas.microsoft.com/2003/10/Serialization/Arrays" />
</NodeGraphMetaData>
)-- */
/* <<Chunk:CBLayout:main>>--(


)--*/

