// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "stdafx.h"

#include "ShaderGenerator.h"
#include "ShaderDiagramDocument.h"
#include "../GUILayer/MarshalString.h"
#include "../../ShaderParser/ShaderPatcher.h"
#include "../../ShaderParser/GraphSyntax.h"
#include "../../ShaderParser/NodeGraphProvider.h"
#include "../../ShaderParser/ShaderInstantiation.h"
#include "../../Assets/ConfigFileContainer.h"
#include "../../Assets/IFileSystem.h"
#include "../../Utility/Streams/FileUtils.h"

using namespace System::Runtime::Serialization;

namespace ShaderPatcher
{
	INodeGraphProvider::NodeGraph LoadGraph(StringSection<> filename, StringSection<> entryPoint);
}

namespace ShaderPatcherLayer 
{
    NodeGraph::NodeGraph()
    {
        _nodes          = gcnew List<Node^>();
        _connections    = gcnew List<NodeConnection^>();
        _visualNodes    = gcnew List<VisualNode^>();
    }
	
///////////////////////////////////////////////////////////////////////////////////////////////////

    static ShaderPatcher::Node::Type     ConvertToNative(Node::Type e)
    {
        switch (e) {
        default:
        case Node::Type::Procedure:					return ShaderPatcher::Node::Type::Procedure;
        case Node::Type::SlotInput:					return ShaderPatcher::Node::Type::SlotInput;
        case Node::Type::SlotOutput:				return ShaderPatcher::Node::Type::SlotOutput;
        case Node::Type::Uniforms:                  return ShaderPatcher::Node::Type::Uniforms;
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
            marshalString<E_UTF8>(connection->InputParameterName),
            ShaderPatcher::Type(marshalString<E_UTF8>(connection->InputType)));
    }

    static ShaderPatcher::ConstantConnection        ConvertToNative(ConstantConnection^ connection)
    {
        return ShaderPatcher::ConstantConnection(
            connection->OutputNodeID,
            marshalString<E_UTF8>(connection->OutputParameterName),
            marshalString<E_UTF8>(connection->Value));
    }

    static ShaderPatcher::InputParameterConnection        ConvertToNative(InputParameterConnection^ connection)
    {
        return ShaderPatcher::InputParameterConnection(
            connection->OutputNodeID,
            marshalString<E_UTF8>(connection->OutputParameterName),
            ShaderPatcher::Type(marshalString<E_UTF8>(connection->Type)),
            marshalString<E_UTF8>(connection->Name),
            marshalString<E_UTF8>(connection->Semantic),
            connection->Default ? marshalString<E_UTF8>(connection->Default) : std::string());
    }

    static ShaderPatcher::NodeConnection        ConvertToNative(OutputParameterConnection^ connection)
    {
            // note -- semantic lost!
        return ShaderPatcher::NodeConnection(
            ~0u, connection->InputNodeID,
            marshalString<E_UTF8>(connection->Name),
            marshalString<E_UTF8>(connection->InputParameterName),
            ShaderPatcher::Type(marshalString<E_UTF8>(connection->Type)));
    }
    
    ShaderPatcher::NodeGraph        NodeGraph::ConvertToNative()
    {
        ShaderPatcher::NodeGraph res;
        for each(Node^ n in Nodes)
            res.Add(ShaderPatcherLayer::ConvertToNative(n));
        for each(NodeConnection^ c in NodeConnections)
            res.Add(ShaderPatcherLayer::ConvertToNative(c));
        for each(ConstantConnection^ c in ConstantConnections)
            res.Add(ShaderPatcherLayer::ConvertToNative(c));
        for each(InputParameterConnection^ c in InputParameterConnections)
            res.Add(ShaderPatcherLayer::ConvertToNative(c));
        for each(OutputParameterConnection^ c in OutputParameterConnections)
            res.Add(ShaderPatcherLayer::ConvertToNative(c));
        return res;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	static Node::Type     ConvertFromNative(ShaderPatcher::Node::Type e)
    {
        switch (e) {
        default:
		case ShaderPatcher::Node::Type::Procedure : return Node::Type::Procedure;
		case ShaderPatcher::Node::Type::SlotInput: return Node::Type::SlotInput;
		case ShaderPatcher::Node::Type::SlotOutput: return Node::Type::SlotOutput;
		case ShaderPatcher::Node::Type::Uniforms: return Node::Type::Uniforms;
        }
    }

	static Node^ ConvertFromNative(const ShaderPatcher::Node& node)
    {
        Node^ result = gcnew Node;
		result->FragmentArchiveName = marshalString<E_UTF8>(node.ArchiveName());
		result->NodeId = node.NodeId();
		result->NodeType = ConvertFromNative(node.GetType());
		return result;
    }
    
    static NodeConnection^ ConvertFromNative(const ShaderPatcher::NodeConnection& connection)
    {
        NodeConnection^ result = gcnew NodeConnection;
        result->OutputNodeID = connection.OutputNodeId(); 
		result->OutputParameterName = marshalString<E_UTF8>(connection.OutputParameterName());
		result->InputNodeID = connection.InputNodeId();
        result->InputParameterName = marshalString<E_UTF8>(connection.InputParameterName());
        result->InputType = marshalString<E_UTF8>(connection.InputType()._name);
		return result;
    }

    static ConstantConnection^ ConvertFromNative(const ShaderPatcher::ConstantConnection& connection)
    {
		ConstantConnection^ result = gcnew ConstantConnection;
        result->OutputNodeID = connection.OutputNodeId(); 
		result->OutputParameterName = marshalString<E_UTF8>(connection.OutputParameterName());
		result->Value = marshalString<E_UTF8>(connection.Value());
		return result;
    }

    static InputParameterConnection^ ConvertFromNative(const ShaderPatcher::InputParameterConnection& connection)
    {
		InputParameterConnection^ result = gcnew InputParameterConnection;
        result->OutputNodeID = connection.OutputNodeId(); 
		result->OutputParameterName = marshalString<E_UTF8>(connection.OutputParameterName());
		result->Type = marshalString<E_UTF8>(connection.InputType()._name);
        result->Name = marshalString<E_UTF8>(connection.InputName());
        result->Semantic = marshalString<E_UTF8>(connection.InputSemantic());
		if (!connection.Default().empty())
            result->Default = marshalString<E_UTF8>(connection.Default());
		return result;
    }

    static OutputParameterConnection^ ConvertFromNativeOutParam(const ShaderPatcher::NodeConnection& connection)
    {
		OutputParameterConnection^ result = gcnew OutputParameterConnection;
		result->InputNodeID = connection.InputNodeId();
        result->InputParameterName = marshalString<E_UTF8>(connection.InputParameterName());
        result->Type = marshalString<E_UTF8>(connection.InputType()._name);
		result->Name = marshalString<E_UTF8>(connection.OutputParameterName());
		return result;
    }

	NodeGraph^ NodeGraph::ConvertFromNative(const ShaderPatcher::NodeGraph& input)
	{
		NodeGraph^ result = gcnew NodeGraph;
		for (const auto& n:input.GetNodes())
			result->Nodes->Add(ShaderPatcherLayer::ConvertFromNative(n));

		for (const auto& c:input.GetNodeConnections())
			if (c.OutputNodeId() != ~0u) {
				result->NodeConnections->Add(ShaderPatcherLayer::ConvertFromNative(c));
			} else {
				result->OutputParameterConnections->Add(ConvertFromNativeOutParam(c));
			}

		for (const auto& c:input.GetConstantConnections())
			result->ConstantConnections->Add(ShaderPatcherLayer::ConvertFromNative(c));
		for (const auto&c:input.GetInputParameterConnections())
			result->InputParameterConnections->Add(ShaderPatcherLayer::ConvertFromNative(c));

		// Construct a list of visual nodes for everything that requires one
		for each(Node^ n in result->Nodes) {
			n->VisualNodeId = (int)result->VisualNodes->Count;
			VisualNode^ vsnode = gcnew VisualNode; vsnode->Location = PointF(0.f, 0.f); vsnode->State = VisualNode::StateType::Normal;
			result->VisualNodes->Add(vsnode);
		}

		for each(InputParameterConnection^ c in result->InputParameterConnections) {
			c->VisualNodeId = (int)result->VisualNodes->Count;
			VisualNode^ vsnode = gcnew VisualNode; vsnode->Location = PointF(0.f, 0.f); vsnode->State = VisualNode::StateType::Normal;
			result->VisualNodes->Add(vsnode);
		}
		for each(OutputParameterConnection^ c in result->OutputParameterConnections) {
			c->VisualNodeId = (int)result->VisualNodes->Count;
			VisualNode^ vsnode = gcnew VisualNode; vsnode->Location = PointF(0.f, 0.f); vsnode->State = VisualNode::StateType::Normal;
			result->VisualNodes->Add(vsnode);
		}

		return result;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

#if 0
    String^         NodeGraph::GenerateShader(NodeGraph^ graph)
    {
        try
        {
            auto nativeGraph = graph->ConvertToNative();
            ShaderPatcher::NodeGraphSignature interf;
			std::string shaderBody;
			std::tie(shaderBody, interf) = ShaderPatcher::GenerateFunction(nativeGraph);
            return marshalString<E_UTF8>(ShaderPatcher::GenerateShaderHeader(nativeGraph) + shaderBody);
        } catch (const std::exception& e) {
            return "Exception while generating shader: " + clix::marshalString<clix::E_UTF8>(e.what());
        } catch (...) {
            return "Unknown exception while generating shader";
        }
    }
    
    static std::string GenerateCBLayoutInt(ShaderPatcher::NodeGraphSignature& interf)
    {
        std::stringstream str;
            // Input parameters that can be stored in a cbuffer become
            // part of our cblayout
        auto globalParams = interf.GetCapturedParameters();
        for (unsigned c=0; c<globalParams.size(); ++c) {
            const auto& p = globalParams[c];
            str << p._type << " " << p._name;
            if (!p._default.empty())
                str << " = " << p._default;
            str << ";" << std::endl;
        }
        return str.str();
    }

    Tuple<String^,String^>^ NodeGraph::GeneratePreviewShader(
		NodeGraph^ graph, UInt32 previewNodeId, 
		PreviewSettings^ settings, IEnumerable<KeyValuePair<String^, String^>>^ variableRestrictions)
    {
        try
        {
            auto nativeGraph = graph->ConvertToNative();
			nativeGraph.Trim(previewNodeId);
			ShaderPatcher::NodeGraphSignature interf;
			std::string shaderBody;
			std::tie(shaderBody, interf) = ShaderPatcher::GenerateFunction(nativeGraph);

            ShaderPatcher::PreviewOptions options = 
				{
					(settings->Geometry == PreviewGeometry::Chart)
						? ShaderPatcher::PreviewOptions::Type::Chart
						: ShaderPatcher::PreviewOptions::Type::Object,
					String::IsNullOrEmpty(settings->OutputToVisualize) 
						? std::string() 
						: marshalString<E_UTF8>(settings->OutputToVisualize),
					ShaderPatcher::PreviewOptions::VariableRestrictions()
				};
			if (variableRestrictions)
				for each(auto v in variableRestrictions)
					options._variableRestrictions.push_back(
						std::make_pair(
							clix::marshalString<clix::E_UTF8>(v.Key),
							clix::marshalString<clix::E_UTF8>(v.Value)));
                
            return gcnew Tuple<String^,String^>(
                marshalString<E_UTF8>(
                        ShaderPatcher::GenerateShaderHeader(nativeGraph)
					+	ShaderPatcher::GenerateMaterialCBuffer(interf)
                    +   shaderBody 
                    +   ShaderPatcher::GenerateStructureForPreview("preview", interf, nativeGraph.GetSearchRules(), options)),
                marshalString<E_UTF8>(GenerateCBLayoutInt(interf)));
        } catch (const std::exception& e) {
            return gcnew Tuple<String^,String^>(
                "Exception while generating shader: " + clix::marshalString<clix::E_UTF8>(e.what()),
                String::Empty);
        } catch (...) {
            return gcnew Tuple<String^,String^>(
                "Unknown exception while generating shader",
                String::Empty);
        }
    }

    String^      NodeGraph::GenerateCBLayout(NodeGraph^ graph)
    {
        try
        {
            auto nativeGraph = graph->ConvertToNative();
			ShaderPatcher::NodeGraphSignature interf;
			std::string shaderBody;
			std::tie(shaderBody, interf) = ShaderPatcher::GenerateFunction(nativeGraph);
            return clix::marshalString<clix::E_UTF8>(GenerateCBLayoutInt(interf));
        } catch (const std::exception& e) {
            return "Exception while generating shader: " + clix::marshalString<clix::E_UTF8>(e.what());
        } catch (...) {
            return "Unknown exception while generating shader";
        }
    }

	NodeGraph::Interface^	NodeGraph::GetInterface()
	{
		auto nativeGraph = ConvertToNative();
        ShaderPatcher::NodeGraphSignature interf;
		std::string shaderBody;
		std::tie(shaderBody, interf) = ShaderPatcher::GenerateFunction(nativeGraph);

		auto variables = gcnew List<Interface::Item>();
		for (const auto& i:interf.GetParameters()) {
			if (i._direction != ShaderPatcher::ParameterDirection::In)
				continue;

			Interface::Item item;
			item.Type = clix::marshalString<clix::E_UTF8>(i._type);
			item.Name = clix::marshalString<clix::E_UTF8>(i._name);
			item.Semantic = clix::marshalString<clix::E_UTF8>(i._semantic);
			variables->Add(item);
		}

		auto resources = gcnew List<Interface::Item>();
		for (const auto& i2:interf.GetCapturedParameters()) {
			Interface::Item item;
			item.Type = clix::marshalString<clix::E_UTF8>(i2._type);
			item.Name = clix::marshalString<clix::E_UTF8>(i2._name);
			item.Semantic = clix::marshalString<clix::E_UTF8>(i2._semantic);
			variables->Add(item);
		}

		auto result = gcnew NodeGraph::Interface();
		result->Variables = variables;
		result->Resources = resources;
		return result;
	}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	ShaderPatcher::NodeGraphSignature	NodeGraphSignature::ConvertToNative()
	{
		return {};
	}

	NodeGraphSignature^			NodeGraphSignature::ConvertFromNative(const ShaderPatcher::NodeGraphSignature& input)
	{
		return nullptr;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class ShaderGeneratorProvider : public ShaderPatcher::BasicNodeGraphProvider, std::enable_shared_from_this<ShaderGeneratorProvider>
    {
    public:
        std::optional<Signature> FindSignature(StringSection<> name);
		std::optional<NodeGraph> FindGraph(StringSection<> name);

        ShaderGeneratorProvider(
			NodeGraphFile^ parsedGraphFile,
			const ::Assets::DirectorySearchRules& searchRules);
        ~ShaderGeneratorProvider();
    protected:
		gcroot<NodeGraphFile^> _parsedGraphFile;
    };

	template<typename Type>
		static Type^ Find(Dictionary<String^, Type^>^ dictionary, StringSection<> search)
	{
		String^ searchKey = clix::marshalString<clix::E_UTF8>(search);
		if (dictionary->ContainsKey(searchKey))
			return (*dictionary)[searchKey];
		return nullptr;
	}
		

	auto ShaderGeneratorProvider::FindSignature(StringSection<> name) -> std::optional<Signature>
	{
		// Interpret the given string to find a function signature that matches it
		// First, check to see if it's scoped as an imported function
		auto *scopingOperator = name.begin() + 1;
		while (scopingOperator < name.end()) {
			if (*(scopingOperator-1) == ':' && *scopingOperator == ':')
				break;
			++scopingOperator;
		}
		if (scopingOperator < name.end()) {
			auto searchName = MakeStringSection(name.begin(), scopingOperator-1);
			auto functionName = MakeStringSection(scopingOperator+1, name.end());

			auto importedName = Find(_parsedGraphFile->Imports, searchName);
			if (importedName)
				return BasicNodeGraphProvider::FindSignature(clix::marshalString<clix::E_UTF8>(importedName) + ":" + functionName.AsString());
			return BasicNodeGraphProvider::FindSignature(searchName.AsString() + ":" + functionName.AsString());
		}

		// Look for the function within the parsed graph syntax file
		auto i = Find(_parsedGraphFile->SubGraphs, name);
		if (i)
			return Signature{ name.AsString(), i->_signature->ConvertToNative() };

		// Just fallback to default behaviour
		return BasicNodeGraphProvider::FindSignature(name);
	}

	auto ShaderGeneratorProvider::FindGraph(StringSection<> name) -> std::optional<NodeGraph>
	{
		// Interpret the given string to find a function signature that matches it
		// First, check to see if it's scoped as an imported function
		auto *scopingOperator = name.begin() + 1;
		while (scopingOperator < name.end()) {
			if (*(scopingOperator-1) == ':' && *scopingOperator == ':')
				break;
			++scopingOperator;
		}
		if (scopingOperator < name.end()) {
			auto searchName = MakeStringSection(name.begin(), scopingOperator-1);
			auto functionName = MakeStringSection(scopingOperator+1, name.end());

			auto importedName = Find(_parsedGraphFile->Imports, searchName);
			if (importedName)
				return ShaderPatcher::LoadGraph(clix::marshalString<clix::E_UTF8>(importedName), functionName);
			return ShaderPatcher::LoadGraph(searchName, functionName);
		}

		// Look for the function within the parsed graph syntax file
		auto i = Find(_parsedGraphFile->SubGraphs, name);
		if (i)
			return NodeGraph{ name.AsString(), i->_subGraph->ConvertToNative(), i->_signature->ConvertToNative(), shared_from_this() };

		return {};
	}

	ShaderGeneratorProvider::ShaderGeneratorProvider(
		NodeGraphFile^ parsedGraphFile,
		const ::Assets::DirectorySearchRules& searchRules)
	: BasicNodeGraphProvider(searchRules)
	, _parsedGraphFile(parsedGraphFile)
	{}

	ShaderGeneratorProvider::~ShaderGeneratorProvider()
	{}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	ShaderPatcher::GraphSyntaxFile	NodeGraphFile::ConvertToNative()
	{
		return {};
	}

	NodeGraphFile^			NodeGraphFile::ConvertFromNative(const ShaderPatcher::GraphSyntaxFile& input)
	{
		return nullptr;
	}

	// auto provider = std::shared_ptr<ShaderGeneratorProvider>(new ShaderGeneratorProvider(this, ::Assets::DirectorySearchRules{}));

	Tuple<String^, String^>^ 
		NodeGraphFile::GeneratePreviewShader(
			String^ subGraphName,
			UInt32 previewNodeId, 
			PreviewSettings^ settings,
			IEnumerable<KeyValuePair<String^, String^>>^ variableRestrictions)
	{
		SubGraph^ subGraph = SubGraphs[subGraphName];
		return subGraph->_subGraph->GeneratePreviewShader(previewNodeId, NodeGraphProvider, settings, variableRestrictions);
	}

	Tuple<String^, String^>^ 
		NodeGraph::GeneratePreviewShader(
			UInt32 previewNodeId, 
			NodeGraphProvider^ nodeGraphProvider,
			PreviewSettings^ settings,
			IEnumerable<KeyValuePair<String^, String^>>^ variableRestrictions)
	{
		try
		{
            auto nativeGraph = ConvertToNative();
			nativeGraph.Trim(previewNodeId);
			
			ShaderPatcher::InstantiationParameters instantiationParams {};
			ShaderPatcher::NodeGraphSignature mainInstantiationSignature {};

			/*auto fragments = ShaderPatcher::InstantiateShader(
				ShaderPatcher::INodeGraphProvider::NodeGraph { 
					std::string("preview_graph"), nativeGraph, 
					mainInstantiationSignature, nodeGraphProvider.AsNative },
				instantiationParams);*/
			std::vector<std::string> fragments;

			ShaderPatcher::PreviewOptions options {
				(settings->Geometry == PreviewGeometry::Chart) ? ShaderPatcher::PreviewOptions::Type::Chart : ShaderPatcher::PreviewOptions::Type::Object,
				String::IsNullOrEmpty(settings->OutputToVisualize) ? std::string() : marshalString<E_UTF8>(settings->OutputToVisualize),
				ShaderPatcher::PreviewOptions::VariableRestrictions() };

			if (variableRestrictions)
				for each(auto v in variableRestrictions)
					options._variableRestrictions.push_back(
						std::make_pair(
							clix::marshalString<clix::E_UTF8>(v.Key),
							clix::marshalString<clix::E_UTF8>(v.Value)));

			auto structureForPreview = GenerateStructureForPreview(
				"preview", mainInstantiationSignature, options);

			fragments.push_back(structureForPreview);

			std::stringstream str;
			for (const auto&f:fragments) str << f;

            return gcnew Tuple<String^,String^>(
                marshalString<E_UTF8>(str.str()),
                marshalString<E_UTF8>(ShaderPatcher::GenerateMaterialCBuffer(mainInstantiationSignature)));
        } catch (const std::exception& e) {
            return gcnew Tuple<String^,String^>(
                "Exception while generating shader: " + clix::marshalString<clix::E_UTF8>(e.what()),
                String::Empty);
        } catch (...) {
            return gcnew Tuple<String^,String^>(
                "Unknown exception while generating shader",
                String::Empty);
        }
	}

    NodeGraph^ NodeGraph::LoadFromXML(System::IO::Stream^ stream)
    {
        auto serializer = gcnew DataContractSerializer(NodeGraph::typeid);
        try
        {
            auto o = serializer->ReadObject(stream);
            return dynamic_cast<NodeGraph^>(o);
        }
        finally { delete serializer; }
    }

    void NodeGraph::SaveToXML(System::IO::Stream^ stream)
    {
        DataContractSerializer^ serializer = nullptr;
        System::Xml::XmlWriterSettings^ settings = nullptr;
        System::Xml::XmlWriter^ writer = nullptr;

        try
        {
            serializer = gcnew DataContractSerializer(NodeGraph::typeid);
            settings = gcnew System::Xml::XmlWriterSettings();
            settings->Indent = true;
            settings->IndentChars = "\t";
            settings->Encoding = System::Text::Encoding::UTF8;

            writer = System::Xml::XmlWriter::Create(stream, settings);
            serializer->WriteObject(writer, this);
        }
        finally
        {
            delete writer;
            delete serializer;
            delete settings;
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static bool IsNodeGraphChunk(const ::Assets::TextChunk<char>& chunk)        { return XlEqString(chunk._type, "NodeGraph"); }
    static bool IsNodeGraphContextChunk(const ::Assets::TextChunk<char>& chunk) { return XlEqString(chunk._type, "NodeGraphContext"); }
	static bool IsGraphSyntaxChunk(const ::Assets::TextChunk<char>& chunk)		{ return XlEqString(chunk._type, "GraphSyntax"); }

    static array<Byte>^ AsManagedArray(const ::Assets::TextChunk<char>* chunk)
    {
        // marshall the native string into a managed array, and from there into
        // a stream... We need to strip off leading whitespace, however (usually
        // there is a leading newline, which confuses the xml loader
        auto begin = chunk->_content.begin();
        while (begin != chunk->_content.end() && *begin == ' ' || *begin == '\t' || *begin == '\r' || *begin == '\n')
            ++begin;

        size_t contentSize = size_t(chunk->_content.end()) - size_t(begin);
        array<Byte>^ managedArray = gcnew array<Byte>((int)contentSize);
        {
            cli::pin_ptr<Byte> pinned = &managedArray[managedArray->GetLowerBound(0)];
            XlCopyMemory(pinned, begin, contentSize);
        }
        return managedArray;
    }

    static NodeGraphContext^ LoadContext(System::IO::Stream^ stream)
    {
        auto serializer = gcnew DataContractSerializer(NodeGraphContext::typeid);
        try
        {
            auto o = serializer->ReadObject(stream);
            return dynamic_cast<NodeGraphContext^>(o);
        }
        finally { delete serializer; }
    }

    static void SaveToXML(System::IO::Stream^ stream, NodeGraphContext^ context)
    {
        DataContractSerializer^ serializer = nullptr;
        System::Xml::XmlWriterSettings^ settings = nullptr;
        System::Xml::XmlWriter^ writer = nullptr;

        try
        {
            serializer = gcnew DataContractSerializer(NodeGraphContext::typeid);
            settings = gcnew System::Xml::XmlWriterSettings();
            settings->Indent = true;
            settings->IndentChars = "\t";
            settings->Encoding = System::Text::Encoding::UTF8;

            writer = System::Xml::XmlWriter::Create(stream, settings);
            serializer->WriteObject(writer, context);
        }
        finally
        {
            delete writer;
            delete serializer;
            delete settings;
        }
    }

    void NodeGraphFile::Load(String^ filename, [Out] NodeGraphFile^% nodeGraph, [Out] NodeGraphContext^% context)
    {
        // Load from a graph model compound text file (that may contain other text chunks)
        // We're going to use a combination of native and managed stuff -- so it's easier
        // if the caller just passes in a filename
        auto nativeFilename = clix::marshalString<clix::E_UTF8>(filename);
		size_t size = 0;
        auto block = ::Assets::TryLoadFileAsMemoryBlock(MakeStringSection(nativeFilename), &size);
        if (!block.get() || !size)
            throw gcnew System::Exception(System::String::Format("Missing or empty file {0}", filename));

        auto chunks = ::Assets::ReadCompoundTextDocument(
            MakeStringSection((const char*)block.get(), (const char*)PtrAdd(block.get(), size)));

		// load the graphChunk first
        auto graphChunk = std::find_if(chunks.cbegin(), chunks.cend(), IsNodeGraphChunk);
        if (graphChunk != chunks.end()) {
			nodeGraph = gcnew NodeGraphFile;
            array<Byte>^ managedArray = nullptr;
            System::IO::MemoryStream^ memStream = nullptr;
            try
            {
                managedArray = AsManagedArray(AsPointer(graphChunk));
                memStream = gcnew System::IO::MemoryStream(managedArray);
                NodeGraphFile::SubGraph^ subGraph = gcnew NodeGraphFile::SubGraph;
				subGraph->_subGraph = NodeGraph::LoadFromXML(memStream);
				nodeGraph->SubGraphs->Add("main", subGraph);
            }
            finally
            {
                delete memStream;
                delete managedArray;
            }
        } else {
			auto graphSyntaxChunk = std::find_if(chunks.cbegin(), chunks.cend(), IsGraphSyntaxChunk);
			if (graphSyntaxChunk != chunks.end()) {
				auto nativeGraph = ::ShaderPatcher::ParseGraphSyntax(graphSyntaxChunk->_content);
				// nativeGraph.SetSearchRules(::Assets::DefaultDirectorySearchRules(MakeStringSection(nativeFilename)));
				nodeGraph = NodeGraphFile::ConvertFromNative(nativeGraph);
			} else {
				throw gcnew System::Exception("Could not find node graph chunk within compound text file");
			}
		}

            // now load the context chunk (if it exists)
		auto contextChunk = std::find_if(chunks.cbegin(), chunks.cend(), IsNodeGraphContextChunk);
        if (contextChunk != chunks.end()) {
            array<Byte>^ managedArray = nullptr;
            System::IO::MemoryStream^ memStream = nullptr;
            try
            {
                managedArray = AsManagedArray(AsPointer(contextChunk));
                memStream = gcnew System::IO::MemoryStream(managedArray);
                context = LoadContext(memStream);
            }
            finally
            {
                delete memStream;
                delete managedArray;
            }
        } else {
            context = gcnew NodeGraphContext();
        }
    }

#if 0
	static void WriteTechniqueConfigSection(
		System::IO::StreamWriter^ sw,
		String^ section, String^ entryPoint, 
		Dictionary<String^, String^>^ shaderParams)
	{
		sw->Write("~"); sw->Write(section); sw->WriteLine();

        // Sometimes we can attach restrictions or defaults to shader parameters -- 
        //      take care of those here...
        if (shaderParams->Count > 0) {
            sw->Write("    ~Parameters"); sw->WriteLine();
            sw->Write("        ~Material"); sw->WriteLine();
            for each(auto i in shaderParams) {
                sw->Write("            ");
                sw->Write(i.Key);
                if (i.Value && i.Value->Length > 0) {
                    sw->Write("=");
                    sw->Write(i.Value);
                }
                sw->WriteLine();
            }
        }

        sw->Write("    PixelShader=<.>:" + entryPoint); sw->WriteLine();
	}
#endif

    void NodeGraphFile::Save(String^ filename, NodeGraphContext^ context)
    {
        // We want to write this node graph to the given stream.
        // But we're going to write a compound text document, which will include
        // the graph in multiple forms.
        // One form will be the XML serialized nodes. Another form will be the
        // HLSL output.

        using namespace System::IO;

        StreamWriter^ sw = nullptr;
        auto stream = gcnew MemoryStream();
        try
        {
            // note --  shader compiler doesn't support the UTF8 BOM properly.
            //          We we have to use an ascii mode
            sw = gcnew System::IO::StreamWriter(stream, System::Text::Encoding::ASCII);
        
            sw->Write("// CompoundDocument:1"); sw->WriteLine();

            auto graphName = Path::GetFileNameWithoutExtension(filename);

            // auto shader = NodeGraph::GenerateShader(nodeGraph, graphName);
            // sw->Write(shader); 

            // embed the node graph in there, as well
            sw->Write("/* <<Chunk:NodeGraph:" + graphName + ">>--("); sw->WriteLine();
            sw->Flush();
            // nodeGraph->SaveToXML(stream); sw->WriteLine();
            sw->Write(")-- */"); sw->WriteLine();

            // embed the node graph context
            sw->Write("/* <<Chunk:NodeGraphContext:" + graphName + ">>--("); sw->WriteLine();
            sw->Flush();
            ShaderPatcherLayer::SaveToXML(stream, context); sw->WriteLine();
            sw->Write(")-- */"); sw->WriteLine();
            sw->Flush();

#if 0
            // also embedded a technique config, if requested
            if (context->HasTechniqueConfig) {

				sw->WriteLine();

				// Unfortunately have to do all of this again to
				// get at the MainFunctionParameters object
				try {
					auto nativeGraph = nodeGraph->ConvertToNative();
					ShaderPatcher::NodeGraphSignature interf;
					std::string shaderBody;
					std::tie(shaderBody, interf) = ShaderPatcher::GenerateFunction(nativeGraph);
					auto str = ShaderPatcher::GenerateStructureForTechniqueConfig(interf, "graph");
					sw->Write(clix::marshalString<clix::E_UTF8>(str));
				} catch (const std::exception& e) {
					sw->Write("Exception while generating technique entry points: " + clix::marshalString<clix::E_UTF8>(e.what()));
				} catch (...) {
					sw->Write("Unknown exception while generating technique entry points");
				}

                sw->WriteLine();
                sw->Write("/* <<Chunk:TechniqueConfig:main>>--("); sw->WriteLine();
                sw->Write("~Inherit; xleres/techniques/illum.tech"); sw->WriteLine();

				WriteTechniqueConfigSection(sw, "Forward", "forward_main", context->ShaderParameters);
                WriteTechniqueConfigSection(sw, "Deferred", "deferred_main", context->ShaderParameters);
				WriteTechniqueConfigSection(sw, "OrderIndependentTransparency", "oi_main", context->ShaderParameters);
                WriteTechniqueConfigSection(sw, "StochasticTransparency", "stochastic_main", context->ShaderParameters);
                WriteTechniqueConfigSection(sw, "DepthOnly", "depthonly_main", context->ShaderParameters);
                
                sw->Write(")--*/"); sw->WriteLine();
                sw->WriteLine();
                sw->Flush();
            }

                // write out a cb layout, as well (sometimes required even if it's not a technique config)
            auto cbLayout = NodeGraph::GenerateCBLayout(nodeGraph);
            if (!String::IsNullOrEmpty(cbLayout)) {
                sw->Write("/* <<Chunk:CBLayout:main>>--("); sw->WriteLine();
                sw->Write(cbLayout); sw->WriteLine();
                sw->Write(")--*/"); sw->WriteLine();
                sw->WriteLine();
                sw->Flush();
            }
#endif

                // If we wrote to the memory stream successfully, we can write to disk -- 
                // maybe we could alternatively write to a temporary file in the same directory,
                // and then move the new file over the top...?
            auto fileMode = File::Exists(filename) ? FileMode::Truncate : FileMode::OpenOrCreate;
            auto fileStream = gcnew FileStream(filename, fileMode);
            try
            {
                stream->WriteTo(fileStream);
                fileStream->Flush();
            }
            finally
            {
                delete fileStream;
            }
        }
        finally
        {
            delete sw;
            delete stream;
        }
    }

}
