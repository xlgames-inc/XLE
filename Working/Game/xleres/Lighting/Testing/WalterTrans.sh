// CompoundDocument:1
#include "game/xleres/System/Prefix.h"

#include "game/xleres/Nodes/Basic.sh"
#include "game/xleres/Lighting/LightingAlgorithm.h"
#include "game/xleres/Lighting/SpecularMethods.h"
#include "game/xleres/Lighting/DirectionalResolve.h"


void WalterTrans(float F0, float iorIncident, float iorOutgoing, float roughness, float3 i, float3 o, float3 n, out float result : SV_Target0)
{
	float3 Output_15_result;
	Output_15_result = CalculateHt( i, o, iorIncident, iorOutgoing );

	float Output_4_result;
	Output_4_result = Dot3( Output_15_result, i );

	float Output_3_result;
	Output_3_result = Abs1( Output_4_result );

	float Output_2_result;
	Output_2_result = SchlickFresnelCore( Output_4_result );

	float Output_5_result;
	Output_5_result = Lerp1( F0, 1.f, Output_2_result );

	float Output_6_result;
	Output_6_result = Subtract1( 1.0f, Output_5_result );

	float Output_38_result;
	Output_38_result = Saturate1( Output_6_result );

	float Output_37_result;
	Output_37_result = Square1( iorOutgoing );

	float Output_18_result;
	Output_18_result = Dot3( i, n );

	float Output_23_result;
	Output_23_result = Abs1( Output_18_result );

	float Output_31_result;
	Output_31_result = RoughnessToGAlpha( roughness );

	float Output_22_result;
	Output_22_result = SmithG( Output_23_result, Output_31_result );

	float Output_17_result;
	Output_17_result = Dot3( o, n );

	float Output_21_result;
	Output_21_result = Abs1( Output_17_result );

	float Output_20_result;
	Output_20_result = SmithG( Output_21_result, Output_31_result );

	float Output_11_result;
	Output_11_result = Multiply1( Output_22_result, Output_20_result );

	float Output_13_result;
	Output_13_result = Multiply1( Output_23_result, Output_21_result );

	float Output_16_result;
	Output_16_result = Dot3( Output_15_result, o );

	float Output_25_result;
	Output_25_result = Abs1( Output_16_result );

	float Output_12_result;
	Output_12_result = Multiply1( Output_3_result, Output_25_result );

	float Output_14_result;
	Output_14_result = Divide1( Output_12_result, Output_13_result );

	float Output_28_result;
	Output_28_result = MultiplyMany1( Output_14_result, Output_38_result, Output_37_result, Output_11_result );

	float Output_19_result;
	Output_19_result = Dot3( Output_15_result, n );

	float Output_30_result;
	Output_30_result = RoughnessToDAlpha( roughness );

	float Output_9_result;
	Output_9_result = TrowReitzD( Output_19_result, Output_30_result );

	float Output_24_result;
	Output_24_result = Multiply1( iorOutgoing, Output_16_result );

	float Output_26_result;
	Output_26_result = Multiply1( iorIncident, Output_4_result );

	float Output_29_result;
	Output_29_result = Add1( Output_26_result, Output_24_result );

	float Output_36_result;
	Output_36_result = Square1( Output_29_result );

	float Output_27_result;
	Output_27_result = Multiply1( Output_28_result, Output_9_result );

	float Output_7_result;
	Output_7_result = Divide1( Output_27_result, Output_36_result );

	result = Output_7_result;

}
/* <<Chunk:NodeGraph:WalterTrans>>--(
<?xml version="1.0" encoding="utf-8"?>
<NodeGraph xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://schemas.datacontract.org/2004/07/ShaderPatcherLayer">
	<ConstantConnections>
		<ConstantConnection>
			<OutputNodeID>6</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Value>1.0f</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>5</OutputNodeID>
			<OutputParameterName>min</OutputParameterName>
			<Value>&lt;F0&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>5</OutputNodeID>
			<OutputParameterName>max</OutputParameterName>
			<Value>1.f</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>15</OutputNodeID>
			<OutputParameterName>iorIncident</OutputParameterName>
			<Value>&lt;iorIncident&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>15</OutputNodeID>
			<OutputParameterName>iorOutgoing</OutputParameterName>
			<Value>&lt;iorOutgoing&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>31</OutputNodeID>
			<OutputParameterName>roughness</OutputParameterName>
			<Value>&lt;roughness&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>30</OutputNodeID>
			<OutputParameterName>roughness</OutputParameterName>
			<Value>&lt;roughness&gt;</Value>
		</ConstantConnection>
	</ConstantConnections>
	<InputParameterConnections>
		<InputParameterConnection>
			<OutputNodeID>24</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Default></Default>
			<Name>&lt;iorOutgoing&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>16</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>26</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Default></Default>
			<Name>&lt;iorIncident&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>16</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>37</OutputNodeID>
			<OutputParameterName>value</OutputParameterName>
			<Default></Default>
			<Name>&lt;iorOutgoing&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>16</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>15</OutputNodeID>
			<OutputParameterName>i</OutputParameterName>
			<Default i:nil="true" />
			<Name>&lt;i&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>25</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>15</OutputNodeID>
			<OutputParameterName>o</OutputParameterName>
			<Default></Default>
			<Name>&lt;o&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>25</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>4</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Default i:nil="true" />
			<Name>&lt;i&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>25</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>16</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Default></Default>
			<Name>&lt;o&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>25</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>18</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Default i:nil="true" />
			<Name>&lt;i&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>25</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>18</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Default></Default>
			<Name>&lt;n&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>25</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>17</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Default></Default>
			<Name>&lt;o&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>25</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>17</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Default></Default>
			<Name>&lt;n&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>25</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>19</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Default></Default>
			<Name>&lt;n&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>25</VisualNodeId>
		</InputParameterConnection>
	</InputParameterConnections>
	<NodeConnections>
		<NodeConnection>
			<OutputNodeID>3</OutputNodeID>
			<OutputParameterName>value</OutputParameterName>
			<InputNodeID>4</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>26</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>4</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>2</OutputNodeID>
			<OutputParameterName>VdotH</OutputParameterName>
			<InputNodeID>4</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>12</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>3</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>5</OutputNodeID>
			<OutputParameterName>alpha</OutputParameterName>
			<InputNodeID>2</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>38</OutputNodeID>
			<OutputParameterName>input</OutputParameterName>
			<InputNodeID>6</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>28</OutputNodeID>
			<OutputParameterName>second</OutputParameterName>
			<InputNodeID>38</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>6</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>5</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>27</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>28</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>11</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>22</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>9</OutputNodeID>
			<OutputParameterName>NdotH</OutputParameterName>
			<InputNodeID>19</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>4</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>15</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>16</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>15</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>19</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>15</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>14</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>13</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>22</OutputNodeID>
			<OutputParameterName>NdotV</OutputParameterName>
			<InputNodeID>23</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>13</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>23</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>21</OutputNodeID>
			<OutputParameterName>value</OutputParameterName>
			<InputNodeID>17</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>23</OutputNodeID>
			<OutputParameterName>value</OutputParameterName>
			<InputNodeID>18</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>27</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>9</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>28</OutputNodeID>
			<OutputParameterName>third</OutputParameterName>
			<InputNodeID>37</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>7</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>36</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>36</OutputNodeID>
			<OutputParameterName>value</OutputParameterName>
			<InputNodeID>29</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>20</OutputNodeID>
			<OutputParameterName>alpha</OutputParameterName>
			<InputNodeID>31</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>22</OutputNodeID>
			<OutputParameterName>alpha</OutputParameterName>
			<InputNodeID>31</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>12</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>25</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>13</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>21</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>20</OutputNodeID>
			<OutputParameterName>NdotV</OutputParameterName>
			<InputNodeID>21</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>11</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>20</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>28</OutputNodeID>
			<OutputParameterName>forth</OutputParameterName>
			<InputNodeID>11</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>28</OutputNodeID>
			<OutputParameterName>first</OutputParameterName>
			<InputNodeID>14</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>14</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>12</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>25</OutputNodeID>
			<OutputParameterName>value</OutputParameterName>
			<InputNodeID>16</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>24</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>16</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>29</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>24</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>29</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>26</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>7</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>27</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>9</OutputNodeID>
			<OutputParameterName>alpha</OutputParameterName>
			<InputNodeID>30</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
	</NodeConnections>
	<Nodes>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Dot3</FragmentArchiveName>
			<NodeId>4</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>0</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Abs1</FragmentArchiveName>
			<NodeId>3</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>1</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Lighting\LightingAlgorithm.h:SchlickFresnelCore</FragmentArchiveName>
			<NodeId>2</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>2</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Subtract1</FragmentArchiveName>
			<NodeId>6</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>3</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Saturate1</FragmentArchiveName>
			<NodeId>38</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>4</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Lerp1</FragmentArchiveName>
			<NodeId>5</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>5</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:MultiplyMany1</FragmentArchiveName>
			<NodeId>28</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>6</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Lighting\SpecularMethods.h:SmithG</FragmentArchiveName>
			<NodeId>22</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>7</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Dot3</FragmentArchiveName>
			<NodeId>19</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>8</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Lighting\DirectionalResolve.h:CalculateHt</FragmentArchiveName>
			<NodeId>15</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>9</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>13</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>10</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Abs1</FragmentArchiveName>
			<NodeId>23</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>11</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Dot3</FragmentArchiveName>
			<NodeId>17</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>12</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Dot3</FragmentArchiveName>
			<NodeId>18</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>13</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Lighting\SpecularMethods.h:TrowReitzD</FragmentArchiveName>
			<NodeId>9</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>14</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Square1</FragmentArchiveName>
			<NodeId>37</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>15</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Square1</FragmentArchiveName>
			<NodeId>36</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>17</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Add1</FragmentArchiveName>
			<NodeId>29</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>18</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Lighting\SpecularMethods.h:RoughnessToGAlpha</FragmentArchiveName>
			<NodeId>31</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>19</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Abs1</FragmentArchiveName>
			<NodeId>25</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>20</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Abs1</FragmentArchiveName>
			<NodeId>21</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>21</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Lighting\SpecularMethods.h:SmithG</FragmentArchiveName>
			<NodeId>20</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>22</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>11</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>23</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Divide1</FragmentArchiveName>
			<NodeId>14</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>24</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Divide1</FragmentArchiveName>
			<NodeId>7</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>26</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>12</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>27</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Dot3</FragmentArchiveName>
			<NodeId>16</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>28</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>24</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>29</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>26</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>30</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>27</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>31</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Lighting\SpecularMethods.h:RoughnessToDAlpha</FragmentArchiveName>
			<NodeId>30</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>32</VisualNodeId>
		</Node>
	</Nodes>
	<OutputParameterConnections />
	<PreviewSettingsObjects>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>0</VisualNodeId>
		</PreviewSettings>
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
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>4</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>5</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>6</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>7</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>8</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize>result * .5 + .5.xxx</OutputToVisualize>
			<VisualNodeId>9</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>10</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>11</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>12</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>13</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>14</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>15</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>17</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>18</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>19</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>20</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>21</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>22</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>23</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>24</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>26</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>27</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>28</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>29</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>30</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>31</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>32</VisualNodeId>
		</PreviewSettings>
	</PreviewSettingsObjects>
	<VisualNodes>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>-253</d4p1:x>
				<d4p1:y>465</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>263</d4p1:x>
				<d4p1:y>462</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>631</d4p1:x>
				<d4p1:y>580</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>979</d4p1:x>
				<d4p1:y>568</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1138</d4p1:x>
				<d4p1:y>604</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>823</d4p1:x>
				<d4p1:y>575</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1438</d4p1:x>
				<d4p1:y>773</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>924</d4p1:x>
				<d4p1:y>829</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>-253</d4p1:x>
				<d4p1:y>620</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>-561</d4p1:x>
				<d4p1:y>429</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>659</d4p1:x>
				<d4p1:y>464</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>263</d4p1:x>
				<d4p1:y>564</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>-253</d4p1:x>
				<d4p1:y>773</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>-253</d4p1:x>
				<d4p1:y>699</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1123</d4p1:x>
				<d4p1:y>978</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1123</d4p1:x>
				<d4p1:y>723</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>-257</d4p1:x>
				<d4p1:y>1015</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1666</d4p1:x>
				<d4p1:y>1171</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>923</d4p1:x>
				<d4p1:y>1219</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>551</d4p1:x>
				<d4p1:y>969</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>263</d4p1:x>
				<d4p1:y>513</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>263</d4p1:x>
				<d4p1:y>610</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>924</d4p1:x>
				<d4p1:y>880</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1123</d4p1:x>
				<d4p1:y>846</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1123</d4p1:x>
				<d4p1:y>418</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>-836.2657</d4p1:x>
				<d4p1:y>582.577942</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1919</d4p1:x>
				<d4p1:y>1119</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>660</d4p1:x>
				<d4p1:y>367</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>-253</d4p1:x>
				<d4p1:y>540</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>681</d4p1:x>
				<d4p1:y>1256</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>681</d4p1:x>
				<d4p1:y>1175</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1666</d4p1:x>
				<d4p1:y>951</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>836</d4p1:x>
				<d4p1:y>1003</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
	</VisualNodes>
</NodeGraph>
)-- */
/* <<Chunk:NodeGraphContext:WalterTrans>>--(
<?xml version="1.0" encoding="utf-8"?>
<NodeGraphContext xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://schemas.datacontract.org/2004/07/ShaderPatcherLayer">
	<HasTechniqueConfig>false</HasTechniqueConfig>
	<ShaderParameters xmlns:d2p1="http://schemas.microsoft.com/2003/10/Serialization/Arrays" />
	<Variables xmlns:d2p1="http://schemas.microsoft.com/2003/10/Serialization/Arrays">
		<d2p1:KeyValueOfstringstring>
			<d2p1:Key>iorIncident</d2p1:Key>
			<d2p1:Value>1.0f/1.33f</d2p1:Value>
		</d2p1:KeyValueOfstringstring>
		<d2p1:KeyValueOfstringstring>
			<d2p1:Key>iorOutgoing</d2p1:Key>
			<d2p1:Value>1</d2p1:Value>
		</d2p1:KeyValueOfstringstring>
		<d2p1:KeyValueOfstringstring>
			<d2p1:Key>F0</d2p1:Key>
			<d2p1:Value>.35</d2p1:Value>
		</d2p1:KeyValueOfstringstring>
		<d2p1:KeyValueOfstringstring>
			<d2p1:Key>roughness</d2p1:Key>
			<d2p1:Value>.33</d2p1:Value>
		</d2p1:KeyValueOfstringstring>
		<d2p1:KeyValueOfstringstring>
			<d2p1:Key>n</d2p1:Key>
			<d2p1:Value>Function:BuildNormal</d2p1:Value>
		</d2p1:KeyValueOfstringstring>
		<d2p1:KeyValueOfstringstring>
			<d2p1:Key>i</d2p1:Key>
			<d2p1:Value>Function:BuildRefractionIncident</d2p1:Value>
		</d2p1:KeyValueOfstringstring>
		<d2p1:KeyValueOfstringstring>
			<d2p1:Key>o</d2p1:Key>
			<d2p1:Value>Function:BuildRefractionOutgoing</d2p1:Value>
		</d2p1:KeyValueOfstringstring>
	</Variables>
</NodeGraphContext>
)-- */
