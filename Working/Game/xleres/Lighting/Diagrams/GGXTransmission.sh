// CompoundDocument:1
#include "game/xleres/System/Prefix.h"

#include "game/xleres/Nodes/Basic.sh"
#include "game/xleres/Lighting/SpecularMethods.h"
#include "game/xleres/Lighting/LightingAlgorithm.h"


void GGXTransmission(float roughness, float F0, float iorIncident, float iorOutgoing, float3 i, float3 o, float3 n, out float result : SV_Target0)
{
	float3 Output_73_result;
	Output_73_result = CalculateHt( i, o, iorIncident, iorOutgoing );

	float Output_64_result;
	Output_64_result = Dot3( Output_73_result, i );

	float Output_66_result;
	Output_66_result = SchlickFresnelCore( Output_64_result );

	float Output_69_result;
	Output_69_result = Lerp1( F0, 1.f, Output_66_result );

	float Output_67_result;
	Output_67_result = Subtract1( 1.0f, Output_69_result );

	float Output_68_result;
	Output_68_result = Saturate1( Output_67_result );

	float Output_79_result;
	Output_79_result = Square1( iorOutgoing );

	float Output_77_result;
	Output_77_result = Dot3( i, n );

	float Output_75_result;
	Output_75_result = Abs1( Output_77_result );

	float Output_82_result;
	Output_82_result = RoughnessToGAlpha( roughness );

	precise float Output_71_result = SmithG( Output_75_result, Output_82_result );

	float Output_76_result;
	Output_76_result = Dot3( o, n );

	float Output_84_result;
	Output_84_result = Abs1( Output_76_result );

	precise float Output_85_result = SmithG( Output_84_result, Output_82_result );

	float Output_86_result;
	Output_86_result = Multiply1( Output_71_result, Output_85_result );

	float Output_74_result;
	Output_74_result = Multiply1( Output_75_result, Output_84_result );

	float Output_65_result;
	Output_65_result = Abs1( Output_64_result );

	float Output_90_result;
	Output_90_result = Dot3( Output_73_result, o );

	float Output_83_result;
	Output_83_result = Abs1( Output_90_result );

	float Output_89_result;
	Output_89_result = Multiply1( Output_65_result, Output_83_result );

	float Output_87_result;
	Output_87_result = Divide1( Output_89_result, Output_74_result );

	float Output_70_result;
	Output_70_result = MultiplyMany1( Output_87_result, Output_68_result, Output_79_result, Output_86_result );

	float Output_63_result;
	Output_63_result = RoughnessToDAlpha( roughness );

	float Output_72_result;
	Output_72_result = Dot3( Output_73_result, n );

	precise float Output_78_result = TrowReitzD( Output_72_result, Output_63_result );

	float Output_93_result;
	Output_93_result = Multiply1( Output_70_result, Output_78_result );

	float Output_91_result;
	Output_91_result = Multiply1( iorOutgoing, Output_90_result );

	float Output_92_result;
	Output_92_result = Multiply1( iorIncident, Output_64_result );

	float Output_81_result;
	Output_81_result = Add1( Output_92_result, Output_91_result );

	float Output_80_result;
	Output_80_result = Square1( Output_81_result );

	float Output_88_result;
	Output_88_result = Divide1( Output_93_result, Output_80_result );

	result = Output_88_result;

}
/* <<Chunk:NodeGraph:GGXTransmission>>--(
<?xml version="1.0" encoding="utf-8"?>
<NodeGraph xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://schemas.datacontract.org/2004/07/ShaderPatcherLayer">
	<ConstantConnections>
		<ConstantConnection>
			<OutputNodeID>63</OutputNodeID>
			<OutputParameterName>roughness</OutputParameterName>
			<Value>&lt;roughness&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>67</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Value>1.0f</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>69</OutputNodeID>
			<OutputParameterName>min</OutputParameterName>
			<Value>&lt;F0&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>69</OutputNodeID>
			<OutputParameterName>max</OutputParameterName>
			<Value>1.f</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>73</OutputNodeID>
			<OutputParameterName>iorIncident</OutputParameterName>
			<Value>&lt;iorIncident&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>73</OutputNodeID>
			<OutputParameterName>iorOutgoing</OutputParameterName>
			<Value>&lt;iorOutgoing&gt;</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>82</OutputNodeID>
			<OutputParameterName>roughness</OutputParameterName>
			<Value>&lt;roughness&gt;</Value>
		</ConstantConnection>
	</ConstantConnections>
	<InputParameterConnections>
		<InputParameterConnection>
			<OutputNodeID>91</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Default></Default>
			<Name>&lt;iorOutgoing&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>1</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>92</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Default></Default>
			<Name>&lt;iorIncident&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>1</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>79</OutputNodeID>
			<OutputParameterName>value</OutputParameterName>
			<Default></Default>
			<Name>&lt;iorOutgoing&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>1</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>73</OutputNodeID>
			<OutputParameterName>i</OutputParameterName>
			<Default i:nil="true" />
			<Name>&lt;i&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>2</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>73</OutputNodeID>
			<OutputParameterName>o</OutputParameterName>
			<Default></Default>
			<Name>&lt;o&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>2</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>64</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Default i:nil="true" />
			<Name>&lt;i&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>2</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>90</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Default></Default>
			<Name>&lt;o&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>2</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>77</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Default i:nil="true" />
			<Name>&lt;i&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>2</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>77</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Default></Default>
			<Name>&lt;n&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>2</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>76</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<Default></Default>
			<Name>&lt;o&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>2</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>76</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Default></Default>
			<Name>&lt;n&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>2</VisualNodeId>
		</InputParameterConnection>
		<InputParameterConnection>
			<OutputNodeID>72</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<Default></Default>
			<Name>&lt;n&gt;</Name>
			<Semantic></Semantic>
			<Type>auto</Type>
			<VisualNodeId>2</VisualNodeId>
		</InputParameterConnection>
	</InputParameterConnections>
	<NodeConnections>
		<NodeConnection>
			<OutputNodeID>70</OutputNodeID>
			<OutputParameterName>second</OutputParameterName>
			<InputNodeID>68</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>88</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>93</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>78</OutputNodeID>
			<OutputParameterName>alpha</OutputParameterName>
			<InputNodeID>63</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>65</OutputNodeID>
			<OutputParameterName>value</OutputParameterName>
			<InputNodeID>64</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>92</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>64</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>66</OutputNodeID>
			<OutputParameterName>VdotH</OutputParameterName>
			<InputNodeID>64</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>89</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>65</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>69</OutputNodeID>
			<OutputParameterName>alpha</OutputParameterName>
			<InputNodeID>66</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>68</OutputNodeID>
			<OutputParameterName>input</OutputParameterName>
			<InputNodeID>67</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>67</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>69</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>93</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>70</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>86</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>71</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>78</OutputNodeID>
			<OutputParameterName>NdotH</OutputParameterName>
			<InputNodeID>72</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>64</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>73</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>90</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>73</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>72</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>73</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>87</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>74</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>71</OutputNodeID>
			<OutputParameterName>NdotV</OutputParameterName>
			<InputNodeID>75</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>74</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>75</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>84</OutputNodeID>
			<OutputParameterName>value</OutputParameterName>
			<InputNodeID>76</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>75</OutputNodeID>
			<OutputParameterName>value</OutputParameterName>
			<InputNodeID>77</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>93</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>78</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>70</OutputNodeID>
			<OutputParameterName>third</OutputParameterName>
			<InputNodeID>79</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>88</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>80</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>80</OutputNodeID>
			<OutputParameterName>value</OutputParameterName>
			<InputNodeID>81</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>85</OutputNodeID>
			<OutputParameterName>alpha</OutputParameterName>
			<InputNodeID>82</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>71</OutputNodeID>
			<OutputParameterName>alpha</OutputParameterName>
			<InputNodeID>82</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>89</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>83</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>74</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>84</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>85</OutputNodeID>
			<OutputParameterName>NdotV</OutputParameterName>
			<InputNodeID>84</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>86</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>85</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>70</OutputNodeID>
			<OutputParameterName>forth</OutputParameterName>
			<InputNodeID>86</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>70</OutputNodeID>
			<OutputParameterName>first</OutputParameterName>
			<InputNodeID>87</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>87</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>89</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>83</OutputNodeID>
			<OutputParameterName>value</OutputParameterName>
			<InputNodeID>90</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>91</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>90</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>81</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>91</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>81</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>92</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
	</NodeConnections>
	<Nodes>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Saturate1</FragmentArchiveName>
			<NodeId>68</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>0</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>93</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>3</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Lighting\SpecularMethods.h:RoughnessToDAlpha</FragmentArchiveName>
			<NodeId>63</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>4</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Dot3</FragmentArchiveName>
			<NodeId>64</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>5</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Abs1</FragmentArchiveName>
			<NodeId>65</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>6</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Lighting\LightingAlgorithm.h:SchlickFresnelCore</FragmentArchiveName>
			<NodeId>66</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>7</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Subtract1</FragmentArchiveName>
			<NodeId>67</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>8</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Lerp1</FragmentArchiveName>
			<NodeId>69</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>9</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:MultiplyMany1</FragmentArchiveName>
			<NodeId>70</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>10</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Lighting\SpecularMethods.h:SmithG</FragmentArchiveName>
			<NodeId>71</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>11</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Dot3</FragmentArchiveName>
			<NodeId>72</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>12</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Lighting\LightingAlgorithm.h:CalculateHt</FragmentArchiveName>
			<NodeId>73</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>13</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>74</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>14</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Abs1</FragmentArchiveName>
			<NodeId>75</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>15</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Dot3</FragmentArchiveName>
			<NodeId>76</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>16</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Dot3</FragmentArchiveName>
			<NodeId>77</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>17</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Lighting\SpecularMethods.h:TrowReitzD</FragmentArchiveName>
			<NodeId>78</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>18</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Square1</FragmentArchiveName>
			<NodeId>79</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>19</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Square1</FragmentArchiveName>
			<NodeId>80</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>20</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Add1</FragmentArchiveName>
			<NodeId>81</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>21</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Lighting\SpecularMethods.h:RoughnessToGAlpha</FragmentArchiveName>
			<NodeId>82</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>22</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Abs1</FragmentArchiveName>
			<NodeId>83</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>23</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Abs1</FragmentArchiveName>
			<NodeId>84</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>24</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Lighting\SpecularMethods.h:SmithG</FragmentArchiveName>
			<NodeId>85</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>25</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>86</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>26</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Divide1</FragmentArchiveName>
			<NodeId>87</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>27</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Divide1</FragmentArchiveName>
			<NodeId>88</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>28</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>89</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>29</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Dot3</FragmentArchiveName>
			<NodeId>90</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>30</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>91</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>31</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>game/xleres/Nodes\Basic.sh:Multiply1</FragmentArchiveName>
			<NodeId>92</NodeId>
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
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>12</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize>result * .5 + .5.xxx</OutputToVisualize>
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
			<Geometry>Sphere</Geometry>
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
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>28</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
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
				<d4p1:x>1138</d4p1:x>
				<d4p1:y>604</d4p1:y>
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
