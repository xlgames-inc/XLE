// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderPatcher.h"
#include "InterfaceSignature.h"
#include "ParameterSignature.h"
#include "../RenderCore/ShaderLangUtil.h"
#include "../Core/Exceptions.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/StringUtils.h"
#include "../Utility/StringFormat.h"
#include "../Utility/PtrUtils.h"
#include <sstream>
#include <set>
#include <assert.h>
#include <algorithm>
#include <tuple>
#include <regex>

    // mustache templates stuff...
#include "../Assets/AssetUtils.h"
#include "../Assets/ConfigFileContainer.h"
#include "../Utility/Streams/StreamFormatter.h"
#include "../Foreign/plustasche/template.hpp"


#pragma warning(disable:4127)       // conditional expression is constant

namespace ShaderPatcher 
{

    static const std::string s_resultName = "result";
    static const uint32 s_nodeId_Invalid = ~0u;

        ///////////////////////////////////////////////////////////////

    Node::Node(const std::string& archiveName, uint32 nodeId, Type::Enum type)
    : _archiveName(archiveName)
    , _nodeId(nodeId)
    , _type(type)
    {}

    Node::Node(Node&& moveFrom) 
    :   _archiveName(std::move(moveFrom._archiveName))
    ,   _nodeId(moveFrom._nodeId)
    ,   _type(moveFrom._type)
    {}

    Node& Node::operator=(Node&& moveFrom) never_throws
    {
        _archiveName = std::move(moveFrom._archiveName);
        _nodeId = moveFrom._nodeId;
        _type = moveFrom._type;
        return *this;
    }

    Node& Node::operator=(const Node& cloneFrom)
    {
        _archiveName = cloneFrom._archiveName;
        _nodeId = cloneFrom._nodeId;
        _type = cloneFrom._type;
        return *this;
    }

        ///////////////////////////////////////////////////////////////

    NodeBaseConnection::NodeBaseConnection(uint32 outputNodeId, const std::string& outputParameterName)
    : _outputNodeId(outputNodeId), _outputParameterName(outputParameterName) {}

    NodeBaseConnection::NodeBaseConnection(NodeBaseConnection&& moveFrom) never_throws
    : _outputNodeId(moveFrom._outputNodeId)
    , _outputParameterName(std::move(moveFrom._outputParameterName))
    {
    }

    NodeBaseConnection& NodeBaseConnection::operator=(NodeBaseConnection&& moveFrom) never_throws
    {
        _outputNodeId = moveFrom._outputNodeId;
        _outputParameterName = std::move(moveFrom._outputParameterName);
        return *this;
    }

        ///////////////////////////////////////////////////////////////

    NodeConnection::NodeConnection( uint32 outputNodeId, uint32 inputNodeId, 
                                    const std::string& outputParameterName,
                                    const std::string& inputParameterName, const Type& inputType)
    :       NodeBaseConnection(outputNodeId, outputParameterName)
    ,       _inputNodeId(inputNodeId)
    ,       _inputParameterName(inputParameterName)
    ,       _inputType(inputType)
    {}

    NodeConnection::NodeConnection(NodeConnection&& moveFrom)
    :       NodeBaseConnection(std::move(moveFrom))
    ,       _inputNodeId(moveFrom._inputNodeId)
    ,       _inputParameterName(moveFrom._inputParameterName)
    ,       _inputType(moveFrom._inputType)
    {}

    NodeConnection& NodeConnection::operator=(NodeConnection&& moveFrom)
    {
        NodeBaseConnection::operator=(std::move(moveFrom));
        _inputNodeId = moveFrom._inputNodeId;
        _inputParameterName = moveFrom._inputParameterName;
        _inputType = moveFrom._inputType;
        return *this;
    }

        ///////////////////////////////////////////////////////////////

    ConstantConnection::ConstantConnection(uint32 outputNodeId, const std::string& outputParameterName, const std::string& value)
    :   NodeBaseConnection(outputNodeId, outputParameterName)
    ,   _value(value) {}

    ConstantConnection::ConstantConnection(ConstantConnection&& moveFrom)
    :   NodeBaseConnection(std::move(moveFrom))
    ,   _value(moveFrom._value) {}

    ConstantConnection& ConstantConnection::operator=(ConstantConnection&& moveFrom)
    {
        NodeBaseConnection::operator=(std::move(moveFrom));
        _value = moveFrom._value;
        return *this;
    }

        ///////////////////////////////////////////////////////////////

    InputParameterConnection::InputParameterConnection(uint32 outputNodeId, const std::string& outputParameterName, const Type& type, const std::string& name, const std::string& semantic, const std::string& defaultValue)
    :   NodeBaseConnection(outputNodeId, outputParameterName)
    ,   _type(type), _name(name), _semantic(semantic), _default(defaultValue) {}

    InputParameterConnection::InputParameterConnection(InputParameterConnection&& moveFrom)
    :   NodeBaseConnection(std::move(moveFrom))
    ,   _type(std::move(moveFrom._type)), _name(std::move(moveFrom._name)), _semantic(std::move(moveFrom._semantic)), _default(std::move(moveFrom._default)) {}

    InputParameterConnection& InputParameterConnection::operator=(InputParameterConnection&& moveFrom)
    {
        NodeBaseConnection::operator=(std::move(moveFrom));
        _type = std::move(moveFrom._type);
        _name = std::move(moveFrom._name);
        _semantic = std::move(moveFrom._semantic);
        _default = std::move(moveFrom._default);
        return *this;
    }

        ///////////////////////////////////////////////////////////////

    NodeGraph::NodeGraph(const std::string& name) : _name(name) {}
    NodeGraph::~NodeGraph() {}

    NodeGraph::NodeGraph(NodeGraph&& moveFrom) 
    :   _nodes(std::move(moveFrom._nodes))
    ,   _nodeConnections(std::move(moveFrom._nodeConnections))
    ,   _constantConnections(std::move(moveFrom._constantConnections))
    ,   _inputParameterConnections(std::move(moveFrom._inputParameterConnections))
    ,   _name(std::move(moveFrom._name))
    {}

    NodeGraph& NodeGraph::operator=(NodeGraph&& moveFrom)
    {
        _nodes = std::move(moveFrom._nodes);
        _nodeConnections = std::move(moveFrom._nodeConnections);
        _constantConnections = std::move(moveFrom._constantConnections);
        _inputParameterConnections = std::move(moveFrom._inputParameterConnections);
        _name = std::move(moveFrom._name);
        return *this;
    }

    void NodeGraph::Add(Node&& a) { _nodes.emplace_back(std::move(a)); }
    void NodeGraph::Add(NodeConnection&& a) { _nodeConnections.emplace_back(std::move(a)); }
    void NodeGraph::Add(ConstantConnection&& a) { _constantConnections.emplace_back(std::move(a)); }
    void NodeGraph::Add(InputParameterConnection&& a) { _inputParameterConnections.emplace_back(std::move(a)); }

    bool NodeGraph::IsUpstream(uint32 startNode, uint32 searchingForNode)
    {
            //  Starting at 'startNode', search upstream and see if we find 'searchingForNode'
        if (startNode == searchingForNode) {
            return true;
        }
            
        for (auto i=_nodeConnections.cbegin(); i!=_nodeConnections.cend(); ++i) {
            if (i->OutputNodeId() == startNode) {
                if (IsUpstream(i->InputNodeId(), searchingForNode)) {
                    return true;
                }
            }
        }

        return false;
    }

    bool NodeGraph::IsDownstream(
        uint32 startNode, 
        const uint32* searchingForNodesStart, const uint32* searchingForNodesEnd)
    {
        if (std::find(searchingForNodesStart, searchingForNodesEnd, startNode) != searchingForNodesEnd) {
            return true;
        }

        for (auto i=_nodeConnections.cbegin(); i!=_nodeConnections.cend(); ++i) {
            if (i->InputNodeId() == startNode) {
                if (IsDownstream(i->OutputNodeId(), searchingForNodesStart, searchingForNodesEnd)) {
                    return true;
                }
            }
        }

        return false;
    }

    bool            NodeGraph::HasNode(uint32 nodeId)
    {
        return std::find_if(_nodes.begin(), _nodes.end(), 
            [=](const Node& node) { return node.NodeId() == nodeId; }) != _nodes.end();
    }

    static std::string LoadSourceFile(const std::string& sourceFileName)
    {
        TRY {
            Utility::BasicFile file(sourceFileName.c_str(), "rb");

            file.Seek(0, SEEK_END);
            size_t size = file.TellP();
            file.Seek(0, SEEK_SET);

            std::string result;
            result.resize(size, '\0');
            file.Read(&result.at(0), 1, size);
            return result;

        } CATCH(const std::exception& ) {
            return std::string();
        } CATCH_END
    }
    
    static std::tuple<std::string, std::string> SplitArchiveName(const std::string& archiveName)
    {
        std::string::size_type pos = archiveName.find_first_of(':');
        if (pos != std::string::npos) {
            return std::make_tuple(archiveName.substr(0, pos), archiveName.substr(pos+1));
        } else {
            return std::make_tuple(std::string(), archiveName);
        }
    }

	class ShaderFragment
	{
	public:
		auto GetFunction(const char fnName[]) const -> const ShaderSourceParser::FunctionSignature*;
		auto GetParameterStruct(const char structName[]) const -> const ShaderSourceParser::ParameterStructSignature*;
		const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }
		ShaderFragment(const ::Assets::ResChar fn[]);
		~ShaderFragment();
	private:
		ShaderSourceParser::ShaderFragmentSignature _sig;
		::Assets::DepValPtr _depVal;
	};

	auto ShaderFragment::GetFunction(const char fnName[]) const -> const ShaderSourceParser::FunctionSignature*
	{
		auto i = std::find_if(
			_sig._functions.cbegin(), _sig._functions.cend(), 
            [fnName](const ShaderSourceParser::FunctionSignature& signature) { return XlEqString(signature._name, fnName); });
        if (i!=_sig._functions.cend())
			return AsPointer(i);
		return nullptr;
	}

	auto ShaderFragment::GetParameterStruct(const char structName[]) const -> const ShaderSourceParser::ParameterStructSignature*
	{
		auto i = std::find_if(
			_sig._parameterStructs.cbegin(), _sig._parameterStructs.cend(), 
            [structName](const ShaderSourceParser::ParameterStructSignature& signature) { return XlEqString(signature._name, structName); });
        if (i!=_sig._parameterStructs.cend())
			return AsPointer(i);
		return nullptr;
	}

	ShaderFragment::ShaderFragment(const ::Assets::ResChar fn[])
	{
		auto shaderFile = LoadSourceFile(fn);
		_sig = ShaderSourceParser::BuildShaderFragmentSignature(shaderFile.c_str(), shaderFile.size());
		_depVal = std::make_shared<::Assets::DependencyValidation>();
		::Assets::RegisterFileDependency(_depVal, fn);
	}

	ShaderFragment::~ShaderFragment() {}

	static const ShaderSourceParser::FunctionSignature& LoadFunctionSignature(const std::tuple<std::string, std::string>& splitName)
    {
        TRY {
			auto& frag = ::Assets::GetAssetDep<ShaderFragment>(std::get<0>(splitName).c_str());
			auto* fn = frag.GetFunction(std::get<1>(splitName).c_str());
			if (fn != nullptr) return *fn;
        } CATCH (...) {
        } CATCH_END
		static ShaderSourceParser::FunctionSignature blank;
        return blank;
    }

    static const ShaderSourceParser::ParameterStructSignature& LoadParameterStructSignature(const std::tuple<std::string, std::string>& splitName)
    {
        if (!std::get<0>(splitName).empty()) {
            using namespace ShaderSourceParser;
            TRY {
				auto& frag = ::Assets::GetAssetDep<ShaderFragment>(std::get<0>(splitName).c_str());
				auto* str = frag.GetParameterStruct(std::get<1>(splitName).c_str());
				if (str != nullptr) return *str;
            } CATCH (...) {
            } CATCH_END
        }

		static ShaderSourceParser::ParameterStructSignature blank;
        return blank;
    }

    static bool HasResultValue(const ShaderSourceParser::FunctionSignature& sig) { return !sig._returnType.empty() && sig._returnType != "void"; }

    bool            NodeGraph::TrimForOutputs(const std::string outputs[], size_t outputCount)
    {
            //
            //      We want to trim the graph so that we end up only with the parts
            //      that output to the given output parameters.
            //
            //      Collect a list of nodes that output to any of the output 
            //      connections,
            //

        std::vector<uint32> trimmingNodes;
        auto outputsEnd = &outputs[outputCount];
        for (auto i=GetNodeConnections().cbegin(); i!=GetNodeConnections().cend(); ++i) {
            if (std::find(outputs, outputsEnd, i->OutputParameterName()) != outputsEnd) {
                trimmingNodes.push_back(i->InputNodeId());
            }
        }

        if (trimmingNodes.empty()) {
            return false;   // nothing leads to these outputs
        }

            //  Remove duplicates in "trimming nodes"
        trimmingNodes.erase(std::unique(trimmingNodes.begin(), trimmingNodes.end()), trimmingNodes.end());

        Trim(AsPointer(trimmingNodes.begin()), AsPointer(trimmingNodes.end()));
        return true;
    }

    uint32            NodeGraph::GetUniqueNodeId() const
    {
        uint32 largestId = 0;
        std::for_each(_nodes.cbegin(), _nodes.cend(), [&](const Node& n) { largestId = std::max(largestId, n.NodeId()); });
        return largestId+1;
    }

    const Node*     NodeGraph::GetNode(uint32 nodeId) const
    {
        auto res = std::find_if(
            _nodes.cbegin(), _nodes.cend(),
            [=](const Node& n) { return n.NodeId() == nodeId; });
        if (res != _nodes.cend()) {
            return &*res;
        }
        return nullptr;
    }

    static unsigned GetDimensionality(const std::string& typeName)
    {
        if (typeName.empty()) {
            return 0;
        }

        size_t length = typeName.length();
        if (XlIsDigit(typeName[length-1])) {
            if (length >= 3 && typeName[length-2] == 'x' && XlIsDigit(typeName[length-3])) {
                return (typeName[length-3] - '0') * (typeName[length-1] - '0');
            }

            return typeName[length-1] - '0';
        }

        return 1;
    }

    void NodeGraph::TrimForPreview(uint32 previewNode)
    {
        Trim(&previewNode, &previewNode+1);

               
            //
            //      Build a new output connection. But we the node itself can't tell us the output type.
            //      So we have to parse the source shader file again to get the result type
            //

        auto i = std::find_if(_nodes.begin(), _nodes.end(), 
            [=](const Node& node) { return node.NodeId() == previewNode; });
        if (i!=_nodes.end()) {
            AddDefaultOutputs(*i);
        }
    }

    static bool HasConnection(const std::vector<NodeConnection>& connections, uint32 destinationId, const std::string& destinationName)
    {
        return
            std::find_if(
                connections.cbegin(), connections.cend(), 
                [=](const NodeConnection& connection) 
                    { return connection.InputNodeId() == destinationId && connection.InputParameterName() == destinationName; }
            ) != connections.end();
    }

    void NodeGraph::AddDefaultOutputs()
    {
            // annoying redirection (because we're modifying the node array)
        std::vector<uint32> starterNodes;
        for (auto i=_nodes.begin(); i!=_nodes.end(); ++i)
            starterNodes.push_back(i->NodeId());

        for (auto i=starterNodes.begin(); i!=starterNodes.end(); ++i)
            AddDefaultOutputs(*GetNode(*i));
    }

    void NodeGraph::AddDefaultOutputs(const Node& node)
    {
        if (node.ArchiveName().empty()) return;

        const auto& sig = LoadFunctionSignature(SplitArchiveName(node.ArchiveName()));

            //  a function can actually output many values. Each output needs it's own default
            //  output node attached. First, look for a "return" value. Then search through
            //  for parameters with "out" set
        if (HasResultValue(sig) && !HasConnection(_nodeConnections, node.NodeId(), s_resultName))
            _nodeConnections.emplace_back(NodeConnection(s_nodeId_Invalid, node.NodeId(), "result", s_resultName, sig._returnType));

        for (const auto& i:sig._parameters) {
            if (i._direction & ShaderSourceParser::FunctionSignature::Parameter::Out) {
                if (!HasConnection(_nodeConnections, node.NodeId(), i._name))
                    _nodeConnections.emplace_back(NodeConnection(s_nodeId_Invalid, node.NodeId(), "result", i._name, i._type));
            }
        }
    }

    void NodeGraph::Trim(const uint32* trimNodesBegin, const uint32* trimNodesEnd)
    {
            //
            //      Trim out all of the nodes that are upstream of
            //      'previewNode' (except for output nodes that are
            //      directly written by one of the trim nodes)
            //
            //      Simply
            //          1.  remove all nodes, unless they are downstream
            //              of 'previewNode'
            //          2.  remove all connections that refer to nodes
            //              that no longer exist
            //
            //      Generally, there won't be an output connection attached
            //      to the previewNode at the end of the process. So, we
            //      may need to create one.
            //

        _nodes.erase(
            std::remove_if(
                _nodes.begin(), _nodes.end(),
                [=](const Node& node) -> bool
                    { 
                         if (IsDownstream(node.NodeId(), trimNodesBegin, trimNodesEnd)) {
                             return false;
                         }
                         if (node.GetType() == Node::Type::Output) {
                             bool directOutput = false;
                                //  search through to see if this node is a direct output
                                //  node of one of the trim nodes
                             for (auto n=trimNodesBegin; n!=trimNodesEnd && !directOutput; ++n) {
                                 for (auto c=_nodeConnections.cbegin(); c!=_nodeConnections.cend(); ++c) {
                                     if (c->InputNodeId() == *n && c->OutputNodeId() == node.NodeId()) {
                                         directOutput = true;
                                         break;
                                     }
                                 }
                             }
                             return !directOutput;
                         }
                         return true;
                    }), 
            _nodes.end());

        _nodeConnections.erase(
            std::remove_if(
                _nodeConnections.begin(), _nodeConnections.end(),
                [=](const NodeConnection& connection) 
                    { return !HasNode(connection.InputNodeId()) || !HasNode(connection.OutputNodeId()); }),
            _nodeConnections.end());

        _constantConnections.erase(
            std::remove_if(
                _constantConnections.begin(), _constantConnections.end(),
                [=](const ConstantConnection& connection) 
                    { return !HasNode(connection.OutputNodeId()); }),
            _constantConnections.end());

        _inputParameterConnections.erase(
            std::remove_if(
                _inputParameterConnections.begin(), _inputParameterConnections.end(),
                [=](const InputParameterConnection& connection) 
                    { return !HasNode(connection.OutputNodeId()); }),
            _inputParameterConnections.end());
    }

        ///////////////////////////////////////////////////////////////

    static bool SortNodesFunction(  
        uint32                  node,
        std::vector<uint32>&    presorted, 
        std::vector<uint32>&    sorted, 
        std::vector<uint32>&    marks,
        const NodeGraph&        graph)
    {
        if (std::find(presorted.begin(), presorted.end(), node) == presorted.end()) {
            return false;   // hit a cycle
        }
        if (std::find(marks.begin(), marks.end(), node) != marks.end()) {
            return false;   // hit a cycle
        }

        marks.push_back(node);
        for (auto i=graph.GetNodeConnections().begin(); i!=graph.GetNodeConnections().end(); ++i) {
            if (i->OutputNodeId() == node) {
                SortNodesFunction(i->InputNodeId(), presorted, sorted, marks, graph);
            }
        }

        sorted.push_back(node);
        presorted.erase(std::find(presorted.begin(), presorted.end(), node));
        return true;
    }

    static std::string AsString(uint32 i)
    {
        char buffer[128];
        XlI64toA(i, buffer, dimof(buffer), 10);
        return buffer;
    }

    static std::string OutputTemporaryForNode(uint32 nodeId, const std::string& outputName)        
    { 
        return std::string("Output_") + AsString(nodeId) + "_" + outputName; 
    }
    static std::string InterpolatorParameterName(uint32 nodeId)     { return std::string("interp_") + AsString(nodeId); }

    template<typename Connection>
        const Connection* FindConnection(
            IteratorRange<const Connection*> connections,
            uint32 nodeId, const std::string& parameterName)
    {
        return std::find_if(
            connections.cbegin(), connections.cend(), 
            [nodeId, &parameterName](const NodeBaseConnection& connection)
            { return    connection.OutputNodeId() == nodeId && 
                        connection.OutputParameterName() == parameterName; } );
    }

    struct ExpressionString { std::string _expression, _type; };

    static void WriteCastExpression(std::stringstream& result, const ExpressionString& expression, const std::string& dstType)
    {
        if (!expression._type.empty() && !dstType.empty() && expression._type != dstType && !XlEqStringI(dstType, "auto") && !XlEqStringI(expression._type, "auto")) {
            result << "Cast_" << expression._type << "_to_" << dstType << "(" << expression._expression << ")";
        } else 
            result << expression._expression;
    }

    static std::string TypeFromShaderFragment(const std::string& archiveName, const std::string& paramName)
    {
            // Go back to the shader fragments to find the current type for the given parameter
        const auto& sig = LoadFunctionSignature(SplitArchiveName(archiveName));
        if (paramName == s_resultName && HasResultValue(sig))
            return sig._returnType;

            // find an "out" parameter with the right name
        for (const auto& p:sig._parameters)
            if (    (p._direction & ShaderSourceParser::FunctionSignature::Parameter::Out) != 0
                &&   p._name == paramName)
                return p._type;

        return std::string();
    }

    static std::string TypeFromParameterStructFragment(const std::string& archiveName, const std::string& paramName)
    {
            // Go back to the shader fragments to find the current type for the given parameter
        const auto& sig = LoadParameterStructSignature(SplitArchiveName(archiveName));
        for (const auto& p:sig._parameters)
            if (p._name == paramName)
                return p._type;
        return std::string();
    }

    static ExpressionString QueryExpression(const NodeGraph& nodeGraph, const NodeConnection& connection)
    {
            //      Check to see what kind of connection it is
            //      By default, let's just assume it's a procedure.
        auto* inputNode = nodeGraph.GetNode(connection.InputNodeId());
        auto inputNodeType = inputNode ? inputNode->GetType() : Node::Type::Procedure;

        if (inputNodeType == Node::Type::Procedure) {

                // We will load the current type from the shader fragments, overriding what is in the
                // the connection. The two might disagree if the shader fragment has changed since the
                // graph was created.
            std::string type;
            if (inputNode) type = TypeFromShaderFragment(inputNode->ArchiveName(), connection.InputParameterName());
            if (type.empty()) type = connection.InputType()._name;
            return ExpressionString{OutputTemporaryForNode(connection.InputNodeId(), connection.InputParameterName()), type}; 

        } else if (inputNodeType == Node::Type::MaterialCBuffer) {

            return ExpressionString{connection.InputParameterName(), connection.InputType()._name};

        } else if (inputNodeType == Node::Type::Constants) {

            std::stringstream result;
            result << "ConstantValue_" << connection.InputNodeId() << "_" << connection.InputParameterName();
            return ExpressionString{result.str(), connection.InputType()._name};

        } else if (inputNodeType == Node::Type::InterpolatorIntoPixel || inputNodeType == Node::Type::InterpolatorIntoVertex || inputNodeType == Node::Type::SystemParameters) {

            std::stringstream result;
            result << InterpolatorParameterName(connection.InputNodeId()) << "." << connection.InputParameterName();
            return ExpressionString{result.str(), connection.InputType()._name};

        } else
            return ExpressionString{std::string(), std::string()};
    }

    static ExpressionString QueryExpression(const NodeGraph& nodeGraph, const ConstantConnection& connection)
    {
            //  we have a "constant connection" value here. We either extract the name of 
            //  the varying parameter, or we interpret this as pure text...
        std::regex filter("<(.*)>");
        std::smatch matchResult;
        if (std::regex_match(connection.Value(), matchResult, filter) && matchResult.size() > 1) {
            return ExpressionString{matchResult[1].str(), std::string()};
        } else {
            return ExpressionString{connection.Value(), std::string()};
        }
    }

    static ExpressionString QueryExpression(const NodeGraph& nodeGraph, const InputParameterConnection& connection)
    {
        std::regex filter("<(.*)>");
        std::smatch matchResult;
        if (std::regex_match(connection.InputName(), matchResult, filter) && matchResult.size() > 1) {
            return ExpressionString{matchResult[1].str(), std::string()};
        } else {
            return ExpressionString{connection.InputName(), connection.InputType()._name};
        }
    }

    static ExpressionString ParameterExpression(const NodeGraph& nodeGraph, uint32 nodeId, const std::string& parameterName)
    {
        auto i = FindConnection(nodeGraph.GetNodeConnections(), nodeId, parameterName);
        if (i!=nodeGraph.GetNodeConnections().cend())
            return QueryExpression(nodeGraph, *i);

        auto ci = FindConnection(nodeGraph.GetConstantConnections(), nodeId, parameterName);
        if (ci!=nodeGraph.GetConstantConnections().cend())
            return QueryExpression(nodeGraph, *ci);

        auto ti = FindConnection(nodeGraph.GetInputParameterConnections(), nodeId, parameterName);
        if (ti!=nodeGraph.GetInputParameterConnections().cend())
            return QueryExpression(nodeGraph, *ti);

        return ExpressionString{std::string(), std::string()};
    }

    static std::stringstream GenerateFunctionCall(const Node& node, const NodeGraph& nodeGraph, const NodeGraph& graphOfTemporaries)
    {
        auto splitName = SplitArchiveName(node.ArchiveName());

            //
            //      Parse the fragment again, to get the correct function
            //      signature at this time.
            //
            //      It's possible that the function signature has changed 
            //      since the node graph was generated. In this case, we
            //      have to try to adapt to match the node graph as closely
            //      as possible. This might require:
            //          * ignoring some connections
            //          * adding casting operations
            //          * adding some default values
            //
            //      \todo --    avoid parsing the same shader file twice (parse
            //                  when writing the #include statements to the file)
            //
        

        auto functionName = std::get<1>(splitName);
        const auto& sig = LoadFunctionSignature(splitName);

        std::stringstream result, warnings;

            //      1.  Declare output variable (made unique by node id)
            //      2.  Call the function, assigning the output variable as appropriate
            //          and passing in the parameters (as required)
        for (const auto& i:sig._parameters)
            if (i._direction & ShaderSourceParser::FunctionSignature::Parameter::Out)
                result << "\t" << i._type << " " << OutputTemporaryForNode(node.NodeId(), i._name) << ";" << std::endl;

        if (HasResultValue(sig)) {
            auto outputName = OutputTemporaryForNode(node.NodeId(), s_resultName);
            result << "\t" << sig._returnType << " " << outputName << ";" << std::endl;
            result << "\t" << outputName << " = " << functionName << "( ";
        } else {
            result << "\t" << functionName << "( ";
        }

        for (auto p=sig._parameters.cbegin(); p!=sig._parameters.cend(); ++p) {
            if (p != sig._parameters.cbegin())
                result << ", ";

                // note -- problem here for in/out parameters
            if (p->_direction == ShaderSourceParser::FunctionSignature::Parameter::Out) {
                result << OutputTemporaryForNode(node.NodeId(), p->_name);
                continue;
            }

            auto expr = ParameterExpression(nodeGraph, node.NodeId(), p->_name);
            if (expr._expression.empty())
                expr = ParameterExpression(graphOfTemporaries, node.NodeId(), p->_name);

            if (!expr._expression.empty()) {
                WriteCastExpression(result, expr, p->_type);
            } else {
                result << "DefaultValue_" << p->_type << "()";
                warnings << "// could not generate parameter pass expression for parameter " << p->_name << std::endl;
            }
        }

        result << " );" << std::endl;
        if (warnings.tellp()) {
            result << "\t// Warnings in function call: " << std::endl;
            result << warnings.str();
        }
        result << std::endl;

        return std::move(result);
    }

    static std::string GenerateMainFunctionBody(const NodeGraph& graph, const NodeGraph& graphOfTemporaries)
    {
        std::stringstream result;

            /*

                We need to create a directed acyclic graph from the nodes in 'graph' 
                    -- and then we need to do a topological sort.

                This will tell us the order in which to call each function

                Basic algorithms:

                    L <- Empty list that will contain the sorted elements
                    S <- Set of all nodes with no incoming edges
                    while S is non-empty do
                        remove a node n from S
                        add n to tail of L
                        for each node m with an edge e from n to m do
                            remove edge e from the graph
                            if m has no other incoming edges then
                                insert m into S
                    if graph has edges then
                        return error (graph has at least one cycle)
                    else 
                        return L (a topologically sorted order)

                Depth first sort:

                    L <- Empty list that will contain the sorted nodes
                    while there are unmarked nodes do
                        select an unmarked node n
                        visit(n) 
                    function visit(node n)
                        if n has a temporary mark then stop (not a DAG)
                        if n is not marked (i.e. has not been visited yet) then
                            mark n temporarily
                            for each node m with an edge from n to m do
                                visit(m)
                            mark n permanently
                            add n to head of L

            */

        std::vector<uint32> presortedNodes, sortedNodes;
        sortedNodes.reserve(graph.GetNodes().size());

        for (auto i=graph.GetNodes().cbegin(); i!=graph.GetNodes().cend(); ++i) {
            presortedNodes.push_back(i->NodeId());
        }

        bool acyclic = true;
        while (!presortedNodes.empty()) {
            std::vector<uint32> temporaryMarks;
            bool sortReturn = SortNodesFunction(
                presortedNodes[0],
                presortedNodes, sortedNodes,
                temporaryMarks, graph);

            if (!sortReturn) {
                acyclic = false;
                break;
            }
        }

            //
            //      Now the function calls can be ordered by walking through the 
            //      directed graph.
            //

        if (!acyclic) {
            result << "// Warning! found a cycle in the graph of nodes. Result will be incomplete!" << std::endl;
        }

        for (auto i=sortedNodes.cbegin(); i!=sortedNodes.cend(); ++i) {
            auto i2 = std::find_if( graph.GetNodes().cbegin(), 
                                    graph.GetNodes().cend(), [i](const Node& n) { return n.NodeId() == *i; } );
            if (i2 != graph.GetNodes().cend()) {
                if (i2->GetType() == Node::Type::Procedure) {
                    result << GenerateFunctionCall(*i2, graph, graphOfTemporaries).str();
                }
            }
        }

        return result.str();
    }

    std::string GenerateShaderHeader(const NodeGraph& graph)
    {
            //  
            //      Generate shader source code for the given input
            //      node graph.
            //
            //          Material parameters need to be sorted into
            //          constant buffers, and then declared at the top of
            //          the source file.
            //
            //          System parameters will become input parameters to
            //          the main function call.
            //
            //          We need to declare some sort of output structure
            //          to contain all of the output variables.
            //
            //          There's some main function call that should bounce
            //          off and call the fragments from the graph.
            //

        std::stringstream result;
        result << "#include \"game/xleres/System/Prefix.h\"" << std::endl;
        result << std::endl;

            //
            //      Dump the text for all of the nodes
            //

        std::vector<std::string> archivesAlreadyLoaded;
        for (auto i=graph.GetNodes().cbegin(); i!=graph.GetNodes().cend(); ++i) {

                //
                //      We have two options -- copy the contents of the fragment shader
                //      into the new shader... 
                // 
                //      Or just add a reference with #include "".
                //      Using an include makes it easier to adapt when the fragment 
                //      changes. But it extra dependencies for the output (and the potential
                //      for missing includes)
                //

            auto splitName = SplitArchiveName(i->ArchiveName());
            if (std::get<0>(splitName).empty())
                continue;

                //      We have to check for duplicates (because nodes are frequently used twice)
            if (std::find(archivesAlreadyLoaded.cbegin(), archivesAlreadyLoaded.cend(), std::get<0>(splitName)) == archivesAlreadyLoaded.cend()) {
                archivesAlreadyLoaded.push_back(std::get<0>(splitName));

                std::string filename = std::get<0>(splitName);
                std::replace(filename.begin(), filename.end(), '\\', '/');
                result << "#include \"" << filename << "\"" << std::endl;
            }

        }

        result << std::endl << std::endl;
        return result.str();
    }

    static bool IsStructType(StringSection<char> typeName)
    {
        // If it's not recognized as a built-in shader language type, then we
        // need to assume this is a struct type. There is no typedef in HLSL, but
        // it could be a #define -- but let's assume it isn't.
        return RenderCore::ShaderLangTypeNameAsTypeDesc(typeName)._type == ImpliedTyping::TypeCat::Void;
    }

    static std::string UniquifyName(const std::string& name, std::map<std::string, unsigned>& workingMap)
    {
        auto i = workingMap.find(name);
        if (i == workingMap.cend()) {
            workingMap.insert(i, std::make_pair(name, 1));
            return name;
        } else {
            std::stringstream str;
            str << name << ++i->second;
            return str.str();
        }
    }

    void MainFunctionInterface::BuildMainFunctionOutputParameters(const NodeGraph& graph)
    {
        std::set<const Node*> outputStructNodes;
        std::map<std::string, unsigned> nameMap;

        unsigned svTargetIndex = 0;
        for (const auto& c : graph.GetNodeConnections()) {
            auto* destinationNode = graph.GetNode(c.OutputNodeId());
            if (destinationNode && destinationNode->GetType() == Node::Type::Output)
                outputStructNodes.insert(destinationNode);
        }

        for (const auto& i:outputStructNodes) {
            if (i->GetType() == Node::Type::Output) {
                StringMeld<64> buffer;
                buffer << "OUT_" << i->NodeId();

				const auto& signature = LoadParameterStructSignature(SplitArchiveName(i->ArchiveName()));
				std::string type = (!signature._name.empty()) ? signature._name : i->ArchiveName();

				StringMeld<64> semantic;
				if (!IsStructType(MakeStringSection(type)))
					semantic << "SV_Target" << svTargetIndex++;

                _outputParameters.emplace_back(MainFunctionParameter(type, UniquifyName(std::string(buffer), nameMap), i->ArchiveName(), std::string(semantic)));
            }
        }

        for (const auto& c : graph.GetNodeConnections()) {
            auto* destinationNode = graph.GetNode(c.OutputNodeId());
            if (!destinationNode) {
                auto type = c.InputType()._name;

                if (type.empty() || XlEqStringI(MakeStringSection(type), "auto")) {
                        // auto types must match whatever is on the other end.
                    auto* srcNode = graph.GetNode(c.InputNodeId());
                    if (srcNode) {
                        const auto& sig = LoadFunctionSignature(SplitArchiveName(srcNode->ArchiveName()));
                        if (c.InputParameterName() == s_resultName && HasResultValue(sig))
                            type = sig._returnType;

                        auto p = std::find_if(sig._parameters.cbegin(), sig._parameters.cend(), 
                            [&c](const ShaderSourceParser::FunctionSignature::Parameter&p)
                                { return p._name == c.InputParameterName(); });
                        if (p!=sig._parameters.end()) {
                            assert(p->_direction & ShaderSourceParser::FunctionSignature::Parameter::Out);
                            type = p->_type;
                        }
                    }
                }

                StringMeld<64> semantic;
				if (!IsStructType(MakeStringSection(type)))
					semantic << "SV_Target" << svTargetIndex++;

                auto finalName = UniquifyName(c.OutputParameterName(), nameMap);
                _outputParameters.emplace_back(MainFunctionParameter(type, finalName, "", std::string(semantic)));
                _outputParameterNames.insert(LowerBound(_outputParameterNames, (const NodeBaseConnection*)&c), std::make_pair(&c, finalName));
            }
        }
    }

    std::string MainFunctionInterface::GetOutputParameterName(const NodeBaseConnection& c) const
    {
        auto i = LowerBound(_outputParameterNames, (const NodeBaseConnection*)&c);
        if (i == _outputParameterNames.end() || i->first != &c)
            return c.OutputParameterName();
        return i->second;
    }

    static bool CanBeStoredInCBuffer(const StringSection<char> type)
    {
        // HLSL keywords are not case sensitive. We could assume that
        // a type name that does not begin with one of the scalar type
        // prefixes is a global resource (like a texture, etc). This
        // should provide some flexibility with new DX12 types, and perhaps
        // allow for manipulating custom "interface" types...?
        // 
        // However, that isn't going to work well with struct types (which
        // can be contained with cbuffers and be passed to functions like
        // scalars)
        //
        // const char* scalarTypePrefixes[] = 
        // {
        //     "bool", "int", "uint", "half", "float", "double"
        // };
        //
        // Note that we only check the prefix -- to catch variations on
        // texture and RWTexture (and <> arguments like Texture2D<float4>)

        const char* resourceTypePrefixes[] = 
        {
            "cbuffer", "tbuffer", 
            "StructuredBuffer", "Buffer", "ByteAddressBuffer", "AppendStructuredBuffer", 
            "RWBuffer", "RWByteAddressBuffer", "RWStructuredBuffer", 
                        
            "sampler", // SamplerState, SamplerComparisonState
            "RWTexture", // RWTexture1D, RWTexture1DArray, RWTexture2D, RWTexture2DArray, RWTexture3D
            "texture", // Texture1D, Texture1DArray, Texture2D, Texture2DArray, Texture2DMS, Texture2DMSArray, Texture3D, TextureCube, TextureCubeArray

            // special .fx file types:
            // "BlendState", "DepthStencilState", "DepthStencilView", "RasterizerState", "RenderTargetView", 
        };
        // note -- some really special function signature-only: InputPatch, OutputPatch, LineStream, TriangleStream, PointStream

        for (unsigned c=0; c<dimof(resourceTypePrefixes); ++c)
            if (XlBeginsWithI(type, MakeStringSection(resourceTypePrefixes[c])))
                return false;
        return true;
    }

    bool MainFunctionInterface::IsCBufferGlobal(unsigned c) const
    {
        return CanBeStoredInCBuffer(MakeStringSection(_globalParameters[c]._type));
    }

    static void AddWithExistingCheck(
        std::vector<MainFunctionParameter>& dst,
        MainFunctionParameter&& param)
    {
	    // Look for another parameter with the same name...
	    auto existing = std::find_if(dst.begin(), dst.end(), 
		    [&param](const MainFunctionParameter& p) { return p._name == param._name; });
	    if (existing != dst.end()) {
		    // If we have 2 parameters with the same name, we're going to expect they
		    // also have the same type and semantic (otherwise we would need to adjust
		    // the name to avoid conflicts).
		    if (existing->_type != param._type || existing->_semantic != param._semantic)
			    Throw(::Exceptions::BasicLabel("Main function parameters with the same name, but different types/semantics (%s)", param._name.c_str()));
	    } else {
		    dst.emplace_back(std::move(param));
	    }
    }

    MainFunctionInterface::MainFunctionInterface(const NodeGraph& graph)
    {
            //
            //      Look for inputs to the graph that aren't
            //      attached to anything.
            //      These should either take some default input, or become 
            //          our "varying parameters".
            //
        for (const auto& i:graph.GetNodes()) {
            if (i.GetType() == Node::Type::Procedure && !i.ArchiveName().empty()) {
                const auto& signature = LoadFunctionSignature(SplitArchiveName(i.ArchiveName()));
                for (auto p=signature._parameters.cbegin(); p!=signature._parameters.cend(); ++p) {

                    if (!(p->_direction & ShaderSourceParser::FunctionSignature::Parameter::In))
                        continue;

                    bool found = 
                            (FindConnection(graph.GetNodeConnections(), i.NodeId(), p->_name) != graph.GetNodeConnections().cend())
                        ||  (FindConnection(graph.GetConstantConnections(), i.NodeId(), p->_name) != graph.GetConstantConnections().cend())
                        ||  (FindConnection(graph.GetInputParameterConnections(), i.NodeId(), p->_name) != graph.GetInputParameterConnections().cend())
                        ;

                        //
                        //      If we didn't find any connection, then it's an
                        //      unattached parameter. Let's consider it a varying parameter
                        //      We can use a "ConstantConnection" to reference the parameter name
                        //
                    if (!found) {
                        auto mainFunctionParamName = p->_name;
                        _graphOfTemporaries.Add(ConstantConnection(i.NodeId(), p->_name, mainFunctionParamName));

                        AddWithExistingCheck(
                            _inputParameters,
                            MainFunctionParameter(p->_type, p->_name, i.ArchiveName(), p->_semantic));
                    }
                }
            }
        }

            //
            //      Any "interpolator" nodes must have values passed into
            //      the main shader body from the framework code. Let's assume
            //      that if the node exists in the graph, then it's going to be
            //      required in the shader.
            //
        for (const auto& i:graph.GetNodes()) {
            if (    i.GetType() == Node::Type::InterpolatorIntoPixel 
                ||  i.GetType() == Node::Type::InterpolatorIntoVertex
                ||  i.GetType() == Node::Type::SystemParameters) {
                
                const auto& signature = LoadParameterStructSignature(SplitArchiveName(i.ArchiveName()));
                std::string type = (!signature._name.empty()) ? signature._name : i.ArchiveName();

                auto paramName = InterpolatorParameterName(i.NodeId());
                AddWithExistingCheck(_inputParameters, MainFunctionParameter(type, paramName, i.ArchiveName()));
            }
        }

            //
            //      Also find "constant" connections on the main graph that are 
            //      marked as input parameters
            //
        std::regex inputParamFilter("<(.*)>");
        for (const auto& i:graph.GetConstantConnections()) {
            std::smatch matchResult;
            if (std::regex_match(i.Value(), matchResult, inputParamFilter) && matchResult.size() > 1) {
                    // We need to find the type by looking out the output node
                auto* destinationNode = graph.GetNode(i.OutputNodeId());
                if (!destinationNode) continue;

                    // We can make this an input parameter, or a global/cbuffer parameter
                    // at the moment, it will always be an input parameter
                const auto& sig = LoadFunctionSignature(SplitArchiveName(destinationNode->ArchiveName()));
                auto p = std::find_if(sig._parameters.cbegin(), sig._parameters.cend(), 
                    [&i](const ShaderSourceParser::FunctionSignature::Parameter&p)
                        { return p._name == i.OutputParameterName(); });

                if (p!=sig._parameters.end()) {
                    assert(p->_direction & ShaderSourceParser::FunctionSignature::Parameter::In);
                    AddWithExistingCheck(_inputParameters, MainFunctionParameter(p->_type, matchResult[1], p->_type, p->_semantic));
                }
            }
        }

        for (const auto& i:graph.GetInputParameterConnections()) {
                // We can choose to make this either a global parameter or a function input parameter
                // 1) If there is a semantic, or the name is in angle brackets, it will be an input parameter. 
                // 2) Otherwise, it's a global parameter.
            bool isInputParam = !i.InputSemantic().empty();
            std::string name = i.InputName();
            if (!isInputParam) {
                std::smatch matchResult;
                if (std::regex_match(name, matchResult, inputParamFilter) && matchResult.size() > 1) {
                    isInputParam = true;
                    name = matchResult[1].str();
                }
            }

            auto type = i.InputType()._name;
            if (type.empty() || XlEqStringI(MakeStringSection(type), "auto")) {
                // auto types must match whatever is on the other end.
                auto* destinationNode = graph.GetNode(i.OutputNodeId());
                if (destinationNode) {
                    const auto& sig = LoadFunctionSignature(SplitArchiveName(destinationNode->ArchiveName()));
                    auto p = std::find_if(sig._parameters.cbegin(), sig._parameters.cend(), 
                        [=](const ShaderSourceParser::FunctionSignature::Parameter&p)
                            { return p._name == i.OutputParameterName(); });
                    if (p!=sig._parameters.end()) {
                        assert(p->_direction & ShaderSourceParser::FunctionSignature::Parameter::In);
                        type = p->_type;
                    }
                }
            }

            if (isInputParam) {
                AddWithExistingCheck(_inputParameters, MainFunctionParameter(type, name, std::string(), i.InputSemantic(), i.Default()));
            } else {
                AddWithExistingCheck(_globalParameters, MainFunctionParameter(type, name, std::string(), std::string(), i.Default()));
            }
        }

        BuildMainFunctionOutputParameters(graph);
    }

    MainFunctionInterface::~MainFunctionInterface() {}

    template<typename Connection>
        static void FillDirectOutputParameters(
            std::stringstream& result,
            const NodeGraph& graph,
            IteratorRange<const Connection*> range,
            const MainFunctionInterface& interf)
    {
        for (const auto& i:range) {
            std::string outputType;

            auto* destinationNode = graph.GetNode(i.OutputNodeId());
            if (!destinationNode) {
                result << "\t" << interf.GetOutputParameterName(i) << " = ";
            } else if (destinationNode->GetType() == Node::Type::Output) {
                result << "\t" << "OUT_" << destinationNode->NodeId() << "." << i.OutputParameterName() << " = ";
                outputType = TypeFromParameterStructFragment(destinationNode->ArchiveName(), i.OutputParameterName());
            } else
                continue;

            ExpressionString expression = QueryExpression(graph, i);
            if (outputType.empty()) outputType = expression._type;
            if (!expression._expression.empty()) {
                WriteCastExpression(result, expression, outputType);
            } else {
                    // no output parameters.. call back to default
                result << "DefaultValue_" << outputType << "()";
                result << "// Warning! Could not generate query expression for node connection!" << std::endl;
            }
            result << ";" << std::endl;
        }
    }

    static void MaybeComma(std::stringstream& stream) { if (stream.tellp() != std::stringstream::pos_type(0)) stream << ", "; }

    std::string GenerateShaderBody(const NodeGraph& graph, const MainFunctionInterface& interf)
    {
        std::stringstream mainFunctionDeclParameters;

        for (const auto& i:interf.GetInputParameters()) {
            MaybeComma(mainFunctionDeclParameters);
            mainFunctionDeclParameters << i._type << " " << i._name;
			if (!i._semantic.empty())
				mainFunctionDeclParameters << " : " << i._semantic;
        }

        std::stringstream result;

            //
            //      We need to write the global parameters as part of the shader body.
            //      Global resources (like textures) appear first. But we put all shader
            //      constants together in a single cbuffer called "BasicMaterialConstants"
            //
        bool hasMaterialConstants = false;
        for(auto i:interf.GetGlobalParameters()) {
            if (!CanBeStoredInCBuffer(MakeStringSection(i._type))) {
                result << i._type << " " << i._name << ";" << std::endl;
            } else 
                hasMaterialConstants = true;
        }
        
        if (hasMaterialConstants) {
                // in XLE, the cbuffer name "BasicMaterialConstants" is reserved for this purpose.
                // But it is not assigned to a fixed cbuffer register.
            result << "cbuffer BasicMaterialConstants" << std::endl;
            result << "{" << std::endl;
            for (const auto& i:interf.GetGlobalParameters())
                if (CanBeStoredInCBuffer(MakeStringSection(i._type)))
                    result << "\t" << i._type << " " << i._name << ";" << std::endl;
            result << "}" << std::endl;
        }

            //  
            //      Our graph function is always a "void" function, and all of the output
            //      parameters are just function parameters with the "out" keyword. This is
            //      convenient for writing out generated functions
            //      We don't want to put the "node id" in the name -- because node ids can 
            //      change from time to time, and that would invalidate any other shaders calling
            //      this function. But ideally we need some way to guarantee uniqueness.
            //
        for (const auto& i:interf.GetOutputParameters()) {
            MaybeComma(mainFunctionDeclParameters);
            mainFunctionDeclParameters << "out " << i._type << " " << i._name;
			if (!i._semantic.empty())
				mainFunctionDeclParameters << " : " << i._semantic;
        }

        result << "void " << graph.GetName() << "(" << mainFunctionDeclParameters.str() << ")" << std::endl;
        result << "{" << std::endl;
        result << GenerateMainFunctionBody(graph, interf.GetGraphOfTemporaries());

        FillDirectOutputParameters(result, graph, graph.GetNodeConnections(), interf);
        FillDirectOutputParameters(result, graph, graph.GetConstantConnections(), interf);
        FillDirectOutputParameters(result, graph, graph.GetInputParameterConnections(), interf);

        result << std::endl << "}" << std::endl;

        return result.str();
    }

        ////////////////////////////////////////////////////////////////////////

    struct VaryingParamsFlags 
    {
        enum Enum { WritesVSOutput = 1<<0 };
        using BitField = unsigned;
    };
    
    class ParameterMachine
    {
    public:
        auto GetBuildInterpolator(const MainFunctionParameter& param) const
            -> std::pair<std::string, VaryingParamsFlags::BitField>;

        auto GetBuildSystem(const MainFunctionParameter& param) const -> std::string;

        ParameterMachine();
        ~ParameterMachine();
    private:
        ShaderSourceParser::ShaderFragmentSignature _systemHeader;
    };

    auto ParameterMachine::GetBuildInterpolator(const MainFunctionParameter& param) const
        -> std::pair<std::string, VaryingParamsFlags::BitField>
    {
        std::string searchName = "BuildInterpolator_" + param._semantic;
        auto i = std::find_if(
            _systemHeader._functions.cbegin(), 
            _systemHeader._functions.cend(),
            [searchName](const ShaderSourceParser::FunctionSignature& sig) { return sig._name == searchName; });

        if (i == _systemHeader._functions.cend()) {
            searchName = "BuildInterpolator_" + param._name;
            i = std::find_if(
                _systemHeader._functions.cbegin(), 
                _systemHeader._functions.cend(),
                [searchName](const ShaderSourceParser::FunctionSignature& sig) { return sig._name == searchName; });
        }

        if (i == _systemHeader._functions.cend()) {
            searchName = "BuildInterpolator_" + param._type;
            i = std::find_if(
                _systemHeader._functions.cbegin(), 
                _systemHeader._functions.cend(),
                [searchName](const ShaderSourceParser::FunctionSignature& sig) { return sig._name == searchName; });
        }

        if (i != _systemHeader._functions.cend()) {
            VaryingParamsFlags::BitField flags = 0;
            if (!i->_returnSemantic.empty()) {
                    // using regex, convert the semantic value into a series of flags...
                static std::regex FlagsParse(R"--(NE(?:_([^_]*))*)--");
                std::smatch match;
                if (std::regex_match(i->_returnSemantic.begin(), i->_returnSemantic.end(), match, FlagsParse))
                    for (unsigned c=1; c<match.size(); ++c)
                        if (XlEqString(MakeStringSection<char>(match[c].first, match[c].second), "WritesVSOutput"))
                            flags |= VaryingParamsFlags::WritesVSOutput;
            }

            return std::make_pair(i->_name, flags);
        }

        return std::make_pair(std::string(), 0);
    }

    auto ParameterMachine::GetBuildSystem(const MainFunctionParameter& param) const -> std::string
    {
        std::string searchName = "BuildSystem_" + param._type;
        auto i = std::find_if(
            _systemHeader._functions.cbegin(), _systemHeader._functions.cend(),
            [searchName](const ShaderSourceParser::FunctionSignature& sig) { return sig._name == searchName; });
        if (i != _systemHeader._functions.cend())
            return i->_name;
        return std::string();
    }

    ParameterMachine::ParameterMachine()
    {
        auto buildInterpolatorsSource = LoadSourceFile("game/xleres/System/BuildInterpolators.h");
        _systemHeader = ShaderSourceParser::BuildShaderFragmentSignature(
            AsPointer(buildInterpolatorsSource.begin()), buildInterpolatorsSource.size());
    }

    ParameterMachine::~ParameterMachine() {}

    static std::string ToPlustache(bool value)
    {
        static std::string T = "true", F = "false";
        return value ? T : F;
    }

    class ParameterGenerator
    {
    public:
        unsigned Count() const              { return (unsigned)_parameters.size(); };
        std::string VSOutputMember() const  { return _vsOutputMember; }

        std::string VaryingStructSignature(unsigned index) const;
        std::string VSInitExpression(unsigned index);
        std::string PSExpression(unsigned index, const char vsOutputName[], const char varyingParameterStruct[]) const;
        bool IsGlobalResource(unsigned index) const;
        const MainFunctionParameter& Param(unsigned index) const { return _parameters[index]; }

        bool IsInitializedBySystem(unsigned index) const { return !_buildSystemFunctions[index].empty(); }

        ParameterGenerator(const NodeGraph& graph, const MainFunctionInterface& interf, const PreviewOptions& previewOptions);
        ~ParameterGenerator();
    private:
        std::vector<MainFunctionParameter>  _parameters;
        std::vector<std::string>            _buildSystemFunctions;
        std::string                         _vsOutputMember;
        ParameterMachine                    _paramMachine;

		const PreviewOptions* _previewOptions;
    };

    std::string ParameterGenerator::VaryingStructSignature(unsigned index) const
    {
        if (!_buildSystemFunctions[index].empty()) return std::string();
        if (IsGlobalResource(index)) return std::string();
        const auto& p = _parameters[index];

        std::stringstream result;
        result << "\t" << p._type << " " << p._name;
        /*if (!p._semantic.empty()) {
            result << " : " << p._semantic;
        } else */ {
            char smallBuffer[128];
            if (!IsStructType(MakeStringSection(p._type)))  // (struct types don't get a semantic)
                result << " : " << "VARYING_" << XlI32toA(index, smallBuffer, dimof(smallBuffer), 10);
        }
        return result.str();
    }

    std::string ParameterGenerator::VSInitExpression(unsigned index)
    {
        const auto& p = _parameters[index];
        if (!_buildSystemFunctions[index].empty()) return std::string();

        // Here, we have to sometimes look for parameters that we understand.
        // First, we should look at the semantic attached.
        // We can look for translator functions in the "BuildInterpolators.h" system header
        // If there is a function signature there that can generate the interpolator
        // we're interested in, then we should use that function.
        //
        // If the parameter is actually a structure, we need to look inside of the structure
        // and bind the individual elements. We should do this recursively incase we have
        // structures within structures.
        //
        // Even if we know that the parameter is a structure, it might be hard to find the
        // structure within the shader source code... It would require following through
        // #include statements, etc. That could potentially create some complications here...
        // Maybe we just need a single BuildInterpolator_ that returns a full structure for
        // things like VSOutput...?

        std::string buildInterpolator;
        VaryingParamsFlags::BitField flags;
        std::tie(buildInterpolator, flags) = _paramMachine.GetBuildInterpolator(p);

        if (!buildInterpolator.empty()) {
            if (flags & VaryingParamsFlags::WritesVSOutput)
                _vsOutputMember = p._name;
            return buildInterpolator + "(vsInput)";
        } else {
            if (!IsStructType(MakeStringSection(p._type))) {

					// Look for a "restriction" applied to this variable.
				auto r = std::find_if(
					_previewOptions->_variableRestrictions.cbegin(), _previewOptions->_variableRestrictions.cend(), 
					[&p](const std::pair<std::string, std::string>& v) { return XlEqStringI(v.first, p._name); });
				if (r != _previewOptions->_variableRestrictions.cend()) {
					static std::regex pattern("([^:]*):([^:]*):([^:]*)");
					std::smatch match;
					if (std::regex_match(r->second, match, pattern) && match.size() >= 4) {
						return std::string("InterpolateVariable_") + match[1].str() + "(" + match[2].str() + ", " + match[3].str() + ", localPosition)";
					} else {
						return r->second;	// interpret as a constant
					}
				} else {
					// attempt to set values 
					int dimensionality = GetDimensionality(p._type);
					if (dimensionality == 1) {
						return "localPosition.x * 0.5 + 0.5.x";
					} else if (dimensionality == 2) {
						return "float2(localPosition.x * 0.5 + 0.5, localPosition.y * -0.5 + 0.5)";
					} else if (dimensionality == 3) {
						return "worldPosition.xyz";
					}
				}
            }
        }

        return std::string();
    }

    std::string ParameterGenerator::PSExpression(unsigned index, const char vsOutputName[], const char varyingParameterStruct[]) const
    {
        if (IsGlobalResource(index))
            return _parameters[index]._name;

        auto buildSystemFunction = _buildSystemFunctions[index];
        if (!buildSystemFunction.empty()) {
            if (!_vsOutputMember.empty())
                return buildSystemFunction + "(" + varyingParameterStruct + "." + _vsOutputMember + ", sys)";
            return buildSystemFunction + "(" + vsOutputName + ", sys)";
        } else {
            return std::string(varyingParameterStruct) + "." + _parameters[index]._name;
        }
    }

    bool ParameterGenerator::IsGlobalResource(unsigned index) const
    {
            // Resource types (eg, texture, etc) can't be handled like scalars
            // they must become globals in the shader.
        return !CanBeStoredInCBuffer(MakeStringSection(_parameters[index]._type));
    }

    ParameterGenerator::ParameterGenerator(const NodeGraph& graph, const MainFunctionInterface& interf, const PreviewOptions& previewOptions)
    {
        _parameters = std::vector<MainFunctionParameter>(interf.GetInputParameters().cbegin(), interf.GetInputParameters().cend());
        for (auto i=_parameters.cbegin(); i!=_parameters.cend(); ++i)
            _buildSystemFunctions.push_back(_paramMachine.GetBuildSystem(*i));
		_previewOptions = &previewOptions;
    }

    ParameterGenerator::~ParameterGenerator() {}

    class TemplateItem
    {
    public:
        std::basic_string<char> _item;
        TemplateItem(
            InputStreamFormatter<char>& formatter,
            const ::Assets::DirectorySearchRules&)
        {
            InputStreamFormatter<char>::InteriorSection name, value;
            using Blob = InputStreamFormatter<char>::Blob;
            if (formatter.PeekNext() == Blob::AttributeName && formatter.TryAttribute(name, value)) {
                _item = value.AsString();
            } else
                Throw(Utility::FormatException("Expecting single string attribute", formatter.GetLocation()));
        }
        TemplateItem() {}
    };

    static std::string GetPreviewTemplate(const char templateName[])
    {
        StringMeld<MaxPath, Assets::ResChar> str;
        str << "game/xleres/System/PreviewTemplates.sh:" << templateName;
        return ::Assets::GetAssetDep<::Assets::ConfigFileListContainer<TemplateItem, InputStreamFormatter<char>>>(str.get())._asset._item;
    }

    std::string         GenerateStructureForPreview(
        const NodeGraph& graph, const MainFunctionInterface& interf, 
        const PreviewOptions& previewOptions)
    {
            //
            //      Generate the shader structure that will surround the main
            //      shader generated from "graph"
            //
            //      We have to analyse the inputs and output.
            //
            //      The type of structure should be determined by the dimensionality
            //      of the outputs, and whether the shader takes position inputs.
            //
            //      We must then look at the inputs, and try to determine which
            //      inputs (if any) should vary over the surface of the preview.
            //  
            //      For example, if our preview is a basic 2d or 1d preview, then
            //      the x and y axes will represent some kind of varying parameter.
            //      But for a 3d preview window, there are no varying parameters
            //      (all parameters must be fixed over the surface of the preview
            //      window)
            //

        ParameterGenerator mainParams(graph, interf, previewOptions);

            //
            //      All varying parameters must have semantics
            //      so, look for free TEXCOORD slots, and set all unset semantics
            //      to a default
            //

        std::stringstream result;
        result << std::endl;
        result << "\t//////// Structure for preview ////////" << std::endl;

        const bool renderAsChart = previewOptions._type == PreviewOptions::Type::Chart;
        if (renderAsChart)
            result << "#define SHADER_NODE_EDITOR_CHART 1" << std::endl;
        result << "#include \"game/xleres/System/BuildInterpolators.h\"" << std::endl;

            //  
            //      First write the "varying" parameters
            //      The varying parameters are always written in the vertex shader and
            //      read by the pixel shader. They will "vary" over the geometry that
            //      we're rendering -- hence the name.
            //      We could use a Mustache template for this, if we were using the
            //      more general implementation of Mustache for C++. But unfortunately
            //      there's no practical way with Plustasche.
            //
        result << "struct NE_Varying" << std::endl << "{" << std::endl;
        for (unsigned index=0; index<mainParams.Count(); ++index) {
            auto sig = mainParams.VaryingStructSignature(index);
            if (sig.empty()) continue;
            result << sig << ";" << std::endl;
        }
        result << "};" << std::endl << std::endl;

            //
            //      Write "_Output" structure. This contains all of the values that are output
            //      from the main function
            //
        result << "struct NE_" << graph.GetName() << "_Output" << std::endl << "{" << std::endl;
        unsigned svTargetCounter = 0;
        for (const auto& i:interf.GetOutputParameters())
            result << "\t" << i._type << " " << i._name << ": SV_Target" << (svTargetCounter++) << ";" << std::endl;
        result << "};" << std::endl << std::endl;

            //
            //      Write all of the global resource types
            //
        for (unsigned index=0; index<mainParams.Count(); ++index)
            if (mainParams.IsGlobalResource(index))
                result << mainParams.Param(index)._type << " " << mainParams.Param(index)._name << ";" << std::endl;
        result << std::endl;

            //
            //      Calculate the code that will fill in the varying parameters from the vertex
            //      shader. We need to do this now because it will effect some of the structure
            //      later.
            //
        std::stringstream varyingInitialization;
        
        for (unsigned index=0; index<mainParams.Count(); ++index) {
            auto initString = mainParams.VSInitExpression(index);
            if (initString.empty()) continue;
            varyingInitialization << "\tOUT.varyingParameters." << mainParams.Param(index)._name << " = " << initString << ";" << std::endl;
        }

        result << "struct NE_PSInput" << std::endl << "{" << std::endl;
        if (mainParams.VSOutputMember().empty())
            result << "\tVSOutput geo;" << std::endl;
        result << "\tNE_Varying varyingParameters;" << std::endl;
        result << "};" << std::endl << std::endl;

        std::string parametersToMainFunctionCall;

            //  Pass each member of the "varyingParameters" struct as a separate input to
            //  the main function
        for (unsigned index=0; index<mainParams.Count(); ++index) {
            if (!parametersToMainFunctionCall.empty())
                parametersToMainFunctionCall += ", ";
            parametersToMainFunctionCall += mainParams.PSExpression(index, "input.geo", "input.varyingParameters");
        }
            
            //  Also pass each output as a parameter to the main function
        for (const auto& i:interf.GetOutputParameters()) {
            if (!parametersToMainFunctionCall.empty())
                parametersToMainFunctionCall += ", ";
            parametersToMainFunctionCall += "functionResult." + i._name;
        }

        Plustache::template_t preprocessor;

        // Render the ps_main template
        {
            Plustache::Context context;
            context.add("GraphName", graph.GetName());
            context.add("ParametersToMainFunctionCall", parametersToMainFunctionCall);
            context.add("PreviewOutput", previewOptions._outputToVisualize);

                // Collect all of the output values into a flat array of floats.
                // This is needed for "charts"
            if (renderAsChart) {
                std::vector<PlustacheTypes::ObjectType> chartLines;

                if (!previewOptions._outputToVisualize.empty()) {
                    // When we have an "outputToVisualize" we only show the
                    // chart for that single output.
                    chartLines.push_back(
                        PlustacheTypes::ObjectType { std::make_pair("Item", previewOptions._outputToVisualize) });
                } else {
                    // Find all of the scalar values written out from main function,
                    // including searching through parameter strructures.
                    for (const auto& i:interf.GetOutputParameters()) {
                        const auto& signature = LoadParameterStructSignature(SplitArchiveName(i._archiveName));
                        if (!signature._name.empty()) {
                            for (auto p=signature._parameters.cbegin(); p!=signature._parameters.cend(); ++p) {
                                    // todo -- what if this is also a parameter struct?
                                auto dim = GetDimensionality(p->_type);
                                for (unsigned c=0; c<dim; ++c) {
                                    std::stringstream str;
                                    str << i._name << "." << p->_name;
                                    if (dim != 1) str << "[" << c << "]";
                                    chartLines.push_back(
                                        PlustacheTypes::ObjectType { std::make_pair("Item", str.str()) });
                                }
                            }
                        } else {
                            auto dim = GetDimensionality(i._type);
                            for (unsigned c=0; c<dim; ++c) {
                                std::stringstream str;
                                str << i._name;
                                if (dim != 1) str << "[" << c << "]";
                                chartLines.push_back(
                                    PlustacheTypes::ObjectType { std::make_pair("Item", str.str()) });
                            }
                        }
                    }
                }

                context.add("ChartLines", chartLines);
                context.add("ChartLineCount", (StringMeld<64>() << unsigned(chartLines.size())).get());
            }

            if (renderAsChart) {
                result << preprocessor.render(GetPreviewTemplate("ps_main_chart"), context);
            } else if (!previewOptions._outputToVisualize.empty()) {
                result << preprocessor.render(GetPreviewTemplate("ps_main_explicit"), context);
            } else
                result << preprocessor.render(GetPreviewTemplate("ps_main"), context);
        }

        // Render the vs_main template
        result << preprocessor.render(GetPreviewTemplate("vs_main"), 
            PlustacheTypes::ObjectType
            {
                {"InitGeo", ToPlustache(mainParams.VSOutputMember().empty())},
                {"VaryingInitialization", varyingInitialization.str()}
            });

        return result.str();
    }

}

