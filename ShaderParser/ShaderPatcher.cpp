// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderPatcher.h"
#include "ShaderPatcher_Internal.h"
#include "../RenderCore/ShaderLangUtil.h"
#include "../Core/Exceptions.h"
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

namespace ShaderPatcher
{

    static const std::string s_resultName = "result";
    static const uint32 s_nodeId_Invalid = ~0u;

        ///////////////////////////////////////////////////////////////

    Node::Node(const std::string& archiveName, uint32 nodeId, Type type)
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

    NodeConnection::NodeConnection(NodeConnection&& moveFrom) never_throws
    :       NodeBaseConnection(std::move(moveFrom))
    ,       _inputNodeId(moveFrom._inputNodeId)
    ,       _inputParameterName(moveFrom._inputParameterName)
    ,       _inputType(moveFrom._inputType)
    {}

    NodeConnection& NodeConnection::operator=(NodeConnection&& moveFrom) never_throws
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

    ConstantConnection::ConstantConnection(ConstantConnection&& moveFrom) never_throws
    :   NodeBaseConnection(std::move(moveFrom))
    ,   _value(moveFrom._value) {}

    ConstantConnection& ConstantConnection::operator=(ConstantConnection&& moveFrom) never_throws
    {
        NodeBaseConnection::operator=(std::move(moveFrom));
        _value = moveFrom._value;
        return *this;
    }

        ///////////////////////////////////////////////////////////////

    InputParameterConnection::InputParameterConnection(uint32 outputNodeId, const std::string& outputParameterName, const Type& type, const std::string& name, const std::string& semantic, const std::string& defaultValue)
    :   NodeBaseConnection(outputNodeId, outputParameterName)
    ,   _type(type), _name(name), _semantic(semantic), _default(defaultValue) {}

    InputParameterConnection::InputParameterConnection(InputParameterConnection&& moveFrom) never_throws
    :   NodeBaseConnection(std::move(moveFrom))
    ,   _type(std::move(moveFrom._type)), _name(std::move(moveFrom._name)), _semantic(std::move(moveFrom._semantic)), _default(std::move(moveFrom._default)) {}

    InputParameterConnection& InputParameterConnection::operator=(InputParameterConnection&& moveFrom) never_throws
    {
        NodeBaseConnection::operator=(std::move(moveFrom));
        _type = std::move(moveFrom._type);
        _name = std::move(moveFrom._name);
        _semantic = std::move(moveFrom._semantic);
        _default = std::move(moveFrom._default);
        return *this;
    }

        ///////////////////////////////////////////////////////////////

    NodeGraph::NodeGraph() {}
    NodeGraph::~NodeGraph() {}

    void NodeGraph::Add(Node&& a) { _nodes.emplace_back(std::move(a)); }
    void NodeGraph::Add(NodeConnection&& a) { _nodeConnections.emplace_back(std::move(a)); }
    void NodeGraph::Add(ConstantConnection&& a) { _constantConnections.emplace_back(std::move(a)); }
    void NodeGraph::Add(InputParameterConnection&& a) { _inputParameterConnections.emplace_back(std::move(a)); }
    // void NodeGraph::AddTemplateParameterBinding(const std::string& parameter, const std::string& binding) { _templateParameterBindings.insert({parameter, binding}); }

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

    void NodeGraph::Trim(uint32 previewNode)
    {
        Trim(&previewNode, &previewNode+1);
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
                         if (node.GetType() == Node::Type::SlotOutput) {
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

///////////////////////////////////////////////////////////////////////////////////////////////////

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

    static std::string TypeFromShaderFragment(
        StringSection<> archiveName, StringSection<> paramName, ParameterDirection direction,
        ISignatureProvider& sigProvider)
    {
            // Go back to the shader fragments to find the current type for the given parameter
        auto sigResult = sigProvider.FindSignature(archiveName);
        if (!sigResult._signature)
            return std::string();
            // Throw(::Exceptions::BasicLabel("Couldn't find signature for (%s)", archiveName.AsString().c_str()));

        const auto& sig = *sigResult._signature;

            // find a parameter with the right direction & name
        for (const auto& p:sig.GetParameters())
            if (p._direction == direction && XlEqString(paramName, p._name))
                return p._type;

        return std::string();
    }

    static ExpressionString QueryExpression(const NodeGraph& nodeGraph, const NodeConnection& connection, NodeGraphSignature& interf, ISignatureProvider& sigProvider)
    {
            //      Check to see what kind of connection it is
            //      By default, let's just assume it's a procedure.
        auto* inputNode = nodeGraph.GetNode(connection.InputNodeId());
        if (inputNode && inputNode->GetType() == Node::Type::Procedure) {

                // We will load the current type from the shader fragments, overriding what is in the
                // the connection. The two might disagree if the shader fragment has changed since the
                // graph was created.
            auto type = TypeFromShaderFragment(inputNode->ArchiveName(), connection.InputParameterName(), ParameterDirection::Out, sigProvider);
            if (type.empty()) type = connection.InputType()._name;
            return ExpressionString{OutputTemporaryForNode(connection.InputNodeId(), connection.InputParameterName()), type};

        } else if (inputNode && inputNode->GetType() == Node::Type::Uniforms) {

            return ExpressionString{connection.InputParameterName(), connection.InputType()._name};

        } else if (!inputNode || inputNode->GetType() == Node::Type::SlotInput) {

			NodeGraphSignature::Parameter param{connection.InputType()._name, connection.InputParameterName(), ParameterDirection::In};
			if (param._type.empty() || XlEqStringI(MakeStringSection(param._type), "auto")) {
				if (inputNode) {
					auto type = TypeFromShaderFragment(inputNode->ArchiveName(), connection.InputParameterName(), ParameterDirection::In, sigProvider);
					if (!type.empty()) param._type = type;
				}
			}
			interf.AddParameter(param);
			return ExpressionString{connection.InputParameterName(), connection.InputType()._name};

		} else {

            return ExpressionString{std::string(), std::string()};

		}
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

    static ExpressionString QueryExpression(const NodeGraph& nodeGraph, const ConstantConnection& connection, NodeGraphSignature& interf, ISignatureProvider& sigProvider)
    {
            //  we have a "constant connection" value here. We either extract the name of
            //  the varying parameter, or we interpret this as pure text...
		return ExpressionString{StripAngleBracket(connection.Value()).first, std::string()};
    }

    static ExpressionString QueryExpression(const NodeGraph& nodeGraph, const InputParameterConnection& connection, NodeGraphSignature& interf, ISignatureProvider& sigProvider)
    {
		auto p = StripAngleBracket(connection.InputName());
		if (p.second) return ExpressionString{p.first, std::string()};
		return ExpressionString{connection.InputName(), connection.InputType()._name};
    }

	static NodeGraphSignature::Parameter AsInterfaceParameter(const ConstantConnection& connection)			{ return NodeGraphSignature::Parameter{std::string(), connection.Value()}; }
	static NodeGraphSignature::Parameter AsInterfaceParameter(const InputParameterConnection& connection)	{ return NodeGraphSignature::Parameter{connection.InputType()._name, connection.InputName(), ParameterDirection::In, std::string(), connection.Default()}; }
	static NodeGraphSignature::Parameter AsInterfaceParameter(const NodeConnection& connection)				{ return NodeGraphSignature::Parameter{connection.InputType()._name, connection.OutputParameterName(), ParameterDirection::In}; }

    static ExpressionString ParameterExpression(const NodeGraph& nodeGraph, uint32 nodeId, const NodeGraphSignature::Parameter& signatureParam, NodeGraphSignature& interf, ISignatureProvider& sigProvider)
    {
        auto i = FindConnection(nodeGraph.GetNodeConnections(), nodeId, signatureParam._name);
        if (i!=nodeGraph.GetNodeConnections().cend()) {
            auto e = QueryExpression(nodeGraph, *i, interf, sigProvider);
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

				interf.AddCapturedParameter(paramToAdd);
				auto e = QueryExpression(nodeGraph, *ci, interf, sigProvider);
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

			if (p.second) interf.AddCapturedParameter(paramToAdd);
			else interf.AddParameter(paramToAdd);
            auto e = QueryExpression(nodeGraph, *ti, interf, sigProvider);
			if (!e._expression.empty()) return e;
		}

		// We must add this request as some kind of input to the function (ie, a parameter input or a global input)
		NodeGraphSignature::Parameter param{signatureParam._type, signatureParam._name, ParameterDirection::In, signatureParam._semantic};
		interf.AddParameter(param);
        return ExpressionString{param._name, param._type};
    }

	static std::string UniquifyName(const std::string& name, IteratorRange<const NodeGraphSignature::Parameter*> existing)
    {
		std::string testName = name;
		unsigned suffix = 0;
		for (;;) {
			auto i = std::find_if(existing.begin(), existing.end(), [&testName](const NodeGraphSignature::Parameter&p) { return p._name == testName; } );
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
            NodeGraphSignature& interf,
            ISignatureProvider& sigProvider)
    {
        for (const auto& i:range) {
			auto* destinationNode = graph.GetNode(i.OutputNodeId());
            if (!destinationNode || destinationNode->GetType() == Node::Type::SlotOutput) {
				ExpressionString expression = QueryExpression(graph, i, interf, sigProvider);

				// This is not connected to anything -- so we just have to add it as a
				// unique output from the interface.
				auto param = AsInterfaceParameter(i);
				auto originalName = param._name;
				param._direction = ParameterDirection::Out;
				param._name = UniquifyName(param._name, interf.GetParameters());
				if (param._type.empty() || XlEqStringI(MakeStringSection(param._type), "auto"))
					param._type = expression._type;
				interf.AddParameter(param);
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

    struct ResolvedFunction
    {
    public:
        std::string _name;
        std::string _finalArchiveName;
        const NodeGraphSignature* _signature = nullptr;
    };

    static ResolvedFunction ResolveFunction(
        const std::string& archiveName, 
        const InstantiationParameters& instantiationParameters, 
        ISignatureProvider& sigProvider)
    {
        ResolvedFunction result;

        // Check to see if this function call is using a template parameter. Templated function names look like this:
        //      name '<' restriction '>'
        // Where name is the name of the parameter, and restriction is a function signature we're expecting it to
        // match.
        // If the template parameter has not been assigned to anything, we'll use the restriction as a kind of default.
        auto marker = std::find(archiveName.begin(), archiveName.end(), '<');
        if (marker != archiveName.end()) {
            auto parameterName = MakeStringSection(archiveName.begin(), marker);
            auto restriction = MakeStringSection(marker+1, std::find(archiveName.begin(), archiveName.end(), '>'));

            auto sigProviderResult = sigProvider.FindSignature(restriction);
            result._signature = sigProviderResult._signature;
            if (!result._signature)
                Throw(::Exceptions::BasicLabel("Couldn't find signature for (%s)", restriction.AsString().c_str()));

            auto i = instantiationParameters._parameterBindings.find(parameterName.AsString());
            if (i!=instantiationParameters._parameterBindings.end()) {
                result._name = i->second;
            } else {
                result._name = restriction.AsString();
            }
            result._finalArchiveName = result._name;

            return result;
        }

        auto sigProviderResult = sigProvider.FindSignature(archiveName);
        result._signature = sigProviderResult._signature;
        if (!result._signature)
            Throw(::Exceptions::BasicLabel("Couldn't find signature for (%s)", archiveName.c_str()));
        result._name = sigProviderResult._name;
        result._finalArchiveName = archiveName;
        return result;
    }

    static std::stringstream GenerateFunctionCall(
        NodeGraphSignature& workingInterface,       // this is the interface the output function we're generating. It will be expanded as necessary in this call
        DependencyTable& workingDependencyTable,    // this is the dependency table into which we'll append this function call
        const Node& node, 
        const NodeGraph& nodeGraph,
        const InstantiationParameters& instantiationParameters,
        ISignatureProvider& sigProvider)
    {
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


        auto sigRes = ResolveFunction(node.ArchiveName(), instantiationParameters, sigProvider);
        auto functionName = sigRes._name;
        auto& sig = *sigRes._signature;

            //
            //  There are template parameters in the signature, and if we are passing values
            //  for those parameters, we must select a specific instantiation of the function
            //
        InstantiationParameters callInstantiation;
        for (const auto& tp:sig.GetTemplateParameters()) {
            auto connection = std::find_if(
                nodeGraph.GetConstantConnections().begin(),
                nodeGraph.GetConstantConnections().end(),
                [tp, &node](const ConstantConnection& p) {
                    return p.OutputNodeId() == node.NodeId()
                        && p.OutputParameterName() == tp._name;
                });
            if (connection!=nodeGraph.GetConstantConnections().end()) {
                callInstantiation._parameterBindings.insert({tp._name, connection->Value()});
            }
        }

        std::stringstream result, warnings;

        auto callInstHash = callInstantiation.CalculateHash();
        if (!callInstantiation._parameterBindings.empty()) {
            for (auto& c:callInstantiation._parameterBindings)
                result << "\t// Instantiating " << c.first << " with " << c.second << " in call to " << functionName << std::endl;
            functionName += "_" + std::to_string(callInstHash);
        }

            //      1.  Declare output variable (made unique by node id)
            //      2.  Call the function, assigning the output variable as appropriate
            //          and passing in the parameters (as required)
        std::string returnType;
        for (const auto& i:sig.GetParameters())
            if (i._direction == ParameterDirection::Out) {
                if (i._name == s_resultName) {
                    returnType = i._type;
                    continue;
                }
                result << "\t" << i._type << " " << OutputTemporaryForNode(node.NodeId(), i._name) << ";" << std::endl;
            }

        if (!returnType.empty()) {
            auto outputName = OutputTemporaryForNode(node.NodeId(), s_resultName);
            result << "\t" << returnType << " " << outputName << " = " << functionName << "( ";
        } else {
            result << "\t" << functionName << "( ";
        }

        bool pendingComma = false;
        for (auto p=sig.GetParameters().cbegin(); p!=sig.GetParameters().cend(); ++p) {
            if (p->_direction == ParameterDirection::Out && p->_name == s_resultName)
                continue;

            if (pendingComma) result << ", ";
            pendingComma = true;

                // note -- problem here for in/out parameters
            if (p->_direction == ParameterDirection::Out) {
                result << OutputTemporaryForNode(node.NodeId(), p->_name);
                continue;
            }

            auto expr = ParameterExpression(nodeGraph, node.NodeId(), *p, workingInterface, sigProvider);
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

        // Append the function call to the dependency table
        DependencyTable::Dependency dep { sigRes._finalArchiveName, std::move(callInstantiation) };
        auto existing = std::find_if(
            workingDependencyTable._dependencies.begin(), workingDependencyTable._dependencies.end(),
            [&dep](const DependencyTable::Dependency& d) 
                { return d._archiveName == dep._archiveName && d._parameters.CalculateHash(); });
        if (existing == workingDependencyTable._dependencies.end())
            workingDependencyTable._dependencies.emplace_back(std::move(dep));

        return result;
    }

    static std::tuple<std::string, NodeGraphSignature, DependencyTable> GenerateMainFunctionBody(
        const NodeGraph& graph,
        const InstantiationParameters& instantiationParameters,
        ISignatureProvider& sigProvider)
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

		NodeGraphSignature interf;
        DependencyTable depTable;

        for (auto i=sortedNodes.cbegin(); i!=sortedNodes.cend(); ++i) {
            auto i2 = std::find_if( graph.GetNodes().cbegin(),
                                    graph.GetNodes().cend(), [i](const Node& n) { return n.NodeId() == *i; } );
            if (i2 != graph.GetNodes().cend()) {
                if (i2->GetType() == Node::Type::Procedure) {
                    result << GenerateFunctionCall(interf, depTable, *i2, graph, instantiationParameters, sigProvider).str();
                }
            }
        }

        for (const auto& dep:depTable._dependencies)
            result << "\t//Dependency: " << dep._archiveName << " inst hash: " << dep._parameters.CalculateHash() << std::endl;

		FillDirectOutputParameters(result, graph, graph.GetNodeConnections(), interf, sigProvider);
        FillDirectOutputParameters(result, graph, graph.GetConstantConnections(), interf, sigProvider);
        FillDirectOutputParameters(result, graph, graph.GetInputParameterConnections(), interf, sigProvider);

        return std::make_tuple(result.str(), std::move(interf), std::move(depTable));
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

    static void AddWithExistingCheck(
        std::vector<NodeGraphSignature::Parameter>& dst,
        const NodeGraphSignature::Parameter& param)
    {
	    // Look for another parameter with the same name...
	    auto existing = std::find_if(dst.begin(), dst.end(),
		    [&param](const NodeGraphSignature::Parameter& p) { return p._name == param._name; });
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

	void NodeGraphSignature::AddParameter(const Parameter& param) { AddWithExistingCheck(_functionParameters, param); }
	void NodeGraphSignature::AddCapturedParameter(const Parameter& param) { AddWithExistingCheck(_capturedParameters, param); }
    void NodeGraphSignature::AddTemplateParameter(const TemplateParameter& param) { return _templateParameters.push_back(param); }

    NodeGraphSignature::NodeGraphSignature() {}
    NodeGraphSignature::~NodeGraphSignature() {}

    static void MaybeComma(std::stringstream& stream) { if (stream.tellp() != std::stringstream::pos_type(0)) stream << ", "; }

    static std::string GenerateSignature(const NodeGraphSignature& sig, const char name[], bool useReturnType = true)
	{
        std::string returnType, returnSemantic;

		std::stringstream mainFunctionDeclParameters;
		for (const auto& i:sig.GetParameters()) {
            if (useReturnType && i._name == s_resultName && i._direction == ParameterDirection::Out) {
                assert(returnType.empty() && returnSemantic.empty());
                returnType = i._type;
                returnSemantic = i._semantic;
                continue;
            }

            MaybeComma(mainFunctionDeclParameters);
			if (i._direction == ParameterDirection::Out) {
				mainFunctionDeclParameters << "out ";
			}
            mainFunctionDeclParameters << i._type << " " << i._name;
			if (!i._semantic.empty())
				mainFunctionDeclParameters << " : " << i._semantic;
        }

        std::stringstream result;
		if (!returnType.empty()) result << returnType << " ";
		else result << "void ";
        result << name << "(" << mainFunctionDeclParameters.str() << ")";
		if (!returnSemantic.empty())
			result << " : " << returnSemantic;
		return result.str();
	}

    GeneratedFunction GenerateFunction(
        const NodeGraph& graph, const char name[],
        const InstantiationParameters& instantiationParameters,
        ISignatureProvider& sigProvider)
    {
		std::string mainBody;
		NodeGraphSignature interf;
        DependencyTable depTable;
		std::tie(mainBody, interf, depTable) = GenerateMainFunctionBody(graph, instantiationParameters, sigProvider);

			//
            //      Our graph function is always a "void" function, and all of the output
            //      parameters are just function parameters with the "out" keyword. This is
            //      convenient for writing out generated functions
            //      We don't want to put the "node id" in the name -- because node ids can
            //      change from time to time, and that would invalidate any other shaders calling
            //      this function. But ideally we need some way to guarantee uniqueness.
            //
        std::stringstream result;     
        result << GenerateSignature(interf, name, false) << std::endl;
        result << "{" << std::endl;
		result << mainBody;
        result << "}" << std::endl;

        return GeneratedFunction{result.str(), std::move(interf), std::move(depTable)};
    }

	std::string GenerateMaterialCBuffer(const NodeGraphSignature& interf)
	{
		std::stringstream result;

		    //
            //      We need to write the global parameters as part of the shader body.
            //      Global resources (like textures) appear first. But we put all shader
            //      constants together in a single cbuffer called "BasicMaterialConstants"
            //
        bool hasMaterialConstants = false;
        for(auto i:interf.GetCapturedParameters()) {
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
            for (const auto& i:interf.GetCapturedParameters())
                if (CanBeStoredInCBuffer(MakeStringSection(i._type)))
                    result << "\t" << i._type << " " << i._name << ";" << std::endl;
            result << "}" << std::endl;
        }
		result << std::endl;

		return result.str();
	}

	std::string GenerateScaffoldFunction(const NodeGraphSignature& slotSignature, const NodeGraphSignature& generatedFunctionSignature, const char name[])
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

        result << "/////// Scaffold function for: " << name << " ///////" << std::endl;
		result << GenerateSignature(slotSignature, name) << std::endl;
		result << "{" << std::endl;

		// make temporaries for all outputs
		for (auto& p:generatedFunctionSignature.GetParameters()) {
			if (p._direction == ParameterDirection::Out) {
				result << "\t" << p._type << " temp_" << p._name << ";" << std::endl;
			}
		}

		std::stringstream paramStream;
		for (const auto& p:generatedFunctionSignature.GetParameters()) {
            MaybeComma(paramStream);

			if (p._direction == ParameterDirection::Out) {
                paramStream << "temp_" << p._name;
				continue;
			}

			// How do we pass a value to this parameter?
			// first, check to see if this parameter is in "slotSignature"
			auto i = std::find_if(slotSignature.GetParameters().begin(), slotSignature.GetParameters().end(),
				[&p](const NodeGraphSignature::Parameter& test) -> bool
				{
					if (test._direction != ParameterDirection::In) return false;
					return test._name == p._name;
				});
			if (i != slotSignature.GetParameters().end()) {
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

		result << "\t" << name << "(" << paramStream.str() << ");" << std::endl;

		// Map the output parameters to their final destination.
        std::string returnType, returnSemantic;
		for (const auto&p:slotSignature.GetParameters()) {
			if (p._direction != ParameterDirection::Out)
                continue;
            
            if (p._name == s_resultName) {
                assert(returnType.empty() && returnSemantic.empty());
                returnType = p._type;
                returnSemantic = p._semantic;
				continue;
            }

			result << "\t" << p._name << " = ";

			// First, look for an output from the generated function
			auto i = std::find_if(generatedFunctionSignature.GetParameters().begin(), generatedFunctionSignature.GetParameters().end(),
				[&p](const NodeGraphSignature::Parameter& test) -> bool
				{
					if (test._direction != ParameterDirection::Out) return false;
					return test._name == p._name;
				});
			if (i != generatedFunctionSignature.GetParameters().end()) {
				WriteCastExpression(result, {std::string("temp_") + i->_name, i->_type}, p._type);
				result << ";" << std::endl;
				continue;
			}

			// Second, just use a default value
			result << "DefaultValue_" << p._type << "();" << std::endl;
		}

		if (!returnType.empty()) {
			result << "\treturn ";

				// First, look for an output from the generated function
			auto i = std::find_if(generatedFunctionSignature.GetParameters().begin(), generatedFunctionSignature.GetParameters().end(),
				[](const NodeGraphSignature::Parameter& test) -> bool
				{ return (test._direction == ParameterDirection::Out) && test._name == s_resultName; });
			if (i != generatedFunctionSignature.GetParameters().end()) {
				WriteCastExpression(result, {std::string("temp_") + i->_name, i->_type}, returnType);
			} else {
				// Second, just use a default value
				result << "DefaultValue_" << returnType << "()";
			}
			result << ";" << std::endl;
		}

		result << "}" << std::endl;

		return result.str();
	}


    uint64_t InstantiationParameters::CalculateHash() const
    {
        if (_parameterBindings.empty()) return 0;
        uint64 result = DefaultSeed64;
        for (const auto&p:_parameterBindings)
            result = HashCombine(result, HashCombine(Hash64(p.first), Hash64(p.second)));
        return result;
    }

    ISignatureProvider::~ISignatureProvider() {}
}
