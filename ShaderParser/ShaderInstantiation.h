// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "NodeGraphProvider.h"
#include "../Utility/StringUtils.h"
#include <vector>
#include <string>

namespace RenderCore { namespace Techniques { class PredefinedCBLayout; } }

namespace ShaderSourceParser
{
	class InstantiationParameters
    {
    public:
		struct Dependency;
        std::unordered_map<std::string, Dependency> _parameterBindings;
		bool _generateDanglingInputs = false;
		GraphLanguage::NodeId _generateDanglingOutputs = GraphLanguage::NodeId_Interface;
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
		std::shared_ptr<GraphLanguage::INodeGraphProvider> _customProvider;
	};

    class DependencyTable
    {
    public:
        struct Dependency 
		{ 
			std::string _archiveName; 
			InstantiationParameters _parameters; 
			bool _isGraphSyntaxFile;
			std::shared_ptr<GraphLanguage::INodeGraphProvider> _customProvider;
		};
        std::vector<Dependency> _dependencies;
    };

	class InstantiatedShader
	{
	public:
		std::vector<std::string> _sourceFragments;
		GraphLanguage::NodeGraphSignature _entryPointSignature;
		DependencyTable _dependencies;

		struct ConstantBuffer
		{
			std::string _name;
			std::shared_ptr<RenderCore::Techniques::PredefinedCBLayout> _layout;
		};
		std::vector<ConstantBuffer> _constantBuffers;
	};

	InstantiatedShader InstantiateShader(
		StringSection<> entryFile,
		StringSection<> entryFn,
		const InstantiationParameters& instantiationParameters);

	InstantiatedShader InstantiateShader(
		const GraphLanguage::INodeGraphProvider::NodeGraph& initialGraph,
		bool useScaffoldFunction,
		const InstantiationParameters& instantiationParameters);

        ///////////////////////////////////////////////////////////////

    InstantiatedShader GenerateFunction(
        const GraphLanguage::NodeGraph& graph, StringSection<char> name, 
        const InstantiationParameters& instantiationParameters,
        GraphLanguage::INodeGraphProvider& sigProvider);

}
