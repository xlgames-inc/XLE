// CompoundDocument:1
#include "xleres/System/Prefix.h"

#include "xleres/Nodes/Basic.sh"


void WoodModulator002(float value, float period, out float int : SV_Target0, out float frac : SV_Target1)
{
	float Output_69_result;
	Output_69_result = Divide1( value, period );

	float Output_70_result;
	Output_70_result = Round1( Output_69_result );

	float Output_71_result;
	Output_71_result = Multiply1( Output_70_result, period );

	float Output_73_result;
	Output_73_result = Subtract1( Output_69_result, Output_70_result );

	float Output_72_result;
	Output_72_result = Multiply1( Output_73_result, period );

	int = Output_71_result;
	frac = Output_72_result;

}
/* <<Chunk:NodeGraph:WoodModulator002>>--(
<?xml version="1.0" encoding="utf-8"?>
<NodeGraph xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://schemas.datacontract.org/2004/07/ShaderPatcherLayer">
	<ConstantConnections>
		<ConstantConnection>
			<OutputNodeID>69</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Value>&lt;value&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>69</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>&lt;period&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>71</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>&lt;period&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>72</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>&lt;period&gt;</Value>
		</ConstantConnection>
	</ConstantConnections>
	<InputParameterConnections />
	<NodeConnections>
		<NodeConnection>
			<OutputNodeID>73</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>69</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>70</OutputNodeID>
			<OutputParameterName>value</OutputParameterName>
			<InputNodeID>69</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>72</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>73</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>73</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>70</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>71</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>70</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
	</NodeConnections>
	<Nodes>
		<Node>
			<FragmentArchiveName>xleres/Nodes/Basic.sh:Divide1</FragmentArchiveName>
			<NodeId>69</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>0</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>xleres/Nodes/Basic.sh:Subtract1</FragmentArchiveName>
			<NodeId>73</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>2</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>xleres/Nodes/Basic.sh:Round1</FragmentArchiveName>
			<NodeId>70</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>3</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>xleres/Nodes/Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>71</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>4</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>xleres/Nodes/Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>72</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>5</VisualNodeId>
		</Node>
	</Nodes>
	<OutputParameterConnections>
		<OutputParameterConnection>
			<InputNodeID>71</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<Name>int</Name>
			<Semantic></Semantic>
			<Type>float</Type>
			<VisualNodeId>1</VisualNodeId>
		</OutputParameterConnection>
		<OutputParameterConnection>
			<InputNodeID>72</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<Name>frac</Name>
			<Semantic></Semantic>
			<Type>float</Type>
			<VisualNodeId>1</VisualNodeId>
		</OutputParameterConnection>
	</OutputParameterConnections>
	<PreviewSettingsObjects>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>0</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>2</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>3</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>4</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>5</VisualNodeId>
		</PreviewSettings>
	</PreviewSettingsObjects>
	<VisualNodes>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>157</d4p1:x>
				<d4p1:y>257</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1009</d4p1:x>
				<d4p1:y>317</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>548</d4p1:x>
				<d4p1:y>268</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>365</d4p1:x>
				<d4p1:y>348</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>677</d4p1:x>
				<d4p1:y>377</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>717</d4p1:x>
				<d4p1:y>279</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
	</VisualNodes>
</NodeGraph>
)-- */
/* <<Chunk:NodeGraphContext:WoodModulator002>>--(
<?xml version="1.0" encoding="utf-8"?>
<NodeGraphContext xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://schemas.datacontract.org/2004/07/ShaderPatcherLayer">
	<HasTechniqueConfig>false</HasTechniqueConfig>
	<ShaderParameters xmlns:d2p1="http://schemas.microsoft.com/2003/10/Serialization/Arrays" />
	<Variables xmlns:d2p1="http://schemas.microsoft.com/2003/10/Serialization/Arrays">
		<d2p1:KeyValueOfstringstring>
			<d2p1:Key>period</d2p1:Key>
			<d2p1:Value>2.5</d2p1:Value>
		</d2p1:KeyValueOfstringstring>
	</Variables>
</NodeGraphContext>
)-- */
