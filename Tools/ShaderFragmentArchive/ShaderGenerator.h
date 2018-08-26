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
        [DataMember] Type          NodeType;
		[DataMember] String^	   AttributeTableName;

		static const UInt32 NodeId_Interface = (UInt32)-1;
		static const UInt32 NodeId_Constant = (UInt32)-2;
    };

        ///////////////////////////////////////////////////////////////
    [DataContract] public ref class Connection
    {
    public:
        [DataMember] UInt32        InputNodeID;
        [DataMember] String^       InputParameterName;
		[DataMember] UInt32        OutputNodeID;
        [DataMember] String^       OutputParameterName;
    };

        ///////////////////////////////////////////////////////////////
    using AttributeTable = Dictionary<String^, String^>;

	public enum class PreviewGeometry { Chart, Plane2D, Box, Sphere, Model };
	public enum class StateType { Normal, Collapsed };

	[DataContract] public ref class PreviewSettings
	{
	public:
		[DataMember] PreviewGeometry    Geometry;
		[DataMember] String^            OutputToVisualize;

		static String^ PreviewGeometryToString(PreviewGeometry geo);
		static PreviewGeometry PreviewGeometryFromString(String^ input);
     };

		///////////////////////////////////////////////////////////////
	[DataContract] public ref class NodeGraphMetaData
    {
    public:
        property String^ DefaultsMaterial;
        property String^ PreviewModelFile;

        // Restrictions placed on the input variables
        [DataMember] property Dictionary<String^, String^>^ Variables { Dictionary<String^, String^>^ get() { if (!_variables) _variables = gcnew Dictionary<String^, String^>(); return _variables; } }

        // Configuration settings for the output file
        [DataMember] bool HasTechniqueConfig;
        [DataMember] property Dictionary<String^, String^>^ ShaderParameters { Dictionary<String^, String^>^ get() { if (!_shaderParameters) _shaderParameters = gcnew Dictionary<String^, String^>(); return _shaderParameters; } }

		NodeGraphMetaData() { HasTechniqueConfig = false; }

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

        [DataMember] property List<Connection^>^ Connections
        {
            List<Connection^>^ get() { if (!_connections) { _connections = gcnew List<Connection^>(); } return _connections; }
        }

		NodeGraph();

        ShaderPatcher::NodeGraph    ConvertToNative(ConversionContext& context);
		static NodeGraph^			ConvertFromNative(const ShaderPatcher::NodeGraph& input, const ConversionContext& context);

		Tuple<String^, String^>^ 
			GeneratePreviewShader(
				UInt32 previewNodeId, 
				NodeGraphFile^ nodeGraphFile,
				PreviewSettings^ settings,
				IEnumerable<KeyValuePair<String^, String^>>^ variableRestrictions);

    private:
        List<Node^>^                        _nodes;
        List<Connection^>^					_connections;
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

		property String^	Implements
		{
			String^ get() { return _implements; }
			void set(String^ value)
			{
				_implements = value;
				if (!_implements) _implements = String::Empty;
			}
		}

		ShaderPatcher::NodeGraphSignature	ConvertToNative(ConversionContext& context);
		static NodeGraphSignature^			ConvertFromNative(const ShaderPatcher::NodeGraphSignature& input, const ConversionContext& context);

	private:
		List<Parameter^>^				_parameters = gcnew List<Parameter^>();
        List<Parameter^>^				_capturedParameters = gcnew List<Parameter^>();
        List<TemplateParameter^>^		_templateParameters = gcnew List<TemplateParameter^>();
		String^							_implements = String::Empty;
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
		property Dictionary<String^, AttributeTable^>^ AttributeTables
        {
            Dictionary<String^, AttributeTable^>^ get() { if (!_attributeTables) { _attributeTables = gcnew Dictionary<String^, AttributeTable^>(); } return _attributeTables; }
        }

        static void		Load(String^ filename, [Out] NodeGraphFile^% nodeGraph, [Out] NodeGraphMetaData^% context);
        void			Serialize(System::IO::Stream^ stream, String^ name, NodeGraphMetaData^ contexts);

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
		Dictionary<String^, AttributeTable^>^ _attributeTables = nullptr;

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


