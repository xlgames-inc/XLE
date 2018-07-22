// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "PreviewRenderManager.h"

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Drawing;
using namespace System::Runtime::Serialization;
using System::Runtime::InteropServices::OutAttribute;

namespace ShaderPatcher { class NodeGraph; class NodeGraphSignature; class GraphSyntaxFile; }
namespace ShaderFragmentArchive { ref class Function; }
namespace Assets { class DirectorySearchRules; }

namespace ShaderPatcherLayer 
{
        ///////////////////////////////////////////////////////////////
    [DataContract] public ref class Node
    {
    public:
        enum class Type 
        {
            Procedure,
			SlotInput,
            SlotOutput,
			Uniforms
        };
        [DataMember] String^       FragmentArchiveName;
        [DataMember] UInt32        NodeId;
        [DataMember] int           VisualNodeId;
        [DataMember] Type          NodeType;
    };

        ///////////////////////////////////////////////////////////////
    [DataContract] public ref class BaseConnection
    {
    public:
        [DataMember] UInt32        OutputNodeID;
        [DataMember] String^       OutputParameterName;
    };

        ///////////////////////////////////////////////////////////////
    [DataContract] public ref class NodeConnection : public BaseConnection
    {
    public:
        [DataMember] UInt32        InputNodeID;
        [DataMember] String^       InputParameterName;
        [DataMember] String^       InputType;

		// [DataMember] String^       OutputType;
        // [DataMember] String^       Semantic;
    };

        ///////////////////////////////////////////////////////////////
    [DataContract] public ref class ConstantConnection : public BaseConnection
    {
    public:
        [DataMember] String^       Value;
    };

		///////////////////////////////////////////////////////////////
	[DataContract] public ref class InputParameterConnection : public BaseConnection
	{
	public:
		[DataMember] String^       Type;
		[DataMember] String^       Name;
		[DataMember] String^       Semantic;
        [DataMember] String^       Default;
        [DataMember] int           VisualNodeId;
	};

    	///////////////////////////////////////////////////////////////
	[DataContract] public ref class OutputParameterConnection
	{
	public:
        [DataMember] UInt32        InputNodeID;
        [DataMember] String^       InputParameterName;
		[DataMember] String^       Type;
        [DataMember] String^       Name;
		[DataMember] String^       Semantic;
        [DataMember] int           VisualNodeId;
	};

        ///////////////////////////////////////////////////////////////
    [DataContract] public ref class VisualNode
    {
    public:
        enum class StateType { Normal, Collapsed };
        [DataMember] PointF        Location;
        [DataMember] StateType     State;
    };

        ///////////////////////////////////////////////////////////////
    [DataContract] public ref class PreviewSettings
    {
    public:
        [DataMember] PreviewGeometry    Geometry;
        [DataMember] String^            OutputToVisualize;
        [DataMember] int                VisualNodeId;
    };

	ref class NodeGraphFile;

        ///////////////////////////////////////////////////////////////
    public ref class NodeGraph
    {
    public:
        [DataMember] property List<Node^>^ Nodes
        {
            List<Node^>^ get() { if (!_nodes) { _nodes = gcnew List<Node^>(); } return _nodes; }
        }

        [DataMember] property List<NodeConnection^>^ NodeConnections
        {
            List<NodeConnection^>^ get() { if (!_connections) { _connections = gcnew List<NodeConnection^>(); } return _connections; }
        }

        [DataMember] property List<ConstantConnection^>^ ConstantConnections
        {
            List<ConstantConnection^>^ get() { if (!_constantConnections) { _constantConnections = gcnew List<ConstantConnection^>(); } return _constantConnections; }
        }

        [DataMember] property List<InputParameterConnection^>^ InputParameterConnections
        {
            List<InputParameterConnection^>^ get() { if (!_inputParameterConnections) { _inputParameterConnections = gcnew List<InputParameterConnection^>(); } return _inputParameterConnections; }
        }

        [DataMember] property List<OutputParameterConnection^>^ OutputParameterConnections
        {
            List<OutputParameterConnection^>^ get() { if (!_outputParameterConnections) { _outputParameterConnections = gcnew List<OutputParameterConnection^>(); } return _outputParameterConnections; }
        }

        [DataMember] property List<VisualNode^>^ VisualNodes
        {
            List<VisualNode^>^ get() { if (!_visualNodes) { _visualNodes = gcnew List<VisualNode^>(); } return _visualNodes; }
        }

        [DataMember] property List<PreviewSettings^>^ PreviewSettingsObjects
        {
            List<PreviewSettings^>^ get() { if (!_previewSettings) { _previewSettings = gcnew List<PreviewSettings^>(); } return _previewSettings; }
        }

		NodeGraph();

        ShaderPatcher::NodeGraph    ConvertToNative();
		static NodeGraph^			ConvertFromNative(const ShaderPatcher::NodeGraph& input);

		static NodeGraph^		LoadFromXML(System::IO::Stream^ stream);
        void					SaveToXML(System::IO::Stream^ stream);

		Tuple<String^, String^>^ 
			GeneratePreviewShader(
				UInt32 previewNodeId, 
				NodeGraphFile^ nodeGraphFile,
				PreviewSettings^ settings,
				IEnumerable<KeyValuePair<String^, String^>>^ variableRestrictions);

    private:
        List<Node^>^                        _nodes;
        List<NodeConnection^>^              _connections;
        List<ConstantConnection^>^          _constantConnections;
        List<InputParameterConnection^>^    _inputParameterConnections;
        List<OutputParameterConnection^>^   _outputParameterConnections;
        List<VisualNode^>^                  _visualNodes;
        List<PreviewSettings^>^             _previewSettings;
    };

	public ref class NodeGraphSignature
	{
	public:
		value class Item {};
		property IEnumerable<Item>^ Parameters;
		property IEnumerable<Item>^ CapturedParameters;
		property IEnumerable<Item>^ TemplateParameters;

		ShaderPatcher::NodeGraphSignature	ConvertToNative();
		static NodeGraphSignature^			ConvertFromNative(const ShaderPatcher::NodeGraphSignature& input);
	};

	public ref class NodeGraphFile
    {
    public:
		ref class SubGraph
		{
		public:
			NodeGraphSignature^		_signature = nullptr;
			NodeGraph^				_subGraph = nullptr;
		};
		property Dictionary<String^, SubGraph^>^ SubGraphs
        {
            Dictionary<String^, SubGraph^>^ get() { if (!_subGraphs) { _subGraphs = gcnew Dictionary<String^, SubGraph^>(); } return _subGraphs; }
        }
		property Dictionary<String^,String^>^ Imports
		{
            Dictionary<String^,String^>^ get() { if (!_imports) { _imports = gcnew Dictionary<String^,String^>(); } return _imports; }
        }

        static void		Load(String^ filename, [Out] NodeGraphFile^% nodeGraph, [Out] NodeGraphContext^% context);
        void			Save(String^ filename, NodeGraphContext^ context);

		ShaderPatcher::GraphSyntaxFile	ConvertToNative();
		static NodeGraphFile^			ConvertFromNative(
			const ShaderPatcher::GraphSyntaxFile& input, 
			const ::Assets::DirectorySearchRules& searchRules);

		Tuple<String^, String^>^ 
            GeneratePreviewShader(
				String^ subGraphName,
				UInt32 previewNodeId, 
			    PreviewSettings^ settings,
			    IEnumerable<KeyValuePair<String^, String^>>^ variableRestrictions);

		ShaderFragmentArchive::Function^ FindSignature(String^ archiveName);

		const ::Assets::DirectorySearchRules& GetSearchRules();

		NodeGraphFile();
	private:
		Dictionary<String^, SubGraph^>^	_subGraphs = nullptr;
		Dictionary<String^, String^>^	_imports = nullptr;

		clix::shared_ptr<::Assets::DirectorySearchRules> _searchRules;
	};

}


