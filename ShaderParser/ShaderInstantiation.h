// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "NodeGraphProvider.h"
#include "../Utility/StringUtils.h"
#include <vector>
#include <string>

namespace GraphLanguage
{
	class InstantiationParameters
    {
    public:
		struct Dependency;
        std::unordered_map<std::string, Dependency> _parameterBindings;
		bool _generateDanglingInputs = false;
		NodeId _generateDanglingOutputs = NodeId_Interface;
        uint64_t CalculateHash() const;

		InstantiationParameters(std::initializer_list<std::pair<const std::string, Dependency>> init)
		: _parameterBindings(init) {}
		InstantiationParameters() {}
    };

	struct InstantiationParameters::Dependency
	{
		std::string _archiveName;
		std::vector<std::string> _parametersToCurry;
		InstantiationParameters _parameters = {};
		std::shared_ptr<INodeGraphProvider> _customProvider;
	};

    class DependencyTable
    {
    public:
        struct Dependency 
		{ 
			std::string _archiveName; 
			InstantiationParameters _parameters; 
			bool _isGraphSyntaxFile;
			std::shared_ptr<INodeGraphProvider> _customProvider;
		};
        std::vector<Dependency> _dependencies;
    };

	class InstantiatedShader
	{
	public:
		std::vector<std::string> _sourceFragments;
		NodeGraphSignature _entryPointSignature;
		DependencyTable _dependencies;
	};

	InstantiatedShader InstantiateShader(
		StringSection<> entryFile,
		StringSection<> entryFn,
		const InstantiationParameters& instantiationParameters);

	InstantiatedShader InstantiateShader(
		const INodeGraphProvider::NodeGraph& initialGraph,
		bool useScaffoldFunction,
		const InstantiationParameters& instantiationParameters);

        ///////////////////////////////////////////////////////////////

    InstantiatedShader GenerateFunction(
        const NodeGraph& graph, StringSection<char> name, 
        const InstantiationParameters& instantiationParameters,
        INodeGraphProvider& sigProvider);

}
