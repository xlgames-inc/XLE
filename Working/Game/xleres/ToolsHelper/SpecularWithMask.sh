// CompoundDocument:1
#include "game/xleres/System/Prefix.h"
#include "game/xleres/System/BuildInterpolators.h"

#include "game/xleres/ToolsHelper/DecodeParametersTexture_ColoredSpecular.sh"
#include "game/xleres/Nodes/Texture.sh"
#include "game/xleres/Nodes/Basic.sh"
#include "game/xleres/Nodes/MaterialParam.sh"
#include "game/xleres/Nodes/Output.sh"


Texture2D Mask;
Texture2D Diffuse;
Texture2D Specular;
cbuffer BasicMaterialConstants
{
	float2 RRoughnessRange;
	float2 RSpecularRange;
	float2 RMetalRange;
	float2 GRoughnessRange;
	float2 GSpecularRange;
	float2 GMetalRange;
	float2 ARoughnessRange;
	float2 ASpecularRange;
	float2 AMetalRange;
	float2 BRoughnessRange;
	float2 BSpecularRange;
	float2 BMetalRange;
}
void SpecularWithMask(float2 texCoord : TEXCOORD0, uint2 pixelCoords, out float4 paramTex : SV_Target0)
{
	float4 Output_43_result;
	Output_43_result = Sample( Specular, texCoord );

	float3 Output_34_rgb;
	float Output_34_alpha;
	SeparateAlpha( Output_43_result, Output_34_rgb, Output_34_alpha );

	float4 Output_42_result;
	Output_42_result = Sample( Diffuse, texCoord );

	float3 Output_14_rgb;
	float Output_14_alpha;
	SeparateAlpha( Output_42_result, Output_14_rgb, Output_14_alpha );

	float3 Output_30_finalDiffuseSample;
	CommonMaterialParam Output_30_materialParam;
	DecodeParametersTexture_ColoredSpecular( RRoughnessRange, RSpecularRange, RMetalRange, Output_14_rgb, Output_34_rgb, Output_30_finalDiffuseSample, Output_30_materialParam );

	float4 Output_33_result;
	Output_33_result = LoadAbsolute( Mask, pixelCoords );

	float Output_31_r;
	float Output_31_g;
	float Output_31_b;
	float Output_31_a;
	Separate4( Output_33_result, Output_31_r, Output_31_g, Output_31_b, Output_31_a );

	float Output_32_result;
	Output_32_result = Remap1( Output_31_a, float2(0,1), float2(1,0) );

	float4 Output_16_result;
	Combine4( Output_31_r, Output_31_g, Output_31_b, Output_32_result, Output_16_result );

	float Output_17_result;
	Output_17_result = AddMany1( Output_31_r, Output_31_g, Output_31_b, Output_32_result );

	float4 Output_15_result;
	Output_15_result = Divide4Scalar( Output_16_result, Output_17_result );

	float Output_18_r;
	float Output_18_g;
	float Output_18_b;
	float Output_18_a;
	Separate4( Output_15_result, Output_18_r, Output_18_g, Output_18_b, Output_18_a );

	CommonMaterialParam Output_26_result;
	Output_26_result = Scale( Output_30_materialParam, Output_18_r );

	float3 Output_29_finalDiffuseSample;
	CommonMaterialParam Output_29_materialParam;
	DecodeParametersTexture_ColoredSpecular( GRoughnessRange, GSpecularRange, GMetalRange, Output_14_rgb, Output_34_rgb, Output_29_finalDiffuseSample, Output_29_materialParam );

	CommonMaterialParam Output_25_result;
	Output_25_result = Scale( Output_29_materialParam, Output_18_g );

	CommonMaterialParam Output_24_result;
	Output_24_result = Add( Output_26_result, Output_25_result );

	float3 Output_27_finalDiffuseSample;
	CommonMaterialParam Output_27_materialParam;
	DecodeParametersTexture_ColoredSpecular( ARoughnessRange, ASpecularRange, AMetalRange, Output_14_rgb, Output_34_rgb, Output_27_finalDiffuseSample, Output_27_materialParam );

	CommonMaterialParam Output_21_result;
	Output_21_result = Scale( Output_27_materialParam, Output_18_a );

	float3 Output_28_finalDiffuseSample;
	CommonMaterialParam Output_28_materialParam;
	DecodeParametersTexture_ColoredSpecular( BRoughnessRange, BSpecularRange, BMetalRange, Output_14_rgb, Output_34_rgb, Output_28_finalDiffuseSample, Output_28_materialParam );

	CommonMaterialParam Output_23_result;
	Output_23_result = Scale( Output_28_materialParam, Output_18_b );

	CommonMaterialParam Output_22_result;
	Output_22_result = Add( Output_24_result, Output_23_result );

	CommonMaterialParam Output_20_result;
	Output_20_result = Add( Output_22_result, Output_21_result );

	float4 Output_19_result;
	Output_19_result = Output_ParamTex( Output_20_result );

	paramTex = Output_19_result;

}
/* <<Chunk:NodeGraph:SpecularWithMask>>--(
<?xml version="1.0" encoding="utf-8"?>
<NodeGraph xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://schemas.datacontract.org/2004/07/ShaderPatcherLayer">
	<ConstantConnections>
		<ConstantConnection>
			<OutputNodeID>32</OutputNodeID>
			<OutputParameterName>inputRange</OutputParameterName>
			<Value>float2(0,1)</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>32</OutputNodeID>
			<OutputParameterName>outputRange</OutputParameterName>
			<Value>float2(1,0)</Value>
		</ConstantConnection>
	</ConstantConnections>
	<InputParameterConnections>
		<InputParameterConnection>
			<OutputNodeID>33</OutputNodeID>
			<OutputParameterName>inputTexture</OutputParameterName>
			<Name>Mask</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>5</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>42</OutputNodeID>
			<OutputParameterName>inputTexture</OutputParameterName>
			<Name>Diffuse</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>5</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>43</OutputNodeID>
			<OutputParameterName>inputTexture</OutputParameterName>
			<Name>Specular</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>5</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>30</OutputNodeID>
			<OutputParameterName>roughnessRange</OutputParameterName>
			<Name>RRoughnessRange</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>9</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>30</OutputNodeID>
			<OutputParameterName>specularRange</OutputParameterName>
			<Name>RSpecularRange</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>9</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>30</OutputNodeID>
			<OutputParameterName>metalRange</OutputParameterName>
			<Name>RMetalRange</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>9</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>29</OutputNodeID>
			<OutputParameterName>roughnessRange</OutputParameterName>
			<Name>GRoughnessRange</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>10</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>29</OutputNodeID>
			<OutputParameterName>specularRange</OutputParameterName>
			<Name>GSpecularRange</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>10</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>29</OutputNodeID>
			<OutputParameterName>metalRange</OutputParameterName>
			<Name>GMetalRange</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>10</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>27</OutputNodeID>
			<OutputParameterName>roughnessRange</OutputParameterName>
			<Name>ARoughnessRange</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>11</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>27</OutputNodeID>
			<OutputParameterName>specularRange</OutputParameterName>
			<Name>ASpecularRange</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>11</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>27</OutputNodeID>
			<OutputParameterName>metalRange</OutputParameterName>
			<Name>AMetalRange</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>11</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>28</OutputNodeID>
			<OutputParameterName>roughnessRange</OutputParameterName>
			<Name>BRoughnessRange</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>12</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>28</OutputNodeID>
			<OutputParameterName>specularRange</OutputParameterName>
			<Name>BSpecularRange</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>12</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>28</OutputNodeID>
			<OutputParameterName>metalRange</OutputParameterName>
			<Name>BMetalRange</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>12</VisualNodeId>
		</InputParameterConnection>
	</InputParameterConnections>
	<NodeConnections>
		<NodeConnection>
			<OutputNodeID>26</OutputNodeID>
			<OutputParameterName>input</OutputParameterName>
			<InputNodeID>30</InputNodeID>
			<InputParameterName>materialParam</InputParameterName>
			<InputType>CommonMaterialParam</InputType>
			<OutputType>CommonMaterialParam</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>14</OutputNodeID>
			<OutputParameterName>input</OutputParameterName>
			<InputNodeID>42</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float4</InputType>
			<OutputType>float4</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>34</OutputNodeID>
			<OutputParameterName>input</OutputParameterName>
			<InputNodeID>43</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float4</InputType>
			<OutputType>float4</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>30</OutputNodeID>
			<OutputParameterName>specColorSample</OutputParameterName>
			<InputNodeID>34</InputNodeID>
			<InputParameterName>rgb</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>29</OutputNodeID>
			<OutputParameterName>specColorSample</OutputParameterName>
			<InputNodeID>34</InputNodeID>
			<InputParameterName>rgb</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>28</OutputNodeID>
			<OutputParameterName>specColorSample</OutputParameterName>
			<InputNodeID>34</InputNodeID>
			<InputParameterName>rgb</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>27</OutputNodeID>
			<OutputParameterName>specColorSample</OutputParameterName>
			<InputNodeID>34</InputNodeID>
			<InputParameterName>rgb</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>30</OutputNodeID>
			<OutputParameterName>diffuseSample</OutputParameterName>
			<InputNodeID>14</InputNodeID>
			<InputParameterName>rgb</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>29</OutputNodeID>
			<OutputParameterName>diffuseSample</OutputParameterName>
			<InputNodeID>14</InputNodeID>
			<InputParameterName>rgb</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>28</OutputNodeID>
			<OutputParameterName>diffuseSample</OutputParameterName>
			<InputNodeID>14</InputNodeID>
			<InputParameterName>rgb</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>27</OutputNodeID>
			<OutputParameterName>diffuseSample</OutputParameterName>
			<InputNodeID>14</InputNodeID>
			<InputParameterName>rgb</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>24</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>26</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>CommonMaterialParam</InputType>
			<OutputType>CommonMaterialParam</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>22</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>24</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>CommonMaterialParam</InputType>
			<OutputType>CommonMaterialParam</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>18</OutputNodeID>
			<OutputParameterName>input</OutputParameterName>
			<InputNodeID>15</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float4</InputType>
			<OutputType>float4</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>15</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>16</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float4</InputType>
			<OutputType>float4</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>15</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>17</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>26</OutputNodeID>
			<OutputParameterName>factor</OutputParameterName>
			<InputNodeID>18</InputNodeID>
			<InputParameterName>r</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>25</OutputNodeID>
			<OutputParameterName>factor</OutputParameterName>
			<InputNodeID>18</InputNodeID>
			<InputParameterName>g</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>23</OutputNodeID>
			<OutputParameterName>factor</OutputParameterName>
			<InputNodeID>18</InputNodeID>
			<InputParameterName>b</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>21</OutputNodeID>
			<OutputParameterName>factor</OutputParameterName>
			<InputNodeID>18</InputNodeID>
			<InputParameterName>a</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>19</OutputNodeID>
			<OutputParameterName>param</OutputParameterName>
			<InputNodeID>20</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>CommonMaterialParam</InputType>
			<OutputType>CommonMaterialParam</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>20</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>21</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>CommonMaterialParam</InputType>
			<OutputType>CommonMaterialParam</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>20</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>22</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>CommonMaterialParam</InputType>
			<OutputType>CommonMaterialParam</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>22</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>23</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>CommonMaterialParam</InputType>
			<OutputType>CommonMaterialParam</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>24</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>25</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>CommonMaterialParam</InputType>
			<OutputType>CommonMaterialParam</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>21</OutputNodeID>
			<OutputParameterName>input</OutputParameterName>
			<InputNodeID>27</InputNodeID>
			<InputParameterName>materialParam</InputParameterName>
			<InputType>CommonMaterialParam</InputType>
			<OutputType>CommonMaterialParam</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>23</OutputNodeID>
			<OutputParameterName>input</OutputParameterName>
			<InputNodeID>28</InputNodeID>
			<InputParameterName>materialParam</InputParameterName>
			<InputType>CommonMaterialParam</InputType>
			<OutputType>CommonMaterialParam</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>25</OutputNodeID>
			<OutputParameterName>input</OutputParameterName>
			<InputNodeID>29</InputNodeID>
			<InputParameterName>materialParam</InputParameterName>
			<InputType>CommonMaterialParam</InputType>
			<OutputType>CommonMaterialParam</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>32</OutputNodeID>
			<OutputParameterName>input</OutputParameterName>
			<InputNodeID>31</InputNodeID>
			<InputParameterName>a</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>17</OutputNodeID>
			<OutputParameterName>third</OutputParameterName>
			<InputNodeID>31</InputNodeID>
			<InputParameterName>b</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>17</OutputNodeID>
			<OutputParameterName>second</OutputParameterName>
			<InputNodeID>31</InputNodeID>
			<InputParameterName>g</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>17</OutputNodeID>
			<OutputParameterName>first</OutputParameterName>
			<InputNodeID>31</InputNodeID>
			<InputParameterName>r</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>16</OutputNodeID>
			<OutputParameterName>r</OutputParameterName>
			<InputNodeID>31</InputNodeID>
			<InputParameterName>r</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>16</OutputNodeID>
			<OutputParameterName>g</OutputParameterName>
			<InputNodeID>31</InputNodeID>
			<InputParameterName>g</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>16</OutputNodeID>
			<OutputParameterName>b</OutputParameterName>
			<InputNodeID>31</InputNodeID>
			<InputParameterName>b</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>17</OutputNodeID>
			<OutputParameterName>forth</OutputParameterName>
			<InputNodeID>32</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>16</OutputNodeID>
			<OutputParameterName>a</OutputParameterName>
			<InputNodeID>32</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>31</OutputNodeID>
			<OutputParameterName>input</OutputParameterName>
			<InputNodeID>33</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float4</InputType>
			<OutputType>float4</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
	</NodeConnections>
	<Nodes>
		<Node>
			<FragmentArchiveName>game/xleres/ToolsHelper\DecodeParametersTexture_ColoredSpecular.sh:DecodeParametersTexture_ColoredSpecular</FragmentArchiveName>
			<NodeId>30</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>0</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Texture.sh:Sample</FragmentArchiveName>
			<NodeId>42</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>1</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Texture.sh:Sample</FragmentArchiveName>
			<NodeId>43</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>2</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:SeparateAlpha</FragmentArchiveName>
			<NodeId>34</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>3</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:SeparateAlpha</FragmentArchiveName>
			<NodeId>14</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>4</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\MaterialParam.sh:Scale</FragmentArchiveName>
			<NodeId>26</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>6</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\MaterialParam.sh:Add</FragmentArchiveName>
			<NodeId>24</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>7</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Divide4Scalar</FragmentArchiveName>
			<NodeId>15</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>13</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Combine4</FragmentArchiveName>
			<NodeId>16</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>14</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:AddMany1</FragmentArchiveName>
			<NodeId>17</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>15</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Separate4</FragmentArchiveName>
			<NodeId>18</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>16</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Output.sh:Output_ParamTex</FragmentArchiveName>
			<NodeId>19</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>17</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\MaterialParam.sh:Add</FragmentArchiveName>
			<NodeId>20</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>18</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\MaterialParam.sh:Scale</FragmentArchiveName>
			<NodeId>21</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>19</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\MaterialParam.sh:Add</FragmentArchiveName>
			<NodeId>22</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>20</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\MaterialParam.sh:Scale</FragmentArchiveName>
			<NodeId>23</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>21</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\MaterialParam.sh:Scale</FragmentArchiveName>
			<NodeId>25</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>22</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/ToolsHelper\DecodeParametersTexture_ColoredSpecular.sh:DecodeParametersTexture_ColoredSpecular</FragmentArchiveName>
			<NodeId>27</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>23</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/ToolsHelper\DecodeParametersTexture_ColoredSpecular.sh:DecodeParametersTexture_ColoredSpecular</FragmentArchiveName>
			<NodeId>28</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>24</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/ToolsHelper\DecodeParametersTexture_ColoredSpecular.sh:DecodeParametersTexture_ColoredSpecular</FragmentArchiveName>
			<NodeId>29</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>25</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Separate4</FragmentArchiveName>
			<NodeId>31</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>26</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Remap1</FragmentArchiveName>
			<NodeId>32</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>27</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Texture.sh:LoadAbsolute</FragmentArchiveName>
			<NodeId>33</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>28</VisualNodeId>
		</Node>
	</Nodes>
	<OutputParameterConnections>
		<OutputParameterConnection>
			<InputNodeID>19</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<Name>paramTex</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>8</VisualNodeId>
		</OutputParameterConnection>
	</OutputParameterConnections>
	<PreviewSettingsObjects>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize>result2.specular</OutputToVisualize>
			<VisualNodeId>0</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
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
			<OutputToVisualize>result.metal</OutputToVisualize>
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
			<VisualNodeId>13</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
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
			<VisualNodeId>16</VisualNodeId>
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
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>23</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>24</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>25</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>26</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>27</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>28</VisualNodeId>
		</PreviewSettings>
	</PreviewSettingsObjects>
	<VisualNodes>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1197</d4p1:x>
				<d4p1:y>2</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>-21</d4p1:x>
				<d4p1:y>178</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>-21</d4p1:x>
				<d4p1:y>286</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>225</d4p1:x>
				<d4p1:y>286</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>225</d4p1:x>
				<d4p1:y>178</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>-484</d4p1:x>
				<d4p1:y>288</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1824</d4p1:x>
				<d4p1:y>46</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>2028</d4p1:x>
				<d4p1:y>156</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>3223</d4p1:x>
				<d4p1:y>498</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>804</d4p1:x>
				<d4p1:y>-61</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>804</d4p1:x>
				<d4p1:y>83</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>804</d4p1:x>
				<d4p1:y>427</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>804</d4p1:x>
				<d4p1:y>251</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1061</d4p1:x>
				<d4p1:y>715</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>822</d4p1:x>
				<d4p1:y>678</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>822</d4p1:x>
				<d4p1:y>764</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1291</d4p1:x>
				<d4p1:y>715</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>2638</d4p1:x>
				<d4p1:y>455</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>2383</d4p1:x>
				<d4p1:y>336</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1824</d4p1:x>
				<d4p1:y>431</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>2205</d4p1:x>
				<d4p1:y>248</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1824</d4p1:x>
				<d4p1:y>301</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1824</d4p1:x>
				<d4p1:y>179</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1193</d4p1:x>
				<d4p1:y>480</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1193</d4p1:x>
				<d4p1:y>315</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1193</d4p1:x>
				<d4p1:y>147</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>337</d4p1:x>
				<d4p1:y>708</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>598</d4p1:x>
				<d4p1:y>857</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>0</d4p1:x>
				<d4p1:y>826</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
	</VisualNodes>
</NodeGraph>
)-- */
/* <<Chunk:NodeGraphContext:SpecularWithMask>>--(
<?xml version="1.0" encoding="utf-8"?>
<NodeGraphContext xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://schemas.datacontract.org/2004/07/ShaderPatcherLayer">
	<HasTechniqueConfig>false</HasTechniqueConfig>
	<ShaderParameters xmlns:d2p1="http://schemas.microsoft.com/2003/10/Serialization/Arrays" />
	<Variables xmlns:d2p1="http://schemas.microsoft.com/2003/10/Serialization/Arrays" />
</NodeGraphContext>
)-- */
