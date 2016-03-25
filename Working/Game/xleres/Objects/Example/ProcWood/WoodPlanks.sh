// CompoundDocument:1
#include "game/xleres/System/Prefix.h"

#include "game/xleres/ProcMat/Wood/WoodModulator002.sh"
#include "game/xleres/Nodes/Basic.sh"
#include "game/xleres/Nodes/Texture.sh"


void WoodPlanks(float width, float length, float height, float3 offcenter, float shift1, float shift3, float shift2, float3 coords, out float3 plankCoords : SV_Target0)
{
	float Output_95_r;
	float Output_95_g;
	float Output_95_b;
	Separate3( coords, Output_95_r, Output_95_g, Output_95_b );

	float Output_79_int;
	float Output_79_frac;
	WoodModulator002( Output_95_b, height, Output_79_int, Output_79_frac );

	float Output_94_result;
	Output_94_result = Divide1( shift2, height );

	float Output_90_result;
	Output_90_result = Multiply1( length, Output_94_result );

	float Output_87_result;
	Output_87_result = Multiply1( Output_90_result, Output_79_int );

	float Output_93_result;
	Output_93_result = Divide1( shift3, height );

	float Output_91_result;
	Output_91_result = Multiply1( width, Output_93_result );

	float Output_81_result;
	Output_81_result = Multiply1( Output_91_result, Output_79_int );

	float Output_88_result;
	Output_88_result = Add1( Output_95_g, Output_81_result );

	float Output_78_int;
	float Output_78_frac;
	WoodModulator002( Output_88_result, width, Output_78_int, Output_78_frac );

	float Output_92_result;
	Output_92_result = Divide1( shift1, width );

	float Output_89_result;
	Output_89_result = Multiply1( Output_92_result, length );

	float Output_80_result;
	Output_80_result = Multiply1( Output_89_result, Output_78_int );

	float Output_96_result;
	Output_96_result = Add1( Output_95_r, Output_80_result );

	float Output_86_result;
	Output_86_result = Add1( Output_96_result, Output_87_result );

	float Output_76_int;
	float Output_76_frac;
	WoodModulator002( Output_86_result, length, Output_76_int, Output_76_frac );

	float3 Output_85_result;
	Combine3( Output_76_int, Output_78_int, Output_79_int, Output_85_result );

	float Output_75_fac;
	float3 Output_75_result;
	Output_75_result = NoiseTexture3( Output_85_result, 3.142, 2, 0, Output_75_fac );

	float3 Output_82_result;
	Combine3( Output_76_frac, Output_78_frac, Output_79_frac, Output_82_result );

	float3 Output_84_result;
	Output_84_result = Subtract3( Output_75_result, float3(.5, .5, .5) );

	float3 Output_83_result;
	Output_83_result = Multiply3( Output_84_result, offcenter );

	float3 Output_77_result;
	Output_77_result = Add3( Output_82_result, Output_83_result );

	plankCoords = Output_77_result;

}
/* <<Chunk:NodeGraph:WoodPlanks>>--(
<?xml version="1.0" encoding="utf-8"?>
<NodeGraph xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://schemas.datacontract.org/2004/07/ShaderPatcherLayer">
	<ConstantConnections>
		<ConstantConnection>
			<OutputNodeID>78</OutputNodeID>
			<OutputParameterName>period</OutputParameterName>
			<Value>&lt;width&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>75</OutputNodeID>
			<OutputParameterName>scale</OutputParameterName>
			<Value>3.142</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>75</OutputNodeID>
			<OutputParameterName>detail</OutputParameterName>
			<Value>2</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>75</OutputNodeID>
			<OutputParameterName>distortion</OutputParameterName>
			<Value>0</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>76</OutputNodeID>
			<OutputParameterName>period</OutputParameterName>
			<Value>&lt;length&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>79</OutputNodeID>
			<OutputParameterName>period</OutputParameterName>
			<Value>&lt;height&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>83</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>&lt;offcenter&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>84</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>float3(.5, .5, .5)</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>89</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>&lt;length&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>90</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Value>&lt;length&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>91</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Value>&lt;width&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>92</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Value>&lt;shift1&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>92</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>&lt;width&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>93</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Value>&lt;shift3&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>93</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>&lt;height&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>94</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Value>&lt;shift2&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>94</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Value>&lt;height&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>95</OutputNodeID>
			<OutputParameterName>input</OutputParameterName>
			<Value>&lt;coords&gt;</Value>
		</ConstantConnection>
	</ConstantConnections>
	<InputParameterConnections />
	<NodeConnections>
		<NodeConnection>
			<OutputNodeID>80</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>78</InputNodeID>
			<InputParameterName>int</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>85</OutputNodeID>
			<OutputParameterName>g</OutputParameterName>
			<InputNodeID>78</InputNodeID>
			<InputParameterName>int</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>82</OutputNodeID>
			<OutputParameterName>g</OutputParameterName>
			<InputNodeID>78</InputNodeID>
			<InputParameterName>frac</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>86</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>96</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>84</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>75</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>85</OutputNodeID>
			<OutputParameterName>r</OutputParameterName>
			<InputNodeID>76</InputNodeID>
			<InputParameterName>int</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>82</OutputNodeID>
			<OutputParameterName>r</OutputParameterName>
			<InputNodeID>76</InputNodeID>
			<InputParameterName>frac</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>81</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>79</InputNodeID>
			<InputParameterName>int</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>87</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>79</InputNodeID>
			<InputParameterName>int</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>85</OutputNodeID>
			<OutputParameterName>b</OutputParameterName>
			<InputNodeID>79</InputNodeID>
			<InputParameterName>int</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>82</OutputNodeID>
			<OutputParameterName>b</OutputParameterName>
			<InputNodeID>79</InputNodeID>
			<InputParameterName>frac</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>96</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>80</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>88</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>81</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>77</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>82</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>77</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>83</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>83</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>84</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>75</OutputNodeID>
			<OutputParameterName>position</OutputParameterName>
			<InputNodeID>85</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>76</OutputNodeID>
			<OutputParameterName>value</OutputParameterName>
			<InputNodeID>86</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>86</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>87</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>78</OutputNodeID>
			<OutputParameterName>value</OutputParameterName>
			<InputNodeID>88</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>80</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>89</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>87</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>90</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>81</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>91</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>89</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>92</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>91</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>93</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>90</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>94</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>79</OutputNodeID>
			<OutputParameterName>value</OutputParameterName>
			<InputNodeID>95</InputNodeID>
			<InputParameterName>b</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>88</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>95</InputNodeID>
			<InputParameterName>g</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>96</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>95</InputNodeID>
			<InputParameterName>r</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
	</NodeConnections>
	<Nodes>
		<Node>
			<FragmentArchiveName>game/xleres/ProcMat\Wood\WoodModulator002.sh:WoodModulator002</FragmentArchiveName>
			<NodeId>78</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>0</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Add1</FragmentArchiveName>
			<NodeId>96</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>2</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Texture.sh:NoiseTexture3</FragmentArchiveName>
			<NodeId>75</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>3</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/ProcMat\Wood\WoodModulator002.sh:WoodModulator002</FragmentArchiveName>
			<NodeId>76</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>4</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Add3</FragmentArchiveName>
			<NodeId>77</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>5</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/ProcMat\Wood\WoodModulator002.sh:WoodModulator002</FragmentArchiveName>
			<NodeId>79</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>6</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>80</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>7</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>81</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>8</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Combine3</FragmentArchiveName>
			<NodeId>82</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>9</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Multiply3</FragmentArchiveName>
			<NodeId>83</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>10</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Subtract3</FragmentArchiveName>
			<NodeId>84</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>11</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Combine3</FragmentArchiveName>
			<NodeId>85</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>12</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Add1</FragmentArchiveName>
			<NodeId>86</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>13</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>87</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>14</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Add1</FragmentArchiveName>
			<NodeId>88</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>15</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>89</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>16</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>90</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>17</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>91</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>18</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Divide1</FragmentArchiveName>
			<NodeId>92</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>19</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Divide1</FragmentArchiveName>
			<NodeId>93</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>20</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Divide1</FragmentArchiveName>
			<NodeId>94</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>21</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes/Basic.sh:Separate3</FragmentArchiveName>
			<NodeId>95</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>22</VisualNodeId>
		</Node>
	</Nodes>
	<OutputParameterConnections>
		<OutputParameterConnection>
			<InputNodeID>77</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<Name>plankCoords</Name>
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
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>3</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize>result2</OutputToVisualize>
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
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>17</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>18</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>19</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>20</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>21</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>22</VisualNodeId>
		</PreviewSettings>
	</PreviewSettingsObjects>
	<VisualNodes>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>446</d4p1:x>
				<d4p1:y>184</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>3747</d4p1:x>
				<d4p1:y>51</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1112</d4p1:x>
				<d4p1:y>-93</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>2266</d4p1:x>
				<d4p1:y>277</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1697</d4p1:x>
				<d4p1:y>54</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>3140</d4p1:x>
				<d4p1:y>75</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>-626</d4p1:x>
				<d4p1:y>658</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>903</d4p1:x>
				<d4p1:y>21</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>-66</d4p1:x>
				<d4p1:y>356</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>2462</d4p1:x>
				<d4p1:y>-43</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>2761</d4p1:x>
				<d4p1:y>213</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>2531</d4p1:x>
				<d4p1:y>321</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>2092</d4p1:x>
				<d4p1:y>146</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1357</d4p1:x>
				<d4p1:y>-14</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>135</d4p1:x>
				<d4p1:y>450</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>138</d4p1:x>
				<d4p1:y>284</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>-373</d4p1:x>
				<d4p1:y>271</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>-374</d4p1:x>
				<d4p1:y>328</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>-397</d4p1:x>
				<d4p1:y>394</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>-639</d4p1:x>
				<d4p1:y>266</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>-630</d4p1:x>
				<d4p1:y>389</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>-637</d4p1:x>
				<d4p1:y>326</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>-1185</d4p1:x>
				<d4p1:y>-260</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
	</VisualNodes>
</NodeGraph>
)-- */
/* <<Chunk:NodeGraphContext:WoodPlanks>>--(
<?xml version="1.0" encoding="utf-8"?>
<NodeGraphContext xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://schemas.datacontract.org/2004/07/ShaderPatcherLayer">
	<HasTechniqueConfig>false</HasTechniqueConfig>
	<ShaderParameters xmlns:d2p1="http://schemas.microsoft.com/2003/10/Serialization/Arrays" />
	<Variables xmlns:d2p1="http://schemas.microsoft.com/2003/10/Serialization/Arrays" />
</NodeGraphContext>
)-- */
