// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../../PlatformRig/AllocationProfiler.h"
#include "../../../ConsoleRig/Log.h"
#include "../../../Utility/Streams/PathUtils.h"
#include "../../../Utility/Streams/FileUtils.h"
#include "../../../Utility/SystemUtils.h"
#include "../../../Core/Exceptions.h"
#include <stdio.h>
#include <random>

    // Note --  when you need to include <windows.h>, generally
    //          prefer to to use the following header ---
    //          This helps prevent name conflicts with 
    //          windows #defines and so forth...
#include "../../../Core/WinAPI/IncludeWindows.h"

namespace Sample
{
    void ExecuteSample();

    static void SetWorkingDirectory()
    {
            //
            //      For convenience, set the working directory to be ../Working 
            //              (relative to the application path)
            //
        nchar_t appPath     [MAX_PATH];
        nchar_t appDir      [MAX_PATH];
        nchar_t workingDir  [MAX_PATH];

        XlGetProcessPath    (appPath, dimof(appPath));
        XlSimplifyPath      (appPath, dimof(appPath), appPath, a2n("\\/"));
        XlDirname           (appDir, dimof(appDir), appPath);
        XlConcatPath        (workingDir, dimof(workingDir), appDir, a2n("..\\Working"));
        XlSimplifyPath      (workingDir, dimof(workingDir), workingDir, a2n("\\/"));
        XlChDir             (workingDir);
    }
}

#include "../../Math/ProjectionMath.h"
#include "../../Utility/TimeUtils.h"
#include "../../Utility/StringFormat.h"
#include <random>

static const float* AsFloatArray(const Float4x4& m) { return &m(0,0); }

unsigned g_TestValue;

static void CullPerformanceTest()
{
    auto worldToProj = Identity<Float4x4>();
    __declspec(align(16)) float alignedWorldToProj[16];
    std::copy(AsFloatArray(worldToProj), AsFloatArray(worldToProj) + 16, alignedWorldToProj);

    std::pair<Float3, Float3> boundingBoxes[10*1024];
    std::mt19937 rng(std::random_device().operator ()());
    float scale = 1000.f / float(rng.max());

    for (unsigned c=0; c<dimof(boundingBoxes); ++c) {
        float x0 = float(rng()) * scale;
        float x1 = float(rng()) * scale;
        float y0 = float(rng()) * scale;
        float y1 = float(rng()) * scale;
        float z0 = float(rng()) * scale;
        float z1 = float(rng()) * scale;
        boundingBoxes[c] = std::make_pair(
            Float3(std::min(x0, x1), std::min(y0, y1), std::min(z0, z1)),
            Float3(std::max(x0, x1), std::max(y0, y1), std::max(z0, z1)));
    }

    const unsigned iterations = 1000u;
    auto frequency = GetPerformanceCounterFrequency();

    ///////////////////////////////////////////////////////////////////////////////////////////////
    auto basicStart = GetPerformanceCounter();
    for (unsigned i=0; i<iterations; ++i) {
        for (unsigned c=0; c<dimof(boundingBoxes); ++c) {
            g_TestValue += unsigned(TestAABB_Basic(worldToProj, boundingBoxes[c].first, boundingBoxes[c].second));
        }
    }
    auto basicEnd = GetPerformanceCounter();

    OutputDebugString(StringMeld<256>() << "TestAABB_Basic result: " << (basicEnd - basicStart) / (frequency / 1000) << "\r\n");

    ///////////////////////////////////////////////////////////////////////////////////////////////
    auto sse2Start = GetPerformanceCounter();
    for (unsigned i=0; i<iterations; ++i) {
        for (unsigned c=0; c<dimof(boundingBoxes); ++c) {
            g_TestValue += unsigned(TestAABB_SSE2(alignedWorldToProj, boundingBoxes[c].first, boundingBoxes[c].second));
        }
    }
    auto sse2End = GetPerformanceCounter();

    OutputDebugString(StringMeld<256>() << "TestAABB_SSE2 result: " << (sse2End - sse2Start) / (frequency / 1000) << "\r\n");

    ///////////////////////////////////////////////////////////////////////////////////////////////
    auto sse3Start = GetPerformanceCounter();
    for (unsigned i=0; i<iterations; ++i) {
        for (unsigned c=0; c<dimof(boundingBoxes); ++c) {
            g_TestValue += unsigned(TestAABB_SSE3(alignedWorldToProj, boundingBoxes[c].first, boundingBoxes[c].second));
        }
    }
    auto sse3End = GetPerformanceCounter();

    OutputDebugString(StringMeld<256>() << "TestAABB_SSE3 result: " << (sse3End - sse3Start) / (frequency / 1000) << "\r\n");

    ///////////////////////////////////////////////////////////////////////////////////////////////
    auto sse4Start = GetPerformanceCounter();
    for (unsigned i=0; i<iterations; ++i) {
        for (unsigned c=0; c<dimof(boundingBoxes); ++c) {
            g_TestValue += unsigned(TestAABB_SSE4(alignedWorldToProj, boundingBoxes[c].first, boundingBoxes[c].second));
        }
    }
    auto sse4End = GetPerformanceCounter();

    OutputDebugString(StringMeld<256>() << "TestAABB_SSE4 result: " << (sse4End - sse4Start) / (frequency / 1000) << "\r\n");

}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    #if CLIBRARIES_ACTIVE == CLIBRARIES_MSVC && defined(_DEBUG)
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_CRT_DF | /*_CRTDBG_CHECK_EVERY_16_DF |*/ _CRTDBG_LEAK_CHECK_DF /*| _CRTDBG_CHECK_ALWAYS_DF*/);
        // _CrtSetBreakAlloc(2497);
    #endif

    using namespace Sample;

        //  There maybe a few basic platform-specific initialisation steps we might need to
        //  perform. We can do these here, before calling into platform-specific code.
    SetWorkingDirectory();
    srand(std::random_device().operator()());

    CullPerformanceTest();

        //  Initialize the "AccumulatedAllocations" profiler as soon as possible, to catch
        //  startup allocation counts.
    PlatformRig::AccumulatedAllocations accumulatedAllocations;
    CreateDirectoryRecursive("int");
    ConsoleRig::Logging_Startup("log.cfg", "int/helloworldlog.txt");
    LogInfo << "------------------------------------------------------------------------------------------";

    TRY {
        Sample::ExecuteSample();
    } CATCH (const std::exception& e) {
        XlOutputDebugString("Hit top-level exception: ");
        XlOutputDebugString(e.what());
        XlOutputDebugString("\n");

        LogAlwaysError << "Hit top level exception. Aborting program!";
        LogAlwaysError << e.what();
        ::MessageBoxA(nullptr, e.what(), "Top level exception", MB_OK);
    } CATCH_END

    return 0;
}
