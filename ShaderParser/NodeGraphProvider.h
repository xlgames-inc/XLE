// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "NodeGraph.h"
#include "NodeGraphSignature.h"
#include "../Utility/StringUtils.h"
#include "../Utility/Optional.h"
#include <unordered_map>
#include <set>

namespace GraphLanguage
{
	class FunctionSignature;

	class INodeGraphProvider
    {
    public:
        struct Signature 
        {
            std::string _name;
            NodeGraphSignature _signature;
			std::string _sourceFile;
			bool _isGraphSyntax = false;
        };
        virtual std::optional<Signature> FindSignature(StringSection<> name) = 0;

		struct NodeGraph
        {
            std::string _name;
            GraphLanguage::NodeGraph _graph;
			NodeGraphSignature _signature;
			std::shared_ptr<INodeGraphProvider> _subProvider;
        };
        virtual std::optional<NodeGraph> FindGraph(StringSection<> name) = 0;

		virtual std::string TryFindAttachedFile(StringSection<> name) = 0;

        virtual ~INodeGraphProvider();
    };

    class BasicNodeGraphProvider : public INodeGraphProvider
    {
    public:
        std::optional<Signature> FindSignature(StringSection<> name);
		std::optional<NodeGraph> FindGraph(StringSection<> name);
		std::string TryFindAttachedFile(StringSection<> name);

        BasicNodeGraphProvider(const ::Assets::DirectorySearchRules& searchRules);
        ~BasicNodeGraphProvider();
    protected:
        ::Assets::DirectorySearchRules _searchRules;
		struct Entry
		{
			std::string _name;
			NodeGraphSignature _sig;
			std::string _sourceFile;
			bool _isGraphSyntaxFile;
		};
        std::unordered_map<uint64, Entry> _cache;        
    };

	void AddAttachedSchemaFiles(
		std::vector<std::pair<std::string, std::string>>& result,
		const std::string& graphArchiveName,
		GraphLanguage::INodeGraphProvider& nodeGraphProvider);
}

