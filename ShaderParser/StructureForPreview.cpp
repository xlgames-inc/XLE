// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderPatcher.h"
#include "ShaderPatcher_Internal.h"
#include "InterfaceSignature.h"
#include "../RenderCore/ShaderLangUtil.h"
#include "../Assets/AssetUtils.h"
#include "../Assets/ConfigFileContainer.h"
#include "../Utility/StringFormat.h"
#include "../Utility/Conversion.h"
#include <regex>
#include <tuple>

#include "plustasche/template.hpp"

namespace ShaderPatcher
{
	class TemplateItem
    {
    public:
        std::basic_string<utf8> _item;

		const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }

        TemplateItem(
            InputStreamFormatter<utf8>& formatter,
            const ::Assets::DirectorySearchRules&,
			const ::Assets::DepValPtr& depVal)
        {
            InputStreamFormatter<utf8>::InteriorSection name, value;
            using Blob = InputStreamFormatter<utf8>::Blob;
            if (formatter.PeekNext() == Blob::AttributeName && formatter.TryAttribute(name, value)) {
                _item = value.AsString();
            } else
                Throw(Utility::FormatException("Expecting single string attribute", formatter.GetLocation()));

			_depVal = depVal;
        }
        TemplateItem() : _depVal(std::make_shared<::Assets::DependencyValidation>()) {}
	private:
		::Assets::DepValPtr _depVal;
    };

	static std::string ToPlustache(bool value)
    {
        static std::string T = "true", F = "false";
        return value ? T : F;
    }

	static std::string GetPreviewTemplate(const char templateName[])
    {
        StringMeld<MaxPath, Assets::ResChar> str;
        str << "xleres/System/PreviewTemplates.sh:" << templateName;
		return Conversion::Convert<std::string>(::Assets::GetAssetDep<TemplateItem>(str.AsStringSection())._item);
    }

	struct VaryingParamsFlags 
    {
        enum Enum { WritesVSOutput = 1<<0 };
        using BitField = unsigned;
    };
    
    class ParameterMachine
    {
    public:
        auto GetBuildInterpolator(const FunctionInterface::Parameter& param) const
            -> std::pair<std::string, VaryingParamsFlags::BitField>;

        auto GetBuildSystem(const FunctionInterface::Parameter& param) const -> std::string;

        ParameterMachine();
        ~ParameterMachine();
    private:
        ShaderSourceParser::ShaderFragmentSignature _systemHeader;
    };

    auto ParameterMachine::GetBuildInterpolator(const FunctionInterface::Parameter& param) const
        -> std::pair<std::string, VaryingParamsFlags::BitField>
    {
        std::string searchName = "BuildInterpolator_" + param._semantic;
        auto i = std::find_if(
            _systemHeader._functions.cbegin(), 
            _systemHeader._functions.cend(),
            [searchName](const ShaderSourceParser::FunctionSignature& sig) { return sig._name == searchName; });

        if (i == _systemHeader._functions.cend()) {
            searchName = "BuildInterpolator_" + param._name;
            i = std::find_if(
                _systemHeader._functions.cbegin(), 
                _systemHeader._functions.cend(),
                [searchName](const ShaderSourceParser::FunctionSignature& sig) { return sig._name == searchName; });
        }

        if (i == _systemHeader._functions.cend()) {
            searchName = "BuildInterpolator_" + param._type;
            i = std::find_if(
                _systemHeader._functions.cbegin(), 
                _systemHeader._functions.cend(),
                [searchName](const ShaderSourceParser::FunctionSignature& sig) { return sig._name == searchName; });
        }

        if (i != _systemHeader._functions.cend()) {
            VaryingParamsFlags::BitField flags = 0;
            if (!i->_returnSemantic.empty()) {
                    // using regex, convert the semantic value into a series of flags...
                static std::regex FlagsParse(R"--(NE(?:_([^_]*))*)--");
                std::smatch match;
                if (std::regex_match(i->_returnSemantic.begin(), i->_returnSemantic.end(), match, FlagsParse))
                    for (unsigned c=1; c<match.size(); ++c)
                        if (XlEqString(MakeStringSection(AsPointer(match[c].first), AsPointer(match[c].second)), "WritesVSOutput"))
                            flags |= VaryingParamsFlags::WritesVSOutput;
            }

            return std::make_pair(i->_name, flags);
        }

        return std::make_pair(std::string(), 0);
    }

    auto ParameterMachine::GetBuildSystem(const FunctionInterface::Parameter& param) const -> std::string
    {
        std::string searchName = "BuildSystem_" + param._type;
        auto i = std::find_if(
            _systemHeader._functions.cbegin(), _systemHeader._functions.cend(),
            [searchName](const ShaderSourceParser::FunctionSignature& sig) { return sig._name == searchName; });
        if (i != _systemHeader._functions.cend())
            return i->_name;
        return std::string();
    }

    ParameterMachine::ParameterMachine()
    {
        auto buildInterpolatorsSource = LoadSourceFile("xleres/System/BuildInterpolators.h");
        _systemHeader = ShaderSourceParser::BuildShaderFragmentSignature(MakeStringSection(buildInterpolatorsSource));
    }

    ParameterMachine::~ParameterMachine() {}

    class ParameterGenerator
    {
    public:
        unsigned Count() const              { return (unsigned)_parameters.size(); };
        std::string VSOutputMember() const  { return _vsOutputMember; }

        std::string VaryingStructSignature(unsigned index) const;
        std::string VSInitExpression(unsigned index);
        std::string PSExpression(unsigned index, const char vsOutputName[], const char varyingParameterStruct[]) const;
        bool IsGlobalResource(unsigned index) const;
        const FunctionInterface::Parameter& Param(unsigned index) const { return _parameters[index]; }

        bool IsInitializedBySystem(unsigned index) const { return !_buildSystemFunctions[index].empty(); }

        ParameterGenerator(const FunctionInterface& interf, const PreviewOptions& previewOptions);
        ~ParameterGenerator();
    private:
        std::vector<FunctionInterface::Parameter>  _parameters;
        std::vector<std::string>            _buildSystemFunctions;
        std::string                         _vsOutputMember;
        ParameterMachine                    _paramMachine;

		const PreviewOptions* _previewOptions;
    };

    std::string ParameterGenerator::VaryingStructSignature(unsigned index) const
    {
        if (!_buildSystemFunctions[index].empty()) return std::string();
        if (IsGlobalResource(index)) return std::string();
        const auto& p = _parameters[index];

        std::stringstream result;
        result << "\t" << p._type << " " << p._name;
        /*if (!p._semantic.empty()) {
            result << " : " << p._semantic;
        } else */ {
            char smallBuffer[128];
            if (!IsStructType(MakeStringSection(p._type)))  // (struct types don't get a semantic)
                result << " : " << "VARYING_" << XlI32toA(index, smallBuffer, dimof(smallBuffer), 10);
        }
        return result.str();
    }

    std::string ParameterGenerator::VSInitExpression(unsigned index)
    {
        const auto& p = _parameters[index];
        if (!_buildSystemFunctions[index].empty()) return std::string();

        // Here, we have to sometimes look for parameters that we understand.
        // First, we should look at the semantic attached.
        // We can look for translator functions in the "BuildInterpolators.h" system header
        // If there is a function signature there that can generate the interpolator
        // we're interested in, then we should use that function.
        //
        // If the parameter is actually a structure, we need to look inside of the structure
        // and bind the individual elements. We should do this recursively incase we have
        // structures within structures.
        //
        // Even if we know that the parameter is a structure, it might be hard to find the
        // structure within the shader source code... It would require following through
        // #include statements, etc. That could potentially create some complications here...
        // Maybe we just need a single BuildInterpolator_ that returns a full structure for
        // things like VSOutput...?

        std::string buildInterpolator;
        VaryingParamsFlags::BitField flags;
        std::tie(buildInterpolator, flags) = _paramMachine.GetBuildInterpolator(p);

        if (!buildInterpolator.empty()) {
            if (flags & VaryingParamsFlags::WritesVSOutput)
                _vsOutputMember = p._name;
            return buildInterpolator + "(vsInput)";
        } else {
            if (!IsStructType(MakeStringSection(p._type))) {

					// Look for a "restriction" applied to this variable.
				auto r = std::find_if(
					_previewOptions->_variableRestrictions.cbegin(), _previewOptions->_variableRestrictions.cend(), 
					[&p](const std::pair<std::string, std::string>& v) { return XlEqStringI(v.first, p._name); });
				if (r != _previewOptions->_variableRestrictions.cend()) {
                     if (XlBeginsWith(MakeStringSection(r->second), MakeStringSection("Function:"))) {
                            // This is actually a function name. It would be great if we could chose to
                            // run this function in the VS or PS, dependant on the parameters.
                        _buildSystemFunctions[index] = r->second.substr(9);
                        return std::string();
                    } else {
					    static std::regex pattern("([^:]*):([^:]*):([^:]*)");
					    std::smatch match;
					    if (std::regex_match(r->second, match, pattern) && match.size() >= 4) {
						    return std::string("InterpolateVariable_") + match[1].str() + "(" + match[2].str() + ", " + match[3].str() + ", localPosition)";
					    } else {
						    return r->second;	// interpret as a constant
					    }
                     }
				} else {
					// attempt to set values
					auto type = RenderCore::ShaderLangTypeNameAsTypeDesc(MakeStringSection(p._type));
					int dimensionality = type._arrayCount;
					if (dimensionality == 1) {
						return "localPosition.x * 0.5 + 0.5.x";
					} else if (dimensionality == 2) {
						return "float2(localPosition.x * 0.5 + 0.5, localPosition.y * -0.5 + 0.5)";
					} else if (dimensionality == 3) {
						return "worldPosition.xyz";
					}
				}
            }
        }

        return std::string();
    }

    std::string ParameterGenerator::PSExpression(unsigned index, const char vsOutputName[], const char varyingParameterStruct[]) const
    {
        if (IsGlobalResource(index))
            return _parameters[index]._name;

        auto buildSystemFunction = _buildSystemFunctions[index];
        if (!buildSystemFunction.empty()) {
            if (!_vsOutputMember.empty())
                return buildSystemFunction + "(" + varyingParameterStruct + "." + _vsOutputMember + ", sys)";
            return buildSystemFunction + "(" + vsOutputName + ", sys)";
        } else {
            return std::string(varyingParameterStruct) + "." + _parameters[index]._name;
        }
    }

    bool ParameterGenerator::IsGlobalResource(unsigned index) const
    {
            // Resource types (eg, texture, etc) can't be handled like scalars
            // they must become globals in the shader.
        return !CanBeStoredInCBuffer(MakeStringSection(_parameters[index]._type));
    }

    ParameterGenerator::ParameterGenerator(const FunctionInterface& interf, const PreviewOptions& previewOptions)
    {
		for (const auto&p:interf.GetFunctionParameters())
			if (p._direction == FunctionInterface::Parameter::In)
				_parameters.push_back(p);
        for (auto i=_parameters.cbegin(); i!=_parameters.cend(); ++i)
            _buildSystemFunctions.push_back(_paramMachine.GetBuildSystem(*i));
		_previewOptions = &previewOptions;
    }

    ParameterGenerator::~ParameterGenerator() {}

    std::string         GenerateStructureForPreview(
        const StringSection<char> graphName, const FunctionInterface& interf, 
		const ::Assets::DirectorySearchRules& searchRules,
        const PreviewOptions& previewOptions)
    {
            //
            //      Generate the shader structure that will surround the main
            //      shader generated from "graph"
            //
            //      We have to analyse the inputs and output.
            //
            //      The type of structure should be determined by the dimensionality
            //      of the outputs, and whether the shader takes position inputs.
            //
            //      We must then look at the inputs, and try to determine which
            //      inputs (if any) should vary over the surface of the preview.
            //  
            //      For example, if our preview is a basic 2d or 1d preview, then
            //      the x and y axes will represent some kind of varying parameter.
            //      But for a 3d preview window, there are no varying parameters
            //      (all parameters must be fixed over the surface of the preview
            //      window)
            //

        ParameterGenerator mainParams(interf, previewOptions);

            //
            //      All varying parameters must have semantics
            //      so, look for free TEXCOORD slots, and set all unset semantics
            //      to a default
            //

        std::stringstream result;
        result << std::endl;
        result << "\t//////// Structure for preview ////////" << std::endl;

        const bool renderAsChart = previewOptions._type == PreviewOptions::Type::Chart;
        if (renderAsChart)
            result << "#define SHADER_NODE_EDITOR_CHART 1" << std::endl;
        result << "#include \"xleres/System/BuildInterpolators.h\"" << std::endl;

            //  
            //      First write the "varying" parameters
            //      The varying parameters are always written in the vertex shader and
            //      read by the pixel shader. They will "vary" over the geometry that
            //      we're rendering -- hence the name.
            //      We could use a Mustache template for this, if we were using the
            //      more general implementation of Mustache for C++. But unfortunately
            //      there's no practical way with Plustasche.
            //
        result << "struct NE_Varying" << std::endl << "{" << std::endl;
        for (unsigned index=0; index<mainParams.Count(); ++index) {
            auto sig = mainParams.VaryingStructSignature(index);
            if (sig.empty()) continue;
            result << sig << ";" << std::endl;
        }
        result << "};" << std::endl << std::endl;

            //
            //      Write "_Output" structure. This contains all of the values that are output
            //      from the main function
            //
        result << "struct NE_" << graphName.AsString() << "_Output" << std::endl << "{" << std::endl;
        unsigned svTargetCounter = 0;
        for (const auto& i:interf.GetFunctionParameters())
			if (i._direction == FunctionInterface::Parameter::Out)
				result << "\t" << i._type << " " << i._name << ": SV_Target" << (svTargetCounter++) << ";" << std::endl;
        result << "};" << std::endl << std::endl;

            //
            //      Write all of the global resource types
            //
        for (unsigned index=0; index<mainParams.Count(); ++index)
            if (mainParams.IsGlobalResource(index))
                result << mainParams.Param(index)._type << " " << mainParams.Param(index)._name << ";" << std::endl;
        result << std::endl;

            //
            //      Calculate the code that will fill in the varying parameters from the vertex
            //      shader. We need to do this now because it will effect some of the structure
            //      later.
            //
        std::stringstream varyingInitialization;
        
        for (unsigned index=0; index<mainParams.Count(); ++index) {
            auto initString = mainParams.VSInitExpression(index);
            if (initString.empty()) continue;
            varyingInitialization << "\tOUT.varyingParameters." << mainParams.Param(index)._name << " = " << initString << ";" << std::endl;
        }

        result << "struct NE_PSInput" << std::endl << "{" << std::endl;
        if (mainParams.VSOutputMember().empty())
            result << "\tVSOutput geo;" << std::endl;
        result << "\tNE_Varying varyingParameters;" << std::endl;
        result << "};" << std::endl << std::endl;

        std::string parametersToMainFunctionCall;

            //  Pass each member of the "varyingParameters" struct as a separate input to
            //  the main function
        for (unsigned index=0; index<mainParams.Count(); ++index) {
            if (!parametersToMainFunctionCall.empty())
                parametersToMainFunctionCall += ", ";
            parametersToMainFunctionCall += mainParams.PSExpression(index, "input.geo", "input.varyingParameters");
        }
            
            //  Also pass each output as a parameter to the main function
        for (const auto& i:interf.GetFunctionParameters()) {
			if (i._direction == FunctionInterface::Parameter::Out) {
				if (!parametersToMainFunctionCall.empty())
					parametersToMainFunctionCall += ", ";
				parametersToMainFunctionCall += "functionResult." + i._name;
			}
        }

        Plustache::template_t preprocessor;

        // Render the ps_main template
        {
            Plustache::Context context;
            context.add("GraphName", graphName.AsString());
            context.add("ParametersToMainFunctionCall", parametersToMainFunctionCall);
            context.add("PreviewOutput", previewOptions._outputToVisualize);

                // Collect all of the output values into a flat array of floats.
                // This is needed for "charts"
            if (renderAsChart) {
                std::vector<PlustacheTypes::ObjectType> chartLines;

                if (!previewOptions._outputToVisualize.empty()) {
                    // When we have an "outputToVisualize" we only show the
                    // chart for that single output.
                    chartLines.push_back(
                        PlustacheTypes::ObjectType { std::make_pair("Item", previewOptions._outputToVisualize) });
                } else {
                    // Find all of the scalar values written out from main function,
                    // including searching through parameter strructures.
                    for (const auto& i:interf.GetFunctionParameters()) {
						if (i._direction != FunctionInterface::Parameter::Out) continue;

                        const auto& signature = LoadParameterStructSignature(SplitArchiveName(i._archiveName), searchRules);
                        if (!signature._name.empty()) {
                            for (auto p=signature._parameters.cbegin(); p!=signature._parameters.cend(); ++p) {
                                    // todo -- what if this is also a parameter struct?
								auto type = RenderCore::ShaderLangTypeNameAsTypeDesc(MakeStringSection(p->_type));
								auto dim = type._arrayCount;
                                for (unsigned c=0; c<dim; ++c) {
                                    std::stringstream str;
                                    str << i._name << "." << p->_name;
                                    if (dim != 1) str << "[" << c << "]";
                                    chartLines.push_back(
                                        PlustacheTypes::ObjectType { std::make_pair("Item", str.str()) });
                                }
                            }
                        } else {
							auto type = RenderCore::ShaderLangTypeNameAsTypeDesc(MakeStringSection(i._type));
							auto dim = type._arrayCount;
                            for (unsigned c=0; c<dim; ++c) {
                                std::stringstream str;
                                str << i._name;
                                if (dim != 1) str << "[" << c << "]";
                                chartLines.push_back(
                                    PlustacheTypes::ObjectType { std::make_pair("Item", str.str()) });
                            }
                        }
                    }
                }

                context.add("ChartLines", chartLines);
                context.add("ChartLineCount", (StringMeld<64>() << unsigned(chartLines.size())).get());
            }

            if (renderAsChart) {
                result << preprocessor.render(GetPreviewTemplate("ps_main_chart"), context);
            } else if (!previewOptions._outputToVisualize.empty()) {
                result << preprocessor.render(GetPreviewTemplate("ps_main_explicit"), context);
            } else
                result << preprocessor.render(GetPreviewTemplate("ps_main"), context);
        }

        // Render the vs_main template
        result << preprocessor.render(GetPreviewTemplate("vs_main"), 
            PlustacheTypes::ObjectType
            {
                {"InitGeo", ToPlustache(mainParams.VSOutputMember().empty())},
                {"VaryingInitialization", varyingInitialization.str()}
            });

        return result.str();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	static std::string GetTechniqueTemplate(const char templateName[])
    {
        StringMeld<MaxPath, Assets::ResChar> str;
        str << "xleres/System/TechniqueTemplates.sh:" << templateName;
        return Conversion::Convert<std::string>(::Assets::GetAssetDep<TemplateItem>(str.AsStringSection())._item);
    }

	static void MaybeComma(std::stringstream& stream) { if (stream.tellp() != std::stringstream::pos_type(0)) stream << ", "; }

	std::string GenerateStructureForTechniqueConfig(const FunctionInterface& interf, const char graphName[])
	{
		std::stringstream mainFunctionParameterSignature;
		std::stringstream forwardMainParameters;
		for (const auto& p:interf.GetFunctionParameters()) {
			if (p._direction != FunctionInterface::Parameter::In) continue;

			MaybeComma(mainFunctionParameterSignature);
			mainFunctionParameterSignature << p._type << " " << p._name;

			MaybeComma(forwardMainParameters);
			forwardMainParameters << p._name;
		}
		
		Plustache::Context context;
		context.add("MainFunctionParameterSignature", mainFunctionParameterSignature.str());
		context.add("ForwardMainParameters", forwardMainParameters.str());
		context.add("GraphName", graphName);

		std::stringstream result;
		Plustache::template_t preprocessor;
		result << preprocessor.render(GetTechniqueTemplate("forward_main"), context);
        result << preprocessor.render(GetTechniqueTemplate("deferred_main"), context);
		result << preprocessor.render(GetTechniqueTemplate("oi_main"), context);
        result << preprocessor.render(GetTechniqueTemplate("stochastic_main"), context);
        result << preprocessor.render(GetTechniqueTemplate("depthonly_main"), context);
		return result.str();
	}
}

