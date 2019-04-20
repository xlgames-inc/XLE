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
	class InstantiationRequest_ArchiveName;

	/// <summary>Parameters used in a shader instantiation operation</summary>
	/// See the InstantiateShader functions for different ways to use this class.
	/// The instantiation request must be attached to some root instantiation (since this
	/// class is used to fill in the parameters for that instantiation. 
	/// The root instantiation can either be referenced by an archive name, or it can be
	/// a NodeGraph object; but the way the parameters are assigned is the same in either
	/// case
	class InstantiationRequest
	{
	public:
		std::unordered_map<std::string, InstantiationRequest_ArchiveName>	_parameterBindings;
		std::vector<std::string>								_parametersToCurry;

		struct SpecialOptions
		{
			bool _generateDanglingInputs = false;
			GraphLanguage::NodeId _generateDanglingOutputs = GraphLanguage::NodeId_Interface;
		};
		SpecialOptions _options;

		uint64_t CalculateHash() const;

		InstantiationRequest(std::initializer_list<std::pair<const std::string, InstantiationRequest_ArchiveName>> init)
		: _parameterBindings(init) {}
		InstantiationRequest() {}
		~InstantiationRequest() {}
	};

	class InstantiationRequest_ArchiveName : public InstantiationRequest
	{
	public:
		std::string			_archiveName;
		std::shared_ptr<GraphLanguage::INodeGraphProvider> _customProvider;

		InstantiationRequest_ArchiveName(
			const std::string& archiveName,
			std::initializer_list<std::pair<const std::string, InstantiationRequest_ArchiveName>> init)
		: InstantiationRequest(init), _archiveName(archiveName) {}

		InstantiationRequest_ArchiveName(
			const std::string& archiveName,
			InstantiationRequest&& moveFrom)
		: InstantiationRequest(moveFrom), _archiveName(archiveName) {}

		InstantiationRequest_ArchiveName() {}
		~InstantiationRequest_ArchiveName() {}
	};

    class DependencyTable
    {
    public:
        struct Dependency 
		{ 
			InstantiationRequest_ArchiveName _instantiation;
			bool _isGraphSyntaxFile;
		};
        std::vector<Dependency> _dependencies;
    };

	class InstantiatedShader
	{
	public:
		std::vector<std::string> _sourceFragments;

		class EntryPoint
		{
		public:
			std::string _name;
			GraphLanguage::NodeGraphSignature _signature;
		};
		std::vector<EntryPoint> _entryPoints;
		
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
		const InstantiationRequest& instantiationParameters);

	InstantiatedShader InstantiateShader(
		const GraphLanguage::INodeGraphProvider::NodeGraph& initialGraph,
		bool useScaffoldFunction,
		const InstantiationRequest& instantiationParameters);

	InstantiatedShader InstantiateShader(IteratorRange<const InstantiationRequest_ArchiveName*> request);

        ///////////////////////////////////////////////////////////////

    InstantiatedShader GenerateFunction(
        const GraphLanguage::NodeGraph& graph, StringSection<char> name, 
        const InstantiationRequest& instantiationParameters,
        GraphLanguage::INodeGraphProvider& sigProvider);

}
