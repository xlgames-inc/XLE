// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../../PlatformRig/AllocationProfiler.h"
#include "../../../ConsoleRig/Log.h"
#include "../../../ConsoleRig/GlobalServices.h"
#include "../../../Utility/SystemUtils.h"
#include "../../../Core/Exceptions.h"

    // Note --  when you need to include <windows.h>, generally
    //          prefer to to use the following header ---
    //          This helps prevent name conflicts with 
    //          windows #defines and so forth...
    //  (this is only actually required for the "WinMain" signature)
#include "../../../Core/WinAPI/IncludeWindows.h"

namespace Sample
{
    void ExecuteSample();
}

#include "../../../Utility/Streams/XmlStreamFormatter.h"
#include "../../../Utility/Streams/FileUtils.h"
#include "../../../Utility/Streams/StreamDom.h"
#include "../../../Utility/Conversion.h"
#include "../../../ConsoleRig/AttachableLibrary.h"
#include <string>

static std::string Ident(unsigned count)
{
    return std::string(count, ' ');
}

template<typename Section>
    static std::string AsString(const Section& section)
{
    using CharType = std::remove_const<std::remove_reference<decltype(*section._start)>::type>::type;
    return Conversion::Convert<std::string>(
        std::basic_string<CharType>(section._start, section._end));
}

static void TestParser2()
{
    ConsoleRig::AttachableLibrary library("ColladaConversion.dll");
    library.TryAttach();
    auto fn = library.GetFunction<void(*)()>("TestParser");
    fn();
}

static void TestParser()
{
    size_t size = 0;
    auto block = LoadFileAsMemoryBlock("game/testmodels/viper/viper.dae", &size);
    using Formatter = XmlInputStreamFormatter<utf8>;
    Formatter formatter(MemoryMappedInputStream(block.get(), PtrAdd(block.get(), size)));

    TRY
    {
        {
            Formatter formatter(MemoryMappedInputStream(block.get(), PtrAdd(block.get(), size)));
            Document<Formatter> doc(formatter);
            int t = 0;
            (void)t;
        }

        unsigned il = 0;
        for (;;) {
            switch (formatter.PeekNext()) {
            case Formatter::Blob::BeginElement:
                {
                    Formatter::InteriorSection eleName;
                    formatter.TryBeginElement(eleName);
                    LogInfo << Ident(il) << "Begin element: " << AsString(eleName);
                    il += 2;
                    break;
                }

            case Formatter::Blob::EndElement:
                {
                    formatter.TryEndElement();
                    il -= 2;
                    LogInfo << Ident(il) << "End element";
                    break;
                }

            case Formatter::Blob::AttributeName:
                {
                    Formatter::InteriorSection name, value;
                    formatter.TryAttribute(name, value);
                    LogInfo << Ident(il) << AsString(name) << " = " << AsString(value);
                    break;
                }

            case Formatter::Blob::None:
                assert(il == 0);
                return; // finished succeesfully

            default:
                Throw(FormatException("Unexpected blob", formatter.GetLocation()));
                break;
            }
        }

    } CATCH(const FormatException& e) {
        LogWarning << "Encountered exception: " << e.what();
    } CATCH_END
}

#include "../../RenderCore/Assets/ModelRunTime.h"
#include "../../RenderCore/Assets/ModelRunTimeInternal.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../Assets/IntermediateResources.h"
#include "../../Assets/Assets.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/Threading/ThreadingUtils.h"

#pragma warning(disable:4505)   //  unreferenced local function has been removed
static void TestParser3()
{
    auto aservices = std::make_shared<::Assets::Services>(0);
	auto& asyncMan = aservices->GetAsyncMan();
    auto raservices = std::make_shared<RenderCore::Assets::Services>(nullptr);
    raservices->InitColladaCompilers();

	const char sampleAsset[] = "game/model/galleon/galleon.dae";
	using RenderCore::Assets::ModelScaffold;

	{
		using ::Assets::ResChar;
		ResChar intermediateFile[256];
		asyncMan.GetIntermediateStore().MakeIntermediateName(
			intermediateFile, dimof(intermediateFile),
			StringMeld<256, ResChar>() << sampleAsset << "-skin");
		XlDeleteFile((utf8*)intermediateFile);
	}

	auto& scaffold = Assets::GetAssetComp<ModelScaffold>(sampleAsset);
    for (;;) {
		TRY {
            scaffold.GetStaticBoundingBox();
			break;
		} CATCH(Assets::Exceptions::PendingAsset&) {}
		CATCH_END

		Threading::Sleep(64);
		asyncMan.Update();
	}

    int t = 0;
    (void)t;
}

#include "../../ColladaConversion/DLLInterface.h"

static void ParserPerformanceTest()
{
    ConsoleRig::AttachableLibrary lib("ColladaConversion.dll");
    lib.TryAttach();

    #if !TARGET_64BIT
        auto newCreateScaffold = lib.GetFunction<RenderCore::ColladaConversion::CreateColladaScaffoldFn*>(
            "?CreateColladaScaffold@ColladaConversion@RenderCore@@YA?AV?$shared_ptr@VColladaScaffold@ColladaConversion@RenderCore@@@std@@QBD@Z");
        auto newSerializeSkin = lib.GetFunction<RenderCore::ColladaConversion::ModelSerializeFn*>(
            "?SerializeSkin@ColladaConversion@RenderCore@@YA?AV?$shared_ptr@V?$vector@VNascentChunk@ColladaConversion@RenderCore@@V?$allocator@VNascentChunk@ColladaConversion@RenderCore@@@std@@@std@@@std@@ABVColladaScaffold@12@@Z");

        // const ::Assets::ResChar testFile[] = "game/testmodels/ironman/ironman.dae";
        const ::Assets::ResChar testFile[] = "game/model/galleon/galleon.dae";

        for (;;) {
            auto scaffold = (*newCreateScaffold)(testFile);
            auto chunks = (*newSerializeSkin)(*scaffold);
        }

    #endif
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    #if CLIBRARIES_ACTIVE == CLIBRARIES_MSVC && defined(_DEBUG)
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | /*_CRTDBG_CHECK_CRT_DF |*/ /*_CRTDBG_CHECK_EVERY_16_DF |*/ _CRTDBG_LEAK_CHECK_DF /*| _CRTDBG_CHECK_ALWAYS_DF*/);
        // _CrtSetBreakAlloc(18160);
    #endif

    using namespace Sample;

        //  There maybe a few basic platform-specific initialisation steps we might need to
        //  perform. We can do these here, before calling into platform-specific code.

            // ...

        //  Initialize the "AccumulatedAllocations" profiler as soon as possible, to catch
        //  startup allocation counts.
    PlatformRig::AccumulatedAllocations accumulatedAllocations;

    ConsoleRig::GlobalServices services;
    LogInfo << "------------------------------------------------------------------------------------------";

    // TestParser3();
    // TestParser2();
    // TestParser();

    // ParserPerformanceTest();

    TRY {
        Sample::ExecuteSample();
    } CATCH (const std::exception& e) {
        XlOutputDebugString("Hit top-level exception: ");
        XlOutputDebugString(e.what());
        XlOutputDebugString("\n");

        LogAlwaysError << "Hit top level exception. Aborting program!";
        LogAlwaysError << e.what();
        XlMessageBox(e.what(), "Top level exception");
    } CATCH_END

    return 0;
}
