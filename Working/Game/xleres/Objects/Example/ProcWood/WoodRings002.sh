// CompoundDocument:1
#include "game/xleres/System/Prefix.h"

#include "game/xleres/Nodes/Basic.sh"


void WoodRings002(float3 coords, float density, float strength, float shape, out float rings : SV_Target0)
{
	float Output_98_r;
	float Output_98_g;
	float Output_98_b;
	Separate3( coords, Output_98_r, Output_98_g, Output_98_b );

	float Output_110_result;
	Output_110_result = Multiply1( Output_98_b, Output_98_b );

	float Output_111_result;
	Output_111_result = Multiply1( Output_98_g, Output_98_g );

	float Output_109_result;
	Output_109_result = Add1( Output_111_result, Output_110_result );

	float Output_108_result;
	Output_108_result = Power1( Output_109_result, .5 );

	float Output_102_result;
	Output_102_result = Multiply1( Output_108_result, density );

	float Output_99_result;
	Output_99_result = Round1( Output_102_result );

	float Output_101_result;
	Output_101_result = Subtract1( Output_102_result, Output_99_result );

	float Output_100_result;
	Output_100_result = Add1( Output_101_result, .5 );

	float Output_107_result;
	Output_107_result = Power1( Output_100_result, shape );

	float Output_106_result;
	Output_106_result = Multiply1( Output_107_result, 6.283 );

	float Output_105_result;
	Output_105_result = Cosine1( Output_106_result );

	float Output_104_result;
	Output_104_result = Multiply1( Output_105_result, strength );

	float Output_103_result;
	Output_103_result = Add1( Output_104_result, .5 );

	rings = Output_103_result;

}
/* <<Chunk:NodeGraph:WoodRings002>>--(
<?xml version="1.0" encoding="utf-8"?>
<NodeGraph xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://schemas.datacontract.org/2004/07/ShaderPatcherLayer">
	<ConstantConnections>
		<ConstantConnection>
			<OutputNodeID>98</OutputNodeID>
			<OutputParameterName>input</OutputParameterName>
			<Value>&lt;coords&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>100</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>.5</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>102</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>&lt;density&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>103</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>.5</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>104</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>&lt;strength&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>106</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>6.283</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>107</OutputNodeID>
			<OutputParameterName>exponent</OutputParameterName>
			<Value>&lt;shape&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>108</OutputNodeID>
			<OutputParameterName>exponent</OutputParameterName>
			<Value>.5</Value>
		</ConstantConnection>
	</ConstantConnections>
	<InputParameterConnections />
	<NodeConnections>
		<NodeConnection>
			<OutputNodeID>109</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>111</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>111</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>98</InputNodeID>
			<InputParameterName>g</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>111</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>98</InputNodeID>
			<InputParameterName>g</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>110</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>98</InputNodeID>
			<InputParameterName>b</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>110</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>98</InputNodeID>
			<InputParameterName>b</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>101</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>99</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>107</OutputNodeID>
			<OutputParameterName>base</OutputParameterName>
			<InputNodeID>100</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>100</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>101</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>101</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>102</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>99</OutputNodeID>
			<OutputParameterName>value</OutputParameterName>
			<InputNodeID>102</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>103</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>104</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>104</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>105</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>105</OutputNodeID>
			<OutputParameterName>x</OutputParameterName>
			<InputNodeID>106</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>106</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>107</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>102</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>108</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>108</OutputNodeID>
			<OutputParameterName>base</OutputParameterName>
			<InputNodeID>109</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>109</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>110</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
	</NodeConnections>
	<Nodes>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>111</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>1</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Separate3</FragmentArchiveName>
			<NodeId>98</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>2</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Round1</FragmentArchiveName>
			<NodeId>99</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>3</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Add1</FragmentArchiveName>
			<NodeId>100</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>4</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Subtract1</FragmentArchiveName>
			<NodeId>101</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>5</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>102</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>6</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Add1</FragmentArchiveName>
			<NodeId>103</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>7</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>104</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>8</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Cosine1</FragmentArchiveName>
			<NodeId>105</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>9</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>106</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>10</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Power1</FragmentArchiveName>
			<NodeId>107</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>11</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Power1</FragmentArchiveName>
			<NodeId>108</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>12</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Add1</FragmentArchiveName>
			<NodeId>109</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>13</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>110</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>14</VisualNodeId>
		</Node>
	</Nodes>
	<OutputParameterConnections>
		<OutputParameterConnection>
			<InputNodeID>103</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<Name>rings</Name>
			<Semantic></Semantic>
			<Type>float</Type>
			<VisualNodeId>0</VisualNodeId>
		</OutputParameterConnection>
	</OutputParameterConnections>
	<PreviewSettingsObjects>
		<PreviewSettings>
			<Geometry>Chart</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>1</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>2</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>3</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>4</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>5</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>6</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>7</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>8</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Chart</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>9</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>10</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>11</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>12</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Chart</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>13</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>14</VisualNodeId>
		</PreviewSettings>
	</PreviewSettingsObjects>
	<VisualNodes>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1203</d4p1:x>
				<d4p1:y>743</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>550</d4p1:x>
				<d4p1:y>221</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>364</d4p1:x>
				<d4p1:y>263</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>509</d4p1:x>
				<d4p1:y>547</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>814</d4p1:x>
				<d4p1:y>474</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>655</d4p1:x>
				<d4p1:y>475</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>373</d4p1:x>
				<d4p1:y>474</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>981</d4p1:x>
				<d4p1:y>754</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>809</d4p1:x>
				<d4p1:y>755</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>629</d4p1:x>
				<d4p1:y>750</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>492</d4p1:x>
				<d4p1:y>746</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>352</d4p1:x>
				<d4p1:y>745</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>859</d4p1:x>
				<d4p1:y>264</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>712</d4p1:x>
				<d4p1:y>265</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>548</d4p1:x>
				<d4p1:y>302</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
	</VisualNodes>
</NodeGraph>
)-- */
/* <<Chunk:NodeGraphContext:WoodRings002>>--(
<?xml version="1.0" encoding="utf-8"?>
<NodeGraphContext xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://schemas.datacontract.org/2004/07/ShaderPatcherLayer">
	<HasTechniqueConfig>false</HasTechniqueConfig>
	<ShaderParameters xmlns:d2p1="http://schemas.microsoft.com/2003/10/Serialization/Arrays" />
	<Variables xmlns:d2p1="http://schemas.microsoft.com/2003/10/Serialization/Arrays" />
</NodeGraphContext>
)-- */
