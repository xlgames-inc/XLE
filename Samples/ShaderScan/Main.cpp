// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderParser/ShaderSignatureParser.h"
#include "ShaderParser/Exceptions.h"
#include "ShaderParser/ShaderInstantiation.h"
#include "ShaderParser/ShaderPatcher.h"
#include "ShaderParser/GraphSyntax.h"
#include "Assets/IFileSystem.h"
#include "Assets/ConfigFileContainer.h"
#include "Assets/AssetServices.h"
#include "Assets/OSFileSystem.h"
#include "Assets/MountingTree.h"
#include "Assets/Assets.h"
#include "ConsoleRig/GlobalServices.h"
#include "ConsoleRig/Log.h"
#include "ConsoleRig/AttachablePtr.h"
#include "Utility/StringUtils.h"
#include "Utility/Streams/StreamFormatter.h"
#include "Utility/Streams/StreamDOM.h"
#include "Utility/Streams/FileUtils.h"
#include "Utility/Streams/PathUtils.h"
#include "Utility/ParameterBox.h"
#include <iostream>
#include <sstream>

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
            ShaderSourceParser::ParseHLSL(MakeStringSection((const char*)inputFileBlock.get(), (const char*)PtrAdd(inputFileBlock.get(), inputFileSize)));
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

	static ShaderSourceParser::InstantiationRequest_ArchiveName DeserializeInstantiationRequest(InputStreamFormatter<utf8>& formatter)
	{
		ShaderSourceParser::InstantiationRequest_ArchiveName result;

		for (;;) {
			auto next = formatter.PeekNext();
			switch (next) {
			case InputStreamFormatter<utf8>::Blob::AttributeName:
				{
					StringSection<utf8> name, value;
					if (!formatter.TryAttribute(name, value))
						Throw(FormatException("Could not parse attribute", formatter.GetLocation()));
					if (!value.IsEmpty())
						Throw(FormatException("Expecting only a single attribute in each fragment, which is the entry point name, with no value", formatter.GetLocation()));
					if (!result._archiveName.empty())
						Throw(FormatException("Multiple entry points found for a single technique fragment declaration", formatter.GetLocation()));
					result._archiveName = name.Cast<char>().AsString();
					continue;
				}

			case InputStreamFormatter<utf8>::Blob::BeginElement:
				{
					StringSection<utf8> name;
					if (!formatter.TryBeginElement(name))
						Throw(FormatException("Could not parse element", formatter.GetLocation()));
					result._parameterBindings.emplace(
						std::make_pair(
							name.Cast<char>().AsString(),
							DeserializeInstantiationRequest(formatter)));

					if (!formatter.TryEndElement())
						Throw(FormatException("Expecting end element", formatter.GetLocation()));
					continue;
				}

			case InputStreamFormatter<utf8>::Blob::EndElement:
			case InputStreamFormatter<utf8>::Blob::None:
				if (result._archiveName.empty())
					Throw(FormatException("No entry point was specified for fragment ending at marked location", formatter.GetLocation()));
				return result;

			default:
				Throw(FormatException("Unexpected blob while parsing TechniqueFragment", formatter.GetLocation()));
			}
		}
	}

	std::vector<ShaderSourceParser::InstantiationRequest_ArchiveName> DeserializeInstantiationRequests(InputStreamFormatter<utf8>& formatter)
	{
		std::vector<ShaderSourceParser::InstantiationRequest_ArchiveName> result;
		for (;;) {
			auto next = formatter.PeekNext();
			switch (next) {
			case InputStreamFormatter<utf8>::Blob::BeginElement:
				{
					StringSection<utf8> name;
					if (!formatter.TryBeginElement(name))
						Throw(FormatException("Could not parse element", formatter.GetLocation()));
					result.emplace_back(DeserializeInstantiationRequest(formatter));

					if (!formatter.TryEndElement())
						Throw(FormatException("Expecting end element", formatter.GetLocation()));
					continue;
				}

			case InputStreamFormatter<utf8>::Blob::EndElement:
			case InputStreamFormatter<utf8>::Blob::None:
				return result;

			default:
				Throw(FormatException("Unexpected blob while parsing TechniqueFragment list", formatter.GetLocation()));
			}
		}
	}

	static void TestGraphSyntax()
	{
		::Assets::MainFileSystem::GetMountingTree()->Mount(u("xleres"), ::Assets::CreateFileSystem_OS(u("Game/xleres")));

		const char techniqueFragments[] = R"--(
		~fragment
			xleres/Objects/Example/ProcWood/Wood_04_G.graph::Wood_04_G
		~fragment
			xleres/Techniques/Graph/Pass_Deferred.graph::deferred_pass_main
			~perPixel
				xleres/Techniques/Graph/Object_Default.graph::Default_PerPixel
		)--";

		InputStreamFormatter<utf8> formattr { techniqueFragments };
		auto instRequests = DeserializeInstantiationRequests(formattr);

		auto inst = InstantiateShader(MakeIteratorRange(instRequests));
		(void)inst;

		/*auto earlyRejection = ShaderSourceParser::InstantiationParameters::Dependency { "xleres/Techniques/Pass_Standard.sh::EarlyRejectionTest_Default" };
		auto perPixel = ShaderSourceParser::InstantiationParameters::Dependency { 
			"xleres/Techniques/Object_Default.graph::Default_PerPixel",
			{},
			{
				{ "materialSampler", { "xleres/Techniques/Object_Default.sh::MaterialSampler_RMS" } }
			}
		};

		ShaderSourceParser::InstantiationParameters instParams {
			{ "rejectionTest", earlyRejection },
			{ "perPixel", perPixel }
		};
		auto fragments = ShaderSourceParser::InstantiateShader("xleres/Techniques/Pass_Deferred.graph", "main", instParams);

        Log(Verbose) << "--- Output ---" << std::endl;
        for (auto frag=fragments._sourceFragments.rbegin(); frag!=fragments._sourceFragments.rend(); ++frag)
            Log(Verbose) << *frag << std::endl;*/
	}
}

int main(int argc, char *argv[])
{
    ConsoleRig::StartupConfig cfg("shaderscan");
    cfg._setWorkingDir = false;
    cfg._redirectCout = false;
    auto services = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(cfg);
	auto assetServices = ConsoleRig::MakeAttachablePtr<::Assets::Services>();

    TRY {
		ShaderScan::TestGraphSyntax();

        std::string cmdLine;
        for (unsigned c=1; c<unsigned(argc); ++c) {
            if (c!=0) cmdLine += " ";
            cmdLine += argv[c];
        }
        ShaderScan::Execute(MakeStringSection(cmdLine));
    } CATCH (const std::exception& e) {
        Log(Error) << "Hit top level exception. Aborting program!" << std::endl;
        Log(Error) << e.what() << std::endl;
    } CATCH_END

    return 0;
}

