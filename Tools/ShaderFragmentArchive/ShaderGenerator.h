// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Drawing;
using namespace System::Runtime::Serialization;

namespace ShaderPatcher { class NodeGraph; }

namespace ShaderPatcherLayer {

        ///////////////////////////////////////////////////////////////
    [DataContract] public ref class Node
    {
    public:
        static enum class Type 
        {
            Procedure,
            MaterialCBuffer,
            InterpolatorIntoVertex,
            InterpolatorIntoPixel,
            SystemCBuffer,
            Output,
            Constants
        };
        [DataMember] property String^       FragmentArchiveName;
        [DataMember] property UInt32        NodeId;
        [DataMember] property int           VisualNodeId;
        [DataMember] property Type          NodeType;
    };

        ///////////////////////////////////////////////////////////////
    [DataContract] public ref class NodeConnection
    {
    public:
        [DataMember] property UInt32        OutputNodeID;
        [DataMember] property UInt32        InputNodeID;
        [DataMember] property String^       OutputParameterName;
        [DataMember] property String^       OutputType;
        [DataMember] property String^       InputParameterName;
        [DataMember] property String^       InputType;
        [DataMember] property String^       Semantic;
    };

        ///////////////////////////////////////////////////////////////
    [DataContract] public ref class NodeConstantConnection
    {
    public:
        [DataMember] property UInt32        OutputNodeID;
        [DataMember] property String^       OutputParameterName;
        [DataMember] property String^       Value;
    };

        ///////////////////////////////////////////////////////////////
    [DataContract] public ref class VisualNode
    {
    public:
        enum class StateType { Normal, Collapsed };
        [DataMember] property PointF        Location;
        [DataMember] property StateType     State;
    };

        ///////////////////////////////////////////////////////////////
    [DataContract] public ref class NodeGraph
    {
    public:
        [DataMember] property List<Node^>^ Nodes
        {
            List<Node^>^ get()                  { if (!_nodes) { _nodes = gcnew List<Node^>(); } return _nodes; }
        }

        [DataMember] property List<NodeConnection^>^ NodeConnections
        {
            List<NodeConnection^>^ get()        { if (!_connections) { _connections = gcnew List<NodeConnection^>(); } return _connections; }
        }

        [DataMember] property List<NodeConstantConnection^>^ NodeConstantConnections
        {
            List<NodeConstantConnection^>^ get() { if (!_constantConnections) { _constantConnections = gcnew List<NodeConstantConnection^>(); } return _constantConnections; }
        }

        [DataMember] property List<VisualNode^>^ VisualNodes
        {
            List<VisualNode^>^ get()            { if (!_visualNodes) { _visualNodes = gcnew List<VisualNode^>(); } return _visualNodes; }
        }

        NodeGraph();

        ShaderPatcher::NodeGraph    ConvertToNative(String^ name);
        ShaderPatcher::NodeGraph    ConvertToNativePreview(UInt32 previewNodeId);

        static public String^       GenerateShader(NodeGraph^ graph, String^ name);
        static public String^       GeneratePreviewShader(NodeGraph^ graph, UInt32 previewNodeId);

    private:
        List<Node^>^                    _nodes;
        List<NodeConnection^>^          _connections;
        List<NodeConstantConnection^>^  _constantConnections;
        List<VisualNode^>^              _visualNodes;
    };

}


