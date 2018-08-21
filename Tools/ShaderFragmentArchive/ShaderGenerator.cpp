// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderGenerator.h"
#include "ShaderFragmentArchive.h"
#include "PreviewRenderManager.h"
#include "../GUILayer/MarshalString.h"
#include "../../ShaderParser/ShaderPatcher.h"
#include "../../ShaderParser/GraphSyntax.h"
#include "../../ShaderParser/NodeGraphProvider.h"
#include "../../ShaderParser/ShaderInstantiation.h"
#include "../../Assets/ConfigFileContainer.h"
#include "../../Assets/IFileSystem.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Conversion.h"
#include <sstream>

#pragma warning (disable:4505) // 'ShaderPatcherLayer::StateTypeToString': unreferenced local function has been removed

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
        _connections    = gcnew List<Connection^>();
    }
	
///////////////////////////////////////////////////////////////////////////////////////////////////

    static ShaderPatcher::Node::Type     ConvertToNative(Node::Type e)
    {
        switch (e) {
        default:
        case Node::Type::Procedure:					return ShaderPatcher::Node::Type::Procedure;
        case Node::Type::Uniforms:                  return ShaderPatcher::Node::Type::Uniforms;
        }
    }

	static std::string StateTypeToString(StateType vn)
	{
		if (vn == StateType::Collapsed) return "Collapsed";
		return "Normal";
	}

	static StateType StateTypeFromString(const std::string& vn)
	{
		if (vn == "Collapsed") return StateType::Collapsed;
		return StateType::Normal;
	}

	String^ PreviewSettings::PreviewGeometryToString(PreviewGeometry geo)
	{
		switch (geo) {
		case PreviewGeometry::Chart: return "chart";
		default:
		case PreviewGeometry::Plane2D: return "plane2d";
		case PreviewGeometry::Box: return "box";
		case PreviewGeometry::Sphere: return "sphere";
		case PreviewGeometry::Model: return "model";
		}
	}

	PreviewGeometry PreviewSettings::PreviewGeometryFromString(String^ input)
	{
		if (!String::Compare(input, "chart", true)) return PreviewGeometry::Chart;
		if (!String::Compare(input, "plane2d", true)) return PreviewGeometry::Box;
		if (!String::Compare(input, "box", true)) return PreviewGeometry::Sphere;
		if (!String::Compare(input, "model", true)) return PreviewGeometry::Model;
		return PreviewGeometry::Plane2D;
	}

	static std::tuple<StringSection<>, StringSection<>> SplitArchiveName(StringSection<> archiveName)
    {
        auto pos = std::find(archiveName.begin(), archiveName.end(), ':');
        if (pos != archiveName.end()) {
            return std::make_tuple(MakeStringSection(archiveName.begin(), pos), MakeStringSection(pos+1, archiveName.end()));
        } else {
            return std::make_tuple(StringSection<>(), archiveName);
        }
    }

	static std::string CompressImportedName(const std::string& name, ConversionContext& context)
	{
		auto splitName = SplitArchiveName(name);
		if (std::get<0>(splitName).IsEmpty())
			return name;

		char imprt[MaxPath];
		MakeSplitPath(std::get<0>(splitName)).Rebuild(imprt);
		auto existing = std::find_if(
			context._importTable.begin(), context._importTable.end(), 
			[imprt](const std::pair<std::string, std::string>& p) { return p.second == imprt; } );
		if (existing == context._importTable.end()) {
			for (unsigned pass=0;; ++pass) {
				std::string attempt = MakeFileNameSplitter(imprt).File().AsString();
				if (pass != 0) attempt += std::to_string(pass);
				if (context._importTable.find(attempt) == context._importTable.end()) {
					existing = context._importTable.insert({attempt, std::string(imprt)}).first;
					break;
				}
			}
		}

		return existing->first + "::" + std::get<1>(splitName).AsString();
	}

	static std::string ExpandImportedName(const std::string& name, const ConversionContext& context)
	{
		auto doubleColons = name.begin();
		for (; doubleColons!=name.end() && (doubleColons+1)!=name.end(); ++doubleColons)
			if (*doubleColons == ':' && *(doubleColons+1) == ':')
				break;

		if (doubleColons==name.end() || (doubleColons+1)==name.end()) return name;

		auto existing = context._importTable.find(std::string(name.begin(), doubleColons));
		if (existing != context._importTable.end())
			return existing->second + std::string(doubleColons+1, name.end());	// note downgrade to single colon here

		return name;
	}

    using namespace clix;
    static ShaderPatcher::Node                  ConvertToNative(Node^ node, ConversionContext& context)
    {
        return ShaderPatcher::Node{
            CompressImportedName(marshalString<E_UTF8>(node->FragmentArchiveName), context), 
			node->NodeId, ConvertToNative(node->NodeType),
			marshalString<E_UTF8>(node->AttributeTableName)};
    }
    
    static ShaderPatcher::Connection        ConvertToNative(Connection^ connection)
    {
        return ShaderPatcher::Connection{
            connection->InputNodeID, marshalString<E_UTF8>(connection->InputParameterName),
			connection->OutputNodeID, marshalString<E_UTF8>(connection->OutputParameterName)};
    }
    
    ShaderPatcher::NodeGraph        NodeGraph::ConvertToNative(ConversionContext& context)
    {
        ShaderPatcher::NodeGraph res;
        for each(Node^ n in Nodes)
            res.Add(ShaderPatcherLayer::ConvertToNative(n, context));
        for each(Connection^ c in Connections)
            res.Add(ShaderPatcherLayer::ConvertToNative(c));
        return res;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	static Node::Type     ConvertFromNative(ShaderPatcher::Node::Type e)
    {
        switch (e) {
        default:
		case ShaderPatcher::Node::Type::Procedure : return Node::Type::Procedure;
		case ShaderPatcher::Node::Type::Uniforms: return Node::Type::Uniforms;
        }
    }

	static Node^ ConvertFromNative(const ShaderPatcher::Node& node, const ConversionContext& context)
    {
        Node^ result = gcnew Node;
		result->FragmentArchiveName = marshalString<E_UTF8>(ExpandImportedName(node.ArchiveName(), context));
		result->NodeId = node.NodeId();
		result->NodeType = ConvertFromNative(node.GetType());
		result->AttributeTableName = marshalString<E_UTF8>(node.AttributeTableName());
		return result;
    }
    
    static Connection^ ConvertFromNative(const ShaderPatcher::Connection& connection)
    {
		Connection^ result = gcnew Connection;
		result->OutputNodeID = connection.OutputNodeId(); 
		result->OutputParameterName = marshalString<E_UTF8>(connection.OutputParameterName());
		result->InputNodeID = connection.InputNodeId();
		result->InputParameterName = marshalString<E_UTF8>(connection.InputParameterName());
		return result;
    }

	NodeGraph^ NodeGraph::ConvertFromNative(const ShaderPatcher::NodeGraph& input, const ConversionContext& context)
	{
		NodeGraph^ result = gcnew NodeGraph;
		for (const auto& n:input.GetNodes())
			result->Nodes->Add(ShaderPatcherLayer::ConvertFromNative(n, context));
		for (const auto& c:input.GetConnections())
			result->Connections->Add(ShaderPatcherLayer::ConvertFromNative(c));
		return result;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	static NodeGraphSignature::ParameterDirection AsManagedParameterDirections(ShaderPatcher::ParameterDirection input)
	{
		switch (input) {
		default:
		case ShaderPatcher::ParameterDirection::In:
			return NodeGraphSignature::ParameterDirection::In;
		case ShaderPatcher::ParameterDirection::Out:
			return NodeGraphSignature::ParameterDirection::Out;
		}
	}

	static ShaderPatcher::ParameterDirection AsNativeParameterDirections(NodeGraphSignature::ParameterDirection input)
	{
		switch (input) {
		default:
		case NodeGraphSignature::ParameterDirection::In:
			return ShaderPatcher::ParameterDirection::In;
		case NodeGraphSignature::ParameterDirection::Out:
			return ShaderPatcher::ParameterDirection::Out;
		}
	}
	
	ShaderPatcher::NodeGraphSignature	NodeGraphSignature::ConvertToNative(ConversionContext& context)
	{
		ShaderPatcher::NodeGraphSignature result;
		for each(auto p in Parameters) {
			result.AddParameter(
				ShaderPatcher::NodeGraphSignature::Parameter {
					p->Type ? clix::marshalString<clix::E_UTF8>(p->Type) : std::string(),
					clix::marshalString<clix::E_UTF8>(p->Name),
					AsNativeParameterDirections(p->Direction),
					p->Semantic ? clix::marshalString<clix::E_UTF8>(p->Semantic) : std::string(),
					p->Default ? clix::marshalString<clix::E_UTF8>(p->Default) : std::string()});
		}
		for each(auto p in CapturedParameters) {
			result.AddCapturedParameter(
				ShaderPatcher::NodeGraphSignature::Parameter {
					p->Type ? clix::marshalString<clix::E_UTF8>(p->Type) : std::string(),
					clix::marshalString<clix::E_UTF8>(p->Name),
					AsNativeParameterDirections(p->Direction),
					p->Semantic ? clix::marshalString<clix::E_UTF8>(p->Semantic) : std::string(),
					p->Default ? clix::marshalString<clix::E_UTF8>(p->Default) : std::string()});
		}
		for each(auto p in TemplateParameters) {
			result.AddTemplateParameter(
				ShaderPatcher::NodeGraphSignature::TemplateParameter {
					clix::marshalString<clix::E_UTF8>(p->Name),
					clix::marshalString<clix::E_UTF8>(p->Restriction)});
		}

		return result;
	}

	NodeGraphSignature^			NodeGraphSignature::ConvertFromNative(const ShaderPatcher::NodeGraphSignature& input, const ConversionContext& context)
	{
		NodeGraphSignature^ result = gcnew NodeGraphSignature;
		result->_parameters = gcnew List<Parameter^>();
		for (auto&p:input.GetParameters()) {
			Parameter^ param = gcnew Parameter;
			param->Type = clix::marshalString<clix::E_UTF8>(p._type);
			param->Name = clix::marshalString<clix::E_UTF8>(p._name);
			param->Direction = AsManagedParameterDirections(p._direction);
			param->Semantic = clix::marshalString<clix::E_UTF8>(p._semantic);
			param->Default = clix::marshalString<clix::E_UTF8>(p._default);
			result->_parameters->Add(param);
		}
		result->_capturedParameters = gcnew List<Parameter^>();
		for (auto&p:input.GetCapturedParameters()) {
			Parameter^ param = gcnew Parameter;
			param->Type = clix::marshalString<clix::E_UTF8>(p._type);
			param->Name = clix::marshalString<clix::E_UTF8>(p._name);
			param->Direction = AsManagedParameterDirections(p._direction);
			param->Semantic = clix::marshalString<clix::E_UTF8>(p._semantic);
			param->Default = clix::marshalString<clix::E_UTF8>(p._default);
			result->_capturedParameters->Add(param);
		}
		result->_templateParameters = gcnew List<TemplateParameter^>();
		for (auto&p:input.GetTemplateParameters()) {
			TemplateParameter^ param = gcnew TemplateParameter;
			param->Name = clix::marshalString<clix::E_UTF8>(p._name);
			param->Restriction = clix::marshalString<clix::E_UTF8>(p._restriction);
			result->_templateParameters->Add(param);
		}
		return result;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class GraphNodeGraphProvider : public ShaderPatcher::BasicNodeGraphProvider, std::enable_shared_from_this<GraphNodeGraphProvider>
    {
    public:
        std::optional<Signature> FindSignature(StringSection<> name);
		std::optional<NodeGraph> FindGraph(StringSection<> name);

        GraphNodeGraphProvider(
			NodeGraphFile^ parsedGraphFile,
			const std::unordered_map<std::string, std::string>& imports,
			const ::Assets::DirectorySearchRules& searchRules);
        ~GraphNodeGraphProvider();
    protected:
		gcroot<NodeGraphFile^> _parsedGraphFile;
		const std::unordered_map<std::string, std::string>* _imports;
    };

	auto GraphNodeGraphProvider::FindSignature(StringSection<> name) -> std::optional<Signature>
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
			auto import = MakeStringSection(name.begin(), scopingOperator-1).AsString();
			auto functionName = MakeStringSection(scopingOperator+1, name.end());

			auto importedName = _imports->find(import);
			if (importedName != _imports->end())
				return BasicNodeGraphProvider::FindSignature(importedName->second + ":" + functionName.AsString());
			return BasicNodeGraphProvider::FindSignature(import + ":" + functionName.AsString());
		}

		// Look for the function within the parsed graph syntax file
		NodeGraphFile::SubGraph^ subGraph = nullptr;
		System::String^ str = clix::marshalString<clix::E_UTF8>(name);
		if (_parsedGraphFile->SubGraphs->TryGetValue(str, subGraph)) {
			ConversionContext convContext;
			return Signature{ name.AsString(), subGraph->Signature->ConvertToNative(convContext) };
		}

		// Just fallback to default behaviour
		return BasicNodeGraphProvider::FindSignature(name);
	}

	auto GraphNodeGraphProvider::FindGraph(StringSection<> name) -> std::optional<NodeGraph>
	{
		auto *scopingOperator = name.begin() + 1;
		while (scopingOperator < name.end()) {
			if (*(scopingOperator-1) == ':' && *scopingOperator == ':')
				break;
			++scopingOperator;
		}
		if (scopingOperator < name.end()) {
			auto import = MakeStringSection(name.begin(), scopingOperator-1).AsString();
			auto functionName = MakeStringSection(scopingOperator+1, name.end());

			auto importedName = _imports->find(import);
			if (importedName != _imports->end())
				return BasicNodeGraphProvider::FindGraph(importedName->second + ":" + functionName.AsString());
			return BasicNodeGraphProvider::FindGraph(import + ":" + functionName.AsString());
		}

		// Look for the function within the parsed graph syntax file
		NodeGraphFile::SubGraph^ subGraph = nullptr;
		System::String^ str = clix::marshalString<clix::E_UTF8>(name);
		if (_parsedGraphFile->SubGraphs->TryGetValue(str, subGraph)) {
			ConversionContext convContext;
			return NodeGraph{ name.AsString(), subGraph->Graph->ConvertToNative(convContext), subGraph->Signature->ConvertToNative(convContext), shared_from_this() };
		}

		// Just fallback to default behaviour
		return BasicNodeGraphProvider::FindGraph(name);
	}

	GraphNodeGraphProvider::GraphNodeGraphProvider(
		NodeGraphFile^ parsedGraphFile,
		const std::unordered_map<std::string, std::string>& imports,
		const ::Assets::DirectorySearchRules& searchRules)
	: BasicNodeGraphProvider(searchRules)
	, _parsedGraphFile(parsedGraphFile)
	, _imports(&imports)
	{}

	GraphNodeGraphProvider::~GraphNodeGraphProvider()
	{}

	std::shared_ptr<ShaderPatcher::INodeGraphProvider> MakeGraphSyntaxProvider(
		NodeGraphFile^ parsedGraphFile,
		const std::unordered_map<std::string, std::string>& imports,
		const ::Assets::DirectorySearchRules& searchRules)
	{
		return std::make_shared<GraphNodeGraphProvider>(parsedGraphFile, imports, searchRules);
	}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	static std::unordered_map<std::string, std::string> ConvertToNative(AttributeTable^ input)
	{
		std::unordered_map<std::string, std::string> result;
		for each(KeyValuePair<String^, String^> entry in input)
			result.emplace(std::make_pair(
				marshalString<E_UTF8>(entry.Key),
				marshalString<E_UTF8>(entry.Value)));
		return result;
	}

	static AttributeTable^ ConvertFromNative(const std::unordered_map<std::string, std::string>& input)
	{
		AttributeTable^ result = gcnew AttributeTable;
		for (const auto&entry:input)
			result->Add(marshalString<E_UTF8>(entry.first), marshalString<E_UTF8>(entry.second));
		return result;
	}

	ShaderPatcher::GraphSyntaxFile	NodeGraphFile::ConvertToNative()
	{
		ConversionContext context;
		ShaderPatcher::GraphSyntaxFile result;
		for each(KeyValuePair<String^, SubGraph^> entry in SubGraphs) {
			result._subGraphs.insert(
				std::make_pair(
					clix::marshalString<clix::E_UTF8>(entry.Key),
					ShaderPatcher::GraphSyntaxFile::SubGraph {
						entry.Value->Signature->ConvertToNative(context), 
						entry.Value->Graph->ConvertToNative(context) }));
		}
		for each(KeyValuePair<String^, AttributeTable^> at in AttributeTables) {
			result._attributeTables.emplace(
				std::make_pair(marshalString<E_UTF8>(at.Key), ShaderPatcherLayer::ConvertToNative(at.Value)));
		}
		result._imports = std::move(context._importTable);
		return result;
	}

	NodeGraphFile^			NodeGraphFile::ConvertFromNative(const ShaderPatcher::GraphSyntaxFile& input, const ::Assets::DirectorySearchRules& searchRules)
	{
		ConversionContext context;
		context._importTable = input._imports;
		NodeGraphFile^ result = gcnew NodeGraphFile;
		for (const auto&p:input._subGraphs) {
			NodeGraphFile::SubGraph^ subGraph = gcnew NodeGraphFile::SubGraph;
			subGraph->Signature = NodeGraphSignature::ConvertFromNative(p.second._signature, context);
			subGraph->Graph = NodeGraph::ConvertFromNative(p.second._graph, context);
			result->SubGraphs->Add(clix::marshalString<clix::E_UTF8>(p.first), subGraph);

			for (const auto&at:input._attributeTables) {
				result->AttributeTables->Add(
					marshalString<E_UTF8>(at.first),
					ShaderPatcherLayer::ConvertFromNative(at.second));
			}
		}
		result->_searchRules = gcnew GUILayer::DirectorySearchRules(searchRules);
		return result;
	}

	Tuple<String^, String^>^ 
		NodeGraphFile::GeneratePreviewShader(
			String^ subGraphName,
			UInt32 previewNodeId, 
			PreviewSettings^ settings,
			IEnumerable<KeyValuePair<String^, String^>>^ variableRestrictions)
	{
		SubGraph^ subGraph = nullptr;
		if (SubGraphs->TryGetValue(subGraphName, subGraph))
			return subGraph->Graph->GeneratePreviewShader(previewNodeId, this, settings, variableRestrictions);
		return gcnew Tuple<String^, String^>(String::Empty, String::Empty);
	}

	GUILayer::DirectorySearchRules^ NodeGraphFile::GetSearchRules()
	{
		return _searchRules;
	}

	NodeGraphFile::NodeGraphFile()
	{
		_searchRules = gcnew GUILayer::DirectorySearchRules();
	}

	NodeGraphFile::~NodeGraphFile()
	{
		delete _searchRules;
	}

	Tuple<String^, String^>^ 
		NodeGraph::GeneratePreviewShader(
			UInt32 previewNodeId, 
			NodeGraphFile^ nodeGraphFile,
			PreviewSettings^ settings,
			IEnumerable<KeyValuePair<String^, String^>>^ variableRestrictions)
	{
		try
		{
			ConversionContext context;
            auto nativeGraph = ConvertToNative(context);
			nativeGraph.Trim(previewNodeId);
			
			ShaderPatcher::InstantiationParameters instantiationParams {};
			instantiationParams._generateDanglingOutputs = true;

			auto provider = MakeGraphSyntaxProvider(nodeGraphFile, context._importTable, nodeGraphFile->GetSearchRules()->GetNative());
			auto mainInstantiation = ShaderPatcher::InstantiateShader(
				"preview_graph", nativeGraph,  provider,
				instantiationParams);

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
				"preview_graph", mainInstantiation._entryPointSignature, options);

			mainInstantiation._sourceFragments.insert(mainInstantiation._sourceFragments.begin(), "#include \"xleres/System/Prefix.h\"\n");

			mainInstantiation._sourceFragments.push_back(structureForPreview);

			std::stringstream str;
			for (const auto&f:mainInstantiation._sourceFragments) str << f;

            return gcnew Tuple<String^,String^>(
                marshalString<E_UTF8>(str.str()),
                marshalString<E_UTF8>(ShaderPatcher::GenerateMaterialCBuffer(mainInstantiation._entryPointSignature)));
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

	template <typename Type>
		static void SaveToXML(System::IO::Stream^ stream, Type^ obj)
	{
		DataContractSerializer^ serializer = nullptr;
        System::Xml::XmlWriterSettings^ settings = nullptr;
        System::Xml::XmlWriter^ writer = nullptr;

        try
        {
            serializer = gcnew DataContractSerializer(Type::typeid);
            settings = gcnew System::Xml::XmlWriterSettings();
            settings->Indent = true;
            settings->IndentChars = "\t";
            settings->Encoding = System::Text::Encoding::UTF8;

            writer = System::Xml::XmlWriter::Create(stream, settings);
            serializer->WriteObject(writer, obj);
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

    static NodeGraphMetaData^ LoadMetaData(System::IO::Stream^ stream)
    {
        auto serializer = gcnew DataContractSerializer(NodeGraphMetaData::typeid);
        try
        {
            auto o = serializer->ReadObject(stream);
            return dynamic_cast<NodeGraphMetaData^>(o);
        }
        finally { delete serializer; }
    }

    void NodeGraphFile::Load(String^ filename, [Out] NodeGraphFile^% nodeGraph, [Out] NodeGraphMetaData^% context)
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

		// Attempt to read the entire file in "graph syntax"
		auto nativeGraph = ::ShaderPatcher::ParseGraphSyntax(MakeStringSection((const char*)block.get(), (const char*)PtrAdd(block.get(), size)));
		nodeGraph = NodeGraphFile::ConvertFromNative(nativeGraph, ::Assets::DefaultDirectorySearchRules(MakeStringSection(nativeFilename)));

            // now load the context chunk (if it exists)
		auto contextChunk = std::find_if(chunks.cbegin(), chunks.cend(), IsNodeGraphContextChunk);
        if (contextChunk != chunks.end()) {
            array<Byte>^ managedArray = nullptr;
            System::IO::MemoryStream^ memStream = nullptr;
            try
            {
                managedArray = AsManagedArray(AsPointer(contextChunk));
                memStream = gcnew System::IO::MemoryStream(managedArray);
                context = LoadMetaData(memStream);
            }
            finally
            {
                delete memStream;
                delete managedArray;
            }
        } else {
            context = gcnew NodeGraphMetaData();
        }
    }

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

    void NodeGraphFile::Serialize(System::IO::Stream^ stream, String^ name, NodeGraphMetaData^ context)
    {
        // We want to write this node graph to the given stream.
        // But we're going to write a compound text document, which will include
        // the graph in multiple forms.
        // One form will be the XML serialized nodes. Another form will be the
        // HLSL output.

        // note --  shader compiler doesn't support the UTF8 BOM properly.
        //          We we have to use an ascii mode
        System::IO::StreamWriter^ sw = gcnew System::IO::StreamWriter(stream, System::Text::Encoding::ASCII);
        
        sw->Write("// CompoundDocument:1"); sw->WriteLine();

		{
			std::stringstream str;
			ShaderPatcher::Serialize(str, ConvertToNative());
			sw->Write(clix::marshalString<clix::E_UTF8>(str.str()));
			sw->Flush();
		}

        // embed the node graph context
        sw->Write("/* <<Chunk:NodeGraphMetaData:" + name + ">>--("); sw->WriteLine();
        sw->Flush();
        SaveToXML(stream, context); sw->WriteLine();
        sw->Write(")-- */"); sw->WriteLine();
        sw->Flush();

        // also embedded a technique config, if requested
		SubGraph^ subGraphForTechConfig = nullptr;
		if (SubGraphs->TryGetValue("main", subGraphForTechConfig)) {
			ConversionContext convContext;
			auto nativeGraph = subGraphForTechConfig->Graph->ConvertToNative(convContext);
			auto interf = subGraphForTechConfig->Signature->ConvertToNative(convContext);
				
			if (context->HasTechniqueConfig) {
				sw->WriteLine();

				try {
					auto str = ShaderPatcher::GenerateStructureForTechniqueConfig(interf, "main");
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
			auto cbLayout = ShaderPatcher::GenerateMaterialCBuffer(interf);
			if (!cbLayout.empty()) {
				sw->Write("/* <<Chunk:CBLayout:main>>--("); sw->WriteLine();
				sw->Write(clix::marshalString<clix::E_UTF8>(cbLayout)); sw->WriteLine();
				sw->Write(")--*/"); sw->WriteLine();
				sw->WriteLine();
				sw->Flush();
			}
		}
    }

}
