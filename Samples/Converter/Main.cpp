// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "GeneralCompiler.h"
#include "../../ConsoleRig/Log.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/IFileSystem.h"
#include "../../Assets/MountingTree.h"
#include "../../Assets/OSFileSystem.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Assets/IntermediateAssets.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include "../../Utility/Streams/StreamDOM.h"
#include "../../Utility/Streams/FileSystemMonitor.h"

#include "../../Core/WinAPI/IncludeWindows.h"

namespace Converter
{
    int Execute(StringSection<char> cmdLine)
    {
        // We're going to run a simple process that loads a texture file, runs some shader
        // process, and then writes out an output file.
        // This is a command line app; so our instructions should be on the command line.
        // We're going to use a stream formatter & our "Document" asbtraction to interpret
        // the command line. 
        // We could replace the formatter with a version specialized for
        // command lines if we wanted a unix style command line syntax (and, actually, some
        // syntax elements of this formatter [like ';'] might conflict on some OSs.

        // Sometimes the system will add quotes around our cmdLine (happens when called from groovy)... We need to remove that here
        if (cmdLine.Length() > 2 && *cmdLine.begin() == '"' && *(cmdLine.end()-1) == '"') { ++cmdLine._start; --cmdLine._end; }

        MemoryMappedInputStream stream(cmdLine.begin(), cmdLine.end());
        InputStreamFormatter<char> formatter(stream);
        Document<InputStreamFormatter<char>> doc(formatter);

        auto outputFile = doc.Attribute("o").Value();
        auto inputFile = doc.Attribute("i").Value();

        if (outputFile.IsEmpty() || inputFile.IsEmpty()) {
            LogAlwaysError << "Output file and input required on the command line";
            LogAlwaysError << "Cmdline: " << cmdLine.AsString().c_str();
            return -1;
        }

		const utf8* xleResDir = u("game/xleres");
		::Assets::MainFileSystem::GetMountingTree()->Mount(u("xleres"), ::Assets::CreateFileSystem_OS(MakeStringSection(xleResDir)));

            // we can now construct basic services
        auto cleanup = MakeAutoCleanup([]() { TerminateFileSystemMonitoring(); });
       
		auto aservices = std::make_shared<::Assets::Services>(0);
		auto& compilers = ::Assets::Services::GetAsyncMan().GetIntermediateCompilers();
		auto& store = ::Assets::Services::GetAsyncMan().GetIntermediateStore();
		auto generalCompiler = std::make_shared<GeneralCompiler>();
		compilers.AddCompiler(ConstHash64<'Worl', 'dmap', 'Geo'>::Value, generalCompiler);

		const StringSection<char> inits[] = { inputFile };
		auto marker = compilers.PrepareAsset(ConstHash64<'Worl', 'dmap', 'Geo'>::Value, inits, dimof(inits), store);

		auto pendingCompile = marker->InvokeCompile();
		auto finalState = pendingCompile->StallWhilePending();
		(void)finalState;
        return 0;
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
{
    ConsoleRig::StartupConfig cfg("converter");
    cfg._setWorkingDir = false;
    ConsoleRig::GlobalServices services(cfg);

    TRY {
        return Converter::Execute(lpCmdLine);
    } CATCH (const std::exception& e) {
        LogAlwaysError << "Hit top level exception. Aborting program!";
        LogAlwaysError << e.what();
        return -1;
    } CATCH_END
}
