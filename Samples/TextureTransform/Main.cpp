// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../BufferUploads/IBufferUploads.h"
#include "../../BufferUploads/DataPacket.h"
#include "../../BufferUploads/ResourceLocator.h"
#include "../../RenderCore/IDevice.h"
#include "../../ConsoleRig/Log.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include "../../Utility/Streams/StreamDOM.h"
#include "../../Utility/ParameterBox.h"

#include "../../../Core/WinAPI/IncludeWindows.h"

namespace TextureTransform
{
    class TextureResult
    {
    public:
        intrusive_ptr<BufferUploads::DataPacket> _pkt;
        unsigned _format;
        UInt2 _dimensions;

        void SaveTIFF(const ::Assets::ResChar destinationFile[]) const;
    };

    TextureResult ExecuteTransform(
        RenderCore::IDevice& device,
        BufferUploads::IManager& bufferUploads,
        BufferUploads::ResourceLocator& source,
        StringSection<char> shader,
        const ParameterBox& shaderParameters);

    using namespace BufferUploads;

    template<typename Formatter>
        static ParameterBox CreateParameterBox(const DocElementHelper<Formatter>& element)
    {
        ParameterBox result;
        for (auto attr = element.FirstAttribute(); attr; attr = attr.Next())
            result.SetParameter((const utf8*)attr.Name().AsString().c_str(), attr.Value().AsString().c_str());
        return std::move(result);
    }

    void Execute(StringSection<char> cmdLine)
    {
        // We're going to run a simple process that loads a texture file, runs some shader
        // process, and then writes out an output file.
        // This is a command line app; so our instructions should be on the command line.
        // We're going to use a stream formatter & our "Document" asbtraction to interpret
        // the command line. 
        // We could replace the formatter with a version specialized for
        // command lines if we wanted a unix style command line syntax (and, actually, some
        // syntax elements of this formatter [like ';'] might conflict on some OSs.

        MemoryMappedInputStream stream(cmdLine.begin(), cmdLine.end());
        InputStreamFormatter<char> formatter(stream);
        Document<InputStreamFormatter<char>> doc(formatter);

        auto inputFile = doc.Attribute("i").Value();
        auto outputFile = doc.Attribute("o").Value();
        auto shader = doc.Attribute("s").Value();

        if (inputFile.Empty() || outputFile.Empty() || shader.Empty()) {
            return;
        }

            // Attach the buffer uploads DLL
        BufferUploads::AttachLibrary(ConsoleRig::GlobalServices::GetInstance());
        auto cleanup = MakeAutoCleanup([](){ BufferUploads::DetachLibrary(); });

            // we can now construct the render device & buffer uploads manager
        auto device = RenderCore::CreateDevice();
        auto bufferUploads = BufferUploads::CreateManager(device.get());
        
            // construct 
        auto inputPacket = CreateStreamingTextureSource(inputFile.begin(), inputFile.end());
        auto inputTexture = bufferUploads->Transaction_Immediate(
            CreateDesc(
                BindFlag::ShaderResource,
                0, GPUAccess::Read,
                TextureDesc::Empty(), "TextureProcessInput"),
            inputPacket.get());

        if (!inputTexture) {
            LogAlwaysError << "Could not load input texture: (" << inputFile.AsString().c_str() << ")";
            return;
        }
            
            // We need to think about SRGB modes... do we want to do the processing in
            // linear or SRGB space? So we want to write out a linear or SRB texture?
        auto shaderParameters = CreateParameterBox(doc.Element("p"));

        auto resultTexture = ExecuteTransform(
            *device, *bufferUploads, *inputTexture, shader, shaderParameters);
        if (resultTexture._pkt) {
            LogAlwaysError << "Error while performing texture transform";
            return;
        }
        
            // save "readback" as an output texture.
            // We will write a uncompressed format; normally a second command line
            // tool will be used to compress the result.
        resultTexture.SaveTIFF(outputFile.AsString().c_str());
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
{
    ConsoleRig::GlobalServices services("texturetransform");

    TRY {
        TextureTransform::Execute(lpCmdLine);
    } CATCH (const std::exception& e) {
        LogAlwaysError << "Hit top level exception. Aborting program!";
        LogAlwaysError << e.what();
    } CATCH_END

    return 0;
}
