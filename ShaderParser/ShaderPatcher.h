// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Types.h"
#include <string>
#include <vector>

namespace ShaderPatcher 
{

        ///////////////////////////////////////////////////////////////

    class Node
    {
    public:
        struct Type
        {
            enum Enum
            {
                Procedure,
                MaterialCBuffer,
                InterpolatorIntoVertex,
                InterpolatorIntoPixel,
                SystemParameters,
                Output,
                Constants           // (ie, true constants -- hard coded into the shader)
            };
        };
        
        Node(const std::string& archiveName, uint32 nodeId, Type::Enum type);

        Node(Node&& moveFrom);
        Node& operator=(Node&& moveFrom) never_throws;
        Node& operator=(const Node& cloneFrom);

        const std::string&  ArchiveName() const     { return _archiveName; }
        uint32              NodeId() const          { return _nodeId; }
        Type::Enum          GetType() const         { return _type; }
        
    private:
        std::string     _archiveName;
        uint32          _nodeId;
        Type::Enum      _type;
    };

        ///////////////////////////////////////////////////////////////

    class Type
    {
    public:
        std::string _name;
        Type() {}
        Type(const std::string& name) : _name(name) {}
    };

        ///////////////////////////////////////////////////////////////

    class NodeConnection
    {
    public:
        NodeConnection( uint32 outputNodeId, uint32 inputNodeId, 
                        const std::string& outputParameterName, const Type& outputType, 
                        const std::string& inputParameterName, const Type& inputType);

        NodeConnection(NodeConnection&& moveFrom);
        NodeConnection& operator=(NodeConnection&& moveFrom);

		#if defined(COMPILER_DEFAULT_IMPLICIT_OPERATORS)
			NodeConnection(const NodeConnection&) = default;
			NodeConnection& operator=(const NodeConnection&) = default;
		#endif

        uint32      OutputNodeId() const        { return _outputNodeId; }
        uint32      InputNodeId() const         { return _inputNodeId; }

        const Type&         InputType() const               { return _inputType; }
        const Type&         OutputType() const              { return _outputType; }
        const std::string&  InputParameterName() const      { return _inputParameterName; }
        const std::string&  OutputParameterName() const     { return _outputParameterName; }

    private:
        uint32          _outputNodeId;
        uint32          _inputNodeId;
        std::string     _outputParameterName;
        Type            _outputType;
        std::string     _inputParameterName;
        Type            _inputType;
    };

        ///////////////////////////////////////////////////////////////

    class NodeConstantConnection
    {
    public:
        NodeConstantConnection(uint32 outputNodeId, const std::string& outputParameterName, const std::string& value);
        NodeConstantConnection(NodeConstantConnection&& moveFrom);
        NodeConstantConnection& operator=(NodeConstantConnection&& moveFrom);

        uint32              OutputNodeId() const            { return _outputNodeId; }
        const std::string&  OutputParameterName() const     { return _outputParameterName; }
        const std::string&  Value() const                   { return _value; }

    private:
        uint32          _outputNodeId;
        std::string     _outputParameterName;
        std::string     _value;
    };

        ///////////////////////////////////////////////////////////////

    class NodeGraph
    {
    public:
        std::vector<Node>&                  GetNodes()                          { return _nodes; }
        const std::vector<Node>&            GetNodes() const                    { return _nodes; }

        std::vector<NodeConnection>&        GetNodeConnections()                { return _nodeConnections; }
        const std::vector<NodeConnection>&  GetNodeConnections() const          { return _nodeConnections; }

        std::vector<NodeConstantConnection>&        GetNodeConstantConnections()          { return _nodeConstantConnections; }
        const std::vector<NodeConstantConnection>&  GetNodeConstantConnections() const    { return _nodeConstantConnections; }

        std::string                         GetName() const                     { return _name; }
        void                                SetName(std::string newName)        { _name = newName; }

        void        TrimForPreview(uint32 previewNode);
        bool        TrimForOutputs(const std::string outputs[], size_t outputCount);
        void        AddDefaultOutputs();

        const Node*         GetNode(uint32 nodeId) const;

        NodeGraph(const std::string& name = std::string());
        NodeGraph(NodeGraph&& moveFrom);
        NodeGraph& operator=(NodeGraph&& moveFrom);

    private:
        std::vector<Node>                   _nodes;
        std::vector<NodeConnection>         _nodeConnections;
        std::vector<NodeConstantConnection> _nodeConstantConnections;
        std::string                         _name;

        void        Trim(const uint32* trimNodesBegin, const uint32* trimNodesEnd);
        bool        IsUpstream(uint32 startNode, uint32 searchingForNode);
        bool        IsDownstream(uint32 startNode, const uint32* searchingForNodesStart, const uint32* searchingForNodesEnd);
        bool        HasNode(uint32 nodeId);
        uint32      GetUniqueNodeId() const;
        void        AddDefaultOutputs(const Node& node);
    };

        ///////////////////////////////////////////////////////////////

    namespace MaterialConstantsStyle
    {
        enum Enum { CBuffer };
    }

    std::string         GenerateShaderHeader(   const NodeGraph& graph, 
                                                MaterialConstantsStyle::Enum materialConstantsStyle = MaterialConstantsStyle::CBuffer, 
                                                bool copyFragmentContents = false);
    std::string         GenerateShaderBody(const NodeGraph& graph, const NodeGraph& graphOfTemporaries);
    std::string         GenerateStructureForPreview(const NodeGraph& graph, const NodeGraph& graphOfTemporaries, const char outputToVisualize[]);
    NodeGraph           GenerateGraphOfTemporaries(const NodeGraph& graph);
    

}

