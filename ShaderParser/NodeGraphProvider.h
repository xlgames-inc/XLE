
#pragma once

#include "ShaderPatcher.h"
#include "../Utility/StringUtils.h"
#include <unordered_map>
#include <optional>

namespace ShaderPatcher
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
        };
        virtual std::optional<Signature> FindSignature(StringSection<> name) = 0;

		struct NodeGraph
        {
            std::string _name;
            ShaderPatcher::NodeGraph _graph;
			NodeGraphSignature _signature;
			std::shared_ptr<INodeGraphProvider> _subProvider;
        };
        virtual std::optional<NodeGraph> FindGraph(StringSection<> name) = 0;

        virtual ~INodeGraphProvider();
    };

    class BasicNodeGraphProvider : public INodeGraphProvider
    {
    public:
        std::optional<Signature> FindSignature(StringSection<> name);

        BasicNodeGraphProvider(const ::Assets::DirectorySearchRules& searchRules);
        ~BasicNodeGraphProvider();
    protected:
        ::Assets::DirectorySearchRules _searchRules;
		struct Entry
		{
			std::string _name;
			NodeGraphSignature _sig;
			std::string _sourceFile;
		};
        std::unordered_map<uint64, Entry> _cache;        
    };
}

