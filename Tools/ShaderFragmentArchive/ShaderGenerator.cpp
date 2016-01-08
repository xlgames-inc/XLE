// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "stdafx.h"

#include "ShaderGenerator.h"
#include "../GUILayer/MarshalString.h"
#include "../../ShaderParser/ShaderPatcher.h"
#include "../../Assets/ConfigFileContainer.h"
#include "../../Utility/Streams/FileUtils.h"

using namespace System::Runtime::Serialization;

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

    static ShaderPatcher::NodeConstantConnection        ConvertToNative(ConstantConnection^ connection)
    {
        return ShaderPatcher::NodeConstantConnection(
            connection->OutputNodeID,
            marshalString<E_UTF8>(connection->OutputParameterName),
            marshalString<E_UTF8>(connection->Value));
    }
    
    ShaderPatcher::NodeGraph        NodeGraph::ConvertToNative(String^ name)
    {
        ShaderPatcher::NodeGraph res(marshalString<E_UTF8>(name));
        for each(Node^ n in Nodes)
            res.GetNodes().push_back(ShaderPatcherLayer::ConvertToNative(n));
        for each(NodeConnection^ c in NodeConnections)
            res.GetNodeConnections().push_back(ShaderPatcherLayer::ConvertToNative(c));
        for each(ConstantConnection^ c in ConstantConnections)
            res.GetNodeConstantConnections().push_back(ShaderPatcherLayer::ConvertToNative(c));
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
		try
		{
			auto nativeGraph = graph->ConvertToNative(name);
			nativeGraph.AddDefaultOutputs();
			ShaderPatcher::NodeGraph graphOfTemporaries = ShaderPatcher::GenerateGraphOfTemporaries(nativeGraph);
			return marshalString<E_UTF8>(
					ShaderPatcher::GenerateShaderHeader(nativeGraph) 
				+   ShaderPatcher::GenerateShaderBody(nativeGraph, graphOfTemporaries));
		} catch (const std::exception& e) {
			return "Exception while generating shader: " + clix::marshalString<clix::E_UTF8>(e.what());
		} catch (...) {
			return "Unknown exception while generating shader";
		}
    }

    String^         NodeGraph::GeneratePreviewShader(NodeGraph^ graph, UInt32 previewNodeId, String^ outputToVisualize)
    {
		try
		{
			auto nativeGraph = graph->ConvertToNativePreview(previewNodeId);
			ShaderPatcher::NodeGraph graphOfTemporaries = ShaderPatcher::GenerateGraphOfTemporaries(nativeGraph);
			std::string structure = ShaderPatcher::GenerateStructureForPreview(
				nativeGraph, graphOfTemporaries, 
				outputToVisualize ? marshalString<E_UTF8>(outputToVisualize).c_str() : "");
			return marshalString<E_UTF8>(
					ShaderPatcher::GenerateShaderHeader(nativeGraph) 
				+   ShaderPatcher::GenerateShaderBody(nativeGraph, graphOfTemporaries) 
				+   structure)
				;
		} catch (const std::exception& e) {
			return "Exception while generating shader: " + clix::marshalString<clix::E_UTF8>(e.what());
		} catch (...) {
			return "Unknown exception while generating shader";
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

    static bool IsNodeGraphChunk(const ::Assets::TextChunk<char>& chunk) 
        { return XlEqString(chunk._type, "NodeGraph"); }

    NodeGraph^   NodeGraph::Load(String^ filename)
    {
        // Load from a graph model compound text file (that may contain other text chunks)
        // We're going to use a combination of native and managed stuff -- so it's easier
        // if the caller just passes in a filename
        size_t size = 0;
        auto block = LoadFileAsMemoryBlock(
            clix::marshalString<clix::E_UTF8>(filename).c_str(), &size);
        if (!block.get() || !size)
            throw gcnew System::Exception(System::String::Format("Missing or empty file {0}", filename));

        auto chunks = ::Assets::ReadCompoundTextDocument(
            MakeStringSection((const char*)block.get(), (const char*)PtrAdd(block.get(), size)));
        auto ci = std::find_if(chunks.cbegin(), chunks.cend(), IsNodeGraphChunk);
        if (ci == chunks.end()) 
            throw gcnew System::Exception("Could not find node graph chunk within compound text file");

        array<Byte>^ managedArray = nullptr;
        System::IO::MemoryStream^ memStream = nullptr;
        try
        {
            // marshall the native string into a managed array, and from there into
            // a stream... We need to strip off leading whitespace, however (usually
            // there is a leading newline, which confuses the xml loader
            auto begin = ci->_content.begin();
            while (begin != ci->_content.end() && *begin == ' ' || *begin == '\t' || *begin == '\r' || *begin == '\n')
                ++begin;

            size_t contentSize = size_t(ci->_content.end()) - size_t(begin);
            managedArray = gcnew array<Byte>((int)contentSize);
            {
                cli::pin_ptr<Byte> pinned = &managedArray[managedArray->GetLowerBound(0)];
                XlCopyMemory(pinned, begin, contentSize);
            }
            memStream = gcnew System::IO::MemoryStream(managedArray);
            // Then we can just load this XML using the managed serialization functionality
            return LoadFromXML(memStream);
        }
        finally
        {
            delete memStream;
            delete managedArray;
        }
    }

    void NodeGraph::Save(String^ filename)
    {
        // We want to write this node graph to the given stream.
        // But we're going to write a compound text document, which will include
        // the graph in multiple forms.
        // One form will be the XML serialized nodes. Another form will be the
        // HLSL output.

        using namespace System::IO;

        StreamWriter^ sw = nullptr;
        auto fileMode = File::Exists(filename) ? FileMode::Truncate : FileMode::OpenOrCreate;
        auto stream = gcnew FileStream(filename, fileMode);
        try
        {
            // note --  shader compiler doesn't support the UTF8 BOM properly.
            //          We we have to use an ascii mode
            auto sw = gcnew System::IO::StreamWriter(stream, System::Text::Encoding::ASCII);
        
            sw->Write("// CompoundDocument:1"); sw->WriteLine();

            auto shader = NodeGraph::GenerateShader(this, "main");
            sw->Write(shader); 

            // embed the node graph in there, as well
            sw->Write("/* <<Chunk:NodeGraph:main>>--("); sw->WriteLine();
            sw->Flush();
            SaveToXML(stream); sw->WriteLine();
            sw->Write(")-- */"); sw->WriteLine();

            // also embedded a technique config
            sw->WriteLine();
            sw->Write("/* <<Chunk:TechniqueConfig:main>>--("); sw->WriteLine();
            sw->Write("~Inherit; Illum"); sw->WriteLine();
            sw->Write("~Deferred"); sw->WriteLine();
            sw->Write("    PixelShader=<.>:main"); sw->WriteLine();
            sw->Write(")--*/"); sw->WriteLine();
            sw->WriteLine();
            sw->Flush();
        }
        finally
        {
            delete sw;
            delete stream;
        }
    }

}
