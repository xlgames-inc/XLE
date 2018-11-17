// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
#include "../Assets/IFileSystem.h"
#include "../ShaderParser/InterfaceSignature.h"
#include "../Utility/Streams/FileUtils.h"
#include <CppUnitTest.h>

namespace UnitTests
{
    TEST_CLASS(ShaderParser)
	{
	public:
		TEST_METHOD(ParserAllShaderSources)
		{
            UnitTest_SetWorkingDirectory();
            ConsoleRig::GlobalServices services(GetStartupConfig());

                // Search for all of the shader sources in the xleres directory
            auto inputFiles = RawFS::FindFilesHierarchical("xleres", "*.h", RawFS::FindFilesFilter::File);
            auto inputFiles1 = RawFS::FindFilesHierarchical("xleres", "*.sh", RawFS::FindFilesFilter::File);
            auto inputFiles2 = RawFS::FindFilesHierarchical("xleres", "*.?sh", RawFS::FindFilesFilter::File);

            inputFiles.insert(inputFiles.end(), inputFiles1.begin(), inputFiles1.end());
            inputFiles.insert(inputFiles.end(), inputFiles2.begin(), inputFiles2.end());

                // Now try to parse each one using the shader parser...
                // Look for parsing errors and unsupported syntax

            for (auto& i:inputFiles) {
                size_t blockSize = 0;
                auto memBlock = ::Assets::TryLoadFileAsMemoryBlock(MakeStringSection(i), &blockSize);

                const char* flgId = "FunctionLinkingGraph";
                if (blockSize > XlStringLen(flgId) && 
                    XlEqString(MakeStringSection((const char*)memBlock.get(), (const char*)&memBlock[XlStringLen(flgId)]), flgId))
                    continue;

                auto signature = ShaderSourceParser::BuildShaderFragmentSignature(
                    MakeStringSection((const char*)memBlock.get(), (const char*)PtrAdd(memBlock.get(), blockSize)));

                (void)signature;
            }
        }
    };
}

