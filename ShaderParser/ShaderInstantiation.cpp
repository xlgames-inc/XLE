// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderInstantiation.h"
#include "GraphSyntax.h"
#include "DescriptorSetInstantiation.h"
#include "NodeGraphSignature.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/StringUtils.h"
#include "../Utility/StringFormat.h"
#include <stack>
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

	static std::pair<StringSection<>, StringSection<>> SplitArchiveName(StringSection<> input)
    {
        auto pos = std::find(input.begin(), input.end(), ':');
        if (pos != input.end())
			if ((pos+1) != input.end() && *(pos+1) == ':')
				return std::make_pair(MakeStringSection(input.begin(), pos), MakeStringSection(pos+2, input.end()));

		return std::make_pair(input, StringSection<>{});
    }

	static InstantiatedShader InstantiateShader(IteratorRange<const PendingInstantiation*> rootInstantiations, RenderCore::ShaderLanguage shaderLanguage)
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

				if (inst._isRootInstantiation) {
					InstantiatedShader::EntryPoint entryPoint { scaffoldName, scaffoldSignature };
					if (!scaffoldSignature.GetImplements().empty()) {
						auto implementsSig = inst._graph._subProvider->FindSignature(scaffoldSignature.GetImplements());
						if (implementsSig) {
							entryPoint._implementsName = implementsSig.value()._name;
							entryPoint._implementsSignature = implementsSig.value()._signature;
						}
					}
					result._entryPoints.emplace_back(std::move(entryPoint));
				}
			}
			else
			{
				if (inst._isRootInstantiation)
					result._entryPoints.push_back(instFnEntryPoint);
			}

			result._sourceFragments.insert(
				result._sourceFragments.end(),
				instFn._sourceFragments.begin(), instFn._sourceFragments.end());

			result._depVals.insert(instFn._depVals.begin(), instFn._depVals.end());

			{
				for (const auto&c:inst._graph._signature.GetCapturedParameters()) {
					auto existing = std::find_if(
						result._captures.begin(), result._captures.end(),
						[c](const GraphLanguage::NodeGraphSignature::Parameter& p) { return XlEqString(MakeStringSection(p._name), c._name); });
					if (existing != result._captures.end()) {
						if (existing->_type != c._type || existing->_direction != c._direction)
							Throw(::Exceptions::BasicLabel("Type mismatch detected for capture (%s). Multiple fragments have this capture, but they are not compatible types.", existing->_name.c_str()));
						continue;
					}
					result._captures.push_back(c);
				}
			}
                
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
						includes.insert(std::string(StringMeld<MaxPath>() << filename.AsString() + "_" << instHash));
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

		// Write the merged captures as a cbuffers
		{
			std::stringstream warningMessages;
			result._descriptorSet = MakeMaterialDescriptorSet(
				MakeIteratorRange(result._captures),
				shaderLanguage,
				warningMessages);

			auto fragment = GenerateDescriptorVariables(*result._descriptorSet, MakeIteratorRange(result._captures));
			if (!fragment.empty())
				result._sourceFragments.push_back(fragment);

			fragment = warningMessages.str();
			if (!fragment.empty())
				result._sourceFragments.push_back(fragment);
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
		const InstantiationRequest& instantiationParameters,
		RenderCore::ShaderLanguage shaderLanguage)
	{
		// Note that we end up with a few extra copies of initialGraph, because PendingInstantiation
		// contains a complete copy of the node graph
		PendingInstantiation pendingInst { initialGraph, true, true, instantiationParameters };
		return InstantiateShader(MakeIteratorRange(&pendingInst, &pendingInst+1), shaderLanguage);
	}

	/*
	InstantiatedShader InstantiateShader(
		StringSection<> entryFile,
		StringSection<> entryFn,
		const InstantiationRequest& instantiationParameters,
		RenderCore::ShaderLanguage shaderLanguage)
	{
		PendingInstantiation pendingInst {
			GraphLanguage::LoadGraphSyntaxFile(entryFile, entryFn), true, true,
			instantiationParameters
		};
		return InstantiateShader(MakeIteratorRange(&pendingInst, &pendingInst+1), shaderLanguage);
	}
	*/

	InstantiatedShader InstantiateShader(
		IteratorRange<const InstantiationRequest_ArchiveName*> request,
		RenderCore::ShaderLanguage shaderLanguage)
	{
		GraphLanguage::BasicNodeGraphProvider defaultProvider(::Assets::DirectorySearchRules{});

		assert(!request.empty());
		std::vector<PendingInstantiation> pendingInst;
		pendingInst.reserve(request.size());
		for (const auto&r:request) {

			// We can either be instantiating from a full graph file, or from a specific graph within that file
			// When the request name has an archive name divider (ie, "::"), we will pull out only a single
			// graph from the file.
			// Otherwise we will load every graph from within the file

			auto split = SplitArchiveName(r._archiveName);
			if (split.second.IsEmpty()) {
				// this is a full filename, we should load all of the node graphs within the given
				// file
				std::vector<GraphLanguage::INodeGraphProvider::Signature> signatures;
				if (r._customProvider) {
					signatures = r._customProvider->FindSignatures(r._archiveName);
				} else {
					signatures = defaultProvider.FindSignatures(r._archiveName);
				}

				if (signatures.empty())
					Throw(::Exceptions::BasicLabel("Did not find any node graph signatures for instantiation request (%s)", r._archiveName.c_str()));

				for (const auto&s:signatures) {
					std::string archiveName = r._archiveName + "::" + s._name;

					std::optional<GraphLanguage::INodeGraphProvider::NodeGraph> nodeGraph;
					if (r._customProvider) {
						nodeGraph = r._customProvider->FindGraph(archiveName);
					} else {
						nodeGraph = defaultProvider.FindGraph(archiveName);
					}

					if (!nodeGraph)
						Throw(::Exceptions::BasicLabel("Failed loading graph with name (%s) in archive (%s)", s._name.c_str(), r._archiveName.c_str()));

					pendingInst.emplace_back(
						PendingInstantiation { nodeGraph.value(), true, true, r });
				}

			} else {
				// this refers to a specific item in graph within an outer graph file
				std::optional<GraphLanguage::INodeGraphProvider::NodeGraph> nodeGraph;
				if (r._customProvider) {
					nodeGraph = r._customProvider->FindGraph(r._archiveName);
				} else {
					nodeGraph = defaultProvider.FindGraph(r._archiveName);
				}

				if (!nodeGraph)
					Throw(::Exceptions::BasicLabel("Could not instantiate shader with node graph request (%s)", r._archiveName.c_str()));

				pendingInst.emplace_back(
					PendingInstantiation { nodeGraph.value(), true, true, r });
			}
			
		}
		return InstantiateShader(MakeIteratorRange(pendingInst), shaderLanguage);
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
		// Also hash the selectors given. We could filter these
		// based on what's relevant to the particular include, but that might be overkill
		result = HashCombine(result, _selectors.GetHash());
		result = HashCombine(result, _selectors.GetParameterNamesHash());
        return result;
    }
}
