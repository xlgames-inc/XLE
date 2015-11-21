// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Transform.h"
#include "MinimalAssetServices.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../BufferUploads/DataPacket.h"
#include "../../BufferUploads/ResourceLocator.h"
#include "../../RenderCore/IDevice.h"
#include "../../ConsoleRig/Log.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../Assets/AssetServices.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include "../../Utility/Streams/StreamDOM.h"
#include "../../Utility/ParameterBox.h"

#include "../../../Core/WinAPI/IncludeWindows.h"

namespace TextureTransform
{
    using namespace BufferUploads;

    template<typename Formatter>
        static ParameterBox CreateParameterBox(const DocElementHelper<Formatter>& element)
    {
        ParameterBox result;
        for (auto attr = element.FirstAttribute(); attr; attr = attr.Next())
            result.SetParameter((const utf8*)attr.Name().AsString().c_str(), attr.Value().AsString().c_str());
        return std::move(result);
    }

    static std::string GetEnv(const char varName[])
    {
        #if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS
            size_t len = 0;
            auto err = getenv_s(&len, nullptr, 0, varName);
            if (err!=0 || len == 0) return std::string();

            std::string result;
            result.resize(len);
            err = getenv_s(&len, AsPointer(result.begin()), result.size(), varName);
            return result.substr(0, len-1); // strip off a null terminator here
        #else
            return std::getenv(varName);
        #endif
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

        auto outputFile = doc.Attribute("o").Value();
        auto shader = doc.Attribute("s").Value();

        if (outputFile.Empty() || shader.Empty()) {
            return;
        }

        auto xleDir = GetEnv("XLE_DIR");
        if (xleDir.empty()) {
            LogAlwaysError << "XLE_DIR environment variable isn't set. Expecting this to be set to root XLE directory";
            LogAlwaysError << "This program loads shaders from the $(XLE_DIR)\\Working\\Game\\xleres folder";
            return;
        }

            // we can now construct basic services
        auto cleanup = MakeAutoCleanup([]() { TerminateFileSystemMonitoring(); });
        auto device = RenderCore::CreateDevice();
        Samples::MinimalAssetServices services(device.get());
            
            // We need to think about SRGB modes... do we want to do the processing in
            // linear or SRGB space? So we want to write out a linear or SRB texture?
        auto shaderParameters = CreateParameterBox(doc.Element("p"));

        auto resultTexture = ExecuteTransform(
            *device, MakeStringSection(xleDir), shader, shaderParameters,
            {
                { "Sky", HosekWilkieSky }
            });
        if (!resultTexture._pkt) {
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
    ConsoleRig::StartupConfig cfg("texturetransform");
    cfg._setWorkingDir = false;
    ConsoleRig::GlobalServices services(cfg);

    TRY {
        TextureTransform::Execute(lpCmdLine);
    } CATCH (const std::exception& e) {
        LogAlwaysError << "Hit top level exception. Aborting program!";
        LogAlwaysError << e.what();
    } CATCH_END

    return 0;
}
