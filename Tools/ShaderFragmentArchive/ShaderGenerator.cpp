// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "stdafx.h"

#include "ShaderGenerator.h"
#include "../GUILayer/MarshalString.h"
#include "../../ShaderParser/ShaderPatcher.h"

namespace ShaderPatcherLayer 
{

    NodeGraph::NodeGraph()
    {
        _nodes          = gcnew List<Node^>();
        _connections    = gcnew List<NodeConnection^>();
        _visualNodes    = gcnew List<VisualNode^>();
    }

    static ShaderPatcher::Node::Type::Enum     ConvertToNative(Node::Type e)
    {
        switch (e) {
        default:
        case Node::Type::Procedure:               return ShaderPatcher::Node::Type::Procedure;
        case Node::Type::MaterialCBuffer:         return ShaderPatcher::Node::Type::MaterialCBuffer;
        case Node::Type::InterpolatorIntoVertex:  return ShaderPatcher::Node::Type::InterpolatorIntoVertex;
        case Node::Type::InterpolatorIntoPixel:   return ShaderPatcher::Node::Type::InterpolatorIntoPixel;
        case Node::Type::SystemCBuffer:           return ShaderPatcher::Node::Type::SystemParameters;
        case Node::Type::Output:                  return ShaderPatcher::Node::Type::Output;
        case Node::Type::Constants:               return ShaderPatcher::Node::Type::Constants;
        }
    }

    using namespace clix;
    static ShaderPatcher::Node                  ConvertToNative(Node^ node)
    {
        return ShaderPatcher::Node(
            marshalString<E_UTF8>(node->FragmentArchiveName), 
            node->NodeId, ConvertToNative(node->NodeType));
    }
    
    static ShaderPatcher::NodeConnection        ConvertToNative(NodeConnection^ connection)
    {
        return ShaderPatcher::NodeConnection(
            connection->OutputNodeID, connection->InputNodeID, 
            marshalString<E_UTF8>(connection->OutputParameterName),
            ShaderPatcher::Type(marshalString<E_UTF8>(connection->OutputType)), 
            marshalString<E_UTF8>(connection->InputParameterName),
            ShaderPatcher::Type(marshalString<E_UTF8>(connection->InputType)));
    }

    static ShaderPatcher::NodeConstantConnection        ConvertToNative(NodeConstantConnection^ connection)
    {
        return ShaderPatcher::NodeConstantConnection(
            connection->OutputNodeID,
            marshalString<E_UTF8>(connection->OutputParameterName),
            marshalString<E_UTF8>(connection->Value));
    }
    
    ShaderPatcher::NodeGraph        NodeGraph::ConvertToNative(String^ name)
    {
        ShaderPatcher::NodeGraph res(marshalString<E_UTF8>(name));

        for each(Node^ n in Nodes) {
            res.GetNodes().push_back(ShaderPatcherLayer::ConvertToNative(n));
        }

        for each(NodeConnection^ c in NodeConnections) {
            res.GetNodeConnections().push_back(ShaderPatcherLayer::ConvertToNative(c));
        }

        for each(NodeConstantConnection^ c in NodeConstantConnections) {
            res.GetNodeConstantConnections().push_back(ShaderPatcherLayer::ConvertToNative(c));
        }

        return res;
    }

    ShaderPatcher::NodeGraph        NodeGraph::ConvertToNativePreview(UInt32 previewNodeId)
    {
        auto graph = ConvertToNative("Preview");
        graph.TrimForPreview(previewNodeId);
        return graph;
    }

    String^         NodeGraph::GenerateShader(NodeGraph^ graph, String^name)
    {
        auto nativeGraph = graph->ConvertToNative(name);
        nativeGraph.AddDefaultOutputs();
        ShaderPatcher::NodeGraph graphOfTemporaries = ShaderPatcher::GenerateGraphOfTemporaries(nativeGraph);
        return marshalString<E_UTF8>(
                ShaderPatcher::GenerateShaderHeader(nativeGraph) 
            +   ShaderPatcher::GenerateShaderBody(nativeGraph, graphOfTemporaries));
    }

    String^         NodeGraph::GeneratePreviewShader(NodeGraph^ graph, UInt32 previewNodeId)
    {
        auto nativeGraph = graph->ConvertToNativePreview(previewNodeId);
        ShaderPatcher::NodeGraph graphOfTemporaries = ShaderPatcher::GenerateGraphOfTemporaries(nativeGraph);
        std::string structure = ShaderPatcher::GenerateStructureForPreview(nativeGraph, graphOfTemporaries);
        return marshalString<E_UTF8>(
                ShaderPatcher::GenerateShaderHeader(nativeGraph) 
            +   ShaderPatcher::GenerateShaderBody(nativeGraph, graphOfTemporaries) 
            +   structure)
            ;
    }


}
