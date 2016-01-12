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

        const std::string&  ArchiveName() const         { return _archiveName; }
        uint32              NodeId() const              { return _nodeId; }
        Type::Enum          GetType() const             { return _type; }
        
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
                        const std::string& outputParameterName,
                        const std::string& inputParameterName, const Type& inputType);

        NodeConnection(NodeConnection&& moveFrom) never_throws;
        NodeConnection& operator=(NodeConnection&& moveFrom) never_throws;

		#if defined(COMPILER_DEFAULT_IMPLICIT_OPERATORS)
			NodeConnection(const NodeConnection&) = default;
			NodeConnection& operator=(const NodeConnection&) = default;
		#endif

        uint32              InputNodeId() const         { return _inputNodeId; }
        const Type&         InputType() const           { return _inputType; }
        const std::string&  InputParameterName() const  { return _inputParameterName; }

    private:
        uint32          _inputNodeId;
        std::string     _inputParameterName;
        Type            _inputType;
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

        const std::string&  Value() const               { return _value; }

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
        
        const Type&         InputType() const           { return _type; }
        const std::string&  InputName() const           { return _name; }
        const std::string&  InputSemantic() const       { return _semantic; }

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

        std::string     GetName() const                 { return _name; }
        void            SetName(std::string newName)    { _name = newName; }

        void            TrimForPreview(uint32 previewNode);
        bool            TrimForOutputs(const std::string outputs[], size_t outputCount);
        void            AddDefaultOutputs();

        const Node*     GetNode(uint32 nodeId) const;

        NodeGraph(NodeGraph&& moveFrom) never_throws;
        NodeGraph& operator=(NodeGraph&& moveFrom) never_throws;
        NodeGraph(const std::string& name = std::string());
        ~NodeGraph();

    private:
        std::vector<Node> _nodes;
        std::vector<NodeConnection> _nodeConnections;
        std::vector<ConstantConnection> _constantConnections;
        std::vector<InputParameterConnection> _inputParameterConnections;
        std::string _name;

        void        Trim(const uint32* trimNodesBegin, const uint32* trimNodesEnd);
        bool        IsUpstream(uint32 startNode, uint32 searchingForNode);
        bool        IsDownstream(uint32 startNode, const uint32* searchingForNodesStart, const uint32* searchingForNodesEnd);
        bool        HasNode(uint32 nodeId);
        uint32      GetUniqueNodeId() const;
        void        AddDefaultOutputs(const Node& node);
    };

        ///////////////////////////////////////////////////////////////

    class MainFunctionParameter
    {
    public:
        std::string _type, _name, _archiveName, _semantic;
        MainFunctionParameter(
            const std::string& type, const std::string& name, 
            const std::string& archiveName, const std::string& semantic = std::string())
            : _type(type), _name(name), _archiveName(archiveName), _semantic(semantic) {}
        MainFunctionParameter() {}
    };

    class MainFunctionInterface
    {
    public:
        auto GetInputParameters() const -> IteratorRange<const MainFunctionParameter*>      { return MakeIteratorRange(_inputParameters); }
        auto GetOutputParameters() const -> IteratorRange<const MainFunctionParameter*>     { return MakeIteratorRange(_outputParameters); }
        auto GetGlobalParameters() const -> IteratorRange<const MainFunctionParameter*>     { return MakeIteratorRange(_globalParameters); }
        const NodeGraph& GetGraphOfTemporaries() const { return _graphOfTemporaries; }

        MainFunctionInterface(const NodeGraph& graph);
        ~MainFunctionInterface();
    private:
        std::vector<MainFunctionParameter> _inputParameters;
        std::vector<MainFunctionParameter> _outputParameters;
        std::vector<MainFunctionParameter> _globalParameters;
        NodeGraph _graphOfTemporaries;
    };

    namespace MaterialConstantsStyle
    {
        enum Enum { CBuffer };
    }

    std::string GenerateShaderHeader(   
        const NodeGraph& graph, 
        MaterialConstantsStyle::Enum materialConstantsStyle = MaterialConstantsStyle::CBuffer, 
        bool copyFragmentContents = false);

    std::string GenerateShaderBody(
        const NodeGraph& graph, const MainFunctionInterface& interf);

    struct PreviewOptions
    {
    public:
        enum class Type { Object, Chart };
        Type _type;
        std::string _outputToVisualize;
    };

    std::string GenerateStructureForPreview(
        const NodeGraph& graph, 
        const MainFunctionInterface& interf, 
        const PreviewOptions& previewOptions = { PreviewOptions::Type::Object, std::string() });

}

