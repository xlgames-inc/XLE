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
#include "../Foreign/plustasche/template.hpp"
#include <sstream>
#include <assert.h>
#include <algorithm>
#include <tuple>
#include <regex>

#pragma warning(disable:4127)       // conditional expression is constant

namespace ShaderPatcher 
{

    static const std::string s_resultName = "result";

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
                                    const std::string& outputParameterName, const Type& outputType, 
                                    const std::string& inputParameterName, const Type& inputType)
    :       NodeBaseConnection(outputNodeId, outputParameterName)
    ,       _inputNodeId(inputNodeId)
    ,       _outputType(outputType)
    ,       _inputParameterName(inputParameterName)
    ,       _inputType(inputType)
    {}

    NodeConnection::NodeConnection(NodeConnection&& moveFrom)
    :       NodeBaseConnection(std::move(moveFrom))
    ,       _inputNodeId(moveFrom._inputNodeId)
    ,       _outputType(moveFrom._outputType)
    ,       _inputParameterName(moveFrom._inputParameterName)
    ,       _inputType(moveFrom._inputType)
    {}

    NodeConnection& NodeConnection::operator=(NodeConnection&& moveFrom)
    {
        NodeBaseConnection::operator=(std::move(moveFrom));
        _inputNodeId = moveFrom._inputNodeId;
        _outputType = moveFrom._outputType;
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

    InputParameterConnection::InputParameterConnection(uint32 outputNodeId, const std::string& outputParameterName, const Type& type, const std::string& name, const std::string& semantic)
    :   NodeBaseConnection(outputNodeId, outputParameterName)
    ,   _type(type), _name(name), _semantic(semantic) {}

    InputParameterConnection::InputParameterConnection(InputParameterConnection&& moveFrom)
    :   NodeBaseConnection(std::move(moveFrom))
    ,   _type(std::move(moveFrom._type)), _name(std::move(moveFrom._name)), _semantic(std::move(moveFrom._semantic)) {}

    InputParameterConnection& InputParameterConnection::operator=(InputParameterConnection&& moveFrom)
    {
        NodeBaseConnection::operator=(std::move(moveFrom));
        _type = std::move(moveFrom._type);
        _name = std::move(moveFrom._name);
        _semantic = std::move(moveFrom._semantic);
        return *this;
    }

        ///////////////////////////////////////////////////////////////

    NodeGraph::NodeGraph(const std::string& name) : _name(name) {}

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

    bool            NodeGraph::IsUpstream(uint32 startNode, uint32 searchingForNode)
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

    bool            NodeGraph::IsDownstream(uint32 startNode, 
                                            const uint32* searchingForNodesStart, 
                                            const uint32* searchingForNodesEnd)
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

    static ShaderSourceParser::FunctionSignature LoadFunctionSignature(const std::tuple<std::string, std::string>& splitName)
    {
        using namespace ShaderSourceParser;
        TRY {
            auto shaderFile = LoadSourceFile(std::get<0>(splitName));
            auto parsedFile = BuildShaderFragmentSignature(shaderFile.c_str(), shaderFile.size());

            auto i = std::find_if(parsedFile._functions.cbegin(), parsedFile._functions.cend(), 
                [&](const FunctionSignature& signature) { return signature._name == std::get<1>(splitName); });
            if (i!=parsedFile._functions.cend()) {
                return std::move(const_cast<FunctionSignature&>(*i));
            }
        } CATCH (...) {
        } CATCH_END

        return FunctionSignature();
    }

    static ShaderSourceParser::ParameterStructSignature LoadParameterStructSignature(const std::tuple<std::string, std::string>& splitName)
    {
        if (!std::get<0>(splitName).empty()) {
            using namespace ShaderSourceParser;
            TRY {
                auto shaderFile = LoadSourceFile(std::get<0>(splitName));
                auto parsedFile = BuildShaderFragmentSignature(shaderFile.c_str(), shaderFile.size());

                auto i = std::find_if(parsedFile._parameterStructs.cbegin(), parsedFile._parameterStructs.cend(), 
                    [&](const ParameterStructSignature& signature) { return signature._name == std::get<1>(splitName); });
                if (i!=parsedFile._parameterStructs.cend()) {
                    return std::move(const_cast<ParameterStructSignature&>(*i));
                }
            } CATCH (...) {
            } CATCH_END
        }

        return ShaderSourceParser::ParameterStructSignature();
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

    static unsigned     GetDimensionality(const std::string& typeName)
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

    void            NodeGraph::TrimForPreview(uint32 previewNode)
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

    void        NodeGraph::AddDefaultOutputs()
    {
            // annoying redirection (because we're modifying the node array)
        std::vector<uint32> starterNodes;
        for (auto i=_nodes.begin(); i!=_nodes.end(); ++i)
            starterNodes.push_back(i->NodeId());

        for (auto i=starterNodes.begin(); i!=starterNodes.end(); ++i)
            AddDefaultOutputs(*GetNode(*i));
    }

    void        NodeGraph::AddDefaultOutputs(const Node& node)
    {
        if (node.ArchiveName().empty()) return;

        auto sig = LoadFunctionSignature(SplitArchiveName(node.ArchiveName()));
        auto nodeId = node.NodeId();

            //  a function can actually output many values. Each output needs it's own default
            //  output node attached. First, look for a "return" value. Then search through
            //  for parameters with "out" set
        if (HasResultValue(sig)) {
            if (!HasConnection(_nodeConnections, node.NodeId(), s_resultName)) {
                auto newNodeId = GetUniqueNodeId();
                _nodes.emplace_back(Node(sig._returnType, newNodeId, Node::Type::Output)); // (note -- this invalidates "node"!)
                _nodeConnections.emplace_back(NodeConnection(newNodeId, nodeId, "value", sig._returnType, s_resultName, sig._returnType));
            }
        }

        for (auto i=sig._parameters.cbegin(); i!=sig._parameters.cend(); ++i) {
            if (i->_direction & ShaderSourceParser::FunctionSignature::Parameter::Out) {
                if (!HasConnection(_nodeConnections, node.NodeId(), i->_name)) {
                    auto newNodeId = GetUniqueNodeId();
                    _nodes.emplace_back(Node(i->_type, newNodeId, Node::Type::Output));    // (note -- this invalidates "node"!)
                    _nodeConnections.emplace_back(NodeConnection(newNodeId, nodeId, "value", i->_type, i->_name, i->_type));
                }
            }
        }
    }

    void            NodeGraph::Trim(const uint32* trimNodesBegin, const uint32* trimNodesEnd)
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

    static bool SortNodesFunction(  uint32                  node,
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
        if (!expression._type.empty() && !dstType.empty() && expression._type != dstType) {
            result << "Cast_" << expression._type << "_to_" << dstType << "(" << expression._expression << ")";
        } else 
            result << expression._expression;
    }

    static std::string TypeFromShaderFragment(const std::string& archiveName, const std::string& paramName)
    {
            // Go back to the shader fragments to find the current type for the given parameter
        auto sig = LoadFunctionSignature(SplitArchiveName(archiveName));
        if (paramName == s_resultName && HasResultValue(sig))
            return sig._returnType;

            // find an "out" parameter with the right name
        for each (auto p in sig._parameters)
            if (    (p._direction & ShaderSourceParser::FunctionSignature::Parameter::Out) != 0
                &&   p._name == paramName)
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
            return ExpressionString{matchResult[1], std::string()};
        } else {
            return ExpressionString{connection.Value(), std::string()};
        }
    }

    static ExpressionString QueryExpression(const NodeGraph& nodeGraph, const InputParameterConnection& connection)
    {
        return ExpressionString{connection.InputName(), connection.InputType()._name};
    }

    static ExpressionString ParameterExpression(
        const NodeGraph& nodeGraph,
        uint32 nodeId, const std::string& parameterName,
        const std::string& expectedType, std::stringstream& warnings)
    {
        auto i = FindConnection(nodeGraph.GetNodeConnections(), nodeId, parameterName);
        if (i!=nodeGraph.GetNodeConnections().cend()) {
                //      Found a connection... write the parameter name.
                //      We can also check for casting and any type errors
            const Type& connectionOutputType    = i->OutputType();
            if (expectedType != connectionOutputType._name)
                warnings
                    << "\t// Type for function parameter name " << parameterName 
                    << " seems to have changed in the shader fragment. Check for valid connections." << std::endl;
            
            return QueryExpression(nodeGraph, *i);
        }

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
        auto sig = LoadFunctionSignature(splitName);

        std::stringstream result, warnings;

            //      1.  Declare output variable (made unique by node id)
            //      2.  Call the function, assigning the output variable as appropriate
            //          and passing in the parameters (as required)
        for each (auto i in sig._parameters)
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

            auto expr = ParameterExpression(nodeGraph, node.NodeId(), p->_name, p->_type, warnings);
            if (expr._expression.empty())
                expr = ParameterExpression(graphOfTemporaries, node.NodeId(), p->_name, p->_type, warnings);

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

    std::string GenerateShaderHeader(   const NodeGraph& graph, 
                                        MaterialConstantsStyle::Enum materialConstantsStyle, 
                                        bool copyFragmentContents)
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
        if (copyFragmentContents) {
            result << LoadSourceFile("game/xleres/System/Prefix.h") << std::endl;
            result << LoadSourceFile("game/xleres/System/BuildInterpolators.h") << std::endl;
        } else {
            result << "#include \"game/xleres/System/Prefix.h\"" << std::endl;
            result << "#include \"game/xleres/System/BuildInterpolators.h\"" << std::endl;
        }
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

                if (copyFragmentContents) {
                    result << std::endl << LoadSourceFile(std::get<0>(splitName)) << std::endl;
                } else {
                    std::string filename = std::get<0>(splitName);
                    std::replace(filename.begin(), filename.end(), '\\', '/');
                    result << "#include \"" << filename << "\"" << std::endl;
                }
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

    static std::vector<MainFunctionParameter> GetMainFunctionOutputParameters(const NodeGraph& graph, const NodeGraph& graphOfTemporaries, bool useNodeId = false)
    {
        unsigned outputIndex = 0;
        std::vector<MainFunctionParameter> result;
        for (auto i =graph.GetNodes().cbegin(); i!=graph.GetNodes().cend(); ++i) {
            if (i->GetType() == Node::Type::Output) {
                    // at the moment, each output element is a struct. It might be convenient
                    // if we could use both structs or value types
                StringMeld<64> buffer;
                if (useNodeId)  { buffer << "OUT_" << i->NodeId(); }
                else            { buffer << "OUT" << outputIndex; }

				auto signature = LoadParameterStructSignature(SplitArchiveName(i->ArchiveName()));
				std::string type = (!signature._name.empty()) ? signature._name : i->ArchiveName();

				StringMeld<64> semantic;
				if (!IsStructType(MakeStringSection(type)))
					semantic << "SV_Target" << outputIndex;

                result.emplace_back(MainFunctionParameter(type, std::string(buffer), i->ArchiveName(), std::string(semantic)));
				outputIndex++;
            }
        }
        return result;
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
			    Throw(::Exceptions::BasicLabel("Main function parameters with the same name, but different types/semantics (%s)", param._name));
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
        for each (auto i in graph.GetNodes()) {
            if (i.GetType() == Node::Type::Procedure && !i.ArchiveName().empty()) {
                auto signature = LoadFunctionSignature(SplitArchiveName(i.ArchiveName()));
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
        for each (auto i in graph.GetNodes()) {
            if (    i.GetType() == Node::Type::InterpolatorIntoPixel 
                ||  i.GetType() == Node::Type::InterpolatorIntoVertex
                ||  i.GetType() == Node::Type::SystemParameters) {
                
                auto signature = LoadParameterStructSignature(SplitArchiveName(i.ArchiveName()));
                std::string type = (!signature._name.empty()) ? signature._name : i.ArchiveName();

                auto paramName = InterpolatorParameterName(i.NodeId());
                AddWithExistingCheck(_inputParameters, MainFunctionParameter(type, paramName, i.ArchiveName()));
            }
        }

            // also find "constant" connections on the main graph that are marked as input parameters
        std::regex filter("<(.*)>");
        for each (auto i in graph.GetConstantConnections()) {
            std::smatch matchResult;
            if (std::regex_match(i.Value(), matchResult, filter) && matchResult.size() > 1) {
                    // we need to find the type by looking out the output node
                auto* destinationNode = graph.GetNode(i.OutputNodeId());
                if (!destinationNode) continue;

                auto sig = LoadFunctionSignature(SplitArchiveName(destinationNode->ArchiveName()));
                auto p = std::find_if(sig._parameters.cbegin(), sig._parameters.cend(), 
                    [=](const ShaderSourceParser::FunctionSignature::Parameter&p)
                        { return p._name == i.OutputParameterName(); });

                if (p!=sig._parameters.end()) {
                    _inputParameters.emplace_back(MainFunctionParameter(p->_type, matchResult[1], p->_type, p->_semantic));
                }
            }
        }

        _outputParameters = GetMainFunctionOutputParameters(graph, _graphOfTemporaries, true);
    }

    MainFunctionInterface::~MainFunctionInterface() {}

    static void MaybeComma(std::stringstream& stream) { if (stream.tellp() != std::stringstream::pos_type(0)) stream << ", "; }

    static const bool s_writeOutputsViaStruct = false;

    static void WriteOutputParamName(std::basic_ostream<char>& result, uint32 nodeId, const std::string& paramName)
    {
        if (!s_writeOutputsViaStruct) {
            if (paramName != "value") {
                result << "\tOUT_" << nodeId << "." << paramName;
            } else {
                result << "\tOUT_" << nodeId;
            }
        } else {
            result << "\tOUT." << "Output_" << nodeId << "." << paramName;
        }
    }

    static Type GetOutputType(const NodeConnection& connection) { return connection.OutputType(); }
    static Type GetOutputType(const ConstantConnection& connection) { return Type(); }
    static Type GetOutputType(const InputParameterConnection& connection) { return connection.InputType(); }

    template<typename Connection>
        static void FillDirectOutputParameters(
            std::stringstream& result,
            const NodeGraph& graph,
            IteratorRange<const Connection*> range)
    {
        for each (auto i in range) {
            auto* destinationNode = graph.GetNode(i.OutputNodeId());
            if (destinationNode && destinationNode->GetType() == Node::Type::Output) {
                result << "\t";
                WriteOutputParamName(result, destinationNode->NodeId(), i.OutputParameterName());
                result << " = ";

                    //
                    //      Enforce a cast if we need it...
                    //      This cast is important - but it requires us to re-parse
                    //      the shader fragment. \todo Avoid redundant re-parsing
                    //      \todo -- do we also need to connect varying parameters to outputs?
                    //
                ExpressionString expression = QueryExpression(graph, i);
                if (!expression._expression.empty()) {
                    WriteCastExpression(result, expression, GetOutputType(i)._name);
                } else {
                        // no output parameters.. call back to default
                    result << "DefaultValue_" << GetOutputType(i)._name << "()";
                    result << "// Warning! Could not generate query expression for node connection!" << std::endl;
                }
                result << ";" << std::endl;
            }
        }
    }

    std::string GenerateShaderBody(const NodeGraph& graph, const MainFunctionInterface& interf)
    {
        std::stringstream mainFunctionDeclParameters;

        for each (auto i in interf.GetInputParameters()) {
            MaybeComma(mainFunctionDeclParameters);
            mainFunctionDeclParameters << i._type << " " << i._name;
			if (!i._semantic.empty())
				mainFunctionDeclParameters << " : " << i._semantic;
        }

            //  
            //      Our graph function is always a "void" function, and all of the output
            //      parameters are just function parameters with the "out" keyword. This is
            //      convenient for writing out generated functions
            //      We don't want to put the "node id" in the name -- because node ids can 
            //      change from time to time, and that would invalidate any other shaders calling
            //      this function. But ideally we need some way to guarantee uniqueness.
            //
        std::stringstream result;
        if (!s_writeOutputsViaStruct) {
            for each (auto i in interf.GetOutputParameters()) {
                MaybeComma(mainFunctionDeclParameters);
                mainFunctionDeclParameters << "out " << i._type << " " << i._name;
				if (!i._semantic.empty())
					mainFunctionDeclParameters << " : " << i._semantic;
            }
        } else {
            result << "struct " << graph.GetName() << "_Output" << std::endl << "{" << std::endl;
            for (auto i =graph.GetNodes().cbegin(); i!=graph.GetNodes().cend(); ++i) {
                if (i->GetType() == Node::Type::Output) {
                    auto signature = LoadParameterStructSignature(SplitArchiveName(i->ArchiveName()));
                    result << "\t" << signature._name << " " << "Output_" << i->NodeId();
                    result << ";" << std::endl;
                }
            }
            result << "};" << std::endl << std::endl;
        }

        result << "void " << graph.GetName() << "(" << mainFunctionDeclParameters.str() << ")" << std::endl;
        result << "{" << std::endl;
        result << GenerateMainFunctionBody(graph, interf.GetGraphOfTemporaries());

            //
            //      Fill in the "OUT" structure with all of the output
            //      values.
            //
        if (s_writeOutputsViaStruct)
            result << "\t" << graph.GetName() << "_Output OUT; " << std::endl;

        FillDirectOutputParameters(result, graph, graph.GetNodeConnections());
        FillDirectOutputParameters(result, graph, graph.GetConstantConnections());
        FillDirectOutputParameters(result, graph, graph.GetInputParameterConnections());

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
            _systemHeader._functions.cbegin(), 
            _systemHeader._functions.cend(),
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
        unsigned Count() const                      { return (unsigned)_parameters.size(); };
        std::string VSOutputMember() const          { return _vsOutputMember; }

        std::string VaryingStructSignature(unsigned index) const;
        std::string VSInitExpression(unsigned index);
        std::string PSExpression(unsigned index, const char vsOutputName[], const char varyingParameterStruct[]) const;
        const MainFunctionParameter& Param(unsigned index) const { return _parameters[index]; }

        bool IsInitializedBySystem(unsigned index) const { return !_buildSystemFunctions[index].empty(); }

        ParameterGenerator(const NodeGraph& graph, const MainFunctionInterface& interf);
        ~ParameterGenerator();
    private:
        std::vector<MainFunctionParameter>  _parameters;
        std::vector<std::string>            _buildSystemFunctions;
        std::string                         _vsOutputMember;

        ParameterMachine _paramMachine;
    };

    std::string ParameterGenerator::VaryingStructSignature(unsigned index) const
    {
        if (!_buildSystemFunctions[index].empty()) return std::string();
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
                    //  \todo -- handle min/max coordinate conversions, etc
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

        return std::string();
    }

    std::string ParameterGenerator::PSExpression(unsigned index, const char vsOutputName[], const char varyingParameterStruct[]) const
    {
        auto buildSystemFunction = _buildSystemFunctions[index];
        if (!buildSystemFunction.empty()) {
            if (!_vsOutputMember.empty())
                return buildSystemFunction + "(" + varyingParameterStruct + "." + _vsOutputMember + ", sys)";
            return buildSystemFunction + "(" + vsOutputName + ", sys)";
        } else {
            return std::string(varyingParameterStruct) + "." + _parameters[index]._name;
        }
    }

    ParameterGenerator::ParameterGenerator(const NodeGraph& graph, const MainFunctionInterface& interf)
    {
        _parameters = std::vector<MainFunctionParameter>(interf.GetInputParameters().cbegin(), interf.GetInputParameters().cend());
        for (auto i=_parameters.cbegin(); i!=_parameters.cend(); ++i)
            _buildSystemFunctions.push_back(_paramMachine.GetBuildSystem(*i));
    }

    ParameterGenerator::~ParameterGenerator() {}

    // todo -- these templates could come from a file... It would be convenient for development
    //          (but it would also add another file dependency)
    static const char ps_main_template_default[] = 
R"--(
NE_{{GraphName}}_Output ps_main(NE_PSInput input, SystemInputs sys)
{
    NE_{{GraphName}}_Output functionResult;
    {{GraphName}}({{ParametersToMainFunctionCall}});
    return functionResult;
}
)--";

    static const char ps_main_template_explicit[] = 
R"--(

float4 AsFloat4(float input)    { return Cast_float_to_float4(input); }
float4 AsFloat4(float2 input)   { return Cast_float2_to_float4(input); }
float4 AsFloat4(float3 input)   { return Cast_float3_to_float4(input); }
float4 AsFloat4(float4 input)   { return input; }

float4 AsFloat4(int input)      { return Cast_float_to_float4(float(input)); }
float4 AsFloat4(int2 input)     { return Cast_float2_to_float4(float2(input)); }
float4 AsFloat4(int3 input)     { return Cast_float3_to_float4(float3(input)); }
float4 AsFloat4(int4 input)     { return float4(input); }

float4 AsFloat4(uint input)     { return Cast_float_to_float4(float(input)); }
float4 AsFloat4(uint2 input)    { return Cast_float2_to_float4(float2(input)); }
float4 AsFloat4(uint3 input)    { return Cast_float3_to_float4(float3(input)); }
float4 AsFloat4(uint4 input)    { return float4(input); }

float4 ps_main(NE_PSInput input, SystemInputs sys) : SV_Target0
{
    NE_{{GraphName}}_Output functionResult;
    {{GraphName}}({{ParametersToMainFunctionCall}});

    {{^IsChart}}
    return AsFloat4(functionResult.{{PreviewOutput}});
    {{/IsChart}}

    {{#IsChart}}
    {{FlatOutputsInit}}
    float comparisonValue = 1.f - input.position.y / NodeEditor_GetOutputDimensions().y;
    bool filled = false;
    for (uint c=0; c<outputDimensionality; c++)
        if (comparisonValue < flatOutputs[c])
            filled = true;
    float3 chartOutput = filled ? FilledGraphPattern(input.position) : BackgroundPattern(input.position);
    for (uint c2=0; c2<outputDimensionality; c2++)
        chartOutput = lerp(chartOutput, NodeEditor_GraphEdgeColour(c2).rgb, NodeEditor_IsGraphEdge(flatOutputs[c2], comparisonValue));
    return float4(chartOutput, 1.f);
    {{/IsChart}}
}
)--";

    static const char vs_main_template[] = 
R"--(
NE_PSInput vs_main(uint vertexId : SV_VertexID, VSInput vsInput)
{
    NE_PSInput OUT;
    {{#InitGeo}}OUT.geo = BuildInterpolator_VSOutput(vsInput);{{/InitGeo}}
    float3 worldPosition = BuildInterpolator_WORLDPOSITION(vsInput);
    float3 localPosition = GetLocalPosition(vsInput);
    {{VaryingInitialization}}
    return OUT;
}
)--";

    std::string         GenerateStructureForPreview(
        const NodeGraph& graph, const MainFunctionInterface& interf, 
        const char outputToVisualize[])
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

        ParameterGenerator mainParams(graph, interf);

            //
            //      All varying parameters must have semantics
            //      so, look for free TEXCOORD slots, and set all unset semantics
            //      to a default
            //

        std::stringstream result;
        result << std::endl;
        result << "\t//////// Structure for preview ////////" << std::endl;

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
        for each (auto i in interf.GetOutputParameters())
            result << "\t" << i._type << " " << i._name << ": SV_Target" << (svTargetCounter++) << ";" << std::endl;
        result << "};" << std::endl << std::endl;

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
        for each (auto i in interf.GetOutputParameters()) {
            if (!parametersToMainFunctionCall.empty())
                parametersToMainFunctionCall += ", ";
            parametersToMainFunctionCall += "functionResult." + i._name;
        }

        unsigned inputDimensionality = 0;
        for (unsigned index=0; index<mainParams.Count(); ++index)
            if (!mainParams.IsInitializedBySystem(index))
                inputDimensionality += GetDimensionality(mainParams.Param(index)._type);

        const bool allowGraphs = false;
        Plustache::template_t preprocessor;

        // Render the ps_main template
        {
            PlustacheTypes::ObjectType context
            {
                {"GraphName", graph.GetName()},
                {"IsChart", ToPlustache(allowGraphs && inputDimensionality == 1)},
                {"ParametersToMainFunctionCall", parametersToMainFunctionCall},
                {"PreviewOutput", outputToVisualize}
            };

                // Collect all of the output values into a flat array of floats.
                // This is needed for "charts"
            unsigned outputDimensionality = 0;
            bool needFlatOutputs = allowGraphs && inputDimensionality == 1;
            if (needFlatOutputs) {
                std::stringstream writingToFlatArray;
                for each (auto i in interf.GetOutputParameters()) {
                    auto signature = LoadParameterStructSignature(SplitArchiveName(i._archiveName));
                    if (!signature._name.empty()) {
                        for (auto p=signature._parameters.cbegin(); p!=signature._parameters.cend(); ++p) {
                            auto dim = GetDimensionality(p->_type);
                            for (unsigned c=0; c<dim; ++c) {
                                writingToFlatArray << "flatOutputs[" << outputDimensionality+c << "] = " << "functionResult." << i._name << "." << p->_name;
                                if (dim != 1) writingToFlatArray << "[" << c << "]";
                                writingToFlatArray << ";";
                            }
                            outputDimensionality += dim;
                        }
                    } else {
                        auto dim = GetDimensionality(i._archiveName);
                        for (unsigned c=0; c<dim; ++c) {
                            writingToFlatArray << "flatOutputs[" << outputDimensionality+c << "] = " << "functionResult." << i._name;
                            if (dim != 1) writingToFlatArray << "[" << c << "]";
                            writingToFlatArray << ";";
                        }
                        outputDimensionality += dim;
                    }
                }

                writingToFlatArray << "const uint outputDimensionality = " << outputDimensionality << ";" << std::endl;
                writingToFlatArray << "float flatOutputs[" << outputDimensionality << "];" << std::endl;
                context["FlatOutputsInit"] = writingToFlatArray.str();
            }

            // outputToVisualize can either be the name of a variable in the functionResult
            // struct, or it can be a number signifying out of the SV_Target<> outputs.
            // When we have a graph that outputs a struct (such as GBufferValues), we don't
            // actually know the contents of that struct from here. So we can access the members
            // directly (or understand their types, etc)
            // However, outputToVisualize allows the caller to explicitly capture the result from
            // one of those struct members.

            if (    outputToVisualize && outputToVisualize[0] != '\0' 
                && !XlBeginsWith(MakeStringSection(outputToVisualize), MakeStringSection("SV_Target"))) {
                result << preprocessor.render(ps_main_template_explicit, context);
            } else
                result << preprocessor.render(ps_main_template_default, context);
        }

        // Render the vs_main template
        result << preprocessor.render(vs_main_template, 
            PlustacheTypes::ObjectType
            {
                {"InitGeo", ToPlustache(mainParams.VSOutputMember().empty())},
                {"VaryingInitialization", varyingInitialization.str()}
            });

        return result.str();
    }

}

