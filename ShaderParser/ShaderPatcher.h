// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/AssetUtils.h"
#include "../Utility/IteratorUtils.h"
#include "../Core/Types.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace ShaderPatcher 
{

        ///////////////////////////////////////////////////////////////

    class Node
    {
    public:
        enum class Type
        {
            Procedure,
            SlotInput,
            SlotOutput,
            Uniforms
        };

        Node(const std::string& archiveName, uint32 nodeId, Type type);

		#if defined(COMPILER_DEFAULT_IMPLICIT_OPERATORS)
			Node(Node&& moveFrom) never_throws = default;
			Node& operator=(Node&& moveFrom) never_throws = default;
			Node(const Node& cloneFrom) = default;
			Node& operator=(const Node& cloneFrom) = default;
		#endif

        const std::string&  ArchiveName() const         { return _archiveName; }
        uint32              NodeId() const              { return _nodeId; }
        Type                GetType() const             { return _type; }
        
    private:
        std::string     _archiveName;
        uint32          _nodeId;
        Type            _type;
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
			ConstantConnection(const ConstantConnection&) = default;
			ConstantConnection& operator=(const ConstantConnection&) = default;
		#endif

        const std::string&  Value() const               { return _value; }

    private:
        std::string     _value;
    };

            ///////////////////////////////////////////////////////////////

    class InputParameterConnection : public NodeBaseConnection
    {
    public:
        InputParameterConnection(
            uint32 outputNodeId, const std::string& outputParameterName, 
            const Type& type, const std::string& name, const std::string& semantic, const std::string& defaultValue);
        InputParameterConnection(InputParameterConnection&& moveFrom) never_throws;
        InputParameterConnection& operator=(InputParameterConnection&& moveFrom) never_throws;

        #if defined(COMPILER_DEFAULT_IMPLICIT_OPERATORS)
			InputParameterConnection(const InputParameterConnection&) = default;
			InputParameterConnection& operator=(const InputParameterConnection&) = default;
		#endif
        
        const Type&         InputType() const           { return _type; }
        const std::string&  InputName() const           { return _name; }
        const std::string&  InputSemantic() const       { return _semantic; }
        const std::string&  Default() const             { return _default; }

    private:
        Type            _type;
        std::string     _name;
        std::string     _semantic;
        std::string     _default;
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

		void			Trim(const uint32* trimNodesBegin, const uint32* trimNodesEnd);
        void            Trim(uint32 previewNode);

        const Node*     GetNode(uint32 nodeId) const;

        NodeGraph();
        ~NodeGraph();

		#if defined(COMPILER_DEFAULT_IMPLICIT_OPERATORS)
			NodeGraph(NodeGraph&&) never_throws = default;
			NodeGraph& operator=(NodeGraph&&) never_throws = default;
			NodeGraph(const NodeGraph&) = default;
			NodeGraph& operator=(const NodeGraph&) = default;
		#endif

    private:
        std::vector<Node> _nodes;
        std::vector<NodeConnection> _nodeConnections;
        std::vector<ConstantConnection> _constantConnections;
        std::vector<InputParameterConnection> _inputParameterConnections;

        bool        IsUpstream(uint32 startNode, uint32 searchingForNode);
        bool        IsDownstream(uint32 startNode, const uint32* searchingForNodesStart, const uint32* searchingForNodesEnd);
        bool        HasNode(uint32 nodeId);
        uint32      GetUniqueNodeId() const;
        void        AddDefaultOutputs(const Node& node);
    };

        ///////////////////////////////////////////////////////////////

    enum class ParameterDirection { In, Out };
    class NodeGraphSignature
    {
    public:
		class Parameter
		{
		public:
			std::string _type, _name;
			ParameterDirection _direction = ParameterDirection::In;
            std::string _semantic, _default;
		};

        // Returns the list of parameters taken as input through the function call mechanism
        auto GetParameters() const -> IteratorRange<const Parameter*>	{ return MakeIteratorRange(_functionParameters); }
        void AddParameter(const Parameter& param);

        // Returns the list of parameters that are accesses as global scope variables (or captured from a containing scope)
        // In other words, these aren't explicitly passed to the function, but the function needs to interact with them, anyway
        auto GetCapturedParameters() const -> IteratorRange<const Parameter*>		{ return MakeIteratorRange(_capturedParameters); }
        void AddCapturedParameter(const Parameter& param);

        class TemplateParameter
        {
        public:
            std::string _name;
            std::string _restriction;
        };
        auto GetTemplateParameters() const -> IteratorRange<const TemplateParameter*>   { return MakeIteratorRange(_templateParameters); }
        void AddTemplateParameter(const TemplateParameter& param);
		
        NodeGraphSignature();
        ~NodeGraphSignature();
    private:
        std::vector<Parameter> _functionParameters;
        std::vector<Parameter> _capturedParameters;
        std::vector<TemplateParameter> _templateParameters;
    };

    class ISignatureProvider
    {
    public:
        struct Result 
        {
            std::string _name;
            const NodeGraphSignature* _signature;
        };
        virtual Result FindSignature(StringSection<> name) = 0;
        virtual ~ISignatureProvider();
    };

        ///////////////////////////////////////////////////////////////

    class InstantiationParameters
    {
    public:
        std::unordered_map<std::string, std::string> _parameterBindings;
        uint64_t CalculateHash() const;
    };

    class DependencyTable
    {
    public:
        struct Dependency { std::string _archiveName; InstantiationParameters _parameters; };
        std::vector<Dependency> _dependencies;
    };

        ///////////////////////////////////////////////////////////////

    std::string GenerateShaderHeader(const NodeGraph& graph);

    struct GeneratedFunction
    {
    public:
        std::string _text;
        NodeGraphSignature _signature;
        DependencyTable _dependencies;
    };
    GeneratedFunction GenerateFunction(
        const NodeGraph& graph, const char name[], 
        const InstantiationParameters& instantiationParameters,
        ISignatureProvider& sigProvider);

	std::string GenerateMaterialCBuffer(const NodeGraphSignature& interf);

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
		const ::Assets::DirectorySearchRules& searchRules,
        const PreviewOptions& previewOptions = { PreviewOptions::Type::Object, std::string(), PreviewOptions::VariableRestrictions() });

	std::string GenerateStructureForTechniqueConfig(const NodeGraphSignature& interf, const char graphName[]);

	std::string GenerateScaffoldFunction(const NodeGraphSignature& outputSignature, const NodeGraphSignature& generatedFunctionSignature, const char name[]);
}

