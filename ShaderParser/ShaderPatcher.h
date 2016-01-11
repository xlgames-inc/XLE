// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Utility/IteratorUtils.h"
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

        Node(Node&& moveFrom) never_throws;
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

    class NodeBaseConnection
    {
    public:
        NodeBaseConnection(uint32 outputNodeId, const std::string& outputParameterName);

        NodeBaseConnection(NodeBaseConnection&& moveFrom) never_throws;
        NodeBaseConnection& operator=(NodeBaseConnection&& moveFrom) never_throws;

		#if defined(COMPILER_DEFAULT_IMPLICIT_OPERATORS)
			NodeBaseConnection(const NodeBaseConnection&) = default;
			NodeBaseConnection& operator=(const NodeBaseConnection&) = default;
		#endif

        uint32      OutputNodeId() const                { return _outputNodeId; }
        const std::string&  OutputParameterName() const { return _outputParameterName; }

    protected:
        uint32          _outputNodeId;
        std::string     _outputParameterName;
    };

        ///////////////////////////////////////////////////////////////

    class NodeConnection : public NodeBaseConnection
    {
    public:
        NodeConnection( uint32 outputNodeId, uint32 inputNodeId, 
                        const std::string& outputParameterName, const Type& outputType, 
                        const std::string& inputParameterName, const Type& inputType);

        NodeConnection(NodeConnection&& moveFrom) never_throws;
        NodeConnection& operator=(NodeConnection&& moveFrom) never_throws;

		#if defined(COMPILER_DEFAULT_IMPLICIT_OPERATORS)
			NodeConnection(const NodeConnection&) = default;
			NodeConnection& operator=(const NodeConnection&) = default;
		#endif

        uint32      InputNodeId() const         { return _inputNodeId; }

        const Type&         InputType() const               { return _inputType; }
        const Type&         OutputType() const              { return _outputType; }
        const std::string&  InputParameterName() const      { return _inputParameterName; }

    private:
        uint32          _inputNodeId;
        std::string     _inputParameterName;
        Type            _inputType;

        Type            _outputType;
    };

        ///////////////////////////////////////////////////////////////

    class ConstantConnection : public NodeBaseConnection
    {
    public:
        ConstantConnection(uint32 outputNodeId, const std::string& outputParameterName, const std::string& value);
        ConstantConnection(ConstantConnection&& moveFrom) never_throws;
        ConstantConnection& operator=(ConstantConnection&& moveFrom) never_throws;

        #if defined(COMPILER_DEFAULT_IMPLICIT_OPERATORS)
			NodeConstantConnection(const NodeConstantConnection&) = default;
			NodeConstantConnection& operator=(const NodeConstantConnection&) = default;
		#endif

        const std::string&  Value() const                   { return _value; }

    private:
        std::string     _value;
    };

            ///////////////////////////////////////////////////////////////

    class InputParameterConnection : public NodeBaseConnection
    {
    public:
        InputParameterConnection(uint32 outputNodeId, const std::string& outputParameterName, const Type& type, const std::string& name, const std::string& semantic);
        InputParameterConnection(InputParameterConnection&& moveFrom) never_throws;
        InputParameterConnection& operator=(InputParameterConnection&& moveFrom) never_throws;

        #if defined(COMPILER_DEFAULT_IMPLICIT_OPERATORS)
			InputParameterConnection(const InputParameterConnection&) = default;
			InputParameterConnection& operator=(const InputParameterConnection&) = default;
		#endif
        
        const Type&         InputType() const        { return _type; }
        const std::string&  InputName() const        { return _name; }
        const std::string&  InputSemantic() const    { return _semantic; }

    private:
        Type            _type;        
        std::string     _name;
        std::string     _semantic;
    };

        ///////////////////////////////////////////////////////////////

    class NodeGraph
    {
    public:
        IteratorRange<const Node*>                      GetNodes() const                        { return MakeIteratorRange(_nodes); }
        IteratorRange<const NodeConnection*>            GetNodeConnections() const              { return MakeIteratorRange(_nodeConnections); }
        IteratorRange<const ConstantConnection*>        GetConstantConnections() const          { return MakeIteratorRange(_constantConnections); }
        IteratorRange<const InputParameterConnection*>  GetInputParameterConnections() const    { return MakeIteratorRange(_inputParameterConnections); }

        void Add(Node&&);
        void Add(NodeConnection&&);
        void Add(ConstantConnection&&);
        void Add(InputParameterConnection&&);

        std::string                         GetName() const                     { return _name; }
        void                                SetName(std::string newName)        { _name = newName; }

        void        TrimForPreview(uint32 previewNode);
        bool        TrimForOutputs(const std::string outputs[], size_t outputCount);
        void        AddDefaultOutputs();

        const Node*         GetNode(uint32 nodeId) const;

        NodeGraph(const std::string& name = std::string());
        NodeGraph(NodeGraph&& moveFrom) never_throws;
        NodeGraph& operator=(NodeGraph&& moveFrom) never_throws;

    private:
        std::vector<Node>                   _nodes;
        std::vector<NodeConnection>         _nodeConnections;
        std::vector<ConstantConnection> _constantConnections;
        std::vector<InputParameterConnection> _inputParameterConnections;
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

