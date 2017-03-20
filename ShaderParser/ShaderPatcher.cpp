// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderPatcher.h"
#include "ShaderPatcher_Internal.h"
#include "InterfaceSignature.h"
#include "ParameterSignature.h"
#include "../RenderCore/ShaderLangUtil.h"
#include "../Assets/IFileSystem.h"
#include "../Core/Exceptions.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/StringUtils.h"
#include "../Utility/StringFormat.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/Conversion.h"
#include "../Utility/Streams/StreamFormatter.h"
#include <sstream>
#include <set>
#include <assert.h>
#include <algorithm>
#include <tuple>
#include <regex>
#include <map>

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

    std::string LoadSourceFile(StringSection<char> sourceFileName)
    {
        TRY {
			auto file = ::Assets::MainFileSystem::OpenBasicFile(sourceFileName.AsString().c_str(), "rb");

            file.Seek(0, FileSeekAnchor::End);
            size_t size = file.TellP();
            file.Seek(0, FileSeekAnchor::Start);

            std::string result;
            result.resize(size, '\0');
            file.Read(&result.at(0), 1, size);
            return result;

        } CATCH(const std::exception& ) {
            return std::string();
        } CATCH_END
    }
    
    std::tuple<std::string, std::string> SplitArchiveName(const std::string& archiveName)
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
		ShaderFragment(StringSection<::Assets::ResChar> fn);
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

	ShaderFragment::ShaderFragment(StringSection<::Assets::ResChar> fn)
	{
		auto shaderFile = LoadSourceFile(fn);
		_sig = ShaderSourceParser::BuildShaderFragmentSignature(MakeStringSection(shaderFile));
		_depVal = std::make_shared<::Assets::DependencyValidation>();
		::Assets::RegisterFileDependency(_depVal, fn);
	}

	ShaderFragment::~ShaderFragment() {}

	const ShaderSourceParser::FunctionSignature& LoadFunctionSignature(const std::tuple<std::string, std::string>& splitName, const ::Assets::DirectorySearchRules& searchRules)
    {
        TRY {
			char resolvedFile[MaxPath];
			searchRules.ResolveFile(resolvedFile, std::get<0>(splitName).c_str());
			auto& frag = ::Assets::GetAssetDep<ShaderFragment>(resolvedFile);
			auto* fn = frag.GetFunction(std::get<1>(splitName).c_str());
			if (fn != nullptr) return *fn;
        } CATCH (...) {
        } CATCH_END
		static ShaderSourceParser::FunctionSignature blank;
        return blank;
    }

    const ShaderSourceParser::ParameterStructSignature& LoadParameterStructSignature(const std::tuple<std::string, std::string>& splitName, const ::Assets::DirectorySearchRules& searchRules)
    {
        if (!std::get<0>(splitName).empty()) {
            using namespace ShaderSourceParser;
            TRY {
				char resolvedFile[MaxPath];
				searchRules.ResolveFile(resolvedFile, std::get<0>(splitName).c_str());
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

    unsigned GetDimensionality(const std::string& typeName)
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

        const auto& sig = LoadFunctionSignature(SplitArchiveName(node.ArchiveName()), _searchRules);

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

	static void OrderNodes(IteratorRange<uint32*> range)
	{
		// We need to sort the upstreams in some way that maintains a 
		// consistant ordering. The simplied way is just to use node id.
		// However we may sometimes want to set priorities for nodes.
		// For example, nodes that can "discard" pixels should be priortized
		// higher.
		std::sort(range.begin(), range.end());
	}

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

		std::vector<uint32> upstream;
		upstream.reserve(graph.GetNodeConnections().size());
        for (const auto& i:graph.GetNodeConnections())
            if (i.OutputNodeId() == node)
				upstream.push_back(i.InputNodeId());

		OrderNodes(MakeIteratorRange(upstream));
		for (const auto& i2:upstream)
			SortNodesFunction(i2, presorted, sorted, marks, graph);

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

    static std::string TypeFromShaderFragment(const std::string& archiveName, const std::string& paramName, const ::Assets::DirectorySearchRules& searchRules)
    {
            // Go back to the shader fragments to find the current type for the given parameter
        const auto& sig = LoadFunctionSignature(SplitArchiveName(archiveName), searchRules);
        if (paramName == s_resultName && HasResultValue(sig))
            return sig._returnType;

            // find an "out" parameter with the right name
        for (const auto& p:sig._parameters)
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
		if (!inputNode || inputNode->GetType() == Node::Type::SystemParameters)
			return ExpressionString{std::string(), std::string()};

        if (inputNode->GetType() == Node::Type::Procedure) {

                // We will load the current type from the shader fragments, overriding what is in the
                // the connection. The two might disagree if the shader fragment has changed since the
                // graph was created.
            std::string type;
            type = TypeFromShaderFragment(inputNode->ArchiveName(), connection.InputParameterName(), nodeGraph.GetSearchRules());
            if (type.empty()) type = connection.InputType()._name;
            return ExpressionString{OutputTemporaryForNode(connection.InputNodeId(), connection.InputParameterName()), type}; 

        } else if (inputNode->GetType() == Node::Type::MaterialCBuffer) {

            return ExpressionString{connection.InputParameterName(), connection.InputType()._name};

        } else if (inputNode->GetType() == Node::Type::Constants) {

            std::stringstream result;
            result << "ConstantValue_" << connection.InputNodeId() << "_" << connection.InputParameterName();
            return ExpressionString{result.str(), connection.InputType()._name};

        } else
            return ExpressionString{std::string(), std::string()};
    }

	static std::pair<std::string, bool> StripAngleBracket(const std::string& str)
	{
		std::regex filter("<(.*)>");
        std::smatch matchResult;
        if (std::regex_match(str, matchResult, filter) && matchResult.size() > 1) {
			return std::make_pair(matchResult[1].str(), true);
		} else {
			return std::make_pair(str, false);
		}
	}

    static ExpressionString QueryExpression(const NodeGraph& nodeGraph, const ConstantConnection& connection)
    {
            //  we have a "constant connection" value here. We either extract the name of 
            //  the varying parameter, or we interpret this as pure text...
		return ExpressionString{StripAngleBracket(connection.Value()).first, std::string()};
    }

    static ExpressionString QueryExpression(const NodeGraph& nodeGraph, const InputParameterConnection& connection)
    {
		auto p = StripAngleBracket(connection.InputName());
		return p.second ? ExpressionString{p.first, std::string()} : ExpressionString{connection.InputName(), connection.InputType()._name};
    }

	static FunctionInterface::Parameter AsInterfaceParameter(const ConstantConnection& connection)			{ return FunctionInterface::Parameter(std::string(), connection.Value(), std::string()); }
	static FunctionInterface::Parameter AsInterfaceParameter(const InputParameterConnection& connection)	{ return FunctionInterface::Parameter(connection.InputType()._name, connection.InputName(), std::string(), FunctionInterface::Parameter::In, std::string(), connection.Default()); }
	static FunctionInterface::Parameter AsInterfaceParameter(const NodeConnection& connection)				{ return FunctionInterface::Parameter(connection.InputType()._name, connection.OutputParameterName(), std::string(), FunctionInterface::Parameter::In); }

    static ExpressionString ParameterExpression(const NodeGraph& nodeGraph, uint32 nodeId, const ShaderSourceParser::FunctionSignature::Parameter& signatureParam, FunctionInterface& interf)
    {
        auto i = FindConnection(nodeGraph.GetNodeConnections(), nodeId, signatureParam._name);
        if (i!=nodeGraph.GetNodeConnections().cend()) {
            auto e = QueryExpression(nodeGraph, *i);
			if (!e._expression.empty()) return e;
		}

        auto ci = FindConnection(nodeGraph.GetConstantConnections(), nodeId, signatureParam._name);
        if (ci!=nodeGraph.GetConstantConnections().cend()) {
			auto p = StripAngleBracket(ci->Value());
			if (p.second) {
				auto paramToAdd = AsInterfaceParameter(*ci);
				paramToAdd._name = p.first;
				if (paramToAdd._type.empty() || XlEqStringI(MakeStringSection(paramToAdd._type), "auto"))
					paramToAdd._type = signatureParam._type;

				interf.AddGlobalParameter(paramToAdd);
				auto e = QueryExpression(nodeGraph, *ci);
				if (!e._expression.empty()) return e;
			} else {
				return ExpressionString{ci->Value(), std::string()};
			}
		}

        auto ti = FindConnection(nodeGraph.GetInputParameterConnections(), nodeId, signatureParam._name);
        if (ti!=nodeGraph.GetInputParameterConnections().cend()) {
				// We can choose to make this either a global parameter or a function input parameter
                // 1) If there is a semantic, or the name is in angle brackets, it will be an input parameter. 
                // 2) Otherwise, it's a global parameter.
			auto p = StripAngleBracket(ti->InputName());

			auto paramToAdd = AsInterfaceParameter(*ti);
			paramToAdd._name = p.first;
            if (paramToAdd._type.empty() || XlEqStringI(MakeStringSection(paramToAdd._type), "auto"))
				paramToAdd._type = signatureParam._type;

			if (p.second) interf.AddFunctionParameter(paramToAdd);
			else interf.AddGlobalParameter(paramToAdd);
            auto e = QueryExpression(nodeGraph, *ti);
			if (!e._expression.empty()) return e;
		}

		// We must add this request as some kind of input to the function (ie, a parameter input or a global input)
		FunctionInterface::Parameter param(signatureParam._type, signatureParam._name, "", FunctionInterface::Parameter::In, signatureParam._semantic);
		interf.AddFunctionParameter(param);
        return ExpressionString{param._name, param._type};
    }

	static std::string UniquifyName(const std::string& name, IteratorRange<const FunctionInterface::Parameter*> existing)
    {
		std::string testName = name;
		unsigned suffix = 0;
		for (;;) {
			auto i = std::find_if(existing.begin(), existing.end(), [&testName](const FunctionInterface::Parameter&p) { return p._name == testName; } );
			if (i == existing.end()) {
				return testName;
			} else {
				std::stringstream str;
				str << name << suffix;
				testName = str.str();
				++suffix;
			}
		}
    }

    template<typename Connection>
        static void FillDirectOutputParameters(
            std::stringstream& result,
            const NodeGraph& graph,
            IteratorRange<const Connection*> range,
            FunctionInterface& interf)
    {
        for (const auto& i:range) {
			auto* destinationNode = graph.GetNode(i.OutputNodeId());
            if (!destinationNode || destinationNode->GetType() == Node::Type::Output) {
				ExpressionString expression = QueryExpression(graph, i);
            
				// This is not connected to anything -- so we just have to add it as a
				// unique output from the interface.
				auto param = AsInterfaceParameter(i);
				auto originalName = param._name;
				param._direction = FunctionInterface::Parameter::Out;
				param._name = UniquifyName(param._name, interf.GetFunctionParameters());
				if (param._type.empty() || XlEqStringI(MakeStringSection(param._type), "auto"))
					param._type = expression._type;
				interf.AddFunctionParameter(param);
				result << "\t" << param._name << " = ";

				if (!expression._expression.empty()) {
					WriteCastExpression(result, expression, param._type);
				} else {
						// no output parameters.. call back to default
					result << "DefaultValue_" << param._type << "()";
					result << "// Warning! Could not generate query expression for node connection!" << std::endl;
				}
				result << ";" << std::endl;
			}
        }
    }

    static std::stringstream GenerateFunctionCall(const Node& node, const NodeGraph& nodeGraph, FunctionInterface& interf)
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
        const auto& sig = LoadFunctionSignature(splitName, nodeGraph.GetSearchRules());

        std::stringstream result, warnings;

            //      1.  Declare output variable (made unique by node id)
            //      2.  Call the function, assigning the output variable as appropriate
            //          and passing in the parameters (as required)
        for (const auto& i:sig._parameters)
            if (i._direction & ShaderSourceParser::FunctionSignature::Parameter::Out)
                result << "\t" << i._type << " " << OutputTemporaryForNode(node.NodeId(), i._name) << ";" << std::endl;

        if (HasResultValue(sig)) {
            auto outputName = OutputTemporaryForNode(node.NodeId(), s_resultName);
            result << "\t" << sig._returnType << " " << outputName << " = " << functionName << "( ";
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

            auto expr = ParameterExpression(nodeGraph, node.NodeId(), *p, interf);
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

        return std::move(result);
    }

    static std::pair<std::string, FunctionInterface> GenerateMainFunctionBody(const NodeGraph& graph)
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

        for (const auto& i:graph.GetNodes())
            presortedNodes.push_back(i.NodeId());

		OrderNodes(MakeIteratorRange(presortedNodes));

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

		FunctionInterface interf;

        for (auto i=sortedNodes.cbegin(); i!=sortedNodes.cend(); ++i) {
            auto i2 = std::find_if( graph.GetNodes().cbegin(), 
                                    graph.GetNodes().cend(), [i](const Node& n) { return n.NodeId() == *i; } );
            if (i2 != graph.GetNodes().cend()) {
                if (i2->GetType() == Node::Type::Procedure) {
                    result << GenerateFunctionCall(*i2, graph, interf).str();
                }
            }
        }

		FillDirectOutputParameters(result, graph, graph.GetNodeConnections(), interf);
        FillDirectOutputParameters(result, graph, graph.GetConstantConnections(), interf);
        FillDirectOutputParameters(result, graph, graph.GetInputParameterConnections(), interf);

        return std::make_pair(result.str(), interf);
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
        result << "#include \"xleres/System/Prefix.h\"" << std::endl;
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

    bool IsStructType(StringSection<char> typeName)
    {
        // If it's not recognized as a built-in shader language type, then we
        // need to assume this is a struct type. There is no typedef in HLSL, but
        // it could be a #define -- but let's assume it isn't.
        return RenderCore::ShaderLangTypeNameAsTypeDesc(typeName)._type == ImpliedTyping::TypeCat::Void;
    }

    bool CanBeStoredInCBuffer(const StringSection<char> type)
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

    bool FunctionInterface::IsCBufferGlobal(unsigned c) const
    {
        return CanBeStoredInCBuffer(MakeStringSection(_globalParameters[c]._type));
    }

    static void AddWithExistingCheck(
        std::vector<FunctionInterface::Parameter>& dst,
        const FunctionInterface::Parameter& param)
    {
	    // Look for another parameter with the same name...
	    auto existing = std::find_if(dst.begin(), dst.end(), 
		    [&param](const FunctionInterface::Parameter& p) { return p._name == param._name; });
	    if (existing != dst.end()) {
		    // If we have 2 parameters with the same name, we're going to expect they
		    // also have the same type and semantic (otherwise we would need to adjust
		    // the name to avoid conflicts).
		    if (existing->_type != param._type || existing->_semantic != param._semantic)
			    Throw(::Exceptions::BasicLabel("Main function parameters with the same name, but different types/semantics (%s)", param._name.c_str()));
	    } else {
		    dst.push_back(param);
	    }
    }

	void FunctionInterface::AddFunctionParameter(const Parameter& param) { AddWithExistingCheck(_functionParameters, param); }
	void FunctionInterface::AddGlobalParameter(const Parameter& param) { AddWithExistingCheck(_globalParameters, param); }

    FunctionInterface::FunctionInterface() {}
    FunctionInterface::~FunctionInterface() {}

    static void MaybeComma(std::stringstream& stream) { if (stream.tellp() != std::stringstream::pos_type(0)) stream << ", "; }

    std::pair<std::string, FunctionInterface> GenerateFunction(const NodeGraph& graph)
    {
		std::string mainBody;
		FunctionInterface interf;
		std::tie(mainBody, interf) = GenerateMainFunctionBody(graph);
		interf.SetName(graph.GetName());

        std::stringstream mainFunctionDeclParameters;

			//  
            //      Our graph function is always a "void" function, and all of the output
            //      parameters are just function parameters with the "out" keyword. This is
            //      convenient for writing out generated functions
            //      We don't want to put the "node id" in the name -- because node ids can 
            //      change from time to time, and that would invalidate any other shaders calling
            //      this function. But ideally we need some way to guarantee uniqueness.
            //
        for (const auto& i:interf.GetFunctionParameters()) {
            MaybeComma(mainFunctionDeclParameters);
			if (i._direction == FunctionInterface::Parameter::Out)
				mainFunctionDeclParameters << "out ";
            mainFunctionDeclParameters << i._type << " " << i._name;
			if (!i._semantic.empty())
				mainFunctionDeclParameters << " : " << i._semantic;
        }

        std::stringstream result;
        result << "void " << graph.GetName() << "(" << mainFunctionDeclParameters.str() << ")" << std::endl;
        result << "{" << std::endl;
		result << mainBody;
        result << "}" << std::endl << std::endl;

        return std::make_pair(result.str(), interf);
    }

	static std::string GenerateMaterialCBuffer(const FunctionInterface& interf)
	{
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
		result << std::endl;

		return result.str();
	}

	static std::string GenerateSignature(const ShaderSourceParser::FunctionSignature& sig)
	{
		// note -- this is almost identical to the code in GenerateFunction (but that is for FunctionInterface)
		std::stringstream mainFunctionDeclParameters;
		for (const auto& i:sig._parameters) {
            MaybeComma(mainFunctionDeclParameters);
			if (i._direction == ShaderSourceParser::FunctionSignature::Parameter::Out) {
				mainFunctionDeclParameters << "out ";
			} else if (i._direction == (ShaderSourceParser::FunctionSignature::Parameter::In|ShaderSourceParser::FunctionSignature::Parameter::Out)) {
				mainFunctionDeclParameters << "inout ";
			}
            mainFunctionDeclParameters << i._type << " " << i._name;
			if (!i._semantic.empty())
				mainFunctionDeclParameters << " : " << i._semantic;
        }

        std::stringstream result;
		if (!sig._returnType.empty()) result << sig._returnType << " ";
		else result << "void ";
        result << sig._name << "(" << mainFunctionDeclParameters.str() << ")";
		if (!sig._returnSemantic.empty())
			result << " : " << sig._returnSemantic;
		return result.str();
	}

	std::string GenerateScaffoldFunction(const ShaderSourceParser::FunctionSignature& slotSignature, const FunctionInterface& generatedFunctionSignature)
	{
			//
			//	Generate the scaffolding function that conforms to the given signature in "slotSignature", but that calls the implementation
			//	function that has the signature in "generatedFunctionSignature"
			//
			//	This is used to tie a node graph to a "slot signature". The slot signature is defined in the shader source code (as a normal function
			//	declaration). But the two may not match exactly (such as different parameter ordering, or some parameters missing in one or the
			//	other, etc). Here, we have to create a function that ties then together, and tries to make the best of mis-matches.
			//

		std::stringstream result;
		result << GenerateMaterialCBuffer(generatedFunctionSignature);

		result << GenerateSignature(slotSignature) << std::endl;
		result << "{" << std::endl;
		
		// make temporaries for all outputs
		for (auto& p:generatedFunctionSignature.GetFunctionParameters()) {
			if (p._direction == FunctionInterface::Parameter::Out) {
				result << "\t" << p._type << " temp_" << p._name << ";" << std::endl;
			}
		}

		std::stringstream paramStream;
		for (const auto& p:generatedFunctionSignature.GetFunctionParameters()) {
			MaybeComma(paramStream);

			if (p._direction == FunctionInterface::Parameter::Out) {
				paramStream << "temp_" << p._name;
				continue;
			}

			// How do we pass a value to this parameter?
			// first, check to see if this parameter is in "slotSignature"
			auto i = std::find_if(slotSignature._parameters.begin(), slotSignature._parameters.end(),
				[&p](const ShaderSourceParser::FunctionSignature::Parameter& test) -> bool
				{
					if (test._direction != ShaderSourceParser::FunctionSignature::Parameter::In) return false;
					return test._name == p._name;
				});
			if (i != slotSignature._parameters.end()) {
				WriteCastExpression(paramStream, {i->_name, i->_type}, p._type);
				continue;
			}

			// second, check for a default value specified on the parameter itself
			if (!p._default.empty()) {
				paramStream << p._default;
				continue;
			}

			// third, just pass a default value
			paramStream << "DefaultValue_" << p._type << "()";
		}

		result << "\t" << generatedFunctionSignature.GetName() << "(" << paramStream.str() << ");" << std::endl;

		// Map the output parameters to their final destination.
		for (const auto&p:slotSignature._parameters) {
			if (p._direction != ShaderSourceParser::FunctionSignature::Parameter::Out || p._name == s_resultName)
				continue;

			result << "\t" << p._name << " = ";

			// First, look for an output from the generated function
			auto i = std::find_if(generatedFunctionSignature.GetFunctionParameters().begin(), generatedFunctionSignature.GetFunctionParameters().end(),
				[&p](const FunctionInterface::Parameter& test) -> bool
				{
					if (test._direction != FunctionInterface::Parameter::Out) return false;
					return test._name == p._name;
				});
			if (i != generatedFunctionSignature.GetFunctionParameters().end()) {
				WriteCastExpression(result, {std::string("temp_") + i->_name, i->_type}, p._type);
				result << ";" << std::endl;
				continue;
			}

			// Second, just use a default value
			result << "DefaultValue_" << p._type << "();" << std::endl;
		}

		if (!slotSignature._returnType.empty()) {
			result << "\treturn ";

				// First, look for an output from the generated function
			auto i = std::find_if(generatedFunctionSignature.GetFunctionParameters().begin(), generatedFunctionSignature.GetFunctionParameters().end(),
				[](const FunctionInterface::Parameter& test) -> bool
				{ return (test._direction == FunctionInterface::Parameter::Out) && test._name == s_resultName; });
			if (i != generatedFunctionSignature.GetFunctionParameters().end()) {
				WriteCastExpression(result, {std::string("temp_") + i->_name, i->_type}, slotSignature._returnType);
			} else {
				// Second, just use a default value
				result << "DefaultValue_" << slotSignature._returnType << "()";
			}
			result << ";" << std::endl;
		}

		result << "}" << std::endl;

		return result.str();
	}
}

