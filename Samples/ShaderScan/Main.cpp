// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderParser/InterfaceSignature.h"
#include "ShaderParser/Exceptions.h"
#include "ShaderParser/GraphSyntax.h"
#include "Assets/IFileSystem.h"
#include "Assets/ConfigFileContainer.h"
#include "Assets/AssetServices.h"
#include "Assets/OSFileSystem.h"
#include "Assets/MountingTree.h"
#include "Assets/Assets.h"
#include "ConsoleRig/GlobalServices.h"
#include "ConsoleRig/Log.h"
#include "ConsoleRig/AttachableInternal.h"
#include "Utility/StringUtils.h"
#include "Utility/Streams/StreamFormatter.h"
#include "Utility/Streams/StreamDOM.h"
#include "Utility/Streams/FileUtils.h"
#include "Utility/ParameterBox.h"
#include <iostream>
#include <sstream>
#include <stack>

namespace ShaderScan
{
    void Execute(StringSection<char> cmdLine)
    {
        MemoryMappedInputStream stream(cmdLine.begin(), cmdLine.end());
        InputStreamFormatter<char> formatter(stream);
        Document<InputStreamFormatter<char>> doc(formatter);

        auto inputFile = doc.Attribute("i").Value();
        if (inputFile.IsEmpty()) {
            return;     // expecting "i=<input filename"> on the command line
        }

        std::cout << "Scanning file: " << inputFile.AsString().c_str() << std::endl;
        size_t inputFileSize;
        auto inputFileBlock = ::Assets::TryLoadFileAsMemoryBlock(inputFile.AsString().c_str(), &inputFileSize);

        TRY {
            ShaderSourceParser::BuildShaderFragmentSignature(MakeStringSection((const char*)inputFileBlock.get(), (const char*)PtrAdd(inputFileBlock.get(), inputFileSize)));
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

    static std::string MakeGraphName(const std::string& baseName, uint64_t instantiationHash = 0)
    {
        if (!instantiationHash) return baseName;
        return baseName + "_" + std::to_string(instantiationHash);
    }

	static void TestGraphSyntax()
	{
		::Assets::MainFileSystem::GetMountingTree()->Mount(u("xleres"), ::Assets::CreateFileSystem_OS(u("Game/xleres")));

		const auto* filename = "xleres/System/SlotPrototype.sh";
		size_t inputFileSize;
        auto inputFileBlock = ::Assets::TryLoadFileAsMemoryBlock(filename, &inputFileSize);
        auto inputStr = MakeStringSection((const char*)inputFileBlock.get(), (const char*)PtrAdd(inputFileBlock.get(), inputFileSize));
        // auto str = ShaderPatcher::ReadGraphSyntax(inputStr, );
        // Log(Verbose) << "--- Output ---\n" << str;

        std::string entryPoint = "main";

        auto graphSyntax = ShaderPatcher::ParseGraphSyntax(inputStr);
        auto main = graphSyntax._subGraphs.find(entryPoint);
        if (main == graphSyntax._subGraphs.end())
            Throw(::Exceptions::BasicLabel("Couldn't find entry point (%s) in input", entryPoint.c_str()));

        auto sigProvider = ShaderPatcher::MakeGraphSyntaxSignatureProvider(graphSyntax, ::Assets::DefaultDirectorySearchRules(filename));

        auto mainInstantiation = 
            ShaderPatcher::GenerateFunction(
                main->second._graph, (MakeGraphName(entryPoint) + "_impl").c_str(), ShaderPatcher::InstantiationParameters {}, *sigProvider);

        std::vector<std::string> fragments;
        fragments.push_back(ShaderPatcher::GenerateScaffoldFunction(main->second._signature, mainInstantiation._signature, MakeGraphName(entryPoint).c_str()));
        fragments.push_back(mainInstantiation._text);

        std::stack<ShaderPatcher::GeneratedFunction> instantiations;
        instantiations.emplace(std::move(mainInstantiation));
        while (!instantiations.empty()) {
            auto inst = std::move(instantiations.top());
            instantiations.pop();

            for (const auto&dep:inst._dependencies._dependencies) {
                if (dep._archiveName.find(':') != std::string::npos) continue;      // exclude functions included from other files

                auto fn = graphSyntax._subGraphs.find(dep._archiveName);
                if (fn == graphSyntax._subGraphs.end())
                    Throw(::Exceptions::BasicLabel("Couldn't find function (%s) in input", dep._archiveName.c_str()));

                auto finalGraphName = MakeGraphName(fn->first, dep._parameters.CalculateHash());
                auto instFn = ShaderPatcher::GenerateFunction(
                    fn->second._graph, 
                    (finalGraphName + "_impl").c_str(), 
                    dep._parameters, *sigProvider);

                fragments.push_back(ShaderPatcher::GenerateScaffoldFunction(fn->second._signature, instFn._signature, finalGraphName.c_str()));
                fragments.push_back(instFn._text);
                
                instantiations.emplace(std::move(instFn));
            }
        }

        Log(Verbose) << "--- Output ---" << std::endl;
        for (auto frag=fragments.rbegin(); frag!=fragments.rend(); ++frag)
            Log(Verbose) << *frag << std::endl;
	}
}

int main(int argc, char *argv[])
{
    ConsoleRig::StartupConfig cfg("shaderscan");
    cfg._setWorkingDir = false;
    cfg._redirectCout = false;
    ConsoleRig::GlobalServices services(cfg);

	::Assets::Services assetServices;
    ConsoleRig::GlobalServices::GetCrossModule().Publish(assetServices);
    assetServices.AttachCurrentModule();

    TRY {
		ShaderScan::TestGraphSyntax();

        std::string cmdLine;
        for (unsigned c=1; c<unsigned(argc); ++c) {
            if (c!=0) cmdLine += " ";
            cmdLine += argv[c];
        }
        ShaderScan::Execute(MakeStringSection(cmdLine));
    } CATCH (const std::exception& e) {
        LogAlwaysError << "Hit top level exception. Aborting program!" << std::endl;
        LogAlwaysError << e.what() << std::endl;
    } CATCH_END

    ConsoleRig::GlobalServices::GetCrossModule().Withhold(assetServices);

    return 0;
}

