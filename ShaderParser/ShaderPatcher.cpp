// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderPatcher.h"
#include "ShaderPatcher_Internal.h"
#include "NodeGraphProvider.h"
#include "../RenderCore/ShaderLangUtil.h"
#include "../Assets/DepVal.h"
#include "../Assets/Assets.h"
#include "../Core/Exceptions.h"
#include "../ConsoleRig/Log.h"
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

    const std::string s_resultName = "result";
	std::string ParameterName_NodeInstantiation = "<instantiation>";
    static const NodeId s_nodeId_Invalid = ~0u;

        ///////////////////////////////////////////////////////////////

    NodeGraph::NodeGraph() {}
    NodeGraph::~NodeGraph() {}

    void NodeGraph::Add(Node&& a) { _nodes.emplace_back(std::move(a)); }
    void NodeGraph::Add(Connection&& a) { _connections.emplace_back(std::move(a)); }

    bool NodeGraph::IsUpstream(NodeId startNode, NodeId searchingForNode)
    {
            //  Starting at 'startNode', search upstream and see if we find 'searchingForNode'
        if (startNode == searchingForNode) {
            return true;
        }

        for (auto i=_connections.cbegin(); i!=_connections.cend(); ++i) {
            if (i->OutputNodeId() == startNode) {
				auto inputNode = i->InputNodeId();
                if (inputNode != NodeId_Interface && inputNode != NodeId_Constant && IsUpstream(i->InputNodeId(), searchingForNode)) {
                    return true;
                }
            }
        }

        return false;
    }

    bool NodeGraph::IsDownstream(
        NodeId startNode,
        const NodeId* searchingForNodesStart, const NodeId* searchingForNodesEnd)
    {
        if (std::find(searchingForNodesStart, searchingForNodesEnd, startNode) != searchingForNodesEnd) {
            return true;
        }

        for (auto i=_connections.cbegin(); i!=_connections.cend(); ++i) {
            if (i->InputNodeId() == startNode) {
				auto outputNode = i->OutputNodeId();
                if (outputNode != NodeId_Interface && outputNode != NodeId_Constant && IsDownstream(outputNode, searchingForNodesStart, searchingForNodesEnd)) {
                    return true;
                }
            }
        }

        return false;
    }

    bool            NodeGraph::HasNode(NodeId nodeId)
    {
		// Special case node ids are considered to always exist (particularly required when called from Trim())
		if (nodeId == NodeId_Interface || nodeId == NodeId_Constant) return true;
        return std::find_if(_nodes.begin(), _nodes.end(),
            [=](const Node& node) { return node.NodeId() == nodeId; }) != _nodes.end();
    }

    const Node*     NodeGraph::GetNode(NodeId nodeId) const
    {
        auto res = std::find_if(
            _nodes.cbegin(), _nodes.cend(),
            [=](const Node& n) { return n.NodeId() == nodeId; });
        if (res != _nodes.cend()) {
            return &*res;
        }
        return nullptr;
    }

    void NodeGraph::Trim(NodeId previewNode)
    {
        Trim(&previewNode, &previewNode+1);
    }

    void NodeGraph::Trim(const NodeId* trimNodesBegin, const NodeId* trimNodesEnd)
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
                [=](const Node& node) { return !IsDownstream(node.NodeId(), trimNodesBegin, trimNodesEnd); }),
            _nodes.end());

        _connections.erase(
            std::remove_if(
                _connections.begin(), _connections.end(),
                [=](const Connection& connection)
                    { return !HasNode(connection.InputNodeId()) || !HasNode(connection.OutputNodeId()) || connection.OutputNodeId() == NodeId_Interface; }),
            _connections.end());
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	static void OrderNodes(IteratorRange<NodeId*> range)
	{
		// We need to sort the upstreams in some way that maintains a
		// consistant ordering. The simplied way is just to use node id.
		// However we may sometimes want to set priorities for nodes.
		// For example, nodes that can "discard" pixels should be priortized
		// higher.
		std::sort(range.begin(), range.end());
	}

    static bool SortNodesFunction(
        NodeId                  node,
        std::vector<NodeId>&    presorted,
        std::vector<NodeId>&    sorted,
        std::vector<NodeId>&    marks,
        const NodeGraph&        graph)
    {
        if (std::find(presorted.begin(), presorted.end(), node) == presorted.end()) {
            return false;   // hit a cycle
        }
        if (std::find(marks.begin(), marks.end(), node) != marks.end()) {
            return false;   // hit a cycle
        }

        marks.push_back(node);

		std::vector<NodeId> upstream;
		upstream.reserve(graph.GetConnections().size());
        for (const auto& i:graph.GetConnections())
            if (i.OutputNodeId() == node)
				upstream.push_back(i.InputNodeId());

		OrderNodes(MakeIteratorRange(upstream));
		for (const auto& i2:upstream)
			SortNodesFunction(i2, presorted, sorted, marks, graph);

        sorted.push_back(node);
        presorted.erase(std::find(presorted.begin(), presorted.end(), node));
        return true;
    }

    static std::string AsString(uint32_t i)
    {
        char buffer[128];
        XlI64toA(i, buffer, dimof(buffer), 10);
        return buffer;
    }

	static std::string SantizeIdentifier(const std::string& input)
	{
		std::string result;
		result.reserve(input.size());
		for (auto c:input)
			if ((c >= '0' && c <= '9') || c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
				result.push_back(c);
			} else
				result.push_back('_');
		return result;
	}

    static std::string OutputTemporaryForNode(NodeId nodeId, const std::string& outputName)
    {
        return std::string("Output_") + AsString(nodeId) + "_" + SantizeIdentifier(outputName);
    }

    template<typename Connection>
        const Connection* FindConnectionThatOutputsTo(
            IteratorRange<const Connection*> connections,
            NodeId nodeId, const std::string& parameterName)
    {
        return std::find_if(
            connections.cbegin(), connections.cend(),
            [nodeId, &parameterName](const Connection& connection)
            { return    connection.OutputNodeId() == nodeId &&
                        connection.OutputParameterName() == parameterName; } );
    }

    struct ExpressionString { std::string _expression, _type; };

    static std::string WriteCastExpression(std::stringstream& result, const ExpressionString& expression, const std::string& dstType)
    {
        if (!expression._type.empty() && !dstType.empty() && expression._type != dstType && !XlEqStringI(dstType, "auto") && !XlEqStringI(expression._type, "auto")) {
            result << "Cast_" << expression._type << "_to_" << dstType << "(" << expression._expression << ")";
			return dstType;
        } else {
            result << expression._expression;
			return expression._type;
		}
    }

    static std::string TypeFromShaderFragment(
        StringSection<> archiveName, StringSection<> paramName, ParameterDirection direction,
        INodeGraphProvider& sigProvider)
    {
            // Go back to the shader fragments to find the current type for the given parameter
		std::optional<INodeGraphProvider::Signature> sigResult;

		// Some typenames contain a template type in angle brackets. For example, when 
		// graphs are passed in as parameters, they are given a template signature, in the form:
		//		parameterName<signatureTemplate>
		auto typenameBegin = std::find(archiveName.begin(), archiveName.end(), '<');
		if (typenameBegin != archiveName.end()) {
			auto typenameEnd = std::find(typenameBegin+1, archiveName.end(), '>');
			sigResult = sigProvider.FindSignature(MakeStringSection(typenameBegin+1, typenameEnd));
		} else {
			sigResult = sigProvider.FindSignature(archiveName);
		}
        if (!sigResult)
            return std::string();
            // Throw(::Exceptions::BasicLabel("Couldn't find signature for (%s)", archiveName.AsString().c_str()));

        const auto& sig = sigResult.value()._signature;

            // find a parameter with the right direction & name
        for (const auto& p:sig.GetParameters())
            if (p._direction == direction && XlEqString(paramName, p._name))
                return p._type;

        return std::string();
    }

	class GraphInterfaceContext
	{
	public:
		IteratorRange<const NodeGraphSignature::Parameter*>		_predefinedParameters;
		std::vector<NodeGraphSignature::Parameter>				_dangingParameters;		// these are input/outputs from nodes in the graph that aren't connected to anything
		std::vector<NodeGraphSignature::Parameter>				_additionalParameters;	// these are interface parameters referenced in the graph that don't match anything in the fixed interface

		std::vector<std::pair<std::string, NodeGraphSignature::Parameter>>	_curriedParameters;
	};

	static bool HasParameterWithName(const std::string& name, IteratorRange<const NodeGraphSignature::Parameter*> params)
	{
		auto i = std::find_if(params.begin(), params.end(), [&name](const NodeGraphSignature::Parameter&p) { return p._name == name; } );
		return i != params.end();
	}

	static std::string UniquifyName(const std::string& name, const GraphInterfaceContext& interfContext)
    {
		std::string testName = name;
		unsigned suffix = 0;
		for (;;) {
			if (	!HasParameterWithName(testName, MakeIteratorRange(interfContext._additionalParameters))
				&&	!HasParameterWithName(testName, MakeIteratorRange(interfContext._dangingParameters))) {
				return testName;
			} else {
				std::stringstream str;
				str << name << suffix;
				testName = str.str();
				++suffix;
			}
		}
    }

	static std::string RemoveTemplateRestrictions(const std::string& input)
	{
		static std::regex filter(R"--((\w*)<.*>)--");
		std::smatch matchResult;
        if (std::regex_match(input, matchResult, filter) && matchResult.size() > 1)
			return matchResult[1];
		return input;
	}

	static std::string TypeOfConstant(StringSection<> constantValue)
	{
		// todo --	we can get the constant type with the implied typing API, but we then need
		//			a way to convert that into a string typename. Different languages will use
		//			different names for basic types, and the rest of the code doesn't currently
		//			make many assumptions about what kinds of type names are used.
		// auto type = ImpliedTyping::Parse(constantValue);
		return {};
	}

    static ExpressionString QueryExpression(const NodeGraph& nodeGraph, const Connection& connection, const std::string& expectedType, GraphInterfaceContext& interfContext, INodeGraphProvider& sigProvider)
    {
		std::stringstream str;
		std::string finalType;
		if (connection.InputNodeId() == NodeId_Constant) {

				//  we have a "constant connection" value here
			auto value = connection.InputParameterName();
			auto type = TypeOfConstant(MakeStringSection(value));
			finalType = WriteCastExpression(str, {value, type}, expectedType);

		} else if (connection.InputNodeId() == NodeId_Interface) {

			auto existing = std::find_if(
				interfContext._additionalParameters.begin(), interfContext._additionalParameters.end(),
				[&connection](const NodeGraphSignature::Parameter& param) { return param._direction == ParameterDirection::In && XlEqString(MakeStringSection(connection.InputParameterName()), param._name); });
			if (existing == interfContext._additionalParameters.end()) {
				auto p = std::find_if(
					interfContext._predefinedParameters.begin(), interfContext._predefinedParameters.end(),
					[&connection](const NodeGraphSignature::Parameter& param) { return param._direction == ParameterDirection::In && XlEqString(MakeStringSection(connection.InputParameterName()), param._name); });
				if (p != interfContext._predefinedParameters.end()) {
					NodeGraphSignature::Parameter newParam = *p;
					if (newParam._type.empty() || XlEqString(newParam._type, "auto"))
						newParam._type = expectedType;
					existing = interfContext._additionalParameters.insert(interfContext._additionalParameters.end(), newParam);
				} else {
					existing = interfContext._additionalParameters.insert(interfContext._additionalParameters.end(), {expectedType, connection.InputParameterName()});
				}
			}

			finalType = WriteCastExpression(str, {existing->_name, existing->_type}, expectedType);

		} else {

			auto n = nodeGraph.GetNode(connection.InputNodeId());
			if (n) {
				auto type = TypeFromShaderFragment(n->ArchiveName(), connection.InputParameterName(), ParameterDirection::Out, sigProvider);
				finalType = WriteCastExpression(str, {OutputTemporaryForNode(connection.InputNodeId(), connection.InputParameterName()), type}, expectedType);
			} else {
				// str << "// ERROR: could not find node for parameter expression. Looking for node id (" << connection.InputNodeId() << ") and input parameter (" << connection.InputParameterName() << ")" << std::endl;
				str << "DefaultValue_" << expectedType << "()";
				finalType = expectedType;
			}

		}

		return {str.str(), finalType};
    }

    static ExpressionString ParameterExpression(
		const NodeGraph& nodeGraph, NodeId nodeId, const NodeGraphSignature::Parameter& signatureParam,
		GraphInterfaceContext& interfContext, 
		bool generateDanglingInputs,
		INodeGraphProvider& sigProvider)
    {
		auto expectedType = signatureParam._type;
        auto i = FindConnectionThatOutputsTo(nodeGraph.GetConnections(), nodeId, signatureParam._name);
        if (i!=nodeGraph.GetConnections().cend()) {
            return QueryExpression(nodeGraph, *i, expectedType, interfContext, sigProvider);
		}

		// We must add this request as some kind of input to the function (ie, a parameter input or a global input)
		// auto uniqueName = UniquifyName(signatureParam._name, interfContext);
		if (generateDanglingInputs) {
			auto uniqueName = "in_" + std::to_string(nodeId) + "_" + signatureParam._name;
			interfContext._dangingParameters.push_back({expectedType, uniqueName, ParameterDirection::In, signatureParam._semantic});
			return {uniqueName, expectedType};
		} else {
			return {"DefaultValue_" + expectedType + "()", expectedType};
		}
    }

    template<typename Connection>
        static void FillDirectOutputParameters(
            std::stringstream& result,
            const NodeGraph& graph,
            IteratorRange<const Connection*> range,
            GraphInterfaceContext& interfContext,
            INodeGraphProvider& sigProvider)
    {
        for (const auto& connection:range) {
			auto* destinationNode = graph.GetNode(connection.OutputNodeId());
            if (!destinationNode) {
				// This is not connected to anything -- so we just have to add it as a
				// unique output from the interface.
				//
				// If the parameter is already in the interface, then use that -- otherwise we
				// must create an "additional parameter" for it

				std::string inputType;
				auto* srcNode = graph.GetNode(connection.InputNodeId());
				if (srcNode)
					inputType = TypeFromShaderFragment(srcNode->ArchiveName(), connection.InputParameterName(), ParameterDirection::Out, sigProvider);

				auto p = std::find_if(
					interfContext._additionalParameters.begin(), interfContext._additionalParameters.end(),
					[&connection](const NodeGraphSignature::Parameter& param) { return param._direction == ParameterDirection::Out && XlEqString(MakeStringSection(connection.OutputParameterName()), param._name); });
				if (p == interfContext._additionalParameters.end()) {
					auto predefined = std::find_if(
						interfContext._predefinedParameters.begin(), interfContext._predefinedParameters.end(),
						[&connection](const NodeGraphSignature::Parameter& param) { return param._direction == ParameterDirection::Out && XlEqString(MakeStringSection(connection.OutputParameterName()), param._name); });
					if (predefined != interfContext._predefinedParameters.end()) {
						NodeGraphSignature::Parameter newParam = *p;
						if (newParam._type.empty() || XlEqString(newParam._type, "auto"))
							newParam._type = inputType;
						p = interfContext._additionalParameters.insert(interfContext._additionalParameters.end(), newParam);
					} else {
						p = interfContext._additionalParameters.insert(interfContext._additionalParameters.end(), {inputType, connection.OutputParameterName(), ParameterDirection::Out});
					}
				}

				result << "\t" << p->_name << " = " << QueryExpression(graph, connection, p->_type, interfContext, sigProvider)._expression << ";" << std::endl;
			}
        }
    }

    struct ResolvedFunction
    {
    public:
        std::string _name;
        std::string _finalArchiveName;
        NodeGraphSignature _signature;
		InstantiationParameters _instantiationParameters;
		bool _isGraphSyntaxFile;
    };

    static ResolvedFunction ResolveFunction(
        const std::string& archiveName, 
        const InstantiationParameters& instantiationParameters, 
        INodeGraphProvider& sigProvider)
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

            auto i = instantiationParameters._parameterBindings.find(parameterName.AsString());
            if (i!=instantiationParameters._parameterBindings.end()) {
				result._finalArchiveName = i->second._archiveName;
				result._instantiationParameters = i->second._parameters;
            } else {
				result._finalArchiveName = restriction.AsString();
            }

			auto sigProviderResult = sigProvider.FindSignature(result._finalArchiveName);
			if (!sigProviderResult)
				sigProviderResult = sigProvider.FindSignature(restriction);
            if (!sigProviderResult)
                Throw(::Exceptions::BasicLabel("Couldn't find signature for (%s)", restriction.AsString().c_str()));
			result._signature = sigProviderResult.value()._signature;

			auto p = result._finalArchiveName.find_last_of(':');
			if (p != std::string::npos) {
				result._name = result._finalArchiveName.substr(p+1);
			} else
				result._name = result._finalArchiveName;

			result._isGraphSyntaxFile = sigProviderResult.value()._isGraphSyntax;

            return result;
        }

        auto sigProviderResult = sigProvider.FindSignature(archiveName);
        if (!sigProviderResult)
            Throw(::Exceptions::BasicLabel("Couldn't find signature for (%s)", archiveName.c_str()));
		result._signature = sigProviderResult.value()._signature;        
        result._name = sigProviderResult.value()._name;
        // result._finalArchiveName = sigProviderResult.value()._sourceFile.empty() ? archiveName : sigProviderResult.value()._sourceFile + ":" + result._name;
		result._finalArchiveName = archiveName;
		result._isGraphSyntaxFile = sigProviderResult.value()._isGraphSyntax;
        return result;
    }

    static std::pair<std::stringstream, ResolvedFunction> GenerateFunctionCall(
        GraphInterfaceContext& interfContext,
        DependencyTable& workingDependencyTable,    // this is the dependency table into which we'll append this function call
        const Node& node, 
        const NodeGraph& nodeGraph,
        const InstantiationParameters& instantiationParameters,
        INodeGraphProvider& sigProvider)
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
        auto& sig = sigRes._signature;

            //
            //  There are template parameters in the signature, and if we are passing values
            //  for those parameters, we must select a specific instantiation of the function
            //
        InstantiationParameters callInstantiation;
		callInstantiation._parameterBindings.insert(
			sigRes._instantiationParameters._parameterBindings.begin(),
			sigRes._instantiationParameters._parameterBindings.end());
		// the node graph can override any instantiation parameters
        for (const auto& tp:sig.GetTemplateParameters()) {
            auto connection = std::find_if(
                nodeGraph.GetConnections().begin(),
                nodeGraph.GetConnections().end(),
                [tp, &node](const Connection& p) {
                    return p.OutputNodeId() == node.NodeId()
                        && p.OutputParameterName() == tp._name;
                });
            if (connection!=nodeGraph.GetConnections().end() && XlEqString(MakeStringSection(connection->InputParameterName()), ParameterName_NodeInstantiation)) {
				// this connection must be used as a template parameter
				// The connected node is called an "instantiation" node -- it represents an instantiation of some function
				// There can be values attached as inputs to the instantiation node; they act like curried parameters.
				auto* instantiationNode = nodeGraph.GetNode(connection->InputNodeId());
				assert(instantiationNode);
				auto param = InstantiationParameters::Dependency { instantiationNode->ArchiveName(), {} };

				// Any input parameters to this node that aren't part of the restriction signature should become curried
				auto restrictionSignature = sigProvider.FindSignature(tp._restriction);
				for (const auto&c:nodeGraph.GetConnections()) {
					if (c.OutputNodeId() == instantiationNode->NodeId()) {
						auto paramName = c.OutputParameterName();
						bool isPartOfRestriction = false;
						if (restrictionSignature) {
							auto i = std::find_if(
								restrictionSignature.value()._signature.GetParameters().begin(),
								restrictionSignature.value()._signature.GetParameters().end(),
								[paramName](const NodeGraphSignature::Parameter&param) {
									return XlEqString(MakeStringSection(param._semantic), paramName);
								});
							isPartOfRestriction = i != restrictionSignature.value()._signature.GetParameters().end();
						}
						if (!isPartOfRestriction) {
							param._parametersToCurry.push_back(paramName);
						}
					}
				}

				callInstantiation._parameterBindings.insert({tp._name, param});
            }
        }

        std::stringstream result, warnings;

        auto callInstHash = callInstantiation.CalculateHash();
        if (!callInstantiation._parameterBindings.empty()) {
            for (const auto& c:callInstantiation._parameterBindings) {
                result << "\t// Instantiating " << functionName << " with " << c.first << " set to " << c.second._archiveName << std::endl;
				for (const auto& p:c.second._parametersToCurry)
					result << "\t//     Curried parameter: " << p << std::endl;
			}
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

		auto n = RemoveTemplateRestrictions(node.ArchiveName());
		auto parameterBindingsForThisNode = std::find_if(
			instantiationParameters._parameterBindings.begin(), instantiationParameters._parameterBindings.end(),
			[n](const std::pair<std::string, InstantiationParameters::Dependency>&p) {
				return XlEqString(MakeStringSection(p.first), n);
			});

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

			if (p->_direction == ParameterDirection::In) {
				// Check if this parameter is marked to be "curried" by a template instantiation
				// When this happens, the value must be passed through the interface from the caller
				if (parameterBindingsForThisNode != instantiationParameters._parameterBindings.end()) {
					auto i = std::find_if(
						parameterBindingsForThisNode->second._parametersToCurry.begin(),
						parameterBindingsForThisNode->second._parametersToCurry.end(),
						[p](const std::string& str) { return XlEqString(MakeStringSection(str), p->_name);});
					if (i != parameterBindingsForThisNode->second._parametersToCurry.end()) {
						interfContext._curriedParameters.push_back({n, *p});
						result << "curried_" << n << "_" << p->_name;
						continue;
					}
				}
			}

            result << ParameterExpression(nodeGraph, node.NodeId(), *p, interfContext, instantiationParameters._generateDanglingInputs, sigProvider)._expression;
        }

		// If the call instantiation itself has curried parameters, they won't appear in the 
		// signature returned from ResolveFunction. We must append their values here
		for (const auto& tp:callInstantiation._parameterBindings) {
			if (tp.second._parametersToCurry.empty()) continue;

			// First, find the instantiation node
			auto connection = std::find_if(
				nodeGraph.GetConnections().begin(),
				nodeGraph.GetConnections().end(),
				[&tp, &node](const Connection& p) {
					return p.OutputNodeId() == node.NodeId()
						&& p.OutputParameterName() == tp.first;
				});
			if (connection!=nodeGraph.GetConnections().end() && XlEqString(MakeStringSection(connection->InputParameterName()), ParameterName_NodeInstantiation)) {
				auto* instantiationNode = nodeGraph.GetNode(connection->InputNodeId());
				assert(instantiationNode);

				// Now generate the syntax as if we're passing the parameter into the instantiation node
				for (const auto& c:tp.second._parametersToCurry) {
					if (pendingComma) result << ", ";
					pendingComma = true;

					NodeGraphSignature::Parameter param{"auto", c};
					result << ParameterExpression(nodeGraph, instantiationNode->NodeId(), param, interfContext, instantiationParameters._generateDanglingInputs, sigProvider)._expression;
				}
			}
		}

        result << " );" << std::endl;
        if (warnings.tellp()) {
            result << "\t// Warnings in function call: " << std::endl;
            result << warnings.str();
        }

        // Append the function call to the dependency table
        DependencyTable::Dependency dep { sigRes._finalArchiveName, std::move(callInstantiation), sigRes._isGraphSyntaxFile };
		auto depHash = dep._parameters.CalculateHash();
        auto existing = std::find_if(
            workingDependencyTable._dependencies.begin(), workingDependencyTable._dependencies.end(),
            [&dep, depHash](const DependencyTable::Dependency& d) 
                { return d._archiveName == dep._archiveName && d._parameters.CalculateHash() == depHash; });
        if (existing == workingDependencyTable._dependencies.end())
            workingDependencyTable._dependencies.emplace_back(std::move(dep));

        return std::make_pair(std::move(result), std::move(sigRes));
    }

	static bool HasConnectionStartingAt(const NodeGraph& nodeGraph, NodeId inputNodeId, StringSection<> parameterName)
	{
		for (const auto&c:nodeGraph.GetConnections())
			if (c.InputNodeId() == inputNodeId && XlEqString(parameterName, c.InputParameterName()))
				return true;
		return false;
	}

	static std::vector<NodeId> SortNodes(const NodeGraph& graph, bool& isAcyclic)
	{
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

        std::vector<NodeId> presortedNodes, sortedNodes;
        sortedNodes.reserve(graph.GetNodes().size());

        for (const auto& i:graph.GetNodes())
            presortedNodes.push_back(i.NodeId());

		OrderNodes(MakeIteratorRange(presortedNodes));

        isAcyclic = true;
		while (!presortedNodes.empty()) {
            std::vector<NodeId> temporaryMarks;
            bool sortReturn = SortNodesFunction(
                presortedNodes[0],
                presortedNodes, sortedNodes,
                temporaryMarks, graph);

            if (!sortReturn) {
                isAcyclic = false;
                break;
            }
        }

		return sortedNodes;
	}

    static std::tuple<std::string, NodeGraphSignature, DependencyTable> GenerateMainFunctionBody(
        const NodeGraph& graph,
		IteratorRange<const NodeGraphSignature::Parameter*> predefinedParameters,
        const InstantiationParameters& instantiationParameters,
        INodeGraphProvider& sigProvider)
    {
        std::stringstream result;

		bool acyclic = false;
		auto sortedNodes = SortNodes(graph, acyclic);

            //
            //      Now the function calls can be ordered by walking through the
            //      directed graph.
            //

        if (!acyclic) {
            result << "// Warning! found a cycle in the graph of nodes. Result will be incomplete!" << std::endl;
        }

        DependencyTable depTable;
		GraphInterfaceContext interfContext;
		interfContext._predefinedParameters = predefinedParameters;
		std::stringstream dangingOutBlock;

        for (auto i=sortedNodes.cbegin(); i!=sortedNodes.cend(); ++i) {
            auto i2 = std::find_if( graph.GetNodes().cbegin(),
                                    graph.GetNodes().cend(), [i](const Node& n) { return n.NodeId() == *i; } );
            if (i2 != graph.GetNodes().cend()) {
                if (i2->GetType() == Node::Type::Procedure) {
					auto fnCall = GenerateFunctionCall(interfContext, depTable, *i2, graph, instantiationParameters, sigProvider);
                    result << fnCall.first.str();

					// Look for "dangling outputs". These are outputs that have been generated by GenerateFunctionCall, but
					// are not attached to any other nodes as inputs. If the flag is set, these will become part of the 
					// function interface (and can be used for preview shaders, etc)
					if (i2->NodeId() == instantiationParameters._generateDanglingOutputs) {
						const auto& fnSig = fnCall.second._signature;
						for (const auto& p:fnSig.GetParameters()) {
							if (p._direction == ParameterDirection::Out) {
								if (!HasConnectionStartingAt(graph, i2->NodeId(), p._name)) {
									auto uniqueName = UniquifyName(p._name, interfContext);
									interfContext._dangingParameters.push_back({p._type, uniqueName, ParameterDirection::Out});
									dangingOutBlock << "\t" << uniqueName << " = " << OutputTemporaryForNode(i2->NodeId(), p._name) << ";" << std::endl;
								}
							}
						}
					}
                }
            }
        }

        for (const auto& dep:depTable._dependencies)
            result << "\t//Dependency: " << dep._archiveName << " inst hash: " << dep._parameters.CalculateHash() << std::endl;

		FillDirectOutputParameters(result, graph, graph.GetConnections(), interfContext, sigProvider);

		// todo -- any outputs in the fixed interface that we didn't write to in FillDirectOutputParameters should get default valuess

		result << dangingOutBlock.str();

		// Generate the final interface based on the fixed interface, any additional / dangling parameters we added
		NodeGraphSignature finalInterface;
		for (auto& p:interfContext._additionalParameters) finalInterface.AddParameter(p);
		for (auto& p:interfContext._dangingParameters) finalInterface.AddParameter(p);
		for (auto& p:interfContext._curriedParameters) {
			auto param = p.second;
			param._name = "curried_" + p.first + "_" + p.second._name;
			finalInterface.AddParameter(param);
		}

        return std::make_tuple(result.str(), std::move(finalInterface), std::move(depTable));
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
		    [&param](const NodeGraphSignature::Parameter& p) { return p._name == param._name && p._direction == param._direction; });
	    if (existing != dst.end()) {
		    // If we have 2 parameters with the same name, we're going to expect they
		    // also have the same type and semantic (otherwise we would need to adjust
		    // the name to avoid conflicts).
		    if (existing->_type != param._type || existing->_semantic != param._semantic) {
			    // Throw(::Exceptions::BasicLabel("Main function parameters with the same name, but different types/semantics (%s)", param._name.c_str()));
				Log(Debug) << "Main function parameters with the same name, but different types/semantics (" << param._name << ")" << std::endl;
			}
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

    static std::string GenerateSignature(const NodeGraphSignature& sig, StringSection<char> name, bool useReturnType = true, bool includeTemplateParameters = false)
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

			//
            //      Our graph function is always a "void" function, and all of the output
            //      parameters are just function parameters with the "out" keyword. This is
            //      convenient for writing out generated functions
            //      We don't want to put the "node id" in the name -- because node ids can
            MaybeComma(mainFunctionDeclParameters);
			if (i._direction == ParameterDirection::Out) {
				mainFunctionDeclParameters << "out ";
			}
            mainFunctionDeclParameters << i._type << " " << i._name;
			if (!i._semantic.empty())
				mainFunctionDeclParameters << " : " << i._semantic;
        }

		if (includeTemplateParameters) {
			for (const auto& i:sig.GetTemplateParameters()) {
				MaybeComma(mainFunctionDeclParameters);
				mainFunctionDeclParameters << "graph<" << i._restriction << "> " << i._name;
			}
		}

        std::stringstream result;
		if (!returnType.empty()) result << returnType << " ";
		else result << "void ";
        result << name << "(" << mainFunctionDeclParameters.str() << ")";
		if (!returnSemantic.empty())
			result << " : " << returnSemantic;

		if (includeTemplateParameters && !sig.GetImplements().empty())
			result << " implements " << sig.GetImplements();

		return result.str();
	}

    GeneratedFunction GenerateFunction(
        const NodeGraph& graph, StringSection<char> name,
        const InstantiationParameters& instantiationParameters,
        INodeGraphProvider& sigProvider)
    {
		std::string mainBody;
		NodeGraphSignature interf;
        DependencyTable depTable;
		std::tie(mainBody, interf, depTable) = GenerateMainFunctionBody(graph, {}, instantiationParameters, sigProvider);

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

	std::string GenerateScaffoldFunction(
		const NodeGraphSignature& inputSlotSignature, 
		const NodeGraphSignature& generatedFunctionSignature, 
		StringSection<char> scaffoldFunctionName,
		StringSection<char> implementationFunctionName)
	{
			//
			//	Generate the scaffolding function that conforms to the given signature in "slotSignature", but that calls the implementation
			//	function that has the signature in "generatedFunctionSignature"
			//
			//	This is used to tie a node graph to a "slot signature". The slot signature is defined in the shader source code (as a normal function
			//	declaration). But the two may not match exactly (such as different parameter ordering, or some parameters missing in one or the
			//	other, etc). Here, we have to create a function that ties then together, and tries to make the best of mis-matches.
			//

		NodeGraphSignature slotSignature = inputSlotSignature;
		std::stringstream result;
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
				if (XlEqString(i->_type, "auto"))
					i->_type = p._type;
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

		result << "\t" << implementationFunctionName << "(" << paramStream.str() << ");" << std::endl;

		// Map the output parameters to their final destination.
		std::stringstream returnExpression;
		for (auto&p:slotSignature.GetParameters()) {
			if (p._direction != ParameterDirection::Out)
                continue;
            
			// First, look for an output from the generated function
			auto i = std::find_if(generatedFunctionSignature.GetParameters().begin(), generatedFunctionSignature.GetParameters().end(),
				[&p](const NodeGraphSignature::Parameter& test) -> bool
				{
					if (test._direction != ParameterDirection::Out) return false;
					return test._name == p._name;
				});
			if (i != generatedFunctionSignature.GetParameters().end()) {
				if (XlEqString(p._type, "auto"))
					p._type = i->_type;

				if (p._name == s_resultName) {
					WriteCastExpression(returnExpression, {std::string("temp_") + i->_name, i->_type}, p._type);
				} else {
					result << "\t" << p._name << " = ";
					WriteCastExpression(result, {std::string("temp_") + i->_name, i->_type}, p._type);
					result << ";" << std::endl;
				}
				continue;
			}

			// Second, just use a default value
			if (p._name == s_resultName) {
				returnExpression << "DefaultValue_" << p._type << "()";
			} else {
				result << "\t" << p._name << " = ";
				result << "DefaultValue_" << p._type << "();" << std::endl;
			}
		}

		auto returnExprStr = returnExpression.str();
		if (!returnExprStr.empty()) {
			result << "\treturn " << returnExprStr << ";" << std::endl;
		}

		result << "}" << std::endl;

		{
			std::stringstream header;
			header << "/////// Scaffold function for: " << implementationFunctionName << " ///////" << std::endl;
			header << GenerateMaterialCBuffer(generatedFunctionSignature);
			header << GenerateSignature(slotSignature, scaffoldFunctionName) << std::endl;

			return header.str() + result.str();
		}
	}

	static void GenerateGraphSyntaxInstantiation(
		std::ostream& result,
		const NodeGraph& graph,
		const NodeGraphSignature& interf,
		NodeId nodeId,
		std::unordered_map<NodeId, std::string>& instantiatedNodes);

	static void GenerateConnectionInstantiation(
		std::ostream& result,
		const NodeGraph& graph,
		const NodeGraphSignature& interf,
		const Connection& connection,
		std::unordered_map<NodeId, std::string>& instantiatedNodes)
	{
		result << connection.OutputParameterName() << ":";

		if (connection.InputNodeId() == NodeId_Interface) {
			result << connection.InputParameterName();
		} else if (connection.InputNodeId() == NodeId_Constant) {
			result << "\"" << connection.InputParameterName() << "\"";
		} else {
			GenerateGraphSyntaxInstantiation(result, graph, interf, connection.InputNodeId(), instantiatedNodes);
			if (!XlEqString(MakeStringSection(connection.InputParameterName()), MakeStringSection(ParameterName_NodeInstantiation)))
				result << "." << connection.InputParameterName();
		}
	}

	static void GenerateGraphSyntaxInstantiation(
		std::ostream& result,
		const NodeGraph& graph,
		const NodeGraphSignature& interf,
		NodeId nodeId,
		std::unordered_map<NodeId, std::string>& instantiatedNodes)
	{
		auto instantiation = instantiatedNodes.find(nodeId);
		if (instantiation != instantiatedNodes.end()) {
			result << instantiation->second;
			return;
		}

		auto n = graph.GetNode(nodeId);

		if (!n->AttributeTableName().empty())
			result << "[[" << n->AttributeTableName() << "]]";

		if (n->GetType() == Node::Type::Procedure) {
			result << RemoveTemplateRestrictions(n->ArchiveName()) << "(";

			bool atLeastOneParam = false;
			for (const auto&i:graph.GetConnections()) {
				if (i.OutputNodeId() != nodeId) continue;
				if (!i._condition.empty()) continue;
				if (atLeastOneParam) result << ", ";
				atLeastOneParam = true;
				GenerateConnectionInstantiation(result, graph, interf, i, instantiatedNodes);
			}
		} else {
			result << "(";
			assert(n->GetType() == Node::Type::Captures);
			// We must get the list of capture parameters from the graph signature itself. The names of the parameters & their default
			// values should be there
			bool atLeastOneParam = false;
			for (const auto& p:interf.GetCapturedParameters()) {
				auto firstDot = std::find(p._name.begin(), p._name.end(), '.');
				if (firstDot == p._name.end()) continue;
				if (!XlEqString(MakeStringSection(p._name.begin(), firstDot), MakeStringSection(n->ArchiveName()))) continue;

				if (atLeastOneParam) result << ", ";
				atLeastOneParam = true;

				result << p._type << " " << std::string(firstDot+1, p._name.end());
				if (!p._default.empty())
					result << " = \"" << p._default << "\"";
			}
		}

		result << ")";
	}

	std::string GenerateGraphSyntax(const NodeGraph& graph, const NodeGraphSignature& interf, StringSection<> name)
	{
		std::stringstream result;

		result << GenerateSignature(interf, name, true, true) << std::endl;
		result << "{" << std::endl;

		bool acyclic = false;
		auto sortedNodes = SortNodes(graph, acyclic);
		if (!acyclic) {
            result << "// Warning! found a cycle in the graph of nodes. Result will be incomplete!" << std::endl;
        }

		std::unordered_map<NodeId, std::string> instantiatedNodes;
		unsigned n = 0;
		for (auto id:sortedNodes) {
			unsigned outputCount = 0;
			for (const auto&i:graph.GetConnections())
				if (i.InputNodeId() == id) ++outputCount;
			// No need to instantiate nodes that have exactly 1 output connection. These will be instantiated in-place
			// when the connected node is instantiated. We do actually want to instantiate nodes with zero output connections
			// -- these are redundant to the final product, but if we don't create them here, they will disappear from the
			// graph entirely
			if (outputCount == 1) continue;

			auto i2 = std::find_if(graph.GetNodes().cbegin(), graph.GetNodes().cend(), [id](const Node& n) { return n.NodeId() == id; });
            if (i2 == graph.GetNodes().cend()) continue;

			assert(instantiatedNodes.find(id) == instantiatedNodes.end());
			std::string nodeName;
			if (i2->GetType() == Node::Type::Procedure) {
				nodeName = "node_" + std::to_string(n); ++n;
				result << "\tnode " << nodeName;
			} else {
				assert(i2->GetType() == Node::Type::Captures);
				nodeName = i2->ArchiveName();
				result << "\tcaptures " << nodeName;
			}
			result << " = ";
			GenerateGraphSyntaxInstantiation(result, graph, interf, id, instantiatedNodes);
			result << ";" << std::endl;

			instantiatedNodes.insert(std::make_pair(id, nodeName));
		}

		for (const auto&i:graph.GetConnections()) {
			if (i.OutputNodeId() != NodeId_Interface || i.OutputParameterName() == ShaderPatcher::s_resultName) continue;
			result << "\t" << i.OutputParameterName() << " = ";
			GenerateGraphSyntaxInstantiation(result, graph, interf, i.InputNodeId(), instantiatedNodes);
			result << "." << i.InputParameterName() << ";" << std::endl;
		}

		for (const auto&i:graph.GetConnections()) {
			if (i.OutputNodeId() != NodeId_Interface || i.OutputParameterName() != ShaderPatcher::s_resultName) continue;
			result << "\treturn ";
			GenerateGraphSyntaxInstantiation(result, graph, interf, i.InputNodeId(), instantiatedNodes);
			result << "." << i.InputParameterName() << ";" << std::endl;
		}

		for (const auto&connection:graph.GetConnections()) {
			if (connection._condition.empty()) continue;

			result << "\tif \"" << connection._condition << "\"" << std::endl;
			result << "\t\t" << instantiatedNodes[connection.OutputNodeId()] << ".";
			GenerateConnectionInstantiation(result, graph, interf, connection, instantiatedNodes);
			result << ";" << std::endl;
		}

		result << "}" << std::endl;

		return result.str();
	}

	static uint64_t CalculateHash(const InstantiationParameters::Dependency& dep, uint64_t seed = DefaultSeed64)
	{
		uint64_t result = Hash64(dep._archiveName);
		// todo -- ordering of parameters matters to the hash here
		for (const auto& d:dep._parameters._parameterBindings)
			result = Hash64(d.first, CalculateHash(d.second, result));
		return result;
	}

    uint64_t InstantiationParameters::CalculateHash() const
    {
        if (_parameterBindings.empty()) return 0;
        uint64 result = DefaultSeed64;
		// todo -- ordering of parameters matters to the hash here
        for (const auto&p:_parameterBindings) {
            result = Hash64(p.first, ShaderPatcher::CalculateHash(p.second, result));
			for (const auto&pc:p.second._parametersToCurry)
				result = Hash64(pc, result);
		}
        return result;
    }
}
