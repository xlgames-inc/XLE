// CompoundDocument:1
#include "game/xleres/System/Prefix.h"

#include "game/xleres/Nodes/Texture.sh"
#include "game/xleres/Nodes/Basic.sh"


void WoodGnarlGrainStain(float gnarlDensity, float3 coords, float gnarlStrength, float grainStrength, float stain, float grainDensity, out float grain : SV_Target0, out float3 gnarledCoords : SV_Target1)
{
	float Output_57_fac;
	float3 Output_57_result;
	Output_57_result = NoiseTexture3( coords, gnarlDensity, 4, 5, Output_57_fac );

	float3 Output_56_result;
	Output_56_result = Subtract3( Output_57_result, float3(0.5, 0.5, 0.5) );

	float3 Output_46_result;
	Output_46_result = Multiply3Scalar( Output_56_result, gnarlStrength );

	float3 Output_53_result;
	Output_53_result = Add3( coords, Output_46_result );

	float3 Output_55_result;
	Output_55_result = Multiply3( Output_53_result, float3(.1, 2, 2) );

	float Output_54_fac;
	float3 Output_54_result;
	Output_54_result = NoiseTexture3( Output_55_result, grainDensity, 2.0f, 0.0f, Output_54_fac );

	float Output_47_result;
	Output_47_result = Multiply1( Output_54_fac, grainStrength );

	float Output_48_result;
	Output_48_result = Multiply1( grainStrength, .5 );

	float Output_49_result;
	Output_49_result = Subtract1( Output_48_result, .5 );

	float Output_51_result;
	Output_51_result = Multiply1( stain, .5 );

	float Output_58_result;
	Output_58_result = Subtract1( Output_51_result, .5 );

	float Output_59_result;
	Output_59_result = Multiply1( Output_57_fac, stain );

	float Output_50_result;
	Output_50_result = Subtract1( Output_59_result, Output_58_result );

	float Output_61_result;
	Output_61_result = Subtract1( Output_47_result, Output_49_result );

	float Output_60_result;
	Output_60_result = Add1( Output_61_result, Output_50_result );

	float Output_52_result;
	Output_52_result = Multiply1( Output_60_result, .5 );

	grain = Output_52_result;
	gnarledCoords = Output_55_result;

}
/* <<Chunk:NodeGraph:WoodGnarlGrainStain>>--(
<?xml version="1.0" encoding="utf-8"?>
<NodeGraph xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://schemas.datacontract.org/2004/07/ShaderPatcherLayer">
	<ConstantConnections>
		<ConstantConnection>
			<OutputNodeID>57</OutputNodeID>
			<OutputParameterName>scale</OutputParameterName>
			<Value>&lt;gnarlDensity&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>57</OutputNodeID>
			<OutputParameterName>detail</OutputParameterName>
			<Value>4</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>57</OutputNodeID>
			<OutputParameterName>distortion</OutputParameterName>
			<Value>5</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>57</OutputNodeID>
			<OutputParameterName>position</OutputParameterName>
			<Value>&lt;coords&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>46</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>&lt;gnarlStrength&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>47</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>&lt;grainStrength&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>48</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Value>&lt;grainStrength&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>48</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>.5</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>49</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>.5</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>51</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>.5</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>51</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Value>&lt;stain&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>52</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>.5</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>53</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Value>&lt;coords&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>54</OutputNodeID>
			<OutputParameterName>detail</OutputParameterName>
			<Value>2.0f</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>54</OutputNodeID>
			<OutputParameterName>scale</OutputParameterName>
			<Value>&lt;grainDensity&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>54</OutputNodeID>
			<OutputParameterName>distortion</OutputParameterName>
			<Value>0.0f</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>55</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>float3(.1, 2, 2)</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>56</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>float3(0.5, 0.5, 0.5)</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>58</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>.5</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>59</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>&lt;stain&gt;</Value>
		</ConstantConnection>
	</ConstantConnections>
	<InputParameterConnections />
	<NodeConnections>
		<NodeConnection>
			<OutputNodeID>59</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>57</InputNodeID>
			<InputParameterName>fac</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>56</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>57</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>60</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>61</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>53</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>46</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>61</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>47</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>49</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>48</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>61</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>49</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>60</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>50</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>58</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>51</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>55</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>53</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>47</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>54</InputNodeID>
			<InputParameterName>fac</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>54</OutputNodeID>
			<OutputParameterName>position</OutputParameterName>
			<InputNodeID>55</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>46</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>56</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>50</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>58</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>50</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>59</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>52</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>60</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
	</NodeConnections>
	<Nodes>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Texture.sh:NoiseTexture3</FragmentArchiveName>
			<NodeId>57</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>0</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Subtract1</FragmentArchiveName>
			<NodeId>61</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>2</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Multiply3Scalar</FragmentArchiveName>
			<NodeId>46</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>3</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>47</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>4</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>48</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>5</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Subtract1</FragmentArchiveName>
			<NodeId>49</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>6</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Subtract1</FragmentArchiveName>
			<NodeId>50</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>7</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>51</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>8</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>52</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>9</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Add3</FragmentArchiveName>
			<NodeId>53</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>10</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Texture.sh:NoiseTexture3</FragmentArchiveName>
			<NodeId>54</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>11</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Multiply3</FragmentArchiveName>
			<NodeId>55</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>12</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Subtract3</FragmentArchiveName>
			<NodeId>56</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>13</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Subtract1</FragmentArchiveName>
			<NodeId>58</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>14</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>59</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>15</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Add1</FragmentArchiveName>
			<NodeId>60</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>16</VisualNodeId>
		</Node>
	</Nodes>
	<OutputParameterConnections>
		<OutputParameterConnection>
			<InputNodeID>52</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<Name>grain</Name>
			<Semantic></Semantic>
			<Type>float</Type>
			<VisualNodeId>1</VisualNodeId>
		</OutputParameterConnection>
		<OutputParameterConnection>
			<InputNodeID>55</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<Name>gnarledCoords</Name>
			<Semantic></Semantic>
			<Type>float3</Type>
			<VisualNodeId>1</VisualNodeId>
		</OutputParameterConnection>
	</OutputParameterConnections>
	<PreviewSettingsObjects>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
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
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>6</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
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
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>15</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>16</VisualNodeId>
		</PreviewSettings>
	</PreviewSettingsObjects>
	<VisualNodes>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>-31</d4p1:x>
				<d4p1:y>161</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>3290</d4p1:x>
				<d4p1:y>243</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>2313</d4p1:x>
				<d4p1:y>387</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>689</d4p1:x>
				<d4p1:y>40</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>2051</d4p1:x>
				<d4p1:y>285</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1744</d4p1:x>
				<d4p1:y>425</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>2038</d4p1:x>
				<d4p1:y>412</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>682.8907</d4p1:x>
				<d4p1:y>517.2711</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>-86</d4p1:x>
				<d4p1:y>623</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>2840</d4p1:x>
				<d4p1:y>446</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1060</d4p1:x>
				<d4p1:y>87</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1728</d4p1:x>
				<d4p1:y>229</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1461</d4p1:x>
				<d4p1:y>145</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>411</d4p1:x>
				<d4p1:y>131</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>318</d4p1:x>
				<d4p1:y>603</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>321</d4p1:x>
				<d4p1:y>441</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>2596</d4p1:x>
				<d4p1:y>481</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
	</VisualNodes>
</NodeGraph>
)-- */
/* <<Chunk:NodeGraphContext:WoodGnarlGrainStain>>--(
<?xml version="1.0" encoding="utf-8"?>
<NodeGraphContext xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://schemas.datacontract.org/2004/07/ShaderPatcherLayer">
	<HasTechniqueConfig>false</HasTechniqueConfig>
	<ShaderParameters xmlns:d2p1="http://schemas.microsoft.com/2003/10/Serialization/Arrays" />
	<Variables xmlns:d2p1="http://schemas.microsoft.com/2003/10/Serialization/Arrays" />
</NodeGraphContext>
)-- */
