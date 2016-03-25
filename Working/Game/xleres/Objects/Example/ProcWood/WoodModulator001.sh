// CompoundDocument:1
#include "game/xleres/System/Prefix.h"

#include "game/xleres/Nodes/Basic.sh"


void WoodModulator001(float value, float period, out float frac : SV_Target0, out float int : SV_Target1)
{
	float Output_67_result;
	Output_67_result = Divide1( value, period );

	float Output_66_result;
	Output_66_result = Round1( value );

	float Output_63_result;
	Output_63_result = Subtract1( Output_67_result, Output_66_result );

	float Output_64_result;
	Output_64_result = Multiply1( Output_66_result, period );

	float Output_65_result;
	Output_65_result = Multiply1( Output_63_result, period );

	frac = Output_65_result;
	int = Output_64_result;

}
/* <<Chunk:NodeGraph:WoodModulator001>>--(
<?xml version="1.0" encoding="utf-8"?>
<NodeGraph xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://schemas.datacontract.org/2004/07/ShaderPatcherLayer">
	<ConstantConnections>
		<ConstantConnection>
			<OutputNodeID>67</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Value>&lt;value&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>67</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>&lt;period&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>66</OutputNodeID>
			<OutputParameterName>multipleOf</OutputParameterName>
			<Value>.5</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>64</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>&lt;period&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>65</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>&lt;period&gt;</Value>
		</ConstantConnection>
	</ConstantConnections>
	<InputParameterConnections />
	<NodeConnections>
		<NodeConnection>
			<OutputNodeID>66</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>67</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>63</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>67</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>64</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>66</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>63</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>66</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>65</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>63</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
	</NodeConnections>
	<Nodes>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Divide1</FragmentArchiveName>
			<NodeId>67</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>1</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Round1</FragmentArchiveName>
			<NodeId>66</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>2</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Subtract1</FragmentArchiveName>
			<NodeId>63</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>3</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>64</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>4</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>65</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>5</VisualNodeId>
		</Node>
	</Nodes>
	<OutputParameterConnections>
		<OutputParameterConnection>
			<InputNodeID>65</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<Name>frac</Name>
			<Semantic></Semantic>
			<Type>float</Type>
			<VisualNodeId>0</VisualNodeId>
		</OutputParameterConnection>
		<OutputParameterConnection>
			<InputNodeID>64</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<Name>int</Name>
			<Semantic></Semantic>
			<Type>float</Type>
			<VisualNodeId>0</VisualNodeId>
		</OutputParameterConnection>
	</OutputParameterConnections>
	<PreviewSettingsObjects>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>1</VisualNodeId>
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
				<d4p1:x>2234</d4p1:x>
				<d4p1:y>421</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1331</d4p1:x>
				<d4p1:y>371</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1540</d4p1:x>
				<d4p1:y>433</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1704</d4p1:x>
				<d4p1:y>345</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1921</d4p1:x>
				<d4p1:y>475</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1916</d4p1:x>
				<d4p1:y>391</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
	</VisualNodes>
</NodeGraph>
)-- */
/* <<Chunk:NodeGraphContext:WoodModulator001>>--(
<?xml version="1.0" encoding="utf-8"?>
<NodeGraphContext xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://schemas.datacontract.org/2004/07/ShaderPatcherLayer">
	<HasTechniqueConfig>false</HasTechniqueConfig>
	<ShaderParameters xmlns:d2p1="http://schemas.microsoft.com/2003/10/Serialization/Arrays" />
	<Variables xmlns:d2p1="http://schemas.microsoft.com/2003/10/Serialization/Arrays" />
</NodeGraphContext>
)-- */
