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
        
        Node(const std::string& archiveName, uint64 nodeId, Type::Enum type);

        Node(Node&& moveFrom);
        Node& operator=(Node&& moveFrom) never_throws;
        Node& operator=(const Node& cloneFrom);

        const std::string&  ArchiveName() const     { return _archiveName; }
        uint64              NodeId() const          { return _nodeId; }
        Type::Enum          GetType() const         { return _type; }
        
    private:
        std::string     _archiveName;
        uint64          _nodeId;
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
        NodeConnection( uint64 outputNodeId, uint64 inputNodeId, 
                        const std::string& outputParameterName, const Type& outputType, 
                        const std::string& inputParameterName, const Type& inputType);

        NodeConnection(NodeConnection&& moveFrom);
        NodeConnection& operator=(NodeConnection&& moveFrom);

        uint64      OutputNodeId() const        { return _outputNodeId; }
        uint64      InputNodeId() const         { return _inputNodeId; }

        const Type&         InputType() const               { return _inputType; }
        const Type&         OutputType() const              { return _outputType; }
        const std::string&  InputParameterName() const      { return _inputParameterName; }
        const std::string&  OutputParameterName() const     { return _outputParameterName; }

    private:
        uint64          _outputNodeId;
        uint64          _inputNodeId;
        std::string     _outputParameterName;
        Type            _outputType;
        std::string     _inputParameterName;
        Type            _inputType;
    };

        ///////////////////////////////////////////////////////////////

    class NodeConstantConnection
    {
    public:
        NodeConstantConnection(uint64 outputNodeId, const std::string& outputParameterName, const std::string& value);
        NodeConstantConnection(NodeConstantConnection&& moveFrom);
        NodeConstantConnection& operator=(NodeConstantConnection&& moveFrom);

        uint64              OutputNodeId() const            { return _outputNodeId; }
        const std::string&  OutputParameterName() const     { return _outputParameterName; }
        const std::string&  Value() const                   { return _value; }

    private:
        uint64          _outputNodeId;
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

        void        TrimForPreview(uint64 previewNode);
        bool        TrimForOutputs(const std::string outputs[], size_t outputCount);
        void        AddDefaultOutputs();

        const Node*         GetNode(uint64 nodeId) const;

        NodeGraph(const std::string& name = std::string());
        NodeGraph(NodeGraph&& moveFrom);
        NodeGraph& operator=(NodeGraph&& moveFrom);

    private:
        std::vector<Node>                   _nodes;
        std::vector<NodeConnection>         _nodeConnections;
        std::vector<NodeConstantConnection> _nodeConstantConnections;
        std::string                         _name;

        void        Trim(const uint64* trimNodesBegin, const uint64* trimNodesEnd);
        bool        IsUpstream(uint64 startNode, uint64 searchingForNode);
        bool        IsDownstream(uint64 startNode, const uint64* searchingForNodesStart, const uint64* searchingForNodesEnd);
        bool        HasNode(uint64 nodeId);
        uint64      GetUniqueNodeId() const;
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
    std::string         GenerateStructureForPreview(const NodeGraph& graph, const NodeGraph& graphOfTemporaries);
    NodeGraph           GenerateGraphOfTemporaries(const NodeGraph& graph);
    

}

