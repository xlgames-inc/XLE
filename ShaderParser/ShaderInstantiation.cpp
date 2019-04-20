// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderInstantiation.h"
#include "GraphSyntax.h"
#include "NodeGraphSignature.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/StringUtils.h"
#include "../Utility/StringFormat.h"
#include <set>
#include <stack>
#include <regex>
#include <sstream>

namespace ShaderSourceParser
{
	static std::string MakeGraphName(const std::string& baseName, uint64_t instantiationHash = 0)
    {
        if (!instantiationHash) return baseName;
        return baseName + "_" + std::to_string(instantiationHash);
    }

	struct PendingInstantiation
	{
		GraphLanguage::INodeGraphProvider::NodeGraph _graph;
		bool _useScaffoldFunction = false;
		bool _isRootInstantiation = true;
		InstantiationRequest _instantiationParams;
	};

	static std::pair<std::string, std::string> SplitArchiveName(const std::string& input)
	{
		static std::regex archiveNameRegex(R"--(([\w\.\\/]+)::(\w+))--");
		std::smatch archiveNameMatch;
		if (std::regex_match(input, archiveNameMatch, archiveNameRegex) && archiveNameMatch.size() >= 3) {
			return { archiveNameMatch[1].str(), archiveNameMatch[2].str() };
		}
		return { input, std::string{} };
	}

	static InstantiatedShader InstantiateShader(IteratorRange<const PendingInstantiation*> rootInstantiations)
	{
		std::set<std::string> includes;

        InstantiatedShader result;
		std::stack<PendingInstantiation> instantiations;

		// Add to the stack in reverse order, so that the first item in rootInstantiations appears highest in
		// the output file
		assert(!rootInstantiations.empty());
		for (auto i=rootInstantiations.end()-1; i>=rootInstantiations.begin(); --i)
			instantiations.emplace(*i);

		std::set<std::pair<std::string, uint64_t>> previousInstantiation;

		while (!instantiations.empty()) {
            auto inst = std::move(instantiations.top());
            instantiations.pop();

			// Slightly different rules for function name generation with inst._scope is not null. inst._scope is
			// only null for the original instantiation request -- in that case, we want the outer most function
			// to have the same name as the original request
			auto scaffoldName = MakeGraphName(inst._graph._name, inst._instantiationParams.CalculateHash());
			auto implementationName = inst._useScaffoldFunction ? (scaffoldName + "_impl") : scaffoldName;
			auto instFn = GenerateFunction(
				inst._graph._graph, implementationName, 
				inst._instantiationParams, *inst._graph._subProvider);

			assert(instFn._entryPoints.size() == 1);
			auto& instFnEntryPoint = instFn._entryPoints[0];

			if (inst._useScaffoldFunction) {
				auto scaffoldSignature = inst._graph._signature;
				for (const auto&tp:inst._instantiationParams._parameterBindings) {
					for (const auto&c:tp.second._parametersToCurry) {
						auto name = "curried_" + tp.first + "_" + c;
						auto instP = std::find_if(
							instFnEntryPoint._signature.GetParameters().begin(), instFnEntryPoint._signature.GetParameters().end(),
							[name](const GraphLanguage::NodeGraphSignature::Parameter& p) { return XlEqString(MakeStringSection(name), p._name); });
						if (instP != instFnEntryPoint._signature.GetParameters().end())
							scaffoldSignature.AddParameter(*instP);
					}
				}

				result._sourceFragments.push_back(ShaderSourceParser::GenerateScaffoldFunction(scaffoldSignature, instFnEntryPoint._signature, scaffoldName, implementationName));
			}

			result._sourceFragments.insert(
				result._sourceFragments.end(),
				instFn._sourceFragments.begin(), instFn._sourceFragments.end());

			{
				// write captured parameters into a cbuffer (todo -- merge captures from all instantiations)
				std::stringstream str;
				str << ShaderSourceParser::GenerateMaterialCBuffer(inst._graph._signature);
				auto fragment = str.str();
				if (!fragment.empty())
					result._sourceFragments.push_back(fragment);
			}

			if (inst._isRootInstantiation)
				result._entryPoints.push_back(instFnEntryPoint);
                
			for (const auto&dep:instFn._dependencies._dependencies) {

				// if it's a graph file, then we must create a specific instantiation
				auto instHash = dep._instantiation.CalculateHash();
				if (dep._isGraphSyntaxFile) {
					// todo -- not taking into account the custom provider on the following line
					if (previousInstantiation.find({dep._instantiation._archiveName, instHash}) == previousInstantiation.end()) {
						if (dep._instantiation._customProvider) {
							instantiations.emplace(
								PendingInstantiation{dep._instantiation._customProvider->FindGraph(dep._instantiation._archiveName).value(), true, false, dep._instantiation});
							previousInstantiation.insert({dep._instantiation._archiveName, instHash});
						} else {
							instantiations.emplace(
								PendingInstantiation{inst._graph._subProvider->FindGraph(dep._instantiation._archiveName).value(), true, false, dep._instantiation});
							previousInstantiation.insert({dep._instantiation._archiveName, instHash});
						}
					}
				} else {
					// This is just an include of a normal shader header
					if (instHash!=0) {
						auto filename = SplitArchiveName(dep._instantiation._archiveName).first;
						includes.insert(std::string(StringMeld<MaxPath>() << filename + "_" << instHash));
					} else {
						if (dep._instantiation._customProvider) {
							auto sig = dep._instantiation._customProvider->FindSignature(dep._instantiation._archiveName);
							includes.insert(sig.value()._sourceFile);
						} else {
							auto sig = inst._graph._subProvider->FindSignature(dep._instantiation._archiveName);
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
        const GraphLanguage::INodeGraphProvider::NodeGraph& initialGraph,
		bool useScaffoldFunction,
		const InstantiationRequest& instantiationParameters)
	{
		// Note that we end up with a few extra copies of initialGraph, because PendingInstantiation
		// contains a complete copy of the node graph
		PendingInstantiation pendingInst { initialGraph, true, true, instantiationParameters };
		return InstantiateShader(MakeIteratorRange(&pendingInst, &pendingInst+1));
	}

	InstantiatedShader InstantiateShader(
		StringSection<> entryFile,
		StringSection<> entryFn,
		const InstantiationRequest& instantiationParameters)
	{
		PendingInstantiation pendingInst {
			GraphLanguage::LoadGraphSyntaxFile(entryFile, entryFn), true, true,
			instantiationParameters
		};
		return InstantiateShader(MakeIteratorRange(&pendingInst, &pendingInst+1));
	}

	InstantiatedShader InstantiateShader(IteratorRange<const InstantiationRequest_ArchiveName*> request)
	{
		assert(!request.empty());
		std::vector<PendingInstantiation> pendingInst;
		pendingInst.reserve(request.size());
		for (const auto&r:request) {
			auto split = SplitArchiveName(r._archiveName);
			pendingInst.emplace_back(
				PendingInstantiation {
					GraphLanguage::LoadGraphSyntaxFile(split.first, split.second), true, true,
					r
				});
		}
		return InstantiateShader(MakeIteratorRange(pendingInst));
	}

	static uint64_t CalculateDepHash(const InstantiationRequest_ArchiveName& dep, uint64_t seed = DefaultSeed64)
	{
		uint64_t result = Hash64(dep._archiveName);
		// todo -- ordering of parameters matters to the hash here
		for (const auto& d:dep._parameterBindings)
			result = Hash64(d.first, CalculateDepHash(d.second, result));
		return result;
	}

    uint64_t InstantiationRequest::CalculateHash() const
    {
        if (_parameterBindings.empty()) return 0;
        uint64 result = DefaultSeed64;
		// todo -- ordering of parameters matters to the hash here
        for (const auto&p:_parameterBindings) {
            result = Hash64(p.first, CalculateDepHash(p.second, result));
			for (const auto&pc:p.second._parametersToCurry)
				result = Hash64(pc, result);
		}
        return result;
    }
}
