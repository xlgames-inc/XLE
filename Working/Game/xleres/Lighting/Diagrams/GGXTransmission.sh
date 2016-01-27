// CompoundDocument:1
#include "game/xleres/System/Prefix.h"

#include "game/xleres/Nodes/Basic.sh"
#include "game/xleres/Lighting/SpecularMethods.h"
#include "game/xleres/Lighting/LightingAlgorithm.h"


void GGXTransmission(float iorIncident, float iorOutgoing, float roughness, float3 i, float3 o, float3 n, out float result : SV_Target0)
{
	float3 Output_63_result;
	Output_63_result = CalculateHt( i, o, iorIncident, iorOutgoing );

	float Output_67_result;
	Output_67_result = RoughnessToDAlpha( roughness );

	float Output_71_result;
	Output_71_result = Dot3( Output_63_result, n );

	float Output_76_result;
	Output_76_result = TrowReitzD( Output_71_result, Output_67_result );

	float Output_77_result;
	Output_77_result = Square1( iorOutgoing );

	float Output_75_result;
	Output_75_result = Dot3( i, n );

	float Output_73_result;
	Output_73_result = Abs1( Output_75_result );

	float Output_80_result;
	Output_80_result = RoughnessToGAlpha( roughness );

	float Output_70_result;
	Output_70_result = SmithG( Output_73_result, Output_80_result );

	float Output_74_result;
	Output_74_result = Dot3( o, n );

	float Output_82_result;
	Output_82_result = Abs1( Output_74_result );

	float Output_83_result;
	Output_83_result = SmithG( Output_82_result, Output_80_result );

	float Output_84_result;
	Output_84_result = Multiply1( Output_70_result, Output_83_result );

	float Output_72_result;
	Output_72_result = Multiply1( Output_73_result, Output_82_result );

	float Output_68_result;
	Output_68_result = Dot3( Output_63_result, i );

	float Output_69_result;
	Output_69_result = Abs1( Output_68_result );

	float Output_88_result;
	Output_88_result = Dot3( Output_63_result, o );

	float Output_81_result;
	Output_81_result = Abs1( Output_88_result );

	float Output_87_result;
	Output_87_result = Multiply1( Output_69_result, Output_81_result );

	float Output_85_result;
	Output_85_result = Divide1( Output_87_result, Output_72_result );

	float Output_64_result;
	Output_64_result = MultiplyMany1( Output_85_result, Output_77_result, Output_84_result, Output_76_result );

	float Output_65_result;
	Output_65_result = Multiply1( iorIncident, Output_68_result );

	float Output_89_result;
	Output_89_result = Multiply1( iorOutgoing, Output_88_result );

	float Output_79_result;
	Output_79_result = Add1( Output_65_result, Output_89_result );

	float Output_78_result;
	Output_78_result = Square1( Output_79_result );

	float Output_86_result;
	Output_86_result = Divide1( Output_64_result, Output_78_result );

	result = Output_86_result;

}
/* <<Chunk:NodeGraph:GGXTransmission>>--(
<?xml version="1.0" encoding="utf-8"?>
<NodeGraph xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://schemas.datacontract.org/2004/07/ShaderPatcherLayer">
	<ConstantConnections>
		<ConstantConnection>
			<OutputNodeID>63</OutputNodeID>
			<OutputParameterName>iorIncident</OutputParameterName>
			<Value>&lt;iorIncident&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>63</OutputNodeID>
			<OutputParameterName>iorOutgoing</OutputParameterName>
			<Value>&lt;iorOutgoing&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>67</OutputNodeID>
			<OutputParameterName>roughness</OutputParameterName>
			<Value>&lt;roughness&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>80</OutputNodeID>
			<OutputParameterName>roughness</OutputParameterName>
			<Value>&lt;roughness&gt;</Value>
		</ConstantConnection>
	</ConstantConnections>
	<InputParameterConnections>
		<InputParameterConnection>
			<OutputNodeID>89</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Default></Default>
			<Name>&lt;iorOutgoing&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>5</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>65</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Default></Default>
			<Name>&lt;iorIncident&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>5</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>77</OutputNodeID>
			<OutputParameterName>value</OutputParameterName>
			<Default></Default>
			<Name>&lt;iorOutgoing&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>5</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>63</OutputNodeID>
			<OutputParameterName>i</OutputParameterName>
			<Default i:nil="true" />
			<Name>&lt;i&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>6</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>63</OutputNodeID>
			<OutputParameterName>o</OutputParameterName>
			<Default></Default>
			<Name>&lt;o&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>6</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>68</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Default i:nil="true" />
			<Name>&lt;i&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>6</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>88</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Default></Default>
			<Name>&lt;o&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>6</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>75</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Default i:nil="true" />
			<Name>&lt;i&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>6</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>75</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Default></Default>
			<Name>&lt;n&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>6</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>74</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Default></Default>
			<Name>&lt;o&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>6</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>74</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Default></Default>
			<Name>&lt;n&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>6</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>71</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Default></Default>
			<Name>&lt;n&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>6</VisualNodeId>
		</InputParameterConnection>
	</InputParameterConnections>
	<NodeConnections>
		<NodeConnection>
			<OutputNodeID>86</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>64</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>64</OutputNodeID>
			<OutputParameterName>forth</OutputParameterName>
			<InputNodeID>76</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>64</OutputNodeID>
			<OutputParameterName>third</OutputParameterName>
			<InputNodeID>84</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>64</OutputNodeID>
			<OutputParameterName>second</OutputParameterName>
			<InputNodeID>77</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>76</OutputNodeID>
			<OutputParameterName>NdotH</OutputParameterName>
			<InputNodeID>71</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>79</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>89</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>68</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>63</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>88</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>63</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>71</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>63</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>79</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>65</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>76</OutputNodeID>
			<OutputParameterName>alpha</OutputParameterName>
			<InputNodeID>67</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>69</OutputNodeID>
			<OutputParameterName>value</OutputParameterName>
			<InputNodeID>68</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>65</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>68</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>87</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>69</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>84</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>70</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>85</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>72</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>70</OutputNodeID>
			<OutputParameterName>NdotV</OutputParameterName>
			<InputNodeID>73</InputNodeID>
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
			<OutputNodeID>82</OutputNodeID>
			<OutputParameterName>value</OutputParameterName>
			<InputNodeID>74</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>73</OutputNodeID>
			<OutputParameterName>value</OutputParameterName>
			<InputNodeID>75</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>86</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>78</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>78</OutputNodeID>
			<OutputParameterName>value</OutputParameterName>
			<InputNodeID>79</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>83</OutputNodeID>
			<OutputParameterName>alpha</OutputParameterName>
			<InputNodeID>80</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>70</OutputNodeID>
			<OutputParameterName>alpha</OutputParameterName>
			<InputNodeID>80</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>87</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>81</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>72</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>82</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>83</OutputNodeID>
			<OutputParameterName>NdotV</OutputParameterName>
			<InputNodeID>82</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>84</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>83</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>64</OutputNodeID>
			<OutputParameterName>first</OutputParameterName>
			<InputNodeID>85</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>85</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>87</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>81</OutputNodeID>
			<OutputParameterName>value</OutputParameterName>
			<InputNodeID>88</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>89</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>88</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
	</NodeConnections>
	<Nodes>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:MultiplyMany1</FragmentArchiveName>
			<NodeId>64</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>0</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Lighting\SpecularMethods.h:TrowReitzD</FragmentArchiveName>
			<NodeId>76</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>1</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>84</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>2</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Square1</FragmentArchiveName>
			<NodeId>77</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>3</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Dot3</FragmentArchiveName>
			<NodeId>71</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>4</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>89</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>7</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Lighting\LightingAlgorithm.h:CalculateHt</FragmentArchiveName>
			<NodeId>63</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>8</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>65</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>9</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Lighting\SpecularMethods.h:RoughnessToDAlpha</FragmentArchiveName>
			<NodeId>67</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>10</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Dot3</FragmentArchiveName>
			<NodeId>68</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>11</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Abs1</FragmentArchiveName>
			<NodeId>69</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>12</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Lighting\SpecularMethods.h:SmithG</FragmentArchiveName>
			<NodeId>70</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>13</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>72</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>14</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Abs1</FragmentArchiveName>
			<NodeId>73</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>15</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Dot3</FragmentArchiveName>
			<NodeId>74</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>16</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Dot3</FragmentArchiveName>
			<NodeId>75</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>17</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Square1</FragmentArchiveName>
			<NodeId>78</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>18</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Add1</FragmentArchiveName>
			<NodeId>79</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>19</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Lighting\SpecularMethods.h:RoughnessToGAlpha</FragmentArchiveName>
			<NodeId>80</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>20</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Abs1</FragmentArchiveName>
			<NodeId>81</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>21</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Abs1</FragmentArchiveName>
			<NodeId>82</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>22</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Lighting\SpecularMethods.h:SmithG</FragmentArchiveName>
			<NodeId>83</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>23</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Divide1</FragmentArchiveName>
			<NodeId>85</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>24</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Divide1</FragmentArchiveName>
			<NodeId>86</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>25</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>87</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>26</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Dot3</FragmentArchiveName>
			<NodeId>88</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>27</VisualNodeId>
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
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>3</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>4</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>7</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize>result * .5 + .5.xxx</OutputToVisualize>
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
			<Geometry>Sphere</Geometry>
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
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>24</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>25</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>26</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>27</VisualNodeId>
		</PreviewSettings>
	</PreviewSettingsObjects>
	<VisualNodes>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1521</d4p1:x>
				<d4p1:y>778</d4p1:y>
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
				<d4p1:y>846</d4p1:y>
			</Location>
			<State>Collapsed</State>
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
				<d4p1:x>-249</d4p1:x>
				<d4p1:y>623</d4p1:y>
			</Location>
			<State>Normal</State>
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
				<d4p1:x>-836.2657</d4p1:x>
				<d4p1:y>582.577942</d4p1:y>
			</Location>
			<State>Normal</State>
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
				<d4p1:x>-561</d4p1:x>
				<d4p1:y>429</d4p1:y>
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
				<d4p1:x>836</d4p1:x>
				<d4p1:y>1003</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
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
				<d4p1:x>924</d4p1:x>
				<d4p1:y>829</d4p1:y>
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
				<d4p1:y>418</d4p1:y>
			</Location>
			<State>Collapsed</State>
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
	</VisualNodes>
</NodeGraph>
)-- */
/* <<Chunk:NodeGraphContext:GGXTransmission>>--(
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
			<d2p1:Value>.2</d2p1:Value>
		</d2p1:KeyValueOfstringstring>
		<d2p1:KeyValueOfstringstring>
			<d2p1:Key>roughness</d2p1:Key>
			<d2p1:Value>.33</d2p1:Value>
		</d2p1:KeyValueOfstringstring>
		<d2p1:KeyValueOfstringstring>
			<d2p1:Key>n</d2p1:Key>
			<d2p1:Value>Function:BuildRefractionNormal</d2p1:Value>
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
