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
            marshalString<E_UTF8>(connection->Semantic));
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
    
    ShaderPatcher::NodeGraph        NodeGraph::ConvertToNative(String^ name)
    {
        ShaderPatcher::NodeGraph res(marshalString<E_UTF8>(name));
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

                // Only when there are no explicit outputs do we attach default outputs -- 
                // The default outputs can get in the way, because sometimes when a function
                // output is ignored, it ends up being considered a "default" output
            if (graph->OutputParameterConnections->Count == 0)
                nativeGraph.AddDefaultOutputs();

            ShaderPatcher::MainFunctionInterface interf(nativeGraph);
            return marshalString<E_UTF8>(
                    ShaderPatcher::GenerateShaderHeader(nativeGraph) 
                +   ShaderPatcher::GenerateShaderBody(nativeGraph, interf));
        } catch (const std::exception& e) {
            return "Exception while generating shader: " + clix::marshalString<clix::E_UTF8>(e.what());
        } catch (...) {
            return "Unknown exception while generating shader";
        }
    }

    String^         NodeGraph::GeneratePreviewShader(
		NodeGraph^ graph, UInt32 previewNodeId, 
		PreviewSettings^ settings, IEnumerable<KeyValuePair<String^, String^>>^ variableRestrictions)
    {
        try
        {
            auto nativeGraph = graph->ConvertToNativePreview(previewNodeId);
            ShaderPatcher::MainFunctionInterface interf(nativeGraph);
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
            return marshalString<E_UTF8>(
                    ShaderPatcher::GenerateShaderHeader(nativeGraph) 
                +   ShaderPatcher::GenerateShaderBody(nativeGraph, interf) 
                +   ShaderPatcher::GenerateStructureForPreview(nativeGraph, interf, options))
                ;
        } catch (const std::exception& e) {
            return "Exception while generating shader: " + clix::marshalString<clix::E_UTF8>(e.what());
        } catch (...) {
            return "Unknown exception while generating shader";
        }
    }

    String^      NodeGraph::GenerateCBLayout(NodeGraph^ graph)
    {
        try
        {
            auto nativeGraph = graph->ConvertToNative("temp");
            ShaderPatcher::MainFunctionInterface interf(nativeGraph);

            std::stringstream str;
                // Input parameters that can be stored in a cbuffer become
                // part of our cblayout
            auto globalParams = interf.GetGlobalParameters();
            for (unsigned c=0; c<globalParams.size(); ++c) {
                if (interf.IsCBufferGlobal(c)) {
                    const auto& p = globalParams[c];
                    str << p._type << " " << p._name << std::endl;
                    // we can also specify a default value here...
                }
            }
            return clix::marshalString<clix::E_UTF8>(str.str());

        } catch (const std::exception& e) {
            return "Exception while generating shader: " + clix::marshalString<clix::E_UTF8>(e.what());
        } catch (...) {
            return "Unknown exception while generating shader";
        }
    }

	NodeGraph::Interface^	NodeGraph::GetInterface(NodeGraph^ graph)
	{
		auto nativeGraph = graph->ConvertToNative("graph");
        ShaderPatcher::MainFunctionInterface interf(nativeGraph);

		auto variables = gcnew List<Interface::Item>();
		for (const auto& i:interf.GetInputParameters()) {
			Interface::Item item;
			item.Type = clix::marshalString<clix::E_UTF8>(i._type);
			item.Name = clix::marshalString<clix::E_UTF8>(i._name);
			item.Semantic = clix::marshalString<clix::E_UTF8>(i._semantic);
			variables->Add(item);
		}

		auto resources = gcnew List<Interface::Item>();
		for (const auto& i2:interf.GetGlobalParameters()) {
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

    void NodeGraph::Load(String^ filename, [Out] NodeGraph^% nodeGraph, [Out] NodeGraphContext^% context)
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

        auto graphChunk = std::find_if(chunks.cbegin(), chunks.cend(), IsNodeGraphChunk);
        if (graphChunk == chunks.end()) 
            throw gcnew System::Exception("Could not find node graph chunk within compound text file");

        auto contextChunk = std::find_if(chunks.cbegin(), chunks.cend(), IsNodeGraphContextChunk);

            // load the graphChunk first
        {
            array<Byte>^ managedArray = nullptr;
            System::IO::MemoryStream^ memStream = nullptr;
            try
            {
                managedArray = AsManagedArray(AsPointer(graphChunk));
                memStream = gcnew System::IO::MemoryStream(managedArray);
                nodeGraph = LoadFromXML(memStream);
            }
            finally
            {
                delete memStream;
                delete managedArray;
            }
        }

            // now load the context chunk (if it exists)
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

    void NodeGraph::Save(String^ filename, NodeGraph^ nodeGraph, NodeGraphContext^ context)
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
            auto sw = gcnew System::IO::StreamWriter(stream, System::Text::Encoding::ASCII);
        
            sw->Write("// CompoundDocument:1"); sw->WriteLine();

            auto graphName = Path::GetFileNameWithoutExtension(filename);

            auto shader = NodeGraph::GenerateShader(nodeGraph, graphName);
            sw->Write(shader); 

            // embed the node graph in there, as well
            sw->Write("/* <<Chunk:NodeGraph:" + graphName + ">>--("); sw->WriteLine();
            sw->Flush();
            nodeGraph->SaveToXML(stream); sw->WriteLine();
            sw->Write(")-- */"); sw->WriteLine();

            // embed the node graph context
            sw->Write("/* <<Chunk:NodeGraphContext:" + graphName + ">>--("); sw->WriteLine();
            sw->Flush();
            ShaderPatcherLayer::SaveToXML(stream, context); sw->WriteLine();
            sw->Write(")-- */"); sw->WriteLine();
            sw->Flush();

            // also embedded a technique config, if requested
            if (context->HasTechniqueConfig) {
                sw->WriteLine();
                sw->Write("/* <<Chunk:TechniqueConfig:main>>--("); sw->WriteLine();
                sw->Write("~Inherit; game/xleres/Illum.txt"); sw->WriteLine();
                sw->Write("~Deferred"); sw->WriteLine();

                // Sometimes we can attach restrictions or defaults to shader parameters -- 
                //      take care of those here...
                auto shaderParams = context->ShaderParameters;
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

                sw->Write("    PixelShader=<.>:" + graphName); sw->WriteLine();
                sw->Write(")--*/"); sw->WriteLine();
                sw->WriteLine();
                sw->Flush();

                // write out a cb layout, as well
                auto cbLayout = NodeGraph::GenerateCBLayout(nodeGraph);
                if (!String::IsNullOrEmpty(cbLayout)) {
                    sw->Write("/* <<Chunk:CBLayout:main>>--("); sw->WriteLine();
                    sw->Write(cbLayout); sw->WriteLine();
                    sw->Write(")--*/"); sw->WriteLine();
                    sw->WriteLine();
                    sw->Flush();
                }
            }

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
