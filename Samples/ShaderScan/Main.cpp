// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../ShaderParser/ParameterSignature.h"
#include "../../Core/WinAPI/IncludeWindows.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include "../../Utility/Streams/StreamDOM.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/ParameterBox.h"

namespace ShaderScan
{
    void Execute(StringSection<char> cmdLine)
    {
        MemoryMappedInputStream stream(cmdLine.begin(), cmdLine.end());
        InputStreamFormatter<char> formatter(stream);
        Document<InputStreamFormatter<char>> doc(formatter);

        auto inputFile = doc.Attribute("i").Value();
        // std::cout << "Scanning file: " << inputFile.AsString().c_str() << std::endl;
        if (inputFile.Empty()) {
            return;     // expecting "i=<input filename"> on the command line
        }

        size_t inputFileSize;
        auto inputFileBlock = LoadFileAsMemoryBlock(inputFile.AsString().c_str(), &inputFileSize);

        auto sig = ShaderSourceParser::LoadSignature((const char*)inputFileBlock.get(), inputFileSize);

        // Write all of the out to "cout" -- 
        std::cerr << "\"" << inputFile.AsString().c_str() << "\""
            << ":" << "(1:1)" << "(5:1)" 
            << ":" << "error"
            << ":" << "Expected a different token!"
            ;
    }
}

int main(int argc, char *argv[])
{
    ConsoleRig::StartupConfig cfg("shaderscan");
    cfg._setWorkingDir = false;
    cfg._redirectCout = false;
    ConsoleRig::GlobalServices services(cfg);

    TRY {
        std::string cmdLine;
        for (unsigned c=1; c<unsigned(argc); ++c) {
            if (c!=0) cmdLine += " ";
            cmdLine += argv[c];
        }
        ShaderScan::Execute(MakeStringSection(cmdLine));
    } CATCH (const std::exception& e) {
        LogAlwaysError << "Hit top level exception. Aborting program!";
        LogAlwaysError << e.what();
    } CATCH_END

    return 0;
}

