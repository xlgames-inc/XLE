// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../GUILayer/CLIXAutoPtr.h"
#include <unordered_map>

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
	public enum class PreviewGeometry
    {
        Chart, Plane2D, Box, Sphere, Model
    };

    [DataContract] public ref class PreviewSettings
    {
    public:
        [DataMember] PreviewGeometry    Geometry;
        [DataMember] String^            OutputToVisualize;
        [DataMember] int                VisualNodeId;
    };

		///////////////////////////////////////////////////////////////
	[DataContract] public ref class NodeGraphContext
    {
    public:
        property String^ DefaultsMaterial;
        property String^ PreviewModelFile;

        // Restrictions placed on the input variables
        [DataMember] property Dictionary<String^, String^>^ Variables { Dictionary<String^, String^>^ get() { if (!_variables) _variables = gcnew Dictionary<String^, String^>(); return _variables; } }

        // Configuration settings for the output file
        [DataMember] bool HasTechniqueConfig;
        [DataMember] property Dictionary<String^, String^>^ ShaderParameters { Dictionary<String^, String^>^ get() { if (!_shaderParameters) _shaderParameters = gcnew Dictionary<String^, String^>(); return _shaderParameters; } }

		NodeGraphContext() { HasTechniqueConfig = false; }

    private:
        Dictionary<String^, String^>^ _variables = nullptr;
        Dictionary<String^, String^>^ _shaderParameters = nullptr;
    };

	ref class NodeGraphFile;

	class ConversionContext
	{
	public:
		std::unordered_map<std::string, std::string> _importTable;
	};

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

        ShaderPatcher::NodeGraph    ConvertToNative(ConversionContext& context);
		static NodeGraph^			ConvertFromNative(const ShaderPatcher::NodeGraph& input, const ConversionContext& context);

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
		enum class ParameterDirection { In, Out };

		ref class Parameter
        {
        public:
            property System::String^		Type;
            property System::String^		Name;
			property ParameterDirection		Direction;
			property System::String^		Semantic;
			property System::String^		Default;
        };

		property List<Parameter^>^	Parameters { List<Parameter^>^ get() { return _parameters; } }
		property List<Parameter^>^	CapturedParameters { List<Parameter^>^ get() { return _capturedParameters; } }

		ref class TemplateParameter
		{
		public:
			property System::String^		Name;
            property System::String^		Restriction;
		};
		property List<TemplateParameter^>^	TemplateParameters { List<TemplateParameter^>^ get() { return _templateParameters; } }

		ShaderPatcher::NodeGraphSignature	ConvertToNative(ConversionContext& context);
		static NodeGraphSignature^			ConvertFromNative(const ShaderPatcher::NodeGraphSignature& input, const ConversionContext& context);

	private:
		List<Parameter^>^				_parameters = gcnew List<Parameter^>();
        List<Parameter^>^				_capturedParameters = gcnew List<Parameter^>();
        List<TemplateParameter^>^		_templateParameters = gcnew List<TemplateParameter^>();
	};

	public ref class NodeGraphFile
    {
    public:
		ref class SubGraph
		{
		public:
			[DataMember] property NodeGraphSignature^	Signature;
			[DataMember] property NodeGraph^			Graph;
		};
		property Dictionary<String^, SubGraph^>^ SubGraphs
        {
            Dictionary<String^, SubGraph^>^ get() { if (!_subGraphs) { _subGraphs = gcnew Dictionary<String^, SubGraph^>(); } return _subGraphs; }
        }

        static void		Load(String^ filename, [Out] NodeGraphFile^% nodeGraph, [Out] NodeGraphContext^% context);
        void			Serialize(System::IO::Stream^ stream, String^ name, NodeGraphContext^ contexts);

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

		GUILayer::DirectorySearchRules^ GetSearchRules();

		NodeGraphFile();
		~NodeGraphFile();
	private:
		Dictionary<String^, SubGraph^>^	_subGraphs = nullptr;
		Dictionary<String^, String^>^	_imports = nullptr;

		GUILayer::DirectorySearchRules^ _searchRules;
	};

	public ref class NodeGraphPreviewConfiguration
	{
	public:
		NodeGraphFile^		_nodeGraph;
		String^				_subGraphName;
		UInt32				_previewNodeId; 
		PreviewSettings^	_settings;
		IEnumerable<KeyValuePair<String^, String^>>^ _variableRestrictions;
	};

}


