// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainFormat.h"
#include "../../SceneEngine/TerrainUberSurface.h"
#include "../../RenderCore/Resource.h"
#include "../../RenderCore/Metal/Format.h"
#include "../../Assets/ChunkFile.h"
#include "../../Assets/Assets.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/PtrUtils.h"
#include "../../Core/Types.h"

#include <stack>
#include <assert.h>

#include "../../Core/WinAPI/IncludeWindows.h"

namespace RenderCore { 
    extern char VersionString[];
    extern char BuildDateString[];
}

namespace RenderCore { namespace Assets
{
    using namespace SceneEngine;

        //////////////////////////////////////////////////////////////////////////////////////////
    namespace Compression
    {
        enum Enum 
        {
            None,
            QuantRange      ///< high precision min-max range, with low precision values in between
        };
        typedef unsigned Type;
    }

    namespace DownsampleMethod
    {
        enum Enum { Average, Corner };
    }

    class CellDesc
    {
    public:
        class Header
        {
        public:
            unsigned    _treeDepth;
            unsigned    _overlapCount;
            unsigned    _dummy[2];
        };
        Header _hdr;
    };

    class NodeDesc
    {
    public:
        class Header
        {
        public:
            unsigned    _nodeHeaderVersion;
            unsigned    _dataOffset;
            unsigned    _dataSize;
            unsigned    _dimensionsInElements;

                //  The following elements define the texture format and compression
                //  method used.
                //      . _format is just a NativeFormat::Enum that represents the texture format
                //      . Some data needs to be decompressed at run-time. The most common form of
                //          decompression is used by most height data. We store floating point
                //          min and scale values per node, and each element contains a quantized
                //          lower precision height value.
                //      . For compressed texture, _compressionType determines the type of compression
                //          applied. When there are compression parameters (like the min and scale 
                //          height values) _compressionDataSize specifies the amount of extra data stored
                //          per node.
            unsigned            _format;
            Compression::Type   _compressionType;
            unsigned            _compressionDataSize;

            unsigned    _dummy[1];
        };
        Header _hdr;
    };

    static NodeDesc LoadNodeStructure(BasicFile& file)
    {
        NodeDesc result;
        file.Read(&result._hdr, sizeof(result._hdr), 1);
        assert(result._hdr._nodeHeaderVersion == 0);
        return result;
    }

    static const uint64 ChunkType_CoverageScaffold = ConstHash64<'Cove','rage','Scaf','fold'>::Value;
    static const uint64 ChunkType_CoverageData = ConstHash64<'Cove','rage','Data'>::Value;

    //////////////////////////////////////////////////////////////////////////////////////////

    TerrainCell::TerrainCell(const char filename[])
    {
        auto validationCallback = std::make_shared<::Assets::DependencyValidation>();

        std::vector<NodeField> nodeFields;
        std::vector<std::unique_ptr<Node>> nodes;

        BasicFile file(filename, "rb");

            // Load the chunks. We should have 2 chunks:
            //  . cell scaffold
            //  . height map data
        auto chunks = Serialization::ChunkFile::LoadChunkTable(file);

        Serialization::ChunkFile::ChunkHeader scaffoldChunk;
        Serialization::ChunkFile::ChunkHeader heightDataChunk;
        for (auto i=chunks.begin(); i!=chunks.end(); ++i) {
            if (i->_type == ChunkType_CoverageScaffold && !scaffoldChunk._fileOffset)   { scaffoldChunk = *i; }
            if (i->_type == ChunkType_CoverageData && !heightDataChunk._fileOffset)     { heightDataChunk = *i; }
        }

        if (!scaffoldChunk._fileOffset || !heightDataChunk._fileOffset) {
            throw ::Assets::Exceptions::FormatError("Missing correct terrain chunks: %s", filename);
        }

        CellDesc cellDesc;
        file.Seek(scaffoldChunk._fileOffset, SEEK_SET);
        file.Read(&cellDesc._hdr, sizeof(cellDesc._hdr), 1);

        {
            //  nodes are stored as a breadth-first quad tree, starting with
            //  a single node. The amount of data stored in each node may not
            //  be constant, so we can't predict where every node will be stored
            //  in the file.

            nodeFields.reserve(cellDesc._hdr._treeDepth);
            size_t nodeCount = 0;
            for (unsigned l=0; l<cellDesc._hdr._treeDepth; ++l) {
                size_t fieldNodeCount = (1<<l) * (1<<l);
                nodeFields.push_back(
                    NodeField(1<<l, 1<<l, unsigned(nodeCount), unsigned(nodeCount + fieldNodeCount)));
                nodeCount += fieldNodeCount;
            }

            nodes.reserve(nodeCount);
            for (unsigned l=0; l<cellDesc._hdr._treeDepth; ++l) {
                float xyDim = std::pow(2.f, -float(l));

                for (unsigned y=0; y<(1u<<l); ++y) {
                    for (unsigned x=0; x<(1u<<l); ++x) {
                        auto loadInfo = LoadNodeStructure(file);
                        if (loadInfo._hdr._nodeHeaderVersion != 0) {
                            throw ::Assets::Exceptions::FormatError(
                                "Unexpected version number in terrain node file: %s (node header version: %i)", 
                                filename, loadInfo._hdr._nodeHeaderVersion);
                        }

                        float compressionData[2] = { 0.f, 1.f };
                        if (loadInfo._hdr._compressionDataSize) {
                            if (loadInfo._hdr._compressionType == Compression::QuantRange && loadInfo._hdr._compressionDataSize >= (sizeof(float)*2)) {
                                file.Read(compressionData, sizeof(float), 2);
                                file.Seek(loadInfo._hdr._compressionDataSize - sizeof(float)*2, SEEK_CUR);
                            } else {
                                file.Seek(loadInfo._hdr._compressionDataSize, SEEK_CUR);
                            }
                        }

                        Float4x4 localToCell(
                            xyDim, 0.f, 0.f, float(x) * xyDim,
                            0.f, xyDim, 0.f, float(y) * xyDim,
                            0.f, 0.f, compressionData[1], compressionData[0],
                            0.f, 0.f, 0.f, 1.f);

                        auto node = std::make_unique<Node>(
                            localToCell, loadInfo._hdr._dataOffset + heightDataChunk._fileOffset, 
                            loadInfo._hdr._dataSize, loadInfo._hdr._dimensionsInElements);

                        nodes.push_back(std::move(node));
                    }
                }
            }
        }

        _sourceFileName = filename;
        _nodeFields = std::move(nodeFields);
        _nodes = std::move(nodes);

        ::Assets::RegisterFileDependency(validationCallback, filename);
        _validationCallback = std::move(validationCallback);
    }

    static unsigned NodeCountFromTreeDepth(unsigned treeDepth)
    {
        unsigned nodeCount = 0;
        for (unsigned l=0; l<treeDepth; ++l) {
            size_t fieldNodeCount = (1<<l) * (1<<l);
            nodeCount += fieldNodeCount;
        }
        return nodeCount;
    }

    TerrainCellTexture::TerrainCellTexture(const char filename[])
    {
        _nodeTextureByteCount = 0;
        _fieldCount = 0;
        auto validationCallback = std::make_shared<::Assets::DependencyValidation>();

        BasicFile file(filename, "rb");

            // Load the chunks. We should have 2 chunks:
            //  . cell scaffold
            //  . coverage data
        auto chunks = Serialization::ChunkFile::LoadChunkTable(file);

        Serialization::ChunkFile::ChunkHeader scaffoldChunk;
        Serialization::ChunkFile::ChunkHeader coverageDataChunk;
        for (auto i=chunks.begin(); i!=chunks.end(); ++i) {
            if (i->_type == ChunkType_CoverageScaffold && !scaffoldChunk._fileOffset) { scaffoldChunk = *i; }
            if (i->_type == ChunkType_CoverageData && !coverageDataChunk._fileOffset) { coverageDataChunk = *i; }
        }

        if (!scaffoldChunk._fileOffset || !coverageDataChunk._fileOffset) {
            throw ::Assets::Exceptions::FormatError("Missing correct terrain chunks: %s", filename);
        }

        CellDesc cellDesc;
        file.Seek(scaffoldChunk._fileOffset, SEEK_SET);
        file.Read(&cellDesc._hdr, sizeof(cellDesc._hdr), 1);

        std::vector<unsigned> fileOffsetsBreadthFirst;
        
        {
            //  nodes are stored as a breadth-first quad tree, starting with
            //  a single node. The amount of data stored in each node may not
            //  be constant, so we can't predict where every node will be stored
            //  in the file.

            auto nodeCount = NodeCountFromTreeDepth(cellDesc._hdr._treeDepth);
            fileOffsetsBreadthFirst.reserve(nodeCount);
            for (unsigned l=0; l<cellDesc._hdr._treeDepth; ++l) {
                for (unsigned y=0; y<(1u<<l); ++y) {
                    for (unsigned x=0; x<(1u<<l); ++x) {
                        auto loadInfo = LoadNodeStructure(file);
                        fileOffsetsBreadthFirst.push_back(loadInfo._hdr._dataOffset + coverageDataChunk._fileOffset);
                        if (!_nodeTextureByteCount) {
                            _nodeTextureByteCount = loadInfo._hdr._dataSize;
                        } else {
                                // assert all nodes have the same size data
                            assert(loadInfo._hdr._dataSize == _nodeTextureByteCount);
                        }
                    }
                }
            }
        }

        ::Assets::RegisterFileDependency(validationCallback, filename);

        _fieldCount = (unsigned)cellDesc._hdr._treeDepth;
        _sourceFileName = filename;
        _nodeFileOffsets = std::move(fileOffsetsBreadthFirst);
        _validationCallback = std::move(validationCallback);
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename Element> Element Add(Element lhs, Element rhs)            { return lhs + rhs; }
    template<typename Element> Element Divide(Element lhs, unsigned rhs)        { return lhs / rhs; }
    template<typename Element> float AsScalar(Element in)                       { return float(in); }
    template<typename Element> void Zero(Element& dst)                          { dst = Element(0); }

    template<typename A, typename B> std::pair<A, B> Add(std::pair<A, B> lhs, std::pair<A, B> rhs)      { return std::make_pair(lhs.first + rhs.first, lhs.second + rhs.second); }
    template<typename A, typename B> std::pair<A, B> Divide(std::pair<A, B> lhs, unsigned rhs)          { return std::make_pair(A(lhs.first / rhs), B(lhs.second / rhs)); }
    template<typename A, typename B> float AsScalar(std::pair<A, B> in)         { return float(in.first); }
    template<typename A, typename B> void Zero(std::pair<A, B>& dst)            { dst = std::pair<A, B>(A(0), B(0)); }

    template<typename Element> Metal::NativeFormat::Enum AsFormat() { return Metal::NativeFormat::Unknown; }

    template<> Metal::NativeFormat::Enum AsFormat<float>()                      { return Metal::NativeFormat::R32_FLOAT; }
    template<> Metal::NativeFormat::Enum AsFormat<uint16>()                     { return Metal::NativeFormat::R16_UINT; }
    template<> Metal::NativeFormat::Enum AsFormat<uint8>()                      { return Metal::NativeFormat::R8_UINT; }
    template<> Metal::NativeFormat::Enum AsFormat<int16>()                      { return Metal::NativeFormat::R16_SINT; }
    template<> Metal::NativeFormat::Enum AsFormat<int8>()                       { return Metal::NativeFormat::R8_SINT; }
    template<> Metal::NativeFormat::Enum AsFormat<std::pair<float, float>>()    { return Metal::NativeFormat::R32G32_FLOAT; }
    template<> Metal::NativeFormat::Enum AsFormat<std::pair<uint16, uint16>>()  { return Metal::NativeFormat::R16G16_UINT; }
    template<> Metal::NativeFormat::Enum AsFormat<std::pair<uint8, uint8>>()    { return Metal::NativeFormat::R8G8_UINT; }
    template<> Metal::NativeFormat::Enum AsFormat<std::pair<int16, int16>>()    { return Metal::NativeFormat::R16G16_SINT; }
    template<> Metal::NativeFormat::Enum AsFormat<std::pair<int8, int8>>()      { return Metal::NativeFormat::R8G8_SINT; }

    class CoverageDataResult
    {
    public:
        std::vector<uint8> _compressionData;
        Metal::NativeFormat::Enum _nativeFormat;
        unsigned _rawDataSize;

        CoverageDataResult(
            std::vector<uint8>&& compressionData, Metal::NativeFormat::Enum nativeFormat, unsigned rawDataSize)
        : _compressionData(std::forward<std::vector<uint8>>(compressionData))
        , _nativeFormat(nativeFormat)
        , _rawDataSize(rawDataSize) {}
    };

    template<typename Element>
        static CoverageDataResult WriteCoverageData(
            BasicFile& destinationFile, TerrainUberSurface<Element>& surface,
            unsigned startx, unsigned starty, signed downsample, unsigned dimensionsInElements,
            Compression::Enum compression)
    {
        float minValue =  FLT_MAX;
        float maxValue = -FLT_MAX;
        auto sampledValues = std::make_unique<Element[]>(dimensionsInElements*dimensionsInElements);
        XlSetMemory(sampledValues.get(), 0, dimensionsInElements*dimensionsInElements*sizeof(Element));

        unsigned kw = 1<<downsample;
        for (unsigned y=0; y<dimensionsInElements; ++y)
            for (unsigned x=0; x<dimensionsInElements; ++x) {

                    //  "Corner" method is required for the LOD to work correctly on node
                    //  boundaries. We need adjacent tiles to match,
                    //  even if they are at different LOD levels. When a high-LOD tile needs
                    //  to match a low-LOD neighbour, we just skip every second sample.
                    //  So, we have to do the same here, when we downsample.
                const DownsampleMethod::Enum downsampleMethod = DownsampleMethod::Corner;

                    //  first, we need to downsample the source data to get the 
                    //  correct values. Simple box filter currently. I'm not sure
                    //  what the best filter for height data is -- but maybe we
                    //  want to try something that will preserve large details in the 
                    //  distance 
                    //      (ie, so that mountains, etc, don't collapse into nothing)
                Element k; 
                Zero(k);
                if (constant_expression<downsampleMethod == DownsampleMethod::Average>::result()) {
                    for (unsigned ky=0; ky<kw; ++ky)
                        for (unsigned kx=0; kx<kw; ++kx)
                            k = Add(k, surface.GetValue(startx + kw*x + kx, starty + kw*y + ky));
                    k = Divide(k, kw*kw);
                } else if (constant_expression<downsampleMethod == DownsampleMethod::Corner>::result()) {
                    k = surface.GetValue(startx + kw*x, starty + kw*y);
                }

                minValue = std::min(minValue, AsScalar(k));
                maxValue = std::max(maxValue, AsScalar(k));
                sampledValues[y*dimensionsInElements+x] = k;

            }

        if (compression == Compression::QuantRange) {

            auto compressedHeightData = std::make_unique<uint16[]>(dimensionsInElements*dimensionsInElements);
            for (unsigned y=0; y<dimensionsInElements; ++y)
                for (unsigned x=0; x<dimensionsInElements; ++x) {
                    auto s = AsScalar(sampledValues[y*dimensionsInElements+x]);
                    float ch = (s - minValue) * float(0xffff) / (maxValue - minValue);
                    compressedHeightData[y*dimensionsInElements+x] = (uint16)std::min(float(0xffff), std::max(0.f, ch));
                }

                // write all these results to the file...
            auto rawDataSize = sizeof(uint16)*dimensionsInElements*dimensionsInElements;
            destinationFile.Write(compressedHeightData.get(), rawDataSize, 1);

            std::vector<uint8> compressionData;
            compressionData.resize(sizeof(float)*2);
            *(std::pair<float, float>*)AsPointer(compressionData.begin()) = std::make_pair(minValue, (maxValue - minValue) / float(0xffff));
            return CoverageDataResult(std::move(compressionData), Metal::NativeFormat::R16_UINT, rawDataSize);

        } else if (compression == Compression::None) {

            auto rawDataSize = sizeof(Element)*dimensionsInElements*dimensionsInElements;
            destinationFile.Write(sampledValues.get(), rawDataSize, 1);
            return CoverageDataResult(std::vector<uint8>(), AsFormat<Element>(), rawDataSize);

        } else {
            return CoverageDataResult(std::vector<uint8>(), Metal::NativeFormat::Unknown, 0);
        }
    }

    template<typename Element>
        static void WriteCellFromUberSurface(
            const char destinationFile[], TerrainUberSurface<Element>& surface, 
            UInt2 cellMins, UInt2 cellMaxs, unsigned treeDepth, unsigned overlapElements,
            Compression::Enum compression, std::pair<const char*, const char*> versionInfo)
    {
        using namespace Serialization::ChunkFile;

        const unsigned chunkCount = 2;
        SimpleChunkFileWriter outputFile(
            chunkCount, destinationFile, "wb", 
            SimpleChunkFileWriter::ShareMode::Read,
            versionInfo.first, versionInfo.second);

            //  write an area of the uber surface to our native terrain format
        auto nodeCount = NodeCountFromTreeDepth(treeDepth);
        unsigned compressionDataPerNode = 0;
        if (compression == Compression::QuantRange) {
            compressionDataPerNode = sizeof(float)*2;
        }
        std::vector<uint8> nodeHeaders;
        nodeHeaders.resize(nodeCount*(sizeof(NodeDesc::Header) + compressionDataPerNode), 0);

        unsigned uniqueElementsDimension = 
            std::min(cellMaxs[0] - cellMins[0], cellMaxs[1] - cellMins[1]) / (1u<<(treeDepth-1));

        outputFile.BeginChunk(ChunkType_CoverageScaffold, 0, "Scaffold");

        CellDesc::Header cellDescHeader;
        cellDescHeader._treeDepth = treeDepth;
        cellDescHeader._overlapCount = overlapElements;
        XlZeroMemory(cellDescHeader._dummy);
        outputFile.Write(&cellDescHeader, sizeof(cellDescHeader), 1);

            // Skip over the node header array for now
        auto nodeHeaderArray = outputFile.TellP();
        outputFile.Seek(nodeHeaders.size(), SEEK_CUR);

            //  Now write the header for the height data part
            //  At the moment each node has the same amount of height data...
            //  If it varies, we would need a separate pass to calculate the 
            //  total size of this chunk before we write it.
        outputFile.BeginChunk(ChunkType_CoverageData, 0, "Data");

        unsigned heightDataOffsetIterator = 0;
        unsigned nodeIndex = 0;
        for (unsigned l=0; l<treeDepth; ++l) {
            for (unsigned y=0; y<(1u<<l); ++y) {
                for (unsigned x=0; x<(1u<<l); ++x, ++nodeIndex) {
                    NodeDesc::Header nodeHdr;
                    nodeHdr._nodeHeaderVersion = 0;
                    nodeHdr._dimensionsInElements = uniqueElementsDimension + overlapElements;
                    std::fill(nodeHdr._dummy, &nodeHdr._dummy[dimof(nodeHdr._dummy)], 0);

                    signed downsample = treeDepth-1-l;
                    unsigned skip = 1 << downsample;
                    unsigned rawCoordX = cellMins[0] + x * uniqueElementsDimension * skip;
                    unsigned rawCoordY = cellMins[1] + y * uniqueElementsDimension * skip;

                    auto p = WriteCoverageData(
                        outputFile, surface, rawCoordX, rawCoordY,
                        downsample, uniqueElementsDimension + overlapElements, compression);
                    assert(p._compressionData.size() == compressionDataPerNode);

                    nodeHdr._dataOffset = heightDataOffsetIterator;
                    nodeHdr._dataSize = p._rawDataSize;
                    heightDataOffsetIterator += nodeHdr._dataSize;

                    nodeHdr._compressionType = compression;
                    nodeHdr._compressionDataSize = compressionDataPerNode;
                    nodeHdr._format = p._nativeFormat;

                    auto* hdr = (NodeDesc::Header*)PtrAdd(AsPointer(nodeHeaders.begin()), nodeIndex * (sizeof(NodeDesc::Header) + compressionDataPerNode));
                    *hdr = nodeHdr;
                    XlCopyMemory(PtrAdd(hdr, sizeof(NodeDesc::Header)), AsPointer(p._compressionData.begin()), p._compressionData.size());
                }
            }
        }
        assert(nodeIndex == nodeCount);
        outputFile.FinishCurrentChunk();

            // go back and write the node headers in the node header chunk
        outputFile.Seek(nodeHeaderArray, SEEK_SET);
        outputFile.Write(AsPointer(nodeHeaders.begin()), nodeHeaders.size(), 1);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////

    SceneEngine::TerrainCell& TerrainFormat::LoadHeights(const char filename[], bool skipDependsCheck) const
    {
        if (skipDependsCheck) {
            return ::Assets::GetAsset<TerrainCell>(filename);
        }
        return ::Assets::GetAssetDep<TerrainCell>(filename);
    }

    SceneEngine::TerrainCellTexture& TerrainFormat::LoadCoverage(const char filename[]) const
    {
        return ::Assets::GetAssetDep<TerrainCellTexture>(filename);
    }

    void TerrainFormat::WriteCell(
        const char destinationFile[], TerrainUberSurface<float>& surface, 
        UInt2 cellMins, UInt2 cellMaxs, unsigned treeDepth, unsigned overlapElements) const
    {
        WriteCellFromUberSurface(
            destinationFile, surface, cellMins, cellMaxs, treeDepth, overlapElements,
            Compression::QuantRange, std::make_pair(VersionString, BuildDateString));
    }

    void TerrainFormat::WriteCellCoverage_Shadow(
        const char destinationFile[], TerrainUberSurface<ShadowSample>& surface, 
        UInt2 cellMins, UInt2 cellMaxs, unsigned treeDepth, unsigned overlapElements) const
    {
        WriteCellFromUberSurface(
            destinationFile, surface, 
            cellMins, cellMaxs, treeDepth, overlapElements,
            Compression::None, std::make_pair(VersionString, BuildDateString));
    }

}}

