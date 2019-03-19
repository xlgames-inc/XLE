// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderInstantiation.h"
#include "GraphSyntax.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/StringUtils.h"
#include "../Utility/StringFormat.h"
#include <set>
#include <stack>
#include <regex>
#include <sstream>

namespace GraphLanguage
{
	static std::string MakeGraphName(const std::string& baseName, uint64_t instantiationHash = 0)
    {
        if (!instantiationHash) return baseName;
        return baseName + "_" + std::to_string(instantiationHash);
    }

	InstantiatedShader InstantiateShader(
        const GraphLanguage::INodeGraphProvider::NodeGraph& initialGraph,
		bool useScaffoldFunction,
		const GraphLanguage::InstantiationParameters& instantiationParameters)
	{
		std::set<std::string> includes;

		struct PendingInstantiation
		{
			GraphLanguage::INodeGraphProvider::NodeGraph _graph;
			bool _useScaffoldFunction;
			GraphLanguage::InstantiationParameters _instantiationParams;
		};

        InstantiatedShader result;
		std::stack<PendingInstantiation> instantiations;
		instantiations.emplace(PendingInstantiation{initialGraph, useScaffoldFunction, instantiationParameters});

		std::set<std::pair<std::string, uint64_t>> previousInstantiation;

		bool entryPointInstantiation = true;
        while (!instantiations.empty()) {
            auto inst = std::move(instantiations.top());
            instantiations.pop();

			// Slightly different rules for function name generation with inst._scope is not null. inst._scope is
			// only null for the original instantiation request -- in that case, we want the outer most function
			// to have the same name as the original request
			auto scaffoldName = entryPointInstantiation ? inst._graph._name : MakeGraphName(inst._graph._name, inst._instantiationParams.CalculateHash());
			auto implementationName = inst._useScaffoldFunction ? (scaffoldName + "_impl") : scaffoldName;
			auto instFn = GraphLanguage::GenerateFunction(
				inst._graph._graph, implementationName, 
				inst._instantiationParams, *inst._graph._subProvider);

			if (inst._useScaffoldFunction) {
				auto scaffoldSignature = inst._graph._signature;
				for (const auto&tp:inst._instantiationParams._parameterBindings) {
					for (const auto&c:tp.second._parametersToCurry) {
						auto name = "curried_" + tp.first + "_" + c;
						auto instP = std::find_if(
							instFn._entryPointSignature.GetParameters().begin(), instFn._entryPointSignature.GetParameters().end(),
							[name](const NodeGraphSignature::Parameter& p) { return XlEqString(MakeStringSection(name), p._name); });
						if (instP != instFn._entryPointSignature.GetParameters().end())
							scaffoldSignature.AddParameter(*instP);
					}
				}

				result._sourceFragments.push_back(GraphLanguage::GenerateScaffoldFunction(scaffoldSignature, instFn._entryPointSignature, scaffoldName, implementationName));
			}

			result._sourceFragments.insert(
				result._sourceFragments.end(),
				instFn._sourceFragments.begin(), instFn._sourceFragments.end());

			{
				// write captured parameters into a cbuffer (todo -- merge captures from all instantiations)
				std::stringstream str;
				str << GraphLanguage::GenerateMaterialCBuffer(inst._graph._signature);
				auto fragment = str.str();
				if (!fragment.empty())
					result._sourceFragments.push_back(fragment);
			}

			if (entryPointInstantiation)
				result._entryPointSignature = instFn._entryPointSignature;
			entryPointInstantiation = false;
                
			for (const auto&dep:instFn._dependencies._dependencies) {

				std::string filename = dep._archiveName;

				static std::regex archiveNameRegex(R"--(([\w\.\\/]+)::(\w+))--");
				std::smatch archiveNameMatch;
				if (std::regex_match(dep._archiveName, archiveNameMatch, archiveNameRegex) && archiveNameMatch.size() >= 3) {
					filename = archiveNameMatch[1].str();
				}

				// if it's a graph file, then we must create a specific instantiation
				auto instHash = dep._parameters.CalculateHash();
				if (dep._isGraphSyntaxFile) {
					// todo -- not taking into account the custom provider on the following line
					if (previousInstantiation.find({dep._archiveName, instHash}) == previousInstantiation.end()) {
						if (dep._customProvider) {
							instantiations.emplace(
								PendingInstantiation{dep._customProvider->FindGraph(dep._archiveName).value(), true, dep._parameters});
							previousInstantiation.insert({dep._archiveName, instHash});
						} else {
							instantiations.emplace(
								PendingInstantiation{inst._graph._subProvider->FindGraph(dep._archiveName).value(), true, dep._parameters});
							previousInstantiation.insert({dep._archiveName, instHash});
						}
					}
				} else {
					// This is just an include of a normal shader header
					if (instHash!=0) {
						includes.insert(std::string(StringMeld<MaxPath>() << filename + "_" << instHash));
					} else {
						if (dep._customProvider) {
							auto sig = dep._customProvider->FindSignature(dep._archiveName);
							includes.insert(sig.value()._sourceFile);
						} else {
							auto sig = inst._graph._subProvider->FindSignature(dep._archiveName);
							includes.insert(sig.value()._sourceFile);
						}
					}
				}
			}
        }

		std::reverse(result._sourceFragments.begin(), result._sourceFragments.end());

		{
			std::stringstream str;
			for (const auto&i:includes)
				str << "#include <" << i << ">" << std::endl;
			result._sourceFragments.insert(result._sourceFragments.begin(), str.str());
		}

		return result;
	}

	InstantiatedShader InstantiateShader(
		StringSection<> entryFile,
		StringSection<> entryFn,
		const GraphLanguage::InstantiationParameters& instantiationParameters)
	{
		return InstantiateShader(
			GraphLanguage::LoadGraphSyntaxFile(entryFile, entryFn), true,
			instantiationParameters);
	}
}
