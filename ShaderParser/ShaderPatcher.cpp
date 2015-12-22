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

        ///////////////////////////////////////////////////////////////

    Node::Node(const std::string& archiveName, uint32 nodeId, Type::Enum type)
    : _archiveName(archiveName)
    , _nodeId(nodeId)
    , _type(type)
    {
    }

    Node::Node(Node&& moveFrom) 
    :   _archiveName(std::move(moveFrom._archiveName))
    ,   _nodeId(moveFrom._nodeId)
    ,   _type(moveFrom._type)
    {
    }

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

    NodeConnection::NodeConnection( uint32 outputNodeId, uint32 inputNodeId, 
                                    const std::string& outputParameterName, const Type& outputType, 
                                    const std::string& inputParameterName, const Type& inputType)
    :       _outputNodeId(outputNodeId)
    ,       _inputNodeId(inputNodeId)
    ,       _outputParameterName(outputParameterName)
    ,       _outputType(outputType)
    ,       _inputParameterName(inputParameterName)
    ,       _inputType(inputType)
    {
    }

    NodeConnection::NodeConnection(NodeConnection&& moveFrom)
    :       _outputNodeId(moveFrom._outputNodeId)
    ,       _inputNodeId(moveFrom._inputNodeId)
    ,       _outputParameterName(moveFrom._outputParameterName)
    ,       _outputType(moveFrom._outputType)
    ,       _inputParameterName(moveFrom._inputParameterName)
    ,       _inputType(moveFrom._inputType)
    {
    }

    NodeConnection& NodeConnection::operator=(NodeConnection&& moveFrom)
    {
        _outputNodeId = moveFrom._outputNodeId;
        _inputNodeId = moveFrom._inputNodeId;
        _outputParameterName = moveFrom._outputParameterName;
        _outputType = moveFrom._outputType;
        _inputParameterName = moveFrom._inputParameterName;
        _inputType = moveFrom._inputType;
        return *this;
    }

        ///////////////////////////////////////////////////////////////

    NodeConstantConnection::NodeConstantConnection(uint32 outputNodeId, const std::string& outputParameterName, const std::string& value)
    :       _outputNodeId(outputNodeId)
    ,       _outputParameterName(outputParameterName)
    ,       _value(value)
    {
    }

    NodeConstantConnection::NodeConstantConnection(NodeConstantConnection&& moveFrom)
    :       _outputNodeId(moveFrom._outputNodeId)
    ,       _outputParameterName(moveFrom._outputParameterName)
    ,       _value(moveFrom._value)
    {
    }

    NodeConstantConnection& NodeConstantConnection::operator=(NodeConstantConnection&& moveFrom)
    {
        _outputNodeId = moveFrom._outputNodeId;
        _outputParameterName = moveFrom._outputParameterName;
        _value = moveFrom._value;
        return *this;
    }

        ///////////////////////////////////////////////////////////////

    NodeGraph::NodeGraph(const std::string& name) : _name(name) {}

    NodeGraph::NodeGraph(NodeGraph&& moveFrom) 
    :   _nodes(std::move(moveFrom._nodes))
    ,   _nodeConnections(std::move(moveFrom._nodeConnections))
    ,   _nodeConstantConnections(std::move(moveFrom._nodeConstantConnections))
    ,   _name(std::move(moveFrom._name))
    {
    }

    NodeGraph& NodeGraph::operator=(NodeGraph&& moveFrom)
    {
        _nodes = std::move(moveFrom._nodes);
        _nodeConnections = std::move(moveFrom._nodeConnections);
        _nodeConstantConnections = std::move(moveFrom._nodeConstantConnections);
        _name = std::move(moveFrom._name);
        return *this;
    }

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
        auto sig = LoadFunctionSignature(SplitArchiveName(node.ArchiveName()));
        auto nodeId = node.NodeId();

            //  a function can actually output many values. Each output needs it's own default
            //  output node attached. First, look for a "return" value. Then search through
            //  for parameters with "out" set
        if (!sig._returnType.empty() && sig._returnType != "void") {
            if (!HasConnection(_nodeConnections, node.NodeId(), "result")) {
                auto newNodeId = GetUniqueNodeId();
                _nodes.push_back(Node(sig._returnType, newNodeId, Node::Type::Output)); // (note -- this invalidates "node"!)
                _nodeConnections.push_back(NodeConnection(newNodeId, nodeId, "value", sig._returnType, "result", sig._returnType));
            }
        }

        for (auto i=sig._parameters.cbegin(); i!=sig._parameters.cend(); ++i) {
            if (i->_direction & ShaderSourceParser::FunctionSignature::Parameter::Out) {
                if (!HasConnection(_nodeConnections, node.NodeId(), i->_name)) {
                    auto newNodeId = GetUniqueNodeId();
                    _nodes.push_back(Node(i->_type, newNodeId, Node::Type::Output));    // (note -- this invalidates "node"!)
                    _nodeConnections.push_back(NodeConnection(newNodeId, nodeId, "value", i->_type, i->_name, i->_type));
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

        _nodeConstantConnections.erase(
            std::remove_if(
                _nodeConstantConnections.begin(), _nodeConstantConnections.end(),
                [=](const NodeConstantConnection& connection) 
                    { return !HasNode(connection.OutputNodeId()); }),
            _nodeConstantConnections.end());
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

        std::stringstream result;
        std::stringstream warnings;

            //      1.  Declare output variable (made unique by node id)
            //      2.  Call the function, assigning the output variable as appropriate
            //          and passing in the parameters (as required)
        for (auto i = sig._parameters.cbegin(); i!=sig._parameters.cend(); ++i) {
            if (i->_direction & ShaderSourceParser::FunctionSignature::Parameter::Out) {
                result << "\t" << i->_type << " " << OutputTemporaryForNode(node.NodeId(), i->_name) << ";" << std::endl;
            }
        }

        if (!sig._returnType.empty() && sig._returnType != "void") {
            auto outputName = OutputTemporaryForNode(node.NodeId(), "result");
            result << "\t" << sig._returnType << " " << outputName << ";" << std::endl;
            result << "\t" << outputName << " = " << functionName << "( ";
        } else {
            result << "\t" << functionName << "( ";
        }

        for (auto p=sig._parameters.cbegin(); p!=sig._parameters.cend(); ++p) {
            if (p != sig._parameters.cbegin()) {
                result << ", ";
            }

                // note -- problem here for in/out parameters
            if (p->_direction == ShaderSourceParser::FunctionSignature::Parameter::Out) {
                result << OutputTemporaryForNode(node.NodeId(), p->_name);
                continue;
            }

            const std::string& name = p->_name;
            auto i = std::find_if(  nodeGraph.GetNodeConnections().cbegin(), nodeGraph.GetNodeConnections().cend(), 
                                    [&](const NodeConnection& connection) 
                                    { return    connection.OutputNodeId() == node.NodeId() && 
                                                connection.OutputParameterName() == name; } );
            if (i!=nodeGraph.GetNodeConnections().cend()) {

                    //      Found a connection... write the parameter name.
                    //      We can also check for casting and any type errors
                const Type& connectionInputType     = i->InputType();
                const Type& connectionOutputType    = i->OutputType();
                const std::string& parsedType       = p->_type;
                if (parsedType != connectionOutputType._name) {
                    warnings    << "\t// Type for function parameter name " << name 
                                << " seems to have changed in the shader fragment. Check for valid connections." << std::endl;
                }

                const bool doCast = connectionInputType._name != parsedType;
                if (doCast) {
                    result << "Cast_" << connectionInputType._name << "_to_" << parsedType << "(";
                }

                    //      Check to see what kind of connnection it is
                    //      By default, let's just assume it's a procedure.
                auto inputNodeType = Node::Type::Procedure;
                auto* inputNode = nodeGraph.GetNode(i->InputNodeId());
                if (inputNode) { inputNodeType = inputNode->GetType(); }

                if (inputNodeType == Node::Type::Procedure) {
                    result << OutputTemporaryForNode(i->InputNodeId(), i->InputParameterName()); 
                } else if (inputNodeType == Node::Type::MaterialCBuffer) {
                    result << i->InputParameterName();
                } else if (inputNodeType == Node::Type::Constants) {
                    result << "ConstantValue_" << i->InputNodeId() << "_" << i->InputParameterName();
                } else if (inputNodeType == Node::Type::InterpolatorIntoPixel || inputNodeType == Node::Type::InterpolatorIntoVertex || inputNodeType == Node::Type::SystemParameters) {
                    result << InterpolatorParameterName(i->InputNodeId()) << "." << i->InputParameterName();
                }

                if (doCast) {
                    result << ")";
                }

            } else {

                auto ci = std::find_if(  nodeGraph.GetNodeConstantConnections().cbegin(), nodeGraph.GetNodeConstantConnections().cend(), 
                                    [&](const NodeConstantConnection& connection) 
                                    { return    connection.OutputNodeId() == node.NodeId() && 
                                                connection.OutputParameterName() == name; } );
                if (ci!=nodeGraph.GetNodeConstantConnections().cend()) {

                        //  we have a "constant connection" value here. We either extract the name of 
                        //  the varying parameter, or we interpret this as pure text...
                    std::regex filter("<(.*)>");
                    std::smatch matchResult;
                    if (std::regex_match(ci->Value(), matchResult, filter) && matchResult.size() > 1) {
                        result << matchResult[1];
                    } else {
                        result << ci->Value();
                    }

                } else {

                        //  If there is a connection in the graph of temporaries, it must mean that we
                        //  have a varying parameter (or a system generated constant) attached
                    auto ti = std::find_if(  graphOfTemporaries.GetNodeConnections().cbegin(), graphOfTemporaries.GetNodeConnections().cend(), 
                                            [&](const NodeConnection& connection) 
                                            { return    connection.OutputNodeId() == node.NodeId() && 
                                                        connection.OutputParameterName() == name; } );
                    if (ti != graphOfTemporaries.GetNodeConnections().cend()) {
                        result << ti->InputParameterName(); // (assume no casting required)
                    }

                }
                
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

    std::vector<MainFunctionParameter> GetMainFunctionOutputParameters(const NodeGraph& graph, const NodeGraph& graphOfTemporaries)
    {
        std::vector<MainFunctionParameter> result;
        for (auto i =graph.GetNodes().cbegin(); i!=graph.GetNodes().cend(); ++i) {
            if (i->GetType() == Node::Type::Output) {

                    // at the moment, each output element is a struct. It might be convenient
                    // if we could use both structs or value types
                StringMeld<64> buffer;
                buffer << "OUT_" << i->NodeId();
                    
                auto signature = LoadParameterStructSignature(SplitArchiveName(i->ArchiveName()));
                if (!signature._name.empty()) {
                    result.push_back(MainFunctionParameter(signature._name, std::string(buffer), i->ArchiveName()));
                } else {
                    result.push_back(MainFunctionParameter(i->ArchiveName(), std::string(buffer), i->ArchiveName()));
                }
            }
        }
        return result;
    }

    std::vector<MainFunctionParameter> GetMainFunctionVaryingParameters(const NodeGraph& graph, const NodeGraph& graphOfTemporaries)
    {
        std::vector<MainFunctionParameter> result;

        for (auto i = graphOfTemporaries.GetNodeConnections().begin(); i!=graphOfTemporaries.GetNodeConnections().end(); ++i) {
            if (i->OutputNodeId() != ~0ull) {
                result.push_back(MainFunctionParameter(i->InputType()._name, i->InputParameterName(), i->InputType()._name));
            }
        }

            // also find "constant" connections on the main graph that are marked as input parameters
        std::regex filter("<(.*)>");
        for (auto i = graph.GetNodeConstantConnections().begin(); i!=graph.GetNodeConstantConnections().end(); ++i) {
            std::smatch matchResult;
            if (std::regex_match(i->Value(), matchResult, filter)) {
                if (matchResult.size() > 1) {
                        // we need to find the type by looking out the output node
                    auto* destinationNode = graph.GetNode(i->OutputNodeId());
                    if (!destinationNode) continue;

                    auto sig = LoadFunctionSignature(SplitArchiveName(destinationNode->ArchiveName()));
                    auto p = std::find_if(sig._parameters.cbegin(), sig._parameters.cend(), 
                        [=](const ShaderSourceParser::FunctionSignature::Parameter&p)
                            { return p._name == i->OutputParameterName(); });

                    if (p!=sig._parameters.end()) {
                        result.push_back(MainFunctionParameter(p->_type, matchResult[1], p->_type, p->_semantic));
                    }
                }
            }
        }

        return result;
    }

    std::string GenerateShaderBody(const NodeGraph& graph, const NodeGraph& graphOfTemporaries)
    {
        std::stringstream mainFunctionDeclParameters;

            //
            //      Any "interpolator" nodes must have values passed into
            //      the main shader body from the framework code. Let's assume
            //      that if the node exists in the graph, then it's going to be
            //      required in the shader.
            //
        for (auto i =graph.GetNodes().cbegin(); i!=graph.GetNodes().cend(); ++i) {
            if (    i->GetType() == Node::Type::InterpolatorIntoPixel 
                ||  i->GetType() == Node::Type::InterpolatorIntoVertex
                ||  i->GetType() == Node::Type::SystemParameters) {

                if (mainFunctionDeclParameters.tellp() != std::stringstream::pos_type(0))
                    mainFunctionDeclParameters << ", ";

                auto signature = LoadParameterStructSignature(SplitArchiveName(i->ArchiveName()));
                mainFunctionDeclParameters << signature._name << " " << InterpolatorParameterName(i->NodeId());

            }
        }

        auto varyingParameters = GetMainFunctionVaryingParameters(graph, graphOfTemporaries);
        for (auto i=varyingParameters.begin(); i!=varyingParameters.end(); ++i) {
            if (mainFunctionDeclParameters.tellp() != std::stringstream::pos_type(0)) {
                mainFunctionDeclParameters << ", ";
            }
            mainFunctionDeclParameters << i->_type << " " << i->_name;
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
        const bool writeOutputsViaStruct = false;
        if (!writeOutputsViaStruct) {
            auto outputs = GetMainFunctionOutputParameters(graph, graphOfTemporaries);
            for (auto i=outputs.begin(); i!=outputs.end(); ++i) {
                if (mainFunctionDeclParameters.tellp() != std::stringstream::pos_type(0)) {
                    mainFunctionDeclParameters << ", ";
                }
                mainFunctionDeclParameters << "out " << i->_type << " " << i->_name;
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
        result << GenerateMainFunctionBody(graph, graphOfTemporaries);

            //
            //      Fill in the "OUT" structure with all of the output
            //      values.
            //
        if (writeOutputsViaStruct) {
            result << "\t" << graph.GetName() << "_Output OUT; " << std::endl;
        }

        for (   auto i =graph.GetNodeConnections().cbegin(); 
                     i!=graph.GetNodeConnections().cend();     ++i) {

            auto* destinationNode = graph.GetNode(i->OutputNodeId());
            if (destinationNode && destinationNode->GetType() == Node::Type::Output) {

                if  (!writeOutputsViaStruct) {
                    if (i->OutputParameterName() != "value") {
                        result << "\tOUT_" << destinationNode->NodeId() << "." << i->OutputParameterName() << " = ";
                    } else {
                        result << "\tOUT_" << destinationNode->NodeId() << " = ";
                    }
                } else {
                    result << "\tOUT." << "Output_" << destinationNode->NodeId() << "." << i->OutputParameterName() << " = ";
                }

                    //
                    //      Enforce a cast if we need it...
                    //      This cast is important - but it requires us to re-parse
                    //      the shader fragment. \todo Avoid redundant re-parsing
                    //      \todo -- do we also need to connect varying parameters to outputs?
                    //
                {
                    auto* inputNode = graph.GetNode(i->InputNodeId());
                    if (inputNode) {

                        auto inputNodeType = inputNode->GetType();
                        if (inputNodeType == Node::Type::Procedure) {

                            auto sig = LoadFunctionSignature(SplitArchiveName(inputNode->ArchiveName()));
                            bool foundConnection = false;
                                // todo -- we could simplify this a little bit by just making the return value an output parameter
                                //          with the name "result" in the signature definition
                            if (i->InputParameterName() == "result") {
                                if (!sig._returnType.empty() && sig._returnType != "void") {
                                    if (sig._returnType != i->OutputType()._name) {
                                        result << "Cast_" << sig._returnType << "_to_" << i->OutputType()._name << "(" << OutputTemporaryForNode(i->InputNodeId(), "result") << ");" << std::endl;
                                    } else {
                                        result << OutputTemporaryForNode(i->InputNodeId(), "result") << ";" << std::endl;
                                    }
                                    foundConnection = true;
                                }
                            } else {
                                    // find an "out" parameter with the right name
                                for (auto p=sig._parameters.cbegin(); p!=sig._parameters.cend(); ++p) {
                                    if (p->_direction & ShaderSourceParser::FunctionSignature::Parameter::Out
                                        && p->_name == i->InputParameterName()) {
                                        
                                        if (p->_type != i->OutputType()._name) {
                                            result << "Cast_" << p->_type << "_to_" << i->OutputType()._name << "(" << OutputTemporaryForNode(i->InputNodeId(), p->_name) << ");" << std::endl;
                                        } else {
                                            result << OutputTemporaryForNode(i->InputNodeId(), p->_name) << ";" << std::endl;
                                        }
                                        foundConnection = true;
                                        break;
                                    }
                                }
                            } 

                            if (!foundConnection) {
                                    // no output parameters.. call back to default
                                result << "DefaultValue_" << i->OutputType()._name << "();" << std::endl;
                            }

                        } else if (inputNodeType == Node::Type::MaterialCBuffer) {
                            result << i->InputParameterName() << ";" << std::endl;
                        } else if (inputNodeType == Node::Type::Constants) {
                            result << "ConstantValue_" << i->InputNodeId() << "_" << i->InputParameterName() << ";" << std::endl;
                        } else if (inputNodeType == Node::Type::InterpolatorIntoPixel || inputNodeType == Node::Type::InterpolatorIntoVertex|| inputNodeType == Node::Type::SystemParameters) {
                            result << InterpolatorParameterName(i->InputNodeId()) << "." << i->InputParameterName() << ";" << std::endl;
                        }

                    } else {

                        result << "// Warning! Missing input node for output connection!" << std::endl;

                    }
                    
                }
                
            }
        }

        result << std::endl << "}" << std::endl;

        return result.str();
    }

        ////////////////////////////////////////////////////////////////////////

    NodeGraph           GenerateGraphOfTemporaries(const NodeGraph& graph)
    {
        NodeGraph graphOfTemporaries;

            //
            //      Look for inputs to the graph that aren't
            //      attached to anything.
            //      These should either take some default input, or become 
            //          our "varying parameters".
            //
        for (auto i=graph.GetNodes().cbegin(); i!=graph.GetNodes().cend(); ++i) {
            if (i->GetType() == Node::Type::Procedure) {

                auto signature = LoadFunctionSignature(SplitArchiveName(i->ArchiveName()));
                for (auto p=signature._parameters.cbegin(); p!=signature._parameters.cend(); ++p) {

                    if (!(p->_direction & ShaderSourceParser::FunctionSignature::Parameter::In))
                        continue;

                    auto searchIterator = 
                        std::find_if(   graph.GetNodeConnections().cbegin(), graph.GetNodeConnections().cend(),
                                        [&](const NodeConnection& connection) 
                        { 
                            return 
                                (connection.OutputNodeId() == i->NodeId())
                                && (connection.OutputParameterName() == p->_name);
                        });

                        //
                        //      If we didn't find any connection, then it's an
                        //      unattached parameter. Let's consider it a varying parameter
                        //
                    if (searchIterator == graph.GetNodeConnections().cend()) {

                        auto searchIterator2 = std::find_if(graph.GetNodeConstantConnections().cbegin(), graph.GetNodeConstantConnections().cend(),
                            [&](const NodeConstantConnection& connection) 
                            { 
                                return 
                                    (connection.OutputNodeId() == i->NodeId())
                                    && (connection.OutputParameterName() == p->_name);
                            });

                        if (searchIterator2 == graph.GetNodeConstantConnections().cend()) {
                                // todo --  check for parameters with duplicate names. It's possible
                                //          that different nodes have the same input parameter name
                                //          (or we might even have the same parameter name multiple times)
                            // std::stringstream stream; stream << "IN_" << p->_name;

                            NodeConnection newConnection(i->NodeId(), ~0ul, p->_name, p->_type, p->_name, p->_type);
                            graphOfTemporaries.GetNodeConnections().push_back(newConnection);
                        }
                    }

                }
            }
        }

        return graphOfTemporaries;
    }

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

    class MainFunctionParameters
    {
    public:
        unsigned Count() const                      { return (unsigned)_parameters.size(); };
        std::string VSOutputMember() const          { return _vsOutputMember; }

        std::string VaryingStructSignature(unsigned index) const;
        std::string VSInitExpression(unsigned index);
        std::string PSExpression(unsigned index, const char vsOutputName[], const char varyingParameterStruct[]) const;
        const MainFunctionParameter& Param(unsigned index) const { return _parameters[index]; }

        bool IsInitializedBySystem(unsigned index) const { return !_buildSystemFunctions[index].empty(); }

        MainFunctionParameters(const NodeGraph& graph, const NodeGraph& graphOfTemporaries);
        ~MainFunctionParameters();
    private:
        std::vector<MainFunctionParameter>  _parameters;
        std::vector<std::string>            _buildSystemFunctions;
        std::string                         _vsOutputMember;

        ParameterMachine _paramMachine;
    };

    std::string MainFunctionParameters::VaryingStructSignature(unsigned index) const
    {
        if (!_buildSystemFunctions[index].empty()) return std::string();
        const auto& p = _parameters[index];

        std::stringstream result;
        result << "\t" << p._type << " " << p._name;
        if (!p._semantic.empty()) {
            result << " : " << p._semantic;
        } else {
            char smallBuffer[128];
            if (!IsStructType(MakeStringSection(p._type)))  // (struct types don't get a semantic)
                result << " : " << "VARYING_" << XlI32toA(index, smallBuffer, dimof(smallBuffer), 10);
        }
        return result.str();
    }

    std::string MainFunctionParameters::VSInitExpression(unsigned index)
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

    std::string MainFunctionParameters::PSExpression(unsigned index, const char vsOutputName[], const char varyingParameterStruct[]) const
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

    MainFunctionParameters::MainFunctionParameters(const NodeGraph& graph, const NodeGraph& graphOfTemporaries)
    {
        _parameters = GetMainFunctionVaryingParameters(graph, graphOfTemporaries);
        for (auto i=_parameters.cbegin(); i!=_parameters.cend(); ++i)
            _buildSystemFunctions.push_back(_paramMachine.GetBuildSystem(*i));
    }

    MainFunctionParameters::~MainFunctionParameters() {}

    // todo -- these templates could come from a file... It would be convenient for development
    //          (but it would also add another file dependency)
    static const char ps_main_template[] = 
R"--(
{{^IsChart}}NE_{{GraphName}}_Output{{/IsChart}}{{#IsChart}}NE_GraphOutput{{/IsChart}} ps_main(NE_PSInput input, SystemInputs sys)
{
    NE_{{GraphName}}_Output functionResult;
    {{GraphName}}({{ParametersToMainFunctionCall}});

    {{^IsChart}}
    return functionResult;
    {{/IsChart}}

    {{FlatOutputsInit}}
    {{#IsChart}}
    NE_GraphOutput graphOutput;
    float comparisonValue = 1.f - input.position.y / NodeEditor_GetOutputDimensions().y;
    bool filled = false;
    for (uint c=0; c<outputDimensionality; c++)
        if (comparisonValue < flatOutputs[c])
            filled = true;
    graphOutput.output = filled ? FilledGraphPattern(input.position) : BackgroundPattern(input.position);
    for (uint c2=0; c2<outputDimensionality; c2++)
        graphOutput.output.rgb = lerp(graphOutput.output.rgb, NodeEditor_GraphEdgeColour(c2).rgb, NodeEditor_IsGraphEdge(flatOutputs[c2], comparisonValue));
    return graphOutput;
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

    std::string         GenerateStructureForPreview(const NodeGraph& graph, const NodeGraph& graphOfTemporaries)
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

        MainFunctionParameters mainParams(graph, graphOfTemporaries);

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
        for (size_t index=0; index<mainParams.Count(); ++index) {
            auto sig = mainParams.VaryingStructSignature(index);
            if (sig.empty()) continue;
            result << sig << ";" << std::endl;
        }
        result << "};" << std::endl << std::endl;

            //
            //      Write "_Output" structure. This contains all of the values that are output
            //      from the main function
            //
        auto mainFunctionOutputs = GetMainFunctionOutputParameters(graph, graphOfTemporaries);
        result << "struct NE_" << graph.GetName() << "_Output" << std::endl << "{" << std::endl;
        for (auto i=mainFunctionOutputs.begin(); i!=mainFunctionOutputs.end(); ++i) {
            result << "\t" << i->_type << " " << i->_name << ": SV_Target" << std::distance(mainFunctionOutputs.begin(), i) << ";" << std::endl;
        }
        result << "};" << std::endl << std::endl;

            //
            //      Calculate the code that will fill in the varying parameters from the vertex
            //      shader. We need to do this now because it will effect some of the structure
            //      later.
            //
        std::stringstream varyingInitialization;
        
        for (size_t index=0; index<mainParams.Count(); ++index) {
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
        for (size_t index=0; index<mainParams.Count(); ++index) {
            if (!parametersToMainFunctionCall.empty())
                parametersToMainFunctionCall += ", ";
            parametersToMainFunctionCall += mainParams.PSExpression(index, "input.geo", "input.varyingParameters");
        }
            
            //  Also pass each output as a parameter to the main function
        for (auto i=mainFunctionOutputs.begin(); i!=mainFunctionOutputs.end(); ++i) {
            if (!parametersToMainFunctionCall.empty())
                parametersToMainFunctionCall += ", ";
            parametersToMainFunctionCall += "functionResult." + i->_name;
        }

        unsigned inputDimensionality = 0;
        for (size_t index=0; index<mainParams.Count(); ++index)
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
                {"ParametersToMainFunctionCall", parametersToMainFunctionCall}
            };

                // Collect all of the output values into a flat array of floats.
                // This is needed for "charts"
            unsigned outputDimensionality = 0;
            bool needFlatOutputs = allowGraphs && inputDimensionality == 1;
            if (needFlatOutputs) {
                std::stringstream writingToFlatArray;
                for (auto i=mainFunctionOutputs.begin(); i!=mainFunctionOutputs.end(); ++i) {
                    auto signature = LoadParameterStructSignature(SplitArchiveName(i->_archiveName));
                    if (!signature._name.empty()) {
                        for (auto p=signature._parameters.cbegin(); p!=signature._parameters.cend(); ++p) {
                            auto dim = GetDimensionality(p->_type);
                            for (unsigned c=0; c<dim; ++c) {
                                writingToFlatArray << "flatOutputs[" << outputDimensionality+c << "] = " << "functionResult." << i->_name << "." << p->_name;
                                if (dim != 1) writingToFlatArray << "[" << c << "]";
                                writingToFlatArray << ";";
                            }
                            outputDimensionality += dim;
                        }
                    } else {
                        auto dim = GetDimensionality(i->_archiveName);
                        for (unsigned c=0; c<dim; ++c) {
                            writingToFlatArray << "flatOutputs[" << outputDimensionality+c << "] = " << "functionResult." << i->_name;
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

            result << preprocessor.render(ps_main_template, context);
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

