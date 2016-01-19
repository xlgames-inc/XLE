// CompoundDocument:1
#include "game/xleres/System/Prefix.h"
#include "game/xleres/System/BuildInterpolators.h"

#include "game/xleres/Nodes/MaterialParam.sh"
#include "game/xleres/Nodes/Basic.sh"
#include "game/xleres/Nodes/Color.sh"


void DecodeParametersTexture_ColoredSpecular(float2 roughnessRange, float2 specularRange, float2 metalRange, float3 diffuseSample, float3 specColorSample, out float3 finalDiffuseSample, out CommonMaterialParam materialParam)
{
	float Output_15_result;
	Output_15_result = Luminance( specColorSample );

	float Output_17_result;
	Output_17_result = Saturate1( Output_15_result );

	float Output_14_result;
	Output_14_result = Luminance( diffuseSample );

	float Output_16_result;
	Output_16_result = Saturate1( Output_14_result );

	float Output_18_result;
	Output_18_result = Add1( Output_16_result, 0.00001f );

	float Output_19_result;
	Output_19_result = Divide1( Output_17_result, Output_18_result );

	float Output_31_result;
	Output_31_result = Remap1( Output_19_result, float2(1.1f, 2.f), float2(0.f, 1.f) );

	float Output_32_result;
	Output_32_result = Saturate1( Output_31_result );

	CommonMaterialParam Output_23_result;
	Output_23_result = CommonMaterialParam_Make( 0, Output_15_result, Output_32_result );

	CommonMaterialParam Output_22_result;
	Output_22_result = ScaleByRange( Output_23_result, roughnessRange, specularRange, metalRange );

	float3 Output_24_result;
	Output_24_result = Lerp3( diffuseSample, specColorSample, Output_32_result );

	finalDiffuseSample = Output_24_result;
	materialParam = Output_22_result;

}
/* <<Chunk:NodeGraph:DecodeParametersTexture_ColoredSpecular>>--(
<?xml version="1.0" encoding="utf-8"?>
<NodeGraph xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://schemas.datacontract.org/2004/07/ShaderPatcherLayer">
	<ConstantConnections>
		<ConstantConnection>
			<OutputNodeID>22</OutputNodeID>
			<OutputParameterName>rRange</OutputParameterName>
			<Value>&lt;roughnessRange&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>22</OutputNodeID>
			<OutputParameterName>sRange</OutputParameterName>
			<Value>&lt;specularRange&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>22</OutputNodeID>
			<OutputParameterName>mRange</OutputParameterName>
			<Value>&lt;metalRange&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>23</OutputNodeID>
			<OutputParameterName>roughness</OutputParameterName>
			<Value>0</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>31</OutputNodeID>
			<OutputParameterName>inputRange</OutputParameterName>
			<Value>float2(1.1f, 2.f)</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>31</OutputNodeID>
			<OutputParameterName>outputRange</OutputParameterName>
			<Value>float2(0.f, 1.f)</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>18</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>0.00001f</Value>
		</ConstantConnection>
	</ConstantConnections>
	<InputParameterConnections>
		<InputParameterConnection>
			<OutputNodeID>14</OutputNodeID>
			<OutputParameterName>srgbInput</OutputParameterName>
			<Name>&lt;diffuseSample&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>1</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>15</OutputNodeID>
			<OutputParameterName>srgbInput</OutputParameterName>
			<Name>&lt;specColorSample&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>1</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>24</OutputNodeID>
			<OutputParameterName>min</OutputParameterName>
			<Name>&lt;diffuseSample&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>1</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>24</OutputNodeID>
			<OutputParameterName>max</OutputParameterName>
			<Name>&lt;specColorSample&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>1</VisualNodeId>
		</InputParameterConnection>
	</InputParameterConnections>
	<NodeConnections>
		<NodeConnection>
			<OutputNodeID>22</OutputNodeID>
			<OutputParameterName>input</OutputParameterName>
			<InputNodeID>23</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>CommonMaterialParam</InputType>
			<OutputType>CommonMaterialParam</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>23</OutputNodeID>
			<OutputParameterName>metal</OutputParameterName>
			<InputNodeID>32</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>24</OutputNodeID>
			<OutputParameterName>alpha</OutputParameterName>
			<InputNodeID>32</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>32</OutputNodeID>
			<OutputParameterName>input</OutputParameterName>
			<InputNodeID>31</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>31</OutputNodeID>
			<OutputParameterName>input</OutputParameterName>
			<InputNodeID>19</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>17</OutputNodeID>
			<OutputParameterName>input</OutputParameterName>
			<InputNodeID>15</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>23</OutputNodeID>
			<OutputParameterName>specular</OutputParameterName>
			<InputNodeID>15</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>19</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>17</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>16</OutputNodeID>
			<OutputParameterName>input</OutputParameterName>
			<InputNodeID>14</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>19</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>18</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>18</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>16</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
	</NodeConnections>
	<Nodes>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\MaterialParam.sh:ScaleByRange</FragmentArchiveName>
			<NodeId>22</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>0</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\MaterialParam.sh:CommonMaterialParam_Make</FragmentArchiveName>
			<NodeId>23</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>2</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Saturate1</FragmentArchiveName>
			<NodeId>32</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>4</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Remap1</FragmentArchiveName>
			<NodeId>31</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>5</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Lerp3</FragmentArchiveName>
			<NodeId>24</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>6</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Divide1</FragmentArchiveName>
			<NodeId>19</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>7</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Color.sh:Luminance</FragmentArchiveName>
			<NodeId>15</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>8</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Saturate1</FragmentArchiveName>
			<NodeId>17</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>9</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Color.sh:Luminance</FragmentArchiveName>
			<NodeId>14</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>10</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Add1</FragmentArchiveName>
			<NodeId>18</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>11</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Saturate1</FragmentArchiveName>
			<NodeId>16</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>12</VisualNodeId>
		</Node>
	</Nodes>
	<OutputParameterConnections>
		<OutputParameterConnection>
			<InputNodeID>24</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<Name>finalDiffuseSample</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>3</VisualNodeId>
		</OutputParameterConnection>
		<OutputParameterConnection>
			<InputNodeID>22</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<Name>materialParam</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>3</VisualNodeId>
		</OutputParameterConnection>
	</OutputParameterConnections>
	<PreviewSettingsObjects>
		<PreviewSettings>
			<Geometry>Chart</Geometry>
			<OutputToVisualize>result.metal</OutputToVisualize>
			<VisualNodeId>0</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize>result.metal</OutputToVisualize>
			<VisualNodeId>2</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Chart</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>4</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Chart</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>5</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Chart</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>6</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>7</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>8</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>9</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>10</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>11</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>12</VisualNodeId>
		</PreviewSettings>
	</PreviewSettingsObjects>
	<VisualNodes>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>2068</d4p1:x>
				<d4p1:y>242</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>-215</d4p1:x>
				<d4p1:y>271</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1665</d4p1:x>
				<d4p1:y>379</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>2513</d4p1:x>
				<d4p1:y>460</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1297</d4p1:x>
				<d4p1:y>557</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1075</d4p1:x>
				<d4p1:y>558</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1787</d4p1:x>
				<d4p1:y>506</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>800</d4p1:x>
				<d4p1:y>537</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>172</d4p1:x>
				<d4p1:y>457</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>406</d4p1:x>
				<d4p1:y>500</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>168</d4p1:x>
				<d4p1:y>572</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>637</d4p1:x>
				<d4p1:y>576</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>406</d4p1:x>
				<d4p1:y>572</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
	</VisualNodes>
</NodeGraph>
)-- */
/* <<Chunk:NodeGraphContext:DecodeParametersTexture_ColoredSpecular>>--(
<?xml version="1.0" encoding="utf-8"?>
<NodeGraphContext xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://schemas.datacontract.org/2004/07/ShaderPatcherLayer">
	<HasTechniqueConfig>false</HasTechniqueConfig>
	<ShaderParameters xmlns:d2p1="http://schemas.microsoft.com/2003/10/Serialization/Arrays" />
	<Variables xmlns:d2p1="http://schemas.microsoft.com/2003/10/Serialization/Arrays">
		<d2p1:KeyValueOfstringstring>
			<d2p1:Key>diffuseSample</d2p1:Key>
			<d2p1:Value>AutoAxis:0.0.xxx:1.0.xxx</d2p1:Value>
		</d2p1:KeyValueOfstringstring>
		<d2p1:KeyValueOfstringstring>
			<d2p1:Key>specColorSample</d2p1:Key>
			<d2p1:Value>0.5.xxx</d2p1:Value>
		</d2p1:KeyValueOfstringstring>
		<d2p1:KeyValueOfstringstring>
			<d2p1:Key>roughnessRange</d2p1:Key>
			<d2p1:Value>float2(0, 1)</d2p1:Value>
		</d2p1:KeyValueOfstringstring>
		<d2p1:KeyValueOfstringstring>
			<d2p1:Key>specularRange</d2p1:Key>
			<d2p1:Value>float2(0, 1)</d2p1:Value>
		</d2p1:KeyValueOfstringstring>
		<d2p1:KeyValueOfstringstring>
			<d2p1:Key>metalRange</d2p1:Key>
			<d2p1:Value>float2(0, 1)</d2p1:Value>
		</d2p1:KeyValueOfstringstring>
	</Variables>
</NodeGraphContext>
)-- */
