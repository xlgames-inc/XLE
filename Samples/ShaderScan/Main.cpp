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
#include "Utility/Streams/PathUtils.h"
#include "Utility/ParameterBox.h"
#include <iostream>
#include <sstream>
#include <stack>
#include <regex>
#include <set>

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


		/*
        auto graphSyntax = std::make_shared<ShaderPatcher::GraphSyntaxFile>(ShaderPatcher::ParseGraphSyntax(inputStr));
        auto main = graphSyntax->_subGraphs.find(entryPoint);
        if (main == graphSyntax->_subGraphs.end())
            Throw(::Exceptions::BasicLabel("Couldn't find entry point (%s) in input", entryPoint.c_str()));

        auto sigProvider = ShaderPatcher::MakeGraphSyntaxSignatureProvider(*graphSyntax, ::Assets::DefaultDirectorySearchRules(filename));


		size_t inputFileSize;
        auto inputFileBlock = ::Assets::TryLoadFileAsMemoryBlock(filename, &inputFileSize);
        auto inputStr = MakeStringSection((const char*)inputFileBlock.get(), (const char*)PtrAdd(inputFileBlock.get(), inputFileSize));
		

        auto mainInstantiation = 
            ShaderPatcher::GenerateFunction(
                main->second._graph, (MakeGraphName(entryPoint) + "_impl").c_str(), instParams, *sigProvider);

        fragments.push_back(ShaderPatcher::GenerateScaffoldFunction(main->second._signature, mainInstantiation._signature, MakeGraphName(entryPoint).c_str()));
        fragments.push_back(mainInstantiation._text);
		*/


		std::set<std::string> includes;

		struct PendingInstantiation
		{
			std::string _archiveName;
			ShaderPatcher::InstantiationParameters _instantiationParams;
			std::shared_ptr<ShaderPatcher::GraphSyntaxFile> _scope;
			std::shared_ptr<ShaderPatcher::ISignatureProvider> _sigProvider;
		};

        std::vector<std::string> fragments;
		std::stack<PendingInstantiation> instantiations;

		{
			std::string entryPoint = "xleres/Techniques/Pass_Deferred.graph::main";

			auto earlyRejection = ShaderPatcher::InstantiationParameters::Dependency { "xleres/Techniques/Pass_Standard.sh::EarlyRejectionTest_Default" };
			auto perPixel = ShaderPatcher::InstantiationParameters::Dependency { 
				"xleres/Techniques/Object_Default.graph::Default_PerPixel",
				{
					{ "materialSampler", { "xleres/Techniques/Object_Default.sh::MaterialSampler_RMS" } }
				}
			};

			ShaderPatcher::InstantiationParameters instParams {
				{ "rejectionTest", earlyRejection },
				{ "perPixel", perPixel }
			};
			instantiations.emplace(PendingInstantiation{entryPoint, instParams, nullptr, nullptr});
		}

        while (!instantiations.empty()) {
            auto inst = std::move(instantiations.top());
            instantiations.pop();

			static std::regex archiveNameRegex(R"--(([\w\.\\/]+)::(\w+))--");
			std::smatch archiveNameMatch;
			if (std::regex_match(inst._archiveName, archiveNameMatch, archiveNameRegex) && archiveNameMatch.size() >= 3) {

				auto filename = archiveNameMatch[1].str();
				auto entryFn = archiveNameMatch[2].str();

				if (inst._scope) {
					auto i = inst._scope->_imports.find(filename);
					if (i != inst._scope->_imports.end())
						filename = i->second;
				}

				// if it's a graph file, then we must create a specific instantiation
				if (XlEqString(MakeFileNameSplitter(filename).Extension(), "graph")) {

					size_t inputFileSize;
					auto inputFileBlock = ::Assets::TryLoadFileAsMemoryBlock(filename, &inputFileSize);
					auto inputStr = MakeStringSection((const char*)inputFileBlock.get(), (const char*)PtrAdd(inputFileBlock.get(), inputFileSize));

					auto graphSyntax = std::make_shared<ShaderPatcher::GraphSyntaxFile>(ShaderPatcher::ParseGraphSyntax(inputStr));
					auto main = graphSyntax->_subGraphs.find(entryFn);
					if (main == graphSyntax->_subGraphs.end())
						Throw(::Exceptions::BasicLabel("Couldn't find entry point (%s) in input", inst._archiveName.c_str()));

					auto sigProvider = ShaderPatcher::MakeGraphSyntaxSignatureProvider(*graphSyntax, ::Assets::DefaultDirectorySearchRules(filename));

					std::string implementationName, scaffoldName;
					if (inst._scope) {
						// Slightly different rules for function name generation with inst._scope is not null. inst._scope is
						// only null for the original instantiation request -- in that case, we want the outer most function
						// to have the same name as the original request
						scaffoldName = MakeGraphName(entryFn, inst._instantiationParams.CalculateHash());
						implementationName = scaffoldName + "_impl";
					} else {
						scaffoldName = entryFn;
						implementationName = scaffoldName + "_impl";
					}

					auto mainInstantiation = ShaderPatcher::GenerateFunction(main->second._graph, implementationName, inst._instantiationParams, *sigProvider);
					auto scaffold = ShaderPatcher::GenerateScaffoldFunction(main->second._signature, mainInstantiation._signature, scaffoldName, implementationName);

					fragments.push_back(scaffold);
					fragments.push_back(mainInstantiation._text);

					for (const auto&dep:mainInstantiation._dependencies._dependencies) {
						instantiations.emplace(
							PendingInstantiation{dep._archiveName, dep._parameters, graphSyntax, sigProvider});
					}

				} else {

					if (!inst._instantiationParams._parameterBindings.empty()) {
						includes.insert(std::string(StringMeld<MaxPath>() << filename + "_" << inst._instantiationParams.CalculateHash()));
					} else {
						auto sig = inst._sigProvider->FindSignature(inst._archiveName);
						includes.insert(sig._sourceFile);
					}

				}
				
            } else if (inst._scope) {

				auto fn = inst._scope->_subGraphs.find(inst._archiveName);
				if (fn == inst._scope->_subGraphs.end())
					Throw(::Exceptions::BasicLabel("Couldn't find function (%s) in input", inst._archiveName.c_str()));

				auto scaffoldName = MakeGraphName(fn->first, inst._instantiationParams.CalculateHash());
				auto implementationName = scaffoldName + "_impl";
				auto instFn = ShaderPatcher::GenerateFunction(
					fn->second._graph,  implementationName, 
					inst._instantiationParams, *inst._sigProvider);

				fragments.push_back(ShaderPatcher::GenerateScaffoldFunction(fn->second._signature, instFn._signature, scaffoldName, implementationName));
				fragments.push_back(instFn._text);
                
				for (const auto&dep:instFn._dependencies._dependencies) {
					instantiations.emplace(
						PendingInstantiation{dep._archiveName, dep._parameters, inst._scope, inst._sigProvider});
				}

			} else
				Throw(::Exceptions::BasicLabel("Unable to handle instantiation request (%s)", inst._archiveName.c_str()));
        }

		{
			std::stringstream str;
			for (const auto&i:includes)
				str << "#include <" << i << ">" << std::endl;
			fragments.push_back(str.str());
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

