// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "InterfaceSignature.h"
#include "../Assets/AssetUtils.h"
#include "../Utility/IteratorUtils.h"
#include "../Core/Types.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace GraphLanguage 
{
	using NodeId = uint32_t;

	static NodeId NodeId_Interface = (NodeId)-1;
	static NodeId NodeId_Constant = (NodeId)-2;
	extern std::string ParameterName_NodeInstantiation;

        ///////////////////////////////////////////////////////////////

    class Node
    {
    public:
        enum class Type { Procedure, Captures };

		std::string				_archiveName;
        NodeId					_nodeId = 0;
        Type					_type = Type::Procedure;
		std::string				_attributeTableName;

        const std::string&		ArchiveName() const         { return _archiveName; }
        NodeId					NodeId() const              { return _nodeId; }
        Type					GetType() const             { return _type; }
		const std::string&		AttributeTableName() const  { return _attributeTableName; }
    };

        ///////////////////////////////////////////////////////////////

    class Connection
    {
    public:
        NodeId				_inputNodeId;
        std::string			_inputParameterName;
		NodeId				_outputNodeId;
        std::string			_outputParameterName;
		std::string			_condition;

        NodeId              InputNodeId() const				{ return _inputNodeId; }
        const std::string&  InputParameterName() const		{ return _inputParameterName; }
		NodeId				OutputNodeId() const            { return _outputNodeId; }
        const std::string&  OutputParameterName() const		{ return _outputParameterName; }
    };
  
        ///////////////////////////////////////////////////////////////

    class NodeGraph
    {
    public:
        IteratorRange<const Node*>			GetNodes() const			{ return MakeIteratorRange(_nodes); }
        IteratorRange<const Connection*>	GetConnections() const		{ return MakeIteratorRange(_connections); }

		IteratorRange<Node*>				GetNodes()			{ return MakeIteratorRange(_nodes); }
        IteratorRange<Connection*>			GetConnections()	{ return MakeIteratorRange(_connections); }

        void			Add(Node&&);
        void			Add(Connection&&);

		void			Trim(const NodeId* trimNodesBegin, const NodeId* trimNodesEnd);
        void			Trim(NodeId previewNode);

        const Node*     GetNode(NodeId nodeId) const;

        NodeGraph();
        ~NodeGraph();

    private:
        std::vector<Node>		_nodes;
        std::vector<Connection> _connections;

        bool        IsUpstream(NodeId startNode, NodeId searchingForNode);
        bool        IsDownstream(NodeId startNode, const NodeId* searchingForNodesStart, const NodeId* searchingForNodesEnd);
        bool        HasNode(NodeId nodeId);
    };

		///////////////////////////////////////////////////////////////

	using AttributeTable = std::unordered_map<std::string, std::string>;
	
	class AttributeTableSet
	{
	public:
		std::unordered_map<std::string, AttributeTable> _tables;
	};

        ///////////////////////////////////////////////////////////////

	std::string GenerateMaterialCBuffer(const NodeGraphSignature& interf);

	std::string GenerateGraphSyntax(const NodeGraph& graph, const NodeGraphSignature& interf, StringSection<> name);

    struct PreviewOptions
    {
    public:
        enum class Type { Object, Chart };
        Type _type;
        std::string _outputToVisualize;
		using VariableRestrictions = std::vector<std::pair<std::string, std::string>>;
        VariableRestrictions _variableRestrictions;
    };

    std::string GenerateStructureForPreview(
        StringSection<char> graphName, 
        const NodeGraphSignature& interf, 
        const PreviewOptions& previewOptions = { PreviewOptions::Type::Object, std::string(), PreviewOptions::VariableRestrictions() });

	std::string GenerateStructureForTechniqueConfig(const NodeGraphSignature& interf, StringSection<char> graphName);

	std::string GenerateScaffoldFunction(
		const NodeGraphSignature& outputSignature, 
		const NodeGraphSignature& generatedFunctionSignature, 
		StringSection<char> scaffoldFunctionName,
		StringSection<char> implementationFunctionName);
}

