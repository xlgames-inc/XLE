// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../ShaderParser/InterfaceSignature.h"
#include "../../ShaderParser/Exceptions.h"
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
        if (inputFile.Empty()) {
            return;     // expecting "i=<input filename"> on the command line
        }

        std::cout << "Scanning file: " << inputFile.AsString().c_str() << std::endl;
        size_t inputFileSize;
        auto inputFileBlock = LoadFileAsMemoryBlock(inputFile.AsString().c_str(), &inputFileSize);

        TRY {
            ShaderSourceParser::BuildShaderFragmentSignature(
                (const char*)inputFileBlock.get(), inputFileSize);
        } CATCH(const ShaderSourceParser::Exceptions::ParsingFailure& e) {

                // catch the list of errors, and report each one...
            auto errors = e.GetErrors();
            for (auto i:errors) {
                std::cerr 
                    << "\"" << inputFile.AsString().c_str() << "\""
                    << ":(" << i._lineStart << ":" << i._charStart << ")("
                    << i._lineEnd << ":" << i._charEnd << "):error:"
                    << i._message
                    << std::endl;
            }

        } CATCH_END
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

