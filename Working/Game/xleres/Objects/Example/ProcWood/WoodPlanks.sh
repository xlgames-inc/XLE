// CompoundDocument:1
import woodmodulator002 = "xleres/objects/example/procwood/woodmodulator002.sh"
import basic = "xleres/nodes/basic.sh"
import texture = "xleres/nodes/texture.sh"

void main(out float3 plankCoords)
{
	node node_0 = [[visualNode22]]basic::Separate3(input:"coords");
	node node_1 = [[visualNode6]]woodmodulator002::WoodModulator002(value:node_0.b, period:"height");
	node node_2 = [[visualNode0]]woodmodulator002::WoodModulator002(value:[[visualNode15]]basic::Add1(rhs:[[visualNode8]]basic::Multiply1(rhs:node_1.int, lhs:[[visualNode18]]basic::Multiply1(rhs:[[visualNode20]]basic::Divide1(lhs:"shift3", rhs:"height").result, lhs:"width").result).result, lhs:node_0.g).result, period:"width");
	node node_3 = [[visualNode4]]woodmodulator002::WoodModulator002(value:[[visualNode13]]basic::Add1(lhs:[[visualNode2]]basic::Add1(rhs:[[visualNode7]]basic::Multiply1(rhs:node_2.int, lhs:[[visualNode16]]basic::Multiply1(lhs:[[visualNode19]]basic::Divide1(lhs:"shift1", rhs:"width").result, rhs:"length").result).result, lhs:node_0.r).result, rhs:[[visualNode14]]basic::Multiply1(rhs:node_1.int, lhs:[[visualNode17]]basic::Multiply1(rhs:[[visualNode21]]basic::Divide1(lhs:"shift2", rhs:"height").result, lhs:"length").result).result).result, period:"length");
	plankCoords = [[visualNode5]]basic::Add3(lhs:[[visualNode9]]basic::Combine3(g:node_2.frac, r:node_3.frac, b:node_1.frac).fnResult, rhs:[[visualNode10]]basic::Multiply3(lhs:[[visualNode11]]basic::Subtract3(lhs:[[visualNode3]]texture::NoiseTexture3(position:[[visualNode12]]basic::Combine3(g:node_2.int, r:node_3.int, b:node_1.int).fnResult, scale:"3.142", detail:"2", distortion:"0").result, rhs:"float3(.5, .5, .5)").result, rhs:"offcenter").result).result;
}
attributes visualNode8(PreviewGeometry:"sphere", X:"-66.000000", Y:"356.000000", State:"Collapsed", OutputToVisualize:"");
attributes visualNode0(PreviewGeometry:"plane2d", X:"446.000000", Y:"184.000000", State:"Normal", OutputToVisualize:"");
attributes visualNode1(X:"3747.000000", Y:"51.000000", State:"Normal");
attributes visualNode2(PreviewGeometry:"sphere", X:"1112.000000", Y:"-93.000000", State:"Collapsed", OutputToVisualize:"");
attributes visualNode3(PreviewGeometry:"plane2d", X:"2266.000000", Y:"277.000000", State:"Collapsed", OutputToVisualize:"");
attributes visualNode4(PreviewGeometry:"plane2d", X:"1697.000000", Y:"54.000000", State:"Normal", OutputToVisualize:"result2");
attributes visualNode5(PreviewGeometry:"plane2d", X:"3140.000000", Y:"75.000000", State:"Normal", OutputToVisualize:"");
attributes visualNode6(PreviewGeometry:"plane2d", X:"-626.000000", Y:"658.000000", State:"Normal", OutputToVisualize:"");
attributes visualNode7(PreviewGeometry:"sphere", X:"903.000000", Y:"21.000000", State:"Collapsed", OutputToVisualize:"");
attributes visualNode9(PreviewGeometry:"sphere", X:"2462.000000", Y:"-43.000000", State:"Collapsed", OutputToVisualize:"");
attributes visualNode10(PreviewGeometry:"sphere", X:"2761.000000", Y:"213.000000", State:"Collapsed", OutputToVisualize:"");
attributes visualNode11(PreviewGeometry:"sphere", X:"2531.000000", Y:"321.000000", State:"Collapsed", OutputToVisualize:"");
attributes visualNode12(PreviewGeometry:"plane2d", X:"2092.000000", Y:"146.000000", State:"Collapsed", OutputToVisualize:"");
attributes visualNode13(PreviewGeometry:"sphere", X:"1357.000000", Y:"-14.000000", State:"Collapsed", OutputToVisualize:"");
attributes visualNode14(PreviewGeometry:"sphere", X:"135.000000", Y:"450.000000", State:"Collapsed", OutputToVisualize:"");
attributes visualNode15(PreviewGeometry:"sphere", X:"138.000000", Y:"284.000000", State:"Collapsed", OutputToVisualize:"");
attributes visualNode16(PreviewGeometry:"sphere", X:"-373.000000", Y:"271.000000", State:"Collapsed", OutputToVisualize:"");
attributes visualNode17(PreviewGeometry:"sphere", X:"-374.000000", Y:"328.000000", State:"Collapsed", OutputToVisualize:"");
attributes visualNode18(PreviewGeometry:"sphere", X:"-397.000000", Y:"394.000000", State:"Collapsed", OutputToVisualize:"");
attributes visualNode19(PreviewGeometry:"sphere", X:"-639.000000", Y:"266.000000", State:"Collapsed", OutputToVisualize:"");
attributes visualNode20(PreviewGeometry:"sphere", X:"-630.000000", Y:"389.000000", State:"Collapsed", OutputToVisualize:"");
attributes visualNode21(PreviewGeometry:"sphere", X:"-637.000000", Y:"326.000000", State:"Collapsed", OutputToVisualize:"");
attributes visualNode22(PreviewGeometry:"sphere", X:"-1185.000000", Y:"-260.000000", State:"Normal", OutputToVisualize:"");
/* <<Chunk:NodeGraphContext:WoodPlanks>>--(
<?xml version="1.0" encoding="utf-8"?>
<NodeGraphContext xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://schemas.datacontract.org/2004/07/ShaderPatcherLayer">
	<HasTechniqueConfig>false</HasTechniqueConfig>
	<ShaderParameters xmlns:d2p1="http://schemas.microsoft.com/2003/10/Serialization/Arrays" />
	<Variables xmlns:d2p1="http://schemas.microsoft.com/2003/10/Serialization/Arrays" />
</NodeGraphContext>
)-- */
/* <<Chunk:CBLayout:main>>--(


)--*/

