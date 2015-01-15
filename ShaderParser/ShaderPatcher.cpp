// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderPatcher.h"
#include "InterfaceSignature.h"
#include "ParameterSignature.h"
#include "../Core/Exceptions.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/StringUtils.h"
#include "../Utility/PtrUtils.h"
#include <sstream>
#include <assert.h>
#include <algorithm>
#include <tuple>
#include <regex>

#pragma warning(disable:4127)       // conditional expression is constant

namespace ShaderPatcher 
{

        ///////////////////////////////////////////////////////////////

    Node::Node(const std::string& archiveName, uint64 nodeId, Type::Enum type)
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

    NodeConnection::NodeConnection( uint64 outputNodeId, uint64 inputNodeId, 
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

    NodeConstantConnection::NodeConstantConnection(uint64 outputNodeId, const std::string& outputParameterName, const std::string& value)
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

    bool            NodeGraph::IsUpstream(uint64 startNode, uint64 searchingForNode)
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

    bool            NodeGraph::IsDownstream(uint64 startNode, 
                                            const uint64* searchingForNodesStart, 
                                            const uint64* searchingForNodesEnd)
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

    bool            NodeGraph::HasNode(uint64 nodeId)
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

        std::vector<uint64> trimmingNodes;
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

    uint64            NodeGraph::GetUniqueNodeId() const
    {
        uint64 largestId = 0;
        std::for_each(_nodes.cbegin(), _nodes.cend(), [&](const Node& n) { largestId = std::max(largestId, n.NodeId()); });
        return largestId+1;
    }

    const Node*     NodeGraph::GetNode(uint64 nodeId) const
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

    void            NodeGraph::TrimForPreview(uint64 previewNode)
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

    static bool HasConnection(const std::vector<NodeConnection>& connections, uint64 destinationId, const std::string& destinationName)
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
        std::vector<uint64> starterNodes;
        for (auto i=_nodes.begin(); i!=_nodes.end(); ++i)
            starterNodes.push_back(i->NodeId());

        for (auto i=starterNodes.begin(); i!=starterNodes.end(); ++i)
            AddDefaultOutputs(*GetNode(*i));
    }

    void        NodeGraph::AddDefaultOutputs(const Node& node)
    {
        auto sig = LoadFunctionSignature(SplitArchiveName(node.ArchiveName()));

            //  a function can actually output many values. Each output needs it's own default
            //  output node attached. First, look for a "return" value. Then search through
            //  for parameters with "out" set
        if (!sig._returnType.empty() && sig._returnType != "void") {
            if (!HasConnection(_nodeConnections, node.NodeId(), "return")) {
                auto newNodeId = GetUniqueNodeId();
                _nodes.push_back(Node(sig._returnType, newNodeId, Node::Type::Output));
                _nodeConnections.push_back(NodeConnection(newNodeId, node.NodeId(), "value", sig._returnType, "return", sig._returnType));
            }
        }

        for (auto i=sig._parameters.cbegin(); i!=sig._parameters.cend(); ++i) {
            if (i->_direction & ShaderSourceParser::FunctionSignature::Parameter::Out) {
                if (!HasConnection(_nodeConnections, node.NodeId(), i->_name)) {
                    auto newNodeId = GetUniqueNodeId();
                    _nodes.push_back(Node(i->_type, newNodeId, Node::Type::Output));
                    _nodeConnections.push_back(NodeConnection(newNodeId, node.NodeId(), "value", i->_type, i->_name, i->_type));
                }
            }
        }
    }

    void            NodeGraph::Trim(const uint64* trimNodesBegin, const uint64* trimNodesEnd)
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

    static bool SortNodesFunction(  uint64                  node,
                                    std::vector<uint64>&    presorted, 
                                    std::vector<uint64>&    sorted, 
                                    std::vector<uint64>&    marks,
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

    static std::string AsString(uint64 i)
    {
        char buffer[128];
        XlI64toA(i, buffer, dimof(buffer), 10);
        return buffer;
    }

    static std::string OutputTemporaryForNode(uint64 nodeId, const std::string& outputName)        
    { 
        return std::string("Output_") + AsString(nodeId) + "_" + outputName; 
    }
    static std::string InterpolatorParameterName(uint64 nodeId)     { return std::string("interp_") + AsString(nodeId); }

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
            auto outputName = OutputTemporaryForNode(node.NodeId(), "return");
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

        std::vector<uint64> presortedNodes, sortedNodes;
        sortedNodes.reserve(graph.GetNodes().size());

        for (auto i=graph.GetNodes().cbegin(); i!=graph.GetNodes().cend(); ++i) {
            presortedNodes.push_back(i->NodeId());
        }

        bool acyclic = true;
        while (!presortedNodes.empty()) {
            std::vector<uint64> temporaryMarks;
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
            result << LoadSourceFile("game/xleres/System/Prefix.shader") << std::endl;
        } else {
            result << "#include \"game/xleres/System/Prefix.shader\"" << std::endl;
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

    struct MainFunctionParameter
    {
        std::string _type, _name, _archiveName;
        MainFunctionParameter(const std::string& type, const std::string& name, const std::string& archiveName) : _type(type), _name(name), _archiveName(archiveName) {}
        MainFunctionParameter() {}
    };

    std::vector<MainFunctionParameter> GetMainFunctionOutputParameters(const NodeGraph& graph, const NodeGraph& graphOfTemporaries)
    {
        std::vector<MainFunctionParameter> result;
        for (auto i =graph.GetNodes().cbegin(); i!=graph.GetNodes().cend(); ++i) {
            if (i->GetType() == Node::Type::Output) {

                    // at the moment, each output element is a struct. It might be convenient
                    // if we could use both structs or value types
                char buffer[64];
                _snprintf_s(buffer, _TRUNCATE, "OUT_%i", i->NodeId());
                    
                auto signature = LoadParameterStructSignature(SplitArchiveName(i->ArchiveName()));
                if (!signature._name.empty()) {
                    result.push_back(MainFunctionParameter(signature._name, buffer, i->ArchiveName()));
                } else {
                    result.push_back(MainFunctionParameter(i->ArchiveName(), buffer, i->ArchiveName()));
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
                        result.push_back(MainFunctionParameter(p->_type, matchResult[1], p->_type));
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
        for (   auto i =graph.GetNodes().cbegin(); 
                     i!=graph.GetNodes().cend();     ++i) {
            
            if (    i->GetType() == Node::Type::InterpolatorIntoPixel 
                ||  i->GetType() == Node::Type::InterpolatorIntoVertex
                ||  i->GetType() == Node::Type::SystemParameters) {

                if (mainFunctionDeclParameters.tellp() != std::stringstream::pos_type(0)) {
                    mainFunctionDeclParameters << ", ";
                }

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
                                //          with the name "return" in the signature definition
                            if (i->InputParameterName() == "return") {
                                if (!sig._returnType.empty() && sig._returnType != "void") {
                                    if (sig._returnType != i->OutputType()._name) {
                                        result << "Cast_" << sig._returnType << "_to_" << i->OutputType()._name << "(" << OutputTemporaryForNode(i->InputNodeId(), "return") << ");" << std::endl;
                                    } else {
                                        result << OutputTemporaryForNode(i->InputNodeId(), "return") << ";" << std::endl;
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

    static const char*      KnownCBufferSystemParameters[]  = { "Time" };
    static const char*      KnowGlobalBufferParameters[]    = { "LocalNegativeLightDirection", "LocalSpaceView", "LocalToWorld", "WorldToClip" };

    class ParameterOperationQueue
    {
    public:
        class VSInputParameter
        {
        public:
            std::string     _vsInputSemantic;
            std::string     _vsInputType;
        };

        class VSLoadOperation
        {
        public:
            std::string     _vsToPSParameter;
            std::string     _vsToPSSemantic;
            std::string     _vsToPSType;
            std::string     _vsInputSemantic;
            std::string     _vsLoadOperation;
            bool            _requires3DTransform;
        };
        
        class PSLoadOperation
        {
        public:
            std::string     _outputSystemParameter;
            std::string     _vsToPsParameter;
            std::string     _psLoadOperation;
        };

        std::vector<const VSInputParameter*>  activeVSInputParameters;
        std::vector<const VSLoadOperation*>   activeVSLoadOperations;
        std::vector<const PSLoadOperation*>   activePSLoadOperations;
        
        bool        QueueSystemParameter(const std::string& systemParameterName);
        bool        QueueVSToPSParameter(const std::string& vsToPsParameter);
        bool        QueueVSInputParameter(const std::string& vsInputSemantic);

        bool        Requires3DTransform() const;

    private:
        static const VSInputParameter   KnownVSInputParameters[];
        static const VSLoadOperation    KnownVSLoadOperations[];
        static const PSLoadOperation    KnownPSLoadOperations[];
    };

    const ParameterOperationQueue::VSInputParameter ParameterOperationQueue::KnownVSInputParameters[] = 
    {
        { "NORMAL0",        "float3" },
        { "TEXCOORD0",      "float2" }
    };

    const ParameterOperationQueue::VSLoadOperation ParameterOperationQueue::KnownVSLoadOperations[] = 
    {
        { "LocalNormal",        "NORMAL0",      "float3", "NORMAL0",        std::string(), true },
        { "TexCoord0",          "TEXCOORD0",    "float2", "TEXCOORD0",      std::string(), false },
        { "LocalViewVector",    std::string(),  "float3", std::string(),    "OUT.LocalViewVector = LocalSpaceView - iPosition;", true }
    };
        
    const ParameterOperationQueue::PSLoadOperation ParameterOperationQueue::KnownPSLoadOperations[] = 
    {
        { "LocalNormal",        "LocalNormal",      std::string() },
        { "TexCoord0",          "TexCoord0",        std::string() },
        { "LocalViewDirection", "LocalViewVector",  "normalize(IN.LocalViewVector)" },
        { "LocalViewVector",    "LocalViewVector",  std::string() },
        { "Light_Direction",    std::string(),      "LocalNegativeLightDirection" }
    };

    template<typename Container>
        static bool Contains(const Container& container, typename Container::value_type& type)
    {
        return std::find(container.cbegin(), container.cend(), type) != container.cend();
    }

    bool        ParameterOperationQueue::QueueSystemParameter(const std::string& systemParameterName)
    {
        for (unsigned c=0; c<dimof(KnownPSLoadOperations); ++c) {
            auto op = &KnownPSLoadOperations[c];
            if (op->_outputSystemParameter == systemParameterName) {

                if (!Contains(activePSLoadOperations, op)) {
                    activePSLoadOperations.push_back(op);

                        //
                        //      The PS load operation might also require a
                        //      ps->vs parameter. Look for one and queue it
                        //      up as appropriate.
                        //

                    if (!op->_vsToPsParameter.empty()) {
                        QueueVSToPSParameter(op->_vsToPsParameter);
                    }
                }

                return true;
            }
        }

        return false;
    }

    bool        ParameterOperationQueue::QueueVSToPSParameter(const std::string& vsToPsParameter)
    {
        auto i = std::find_if(KnownVSLoadOperations, &KnownVSLoadOperations[dimof(KnownVSLoadOperations)], 
            [&](const VSLoadOperation& vsLoadOp) { return vsLoadOp._vsToPSParameter == vsToPsParameter; } );
        if (i != &KnownVSLoadOperations[dimof(KnownVSLoadOperations)]) {

            if (!Contains(activeVSLoadOperations, i)) {
                activeVSLoadOperations.push_back(i);

                    //
                    //  This operation may also require a vs input parameter
                    //  Just as before, queue it up.
                    //

                if (!i->_vsInputSemantic.empty()) {
                    QueueVSInputParameter(i->_vsInputSemantic);
                }
            }

            return true;
        }

        return false;
    }

    bool        ParameterOperationQueue::QueueVSInputParameter(const std::string& vsInputSemantic)
    {
        auto i = std::find_if(KnownVSInputParameters, &KnownVSInputParameters[dimof(KnownVSInputParameters)], 
            [&](const VSInputParameter& vsLoadOp) { return vsLoadOp._vsInputSemantic == vsInputSemantic; } );
        if (i != &KnownVSInputParameters[dimof(KnownVSInputParameters)]) {

            if (!Contains(activeVSInputParameters, i)) {
                activeVSInputParameters.push_back(i);
            }

            return true;
        }

        return false;
    }

    bool        ParameterOperationQueue::Requires3DTransform() const
    {
        for (auto i=activeVSLoadOperations.begin(); i!=activeVSLoadOperations.end(); ++i) {
            if ((*i)->_requires3DTransform) {
                return true;
            }
        }
        return false;
    }

    static std::string InitializationForSystemStruct(const ShaderPatcher::Node& node, std::string& extraHeaders)
    {
            //  some system structs are known by the system, and 
            //  have special initialisation
        auto split = SplitArchiveName(node.ArchiveName());

        if (std::get<1>(split) == "GBufferValues") {
            extraHeaders += "#include \"System\\LoadGBuffer.h\"\n";
            return "LoadGBuffer(IN.position, IN.sys)";
        }

        return "GetSystemStruct_" + std::get<1>(split) + "()";
    }

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
                            std::stringstream stream; stream << "IN_" << p->_name;

                            NodeConnection newConnection(i->NodeId(), ~0ull, p->_name, p->_type, stream.str(), p->_type);
                            graphOfTemporaries.GetNodeConnections().push_back(newConnection);
                        }
                    }

                }
            }
        }

        return graphOfTemporaries;
    }

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

        auto varyingParameters = GetMainFunctionVaryingParameters(graph, graphOfTemporaries);

        ParameterOperationQueue     operationQueue;
     


            //
            //      All varying parameters must have semantics
            //      so, look for free TEXCOORD slots, and set all unset semantics
            //      to a default
            //

        signed maxTexCoordForVSToPS = -1;
        for (auto p=operationQueue.activeVSLoadOperations.cbegin(); p!=operationQueue.activeVSLoadOperations.cend(); ++p) {
            if ((*p)->_vsToPSSemantic.substr(0, 8) == "TEXCOORD") {
                maxTexCoordForVSToPS = std::max(signed(XlAtoI32((*p)->_vsToPSSemantic.substr(8).c_str())), maxTexCoordForVSToPS);
            }
        }

        char smallBuffer[128];
        std::stringstream result;
        
        result << std::endl;
        result << "\t//////// Structure for preview ////////" << std::endl;

            //  
            //      First write the "varying" parameters
            //
        {
            result << "struct Varying" << std::endl << "{" << std::endl;
            for (auto v=varyingParameters.cbegin(); v!=varyingParameters.cend(); ++v) {
                int index = (int)std::distance(varyingParameters.cbegin(), v);
                result << "\t" << v->_type << " " << v->_name << " : " << "VARYING_" << XlI32toA(index, smallBuffer, dimof(smallBuffer), 10) <<  ";" << std::endl;
            }
            result << "};" << std::endl << std::endl;
        }

            //
            //      Write "_Output" structure. This contains all of the values that are output
            //      from the main function
            //
        auto mainFunctionOutputs = GetMainFunctionOutputParameters(graph, graphOfTemporaries);
        {
            result << "struct " << graph.GetName() << "_Output" << std::endl << "{" << std::endl;
            for (auto i=mainFunctionOutputs.begin(); i!=mainFunctionOutputs.end(); ++i) {
                result << "\t" << i->_type << " " << i->_name << ": SV_Target" << std::distance(mainFunctionOutputs.begin(), i) << ";" << std::endl;
            }
            result << "};" << std::endl << std::endl;
        }

        result << "struct PSInput" << std::endl << "{" << std::endl;
        result << "\tfloat4 position : SV_Position;" << std::endl;
        result << "\tVarying varyingParameters;" << std::endl;

        std::string extraIncludeStatements;

            //
            //      Write fixed "InterpolatorIntoPixel" parameters
            //
        std::string parametersToMainFunctionCall;
        std::vector<MainFunctionParameter> interpolatorsIntoPixel;
        for (auto n=graph.GetNodes().cbegin(); n!=graph.GetNodes().cend(); ++n) {
            if (n->GetType() == Node::Type::InterpolatorIntoPixel) {
                auto signature = LoadParameterStructSignature(SplitArchiveName(n->ArchiveName()));
                std::string memberName = std::string("In_") + XlI64toA(n->NodeId(), smallBuffer, dimof(smallBuffer), 10);
                result << "\t" << signature._name << " " << memberName << ";" << std::endl;

                if (!parametersToMainFunctionCall.empty()) {
                    parametersToMainFunctionCall += ", ";
                }
                parametersToMainFunctionCall += "IN." + memberName;
                interpolatorsIntoPixel.push_back(MainFunctionParameter(signature._name, memberName, n->ArchiveName()));
            } else if (n->GetType() == Node::Type::SystemParameters) {
                if (!parametersToMainFunctionCall.empty()) {
                    parametersToMainFunctionCall += ", ";
                }
                parametersToMainFunctionCall += InitializationForSystemStruct(*n, extraIncludeStatements);
            }
        }

            //  pass each member of the "varyingParameters" struct as a separate input to
            //  the main function
        for (auto v=varyingParameters.cbegin(); v!=varyingParameters.cend(); ++v) {
            if (!parametersToMainFunctionCall.empty()) {
                parametersToMainFunctionCall += ", ";
            }
            parametersToMainFunctionCall += "IN.varyingParameters." + v->_name;
        }
            
            // also pass each output as a parameter to the main function
        for (auto i=mainFunctionOutputs.begin(); i!=mainFunctionOutputs.end(); ++i) {
            if (!parametersToMainFunctionCall.empty()) {
                parametersToMainFunctionCall += ", ";
            }
            parametersToMainFunctionCall += "functionResult." + i->_name;
        }

            //
            //      Now write the queued vs-to-ps parameters
            //
        for (auto v=operationQueue.activeVSLoadOperations.cbegin(); v!=operationQueue.activeVSLoadOperations.cend(); ++v) {
            if (std::find_if(operationQueue.activeVSLoadOperations.cbegin(), v, 
                    [=](const ParameterOperationQueue::VSLoadOperation* op) { return op->_vsToPSSemantic == (*v)->_vsToPSSemantic; }) != v) {
                continue;
            }

            std::string semantic = (*v)->_vsToPSSemantic;
            if (semantic.empty()) {
                semantic = std::string("TEXCOORD") + XlI32toA(++maxTexCoordForVSToPS, smallBuffer, dimof(smallBuffer), 10);
            }
            result << "\t" << (*v)->_vsToPSType << " " << (*v)->_vsToPSParameter << " : " << semantic << ";" << std::endl;
        }

            //
            //      Finally write the pixel shader system inputs (default SV_ type inputs like position and msaa sample)
            //
        result << "\tSystemInputs sys;" << std::endl;
        result << "};" << std::endl << std::endl;

        result << extraIncludeStatements << std::endl;

        unsigned inputDimensionality = 0;
        for (auto v=varyingParameters.cbegin(); v!=varyingParameters.cend(); ++v) {
            inputDimensionality += GetDimensionality(v->_type);
        }

        std::string finalOutputType = graph.GetName() + "_Output";
        if (inputDimensionality==1) {
            finalOutputType = "NodeEditor_GraphOutput";
        }

        result << finalOutputType << " PixelShaderEntry(PSInput IN)" << std::endl;
        result << "{" << std::endl;
        result << "\t" << graph.GetName() << "_Output functionResult;" << std::endl;
        result << "\t" << graph.GetName() << "(";
        result << parametersToMainFunctionCall;

#if 0
        bool first = true;
        for (   auto i =graph.GetParameterConnections().cbegin(); 
                     i!=graph.GetParameterConnections().cend();     ++i) {

            if (std::find_if(graph.GetParameterConnections().cbegin(), i,
                    [&](const ParameterConnection& connection) { return connection.ParameterName() == i->ParameterName(); } ) == i) {

                if (i->ParameterSource() == ParameterConnection::SourceType::System) {
                    if (!first) {
                        result << ", ";
                    }
                    first = false;

                    if (std::find_if(varyingParameters.cbegin(), varyingParameters.cend(), 
                        [=](const Parameter& parameter) { return parameter._name == i->ParameterName(); }) !=varyingParameters.cend()) {

                        result << "IN." << i->ParameterName();

                    } else {

                        auto p = std::find_if(operationQueue.activePSLoadOperations.cbegin(), operationQueue.activePSLoadOperations.cend(),
                            [&](const ParameterOperationQueue::PSLoadOperation* op) { return op->_outputSystemParameter == i->ParameterName(); });
                        if (p != operationQueue.activePSLoadOperations.cend()) {
                            if (!(*p)->_psLoadOperation.empty()) {
                                result << (*p)->_psLoadOperation;
                            } else {
                                result << "IN." << (*p)->_outputSystemParameter;
                            }
                        } else {
                            result << i->ParameterName();
                        }
                    }
                }
            }
        }
        
        result << ")." << graph.GetOutputConnections()[0].OutputParameterName() << ";" << std::endl;
#endif
        result << ");" << std::endl;


            //
            //      Check for 2D or 1D graphs...
            //

            // Collect all of the output values into a flat array of floats.
        unsigned outputDimensionality = 0;
        {
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

            result << "const uint outputDimensionality = " << outputDimensionality << ";" << std::endl;
            result << "float flatOutputs[" << outputDimensionality << "];" << std::endl;
            result << writingToFlatArray.str() << std::endl;
        }

        if (inputDimensionality == 1) {
                //  special case for 1D graphs. Filter the output via a function to render a graph

            result << "NodeEditor_GraphOutput graphOutput;" << std::endl;
            result << "float comparisonValue = 1.f - IN.position.y / NodeEditor_GetOutputDimensions().y;" << std::endl;
	        result << "bool filled = false;" << std::endl;
	        result << "for (uint c=0; c<outputDimensionality; c++)" << std::endl;
		    result << "    if (comparisonValue < flatOutputs[c])" << std::endl;
			result << "        filled = true;" << std::endl;
	        result << "graphOutput.output = filled ? FilledGraphPattern(IN.position) : BackgroundPattern(IN.position);" << std::endl;
	        result << "for (uint c2=0; c2<outputDimensionality; c2++)" << std::endl;
		    result << "    graphOutput.output.rgb = lerp(graphOutput.output.rgb, NodeEditor_GraphEdgeColour(c2).rgb, NodeEditor_IsGraphEdge(flatOutputs[c2], comparisonValue));" << std::endl;
            result << "return graphOutput;" << std::endl;
            
            // result << "\tfunctionResult.target0 = NodeEditor_RenderGraph(functionResult.target0, IN.position);" << std::endl;
        } else {
            result << "\treturn functionResult;" << std::endl;
        }

        result << "}" << std::endl << std::endl;

            //
            //      If the shader takes 3d position as input, then we must
            //      transform the coordinates by local-to-world and world-to-clip
            //      
        const bool requires3DTransform = operationQueue.Requires3DTransform();
        if (requires3DTransform) {
            result << "PSInput VertexShaderEntry(uint vertexId : SV_VertexID, float3 iPosition : POSITION0";
        } else {
            result << "PSInput VertexShaderEntry(uint vertexId : SV_VertexID, float" << std::max(2u,std::min(inputDimensionality, 4u)) << " iPosition : POSITION0";
        }

        for (auto i=operationQueue.activeVSInputParameters.cbegin(); i!=operationQueue.activeVSInputParameters.cend(); ++i) {
            auto o = std::find_if(operationQueue.activeVSInputParameters.cbegin(), i, 
                [=](const ParameterOperationQueue::VSInputParameter* i2) { return (*i)->_vsInputSemantic == i2->_vsInputSemantic; });
            if (o != i) continue;

            result << ", " << (*i)->_vsInputType << " i" << (*i)->_vsInputSemantic << " : " << (*i)->_vsInputSemantic;
        }
            
        result << ")" << std::endl;
        result << "{" << std::endl;
        result << "\tPSInput OUT;" << std::endl;
            
        if (requires3DTransform) {
            result << "\tfloat3 worldPosition = mul(LocalToWorld, float4(iPosition,1)).xyz;" << std::endl;
            result << "\tfloat4 clipPosition = mul(WorldToClip, float4(worldPosition,1));" << std::endl;
            result << "\tOUT.position = clipPosition;" << std::endl;
        } else {
            if (inputDimensionality<=2) {
                result << "\tOUT.position = float4(iPosition.xy, 0.0f, 1.0f);" << std::endl;
                result << "\tfloat3 worldPosition = float3(iPosition.xy, 0.0f);" << std::endl;
            } else if (inputDimensionality==3) {
                result << "\tOUT.position = float4(iPosition.xyz, 1.0f);" << std::endl;
                result << "\tfloat3 worldPosition = iPosition.xyz;" << std::endl;
            } else {
                result << "\tOUT.position = iPosition;" << std::endl;
                result << "\tfloat3 worldPosition = iPosition.xyz;" << std::endl;
            }
        }
            
            //
            //      For each varying parameter, write out the appropriate value for
            //      this point on the rendering surface
            //
        for (auto v=varyingParameters.begin(); v!=varyingParameters.end(); ++v) {
                //  \todo -- handle min/max coordinate conversions, etc
            int dimensionality = GetDimensionality(v->_type);
            if (dimensionality == 1) {
                result << "\tOUT.varyingParameters." << v->_name << " = iPosition.x * 0.5 + 0.5.x;" << std::endl;
            } else if (dimensionality == 2) {
                result << "\tOUT.varyingParameters." << v->_name << " = float2(iPosition.x * 0.5 + 0.5, iPosition.y * -0.5 + 0.5);" << std::endl;
            } else if (dimensionality == 3) {
                result << "\tOUT.varyingParameters." << v->_name << " = worldPosition.xyz;" << std::endl;
            }
        }

        for (auto i=interpolatorsIntoPixel.cbegin(); i!=interpolatorsIntoPixel.cend(); ++i) {
            result << "\tOUT." << i->_name << " = BuildInterpolator_" << i->_type << "(vertexId);" << std::endl;
        }

        for (auto p=operationQueue.activeVSLoadOperations.cbegin(); p!=operationQueue.activeVSLoadOperations.cend(); ++p) {
            if (!(*p)->_vsLoadOperation.empty()) {
                result << (*p)->_vsLoadOperation << std::endl;
            } else {
                result << "\tOUT." << (*p)->_vsToPSParameter << " = i" << (*p)->_vsInputSemantic << ";" << std::endl;
            }
        }

        result << "\treturn OUT;" << std::endl;
        result << "}" << std::endl;

        return result.str();
    }

}

