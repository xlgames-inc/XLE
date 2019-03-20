// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../ShaderParser/GraphSyntax.h"
#include <unordered_map>

using namespace System;
using namespace System::Collections::Generic;

namespace GraphLanguage { class NodeGraph; class NodeGraphSignature; class GraphSyntaxFile; class INodeGraphProvider; }

namespace GUILayer
{
	using AttributeTable = Dictionary<String^, String^>;

	        ///////////////////////////////////////////////////////////////
    public ref class Node
    {
    public:
        enum class Type 
        {
            Procedure,
			Captures
        };
        property String^       FragmentArchiveName;
        property UInt32        NodeId;
        property Type          NodeType;
		property String^	   AttributeTableName;

		static const UInt32 NodeId_Interface = (UInt32)-1;
		static const UInt32 NodeId_Constant = (UInt32)-2;
    };

        ///////////////////////////////////////////////////////////////
    public ref class Connection
    {
    public:
        property UInt32        InputNodeID;
        property String^       InputParameterName;
		property UInt32        OutputNodeID;
        property String^       OutputParameterName;
		property String^       Condition;
    };

	ref class NodeGraphFile;

	class ConversionContext
	{
	public:
		std::unordered_map<std::string, std::string> _importTable;
	};

        ///////////////////////////////////////////////////////////////
	ref class NodeGraphSignature;

    public ref class NodeGraph
    {
    public:
        property IEnumerable<Node^>^ Nodes { IEnumerable<Node^>^ get() { return _nodes; } }
        property IEnumerable<Connection^>^ Connections { IEnumerable<Connection^>^ get() { return _connections; } }

		void AddNode(Node^ node);
		void AddConnection(Connection^ connection);

		NodeGraph();

        GraphLanguage::NodeGraph    ConvertToNative(ConversionContext& context);
		static NodeGraph^			ConvertFromNative(const GraphLanguage::NodeGraph& input, const ConversionContext& context);

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

		GraphLanguage::NodeGraphSignature	ConvertToNative(ConversionContext& context);
		static NodeGraphSignature^			ConvertFromNative(const GraphLanguage::NodeGraphSignature& input, const ConversionContext& context);

	private:
		List<Parameter^>^				_parameters = gcnew List<Parameter^>();
        List<Parameter^>^				_capturedParameters = gcnew List<Parameter^>();
        List<TemplateParameter^>^		_templateParameters = gcnew List<TemplateParameter^>();
		String^							_implements = String::Empty;
	};

	ref class DirectorySearchRules;

	public ref class NodeGraphFile
    {
    public:
		ref class SubGraph
		{
		public:
			property NodeGraphSignature^	Signature;
			property NodeGraph^			Graph;
		};
		property Dictionary<String^, SubGraph^>^ SubGraphs
        {
            Dictionary<String^, SubGraph^>^ get() { if (!_subGraphs) { _subGraphs = gcnew Dictionary<String^, SubGraph^>(); } return _subGraphs; }
        }
		property Dictionary<String^, AttributeTable^>^ AttributeTables
        {
            Dictionary<String^, AttributeTable^>^ get() { if (!_attributeTables) { _attributeTables = gcnew Dictionary<String^, AttributeTable^>(); } return _attributeTables; }
        }

		GraphLanguage::GraphSyntaxFile	ConvertToNative();
		static NodeGraphFile^			ConvertFromNative(
			const GraphLanguage::GraphSyntaxFile& input, 
			const ::Assets::DirectorySearchRules& searchRules);

		std::shared_ptr<GraphLanguage::INodeGraphProvider> MakeNodeGraphProvider();

		DirectorySearchRules^ GetSearchRules();

		NodeGraphFile();
		~NodeGraphFile();
	private:
		Dictionary<String^, SubGraph^>^	_subGraphs = nullptr;
		Dictionary<String^, AttributeTable^>^ _attributeTables = nullptr;

		DirectorySearchRules^ _searchRules;
	};
}

