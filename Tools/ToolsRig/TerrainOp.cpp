// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainOp.h"
#include "../../SceneEngine/TerrainUberSurface.h"
#include "../../Assets/IFileSystem.h"
#include "../../ConsoleRig/IProgress.h"
#include "../../OSServices/RawFS.h"
#include "../../Utility/StringFormat.h"
#include "../../OSServices/RawFS.h"
#include "../../Utility/ParameterBox.h"
#include "../../Utility/Threading/ThreadingUtils.h"
#include <thread>

namespace ToolsRig
{
    using namespace SceneEngine;

    class UberSurfaceWriter
    {
    public:
        void* GetData();
        const void* GetData() const;

        UberSurfaceWriter(
            const ::Assets::ResChar outFile[],
            UInt2 dims,
            ImpliedTyping::TypeDesc format);
        ~UberSurfaceWriter();

        UberSurfaceWriter& operator=(const UberSurfaceWriter&) = delete;
        UberSurfaceWriter(const UberSurfaceWriter&) = delete;
    protected:
        MemoryMappedFile _file;
    };

    void* UberSurfaceWriter::GetData()              { return PtrAdd(_file.GetData().begin(), sizeof(TerrainUberHeader)); }
    const void* UberSurfaceWriter::GetData() const  { return PtrAdd(_file.GetData().begin(), sizeof(TerrainUberHeader)); }

    UberSurfaceWriter::UberSurfaceWriter(
        const ::Assets::ResChar outFile[],
        UInt2 dims,
        ImpliedTyping::TypeDesc format)
    {
        _file = ::Assets::MainFileSystem::OpenMemoryMappedFile(
            outFile,
            sizeof(TerrainUberHeader) + dims[0] * dims[1] * format.GetSize(),
            "w");

        TerrainUberHeader hdr;
        hdr._magic = TerrainUberHeader::Magic;
        hdr._width = dims[0];
        hdr._height = dims[1];
        hdr._typeCat = unsigned(format._type);
        hdr._typeArrayCount = format._arrayCount;
        hdr._dummy[0] = hdr._dummy[1] = hdr._dummy[2] = 0;

        *(TerrainUberHeader*)_file.GetData().begin() = hdr;
    }

    UberSurfaceWriter::~UberSurfaceWriter() {}

    void BuildUberSurface(
        const ::Assets::ResChar destinationFile[],
        ITerrainOp& op,
        TerrainUberHeightsSurface& heightsSurface,
        Int2 interestingMins, Int2 interestingMaxs,
        float xyScale, float relativeResolution,
        const TerrainOpConfig& cfg,
        ConsoleRig::IProgress* progress)
    {
        UInt2 outDims(0,0);
        outDims[0] = unsigned(heightsSurface.GetWidth() / relativeResolution);
        outDims[1] = unsigned(heightsSurface.GetHeight() / relativeResolution);

        auto outFormat = op.GetOutputFormat();
        auto bpp = outFormat.GetSize()*8;
        StringMeld<MaxPath, ::Assets::ResChar> tempFile; tempFile << destinationFile << ".building";

        {
            UberSurfaceWriter writer(tempFile.get(), outDims, outFormat);

            auto step = progress ? progress->BeginStep(op.GetName(), outDims[1], true) : nullptr;

            const int border = int(2.f / relativeResolution);
            void* linesDest = writer.GetData();
            auto lineSize = outDims[0]*bpp/8;
            
            auto lineCount = int(outDims[1])-border;
            Interlocked::Value queueLoc = border;

            auto threadFunction = 
                [   &queueLoc, &interestingMins, &interestingMaxs, 
                    border, &outDims, relativeResolution,
                    bpp, &heightsSurface, xyScale, &op, 
                    linesDest, lineSize, lineCount, &step]()
                {
                    auto lineOfSamples = std::make_unique<char[]>(lineSize);
                    op.FillDefault(lineOfSamples.get(), outDims[0]);

                    for (;;) {
                        auto y = Interlocked::Increment(&queueLoc);
                        if (y >= lineCount) return;

                        if (y >= interestingMins[1] && y < interestingMaxs[1]) {
                            for (   int x=std::max(interestingMins[0], border); 
                                    x<std::min(interestingMaxs[0], int(outDims[0])-border); 
                                    ++x) {

                                    Float2 coord = Float2(float(x), float(y)) * relativeResolution;
                                    op.Calculate(PtrAdd(lineOfSamples.get(), x*bpp/8), coord, heightsSurface, xyScale);
                                }
                        } else {
                            op.FillDefault(
                                PtrAdd(lineOfSamples.get(), interestingMins[0]*bpp/8), 
                                std::min(interestingMaxs[0]+1, int(outDims[0])) - interestingMins[0]);
                        }

                        XlCopyMemory(PtrAdd(linesDest, y*lineSize), lineOfSamples.get(), lineSize);
                        if (step) {
                            step->Advance();
                            if (step->IsCancelled()) return;
                        }
                    }
                };

            std::vector<std::thread> threads;
            for (unsigned c=0; c<std::max(1u, cfg._maxThreadCount); ++c)
                threads.emplace_back(std::thread(threadFunction));

            for (auto&t : threads) t.join();

            // fill in the border and any other space untouched...
            {
                auto lineOfSamples = std::make_unique<char[]>(lineSize);
                op.FillDefault(lineOfSamples.get(), outDims[0]);

                for (int y=0;y<border;++y)
                    XlCopyMemory(PtrAdd(linesDest, y*lineSize), lineOfSamples.get(), lineSize);

                for (; queueLoc<int(outDims[1]); ++queueLoc)
                    XlCopyMemory(PtrAdd(linesDest, queueLoc*lineSize), lineOfSamples.get(), lineSize);
            }
        }

        DeleteFile((const utf8*)destinationFile);
        MoveFile((const utf8*)destinationFile, (const utf8*)tempFile.get());
    }


    TerrainOpConfig::TerrainOpConfig() 
    { 
            // "hardware_concurrency" returns 0 if it can't be calculated 
            //  (we'll consider it 1 in that case)
            // let's saturate the CPU with threads by default
        auto hardwareConc = std::thread::hardware_concurrency();
        _maxThreadCount = std::max(1u, hardwareConc);
    }

}

