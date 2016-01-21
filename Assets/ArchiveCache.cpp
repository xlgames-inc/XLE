// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ArchiveCache.h"
#include "ChunkFile.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/HeapUtils.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/IteratorUtils.h"
#include <algorithm>

namespace Assets
{
    static const uint64 ChunkType_ArchiveDirectory = ConstHash64<'Arch', 'ive', 'Dir'>::Value;
    static const uint64 ChunkType_ArchiveAttachments = ConstHash64<'Arch', 'ive', 'Attc'>::Value;

    class ArchiveDirectoryBlock 
    {
    public:
        uint64 _id;
        unsigned _start, _size;
    };
    
    class DirectoryChunk
    {
    public:
        unsigned _blockCount;
        unsigned _spanningHeapSize;

            // list of blocks follows, and then the flattened spanning heap data
        DirectoryChunk() : _blockCount(0), _spanningHeapSize(0) {}

        class CompareBlock
        {
        public:
            bool operator()(const ArchiveDirectoryBlock& lhs, uint64 rhs) { return lhs._id < rhs; }
            bool operator()(uint64 lhs, const ArchiveDirectoryBlock& rhs) { return lhs < rhs._id; }
            bool operator()(const ArchiveDirectoryBlock& lhs, const ArchiveDirectoryBlock& rhs) { return lhs._id < rhs._id; }
        };
    };

    class AttachedStringChunk
    {
    public:
        class Block
        {
        public:
            uint64 _id;
            unsigned _start, _size;
        };
        unsigned _blockCount;

        AttachedStringChunk() : _blockCount(0) {}

        class CompareBlock
        {
        public:
            bool operator()(const Block& lhs, uint64 rhs) { return lhs._id < rhs; }
            bool operator()(uint64 lhs, const Block& rhs) { return lhs < rhs._id; }
            bool operator()(const Block& lhs, const Block& rhs) { return lhs._id < rhs._id; }
        };
    };

    ArchiveCache::PendingCommit::PendingCommit(PendingCommit&& moveFrom)
        : _id(moveFrom._id)
        , _pendingCommitPtr(moveFrom._pendingCommitPtr)
        , _onFlush(std::move(moveFrom._onFlush))
    {
        _data = std::move(moveFrom._data);
        #if defined(ARCHIVE_CACHE_ATTACHED_STRINGS)
            _attachedString = std::move(moveFrom._attachedString);
        #endif
    }

    auto ArchiveCache::PendingCommit::operator=(PendingCommit&& moveFrom) -> PendingCommit&
    {
        _id = moveFrom._id;
        _pendingCommitPtr = moveFrom._pendingCommitPtr;
        _data = std::move(moveFrom._data);
        #if defined(ARCHIVE_CACHE_ATTACHED_STRINGS)
            _attachedString = std::move(moveFrom._attachedString);
        #endif
        _onFlush = std::move(moveFrom._onFlush);
        return *this;
    }

    ArchiveCache::PendingCommit::PendingCommit(uint64 id, BlockAndSize&& data, const std::string& attachedString, std::function<void()>&& onFlush)
        : _id(id)
        #if defined(ARCHIVE_CACHE_ATTACHED_STRINGS)
        , _attachedString(attachedString)
        #endif
        , _onFlush(std::forward<std::function<void()>>(onFlush))
    {
        _data = std::move(data);
    }

    void ArchiveCache::Commit(uint64 id, BlockAndSize&& data, const std::string& attachedString, std::function<void()>&& onFlush)
    {
            // for for an existing pending commit, and replace it if it exists
        ScopedLock(_pendingBlocksLock);
        auto i = std::lower_bound(_pendingBlocks.begin(), _pendingBlocks.end(), id, ComparePendingCommit());
        if (i!=_pendingBlocks.end() && i->_id == id) {
            i->_data = std::forward<BlockAndSize>(data);
            #if defined(ARCHIVE_CACHE_ATTACHED_STRINGS)
                i->_attachedString = attachedString;
            #endif
            i->_onFlush = std::forward<std::function<void()>>(onFlush);
        } else {
            _pendingBlocks.insert(i, PendingCommit(id, std::forward<BlockAndSize>(data), attachedString, std::forward<std::function<void()>>(onFlush)));
        }
    }

    static std::vector<ArchiveDirectoryBlock> LoadBlockList(const char filename[])
    {
        using namespace Serialization::ChunkFile;
        BasicFile directoryFile(filename, "rb");
        auto chunkTable = LoadChunkTable(directoryFile);
        auto chunk = FindChunk(filename, chunkTable, ChunkType_ArchiveDirectory, 0);

        DirectoryChunk dirHdr;
        directoryFile.Seek(chunk._fileOffset, SEEK_SET);
        directoryFile.Read(&dirHdr, sizeof(dirHdr), 1);

        std::vector<ArchiveDirectoryBlock> blocks;  // note -- use a fixed size vector here?
        blocks.resize(dirHdr._blockCount);
        directoryFile.Read(AsPointer(blocks.begin()), sizeof(ArchiveDirectoryBlock), dirHdr._blockCount);

        return std::move(blocks);
    }

    auto ArchiveCache::GetBlockList() const -> const std::vector<ArchiveDirectoryBlock>&
    {
        if (!_cachedBlockListValid) {
            _cachedBlockList = LoadBlockList(_directoryFileName.c_str());
            _cachedBlockListValid = true;
        }
        return _cachedBlockList;
    }

    auto ArchiveCache::OpenFromCache(uint64 id) -> BlockAndSize
    {
            // first, check our pending commits
            // if it's not there, we have to lock and open the file
            // note that we're keeping the pending block lock permanently locked
        ScopedLock(_pendingBlocksLock);
        auto i = std::lower_bound(_pendingBlocks.begin(), _pendingBlocks.end(), id, ComparePendingCommit());
        if (i!=_pendingBlocks.end() && i->_id == id) {
            return i->_data;
        }

            // lock and open the directory, and look for the given item
            // note that a flush could be happening in a background 
            // thread -- in that case, we need to stall waiting for the
            // flush to complete.

        {
            const auto& blocks = GetBlockList();
                // we maintain the blocks array sorted by id to make this check faster...
            auto bi = std::lower_bound(blocks.begin(), blocks.end(), id, DirectoryChunk::CompareBlock());
            if (bi != blocks.end() && bi->_id == id) {
                BasicFile dataFile(_mainFileName.c_str(), "rb");
                dataFile.Seek(bi->_start, SEEK_SET);

                auto result = std::make_shared<std::vector<uint8>>(bi->_size);
                dataFile.Read(AsPointer(result->begin()), 1, bi->_size);
                return result;
            }
        }

        return nullptr;     // this block doesn't exist in the cache
    }
    
    bool ArchiveCache::HasItem(uint64 id) const
    {
        ScopedLock(_pendingBlocksLock);
        auto i = std::lower_bound(_pendingBlocks.begin(), _pendingBlocks.end(), id, ComparePendingCommit());
        if (i!=_pendingBlocks.end() && i->_id == id) {
            return true;
        }

        TRY {
            const auto& blocks = GetBlockList();
            auto bi = std::lower_bound(blocks.begin(), blocks.end(), id, DirectoryChunk::CompareBlock());
            return (bi != blocks.end() && bi->_id == id);
        } CATCH (...) {
            return false;
        } CATCH_END
    }

    void ArchiveCache::FlushToDisk()
    {
        ScopedLock(_pendingBlocksLock);
        if (_pendingBlocks.empty()) { return; }

        _cachedBlockListValid = false;

            // 1.   Open the directory and initialize our heap
            //      representation
            // 2.   Find older versions of the same blocks we
            //      want to write
            // 3.   Deallocate all blocks that have changed size
            // 4.   Allocate new blocks as required
            // 5.   Open the data file and write all the new blocks
            //      to disk
            // 6.   Flush out the new directory file
            //
            //  Note that the table of blocks is stored in order of id (for fast
            //  searches) not in the order that they appear in the file.

        using namespace Serialization::ChunkFile;
        {
            DirectoryChunk dirHdr;
            std::vector<ArchiveDirectoryBlock> blocks;
            std::unique_ptr<uint8[]> flattenedSpanningHeap;
        
            BasicFile directoryFile;
            bool directoryFileOpened = false;

            TRY {
                directoryFile = BasicFile(_directoryFileName.c_str(), "r+b");

                auto chunkTable = LoadChunkTable(directoryFile);
                auto chunk = FindChunk(_directoryFileName.c_str(), chunkTable, ChunkType_ArchiveDirectory, 0);

                directoryFile.Seek(chunk._fileOffset, SEEK_SET);
                directoryFile.Read(&dirHdr, sizeof(dirHdr), 1);

                blocks.resize(dirHdr._blockCount);
                directoryFile.Read(AsPointer(blocks.begin()), sizeof(ArchiveDirectoryBlock), dirHdr._blockCount);
                flattenedSpanningHeap = std::make_unique<uint8[]>(dirHdr._spanningHeapSize);
                directoryFile.Read(flattenedSpanningHeap.get(), 1, dirHdr._spanningHeapSize);
                directoryFileOpened = true;
            } CATCH (...) {
                    // any exceptions indicate the current file is empty
            } CATCH_END

            if (!directoryFileOpened) {
                directoryFile = BasicFile();
                directoryFile = BasicFile(_directoryFileName.c_str(), "wb");
            }

            SpanningHeap<uint32> spanningHeap(flattenedSpanningHeap.get(), dirHdr._spanningHeapSize);
            for (auto i=_pendingBlocks.begin(); i!=_pendingBlocks.end(); ++i) {
                i->_pendingCommitPtr = ~unsigned(0x0);

                // find an existing block with the same id
                auto b = std::lower_bound(blocks.cbegin(), blocks.cend(), i->_id, DirectoryChunk::CompareBlock());
                if (b != blocks.cend() && b->_id == i->_id) {
                        // todo -- Often we just want to resize the last block. This would be better if
                        //          we could reallocate that last block (by shortening or expanding it)
                    if (b->_size == i->_data->size()) {
                            // same size; just reuse
                        i->_pendingCommitPtr = b->_start;
                    } else {
                            // destroy the old block (new block will be reallocated later)
                        spanningHeap.Deallocate(b->_start, b->_size);
                        blocks.erase(b);
                    }
                }
            }

                // Allocate space for new blocks. Allocate from largest to smallest.
            std::sort(_pendingBlocks.begin(), _pendingBlocks.end(), 
                [](const PendingCommit& lhs, const PendingCommit& rhs) { return lhs._data->size() > rhs._data->size(); });
            for (auto i=_pendingBlocks.begin(); i!=_pendingBlocks.end(); ++i) {
                if (i->_pendingCommitPtr == ~unsigned(0x0)) {

                        // we need to allocate a new block
                    auto newBlockSize = (unsigned)i->_data->size();
                    
                    #if defined(_DEBUG)
                        auto originalHeapSize = spanningHeap.CalculateHeapSize();
                        auto originalAllocatedSize = spanningHeap.CalculateAllocatedSpace();
                    #endif

                    i->_pendingCommitPtr = spanningHeap.Allocate(newBlockSize);
                    if (i->_pendingCommitPtr == ~unsigned(0x0)) {
                        i->_pendingCommitPtr = spanningHeap.AppendNewBlock(newBlockSize);
                    }

                    assert(spanningHeap.CalculateAllocatedSpace() >= (originalAllocatedSize + newBlockSize));
                    assert(spanningHeap.CalculateHeapSize() >= originalHeapSize);

                        // make sure we're not overlapping another block (just to make sure the allocators are working)
                    #if defined(_DEBUG)
                        for (auto b=blocks.cbegin(); b!=blocks.cend(); ++b) {
                            assert((b->_start + b->_size) <= originalHeapSize);
                            assert(
                                ((i->_pendingCommitPtr + newBlockSize) <= b->_start)
                                || (i->_pendingCommitPtr >= (b->_start + b->_size)));
                        }
                    #endif

                    auto b = std::lower_bound(blocks.begin(), blocks.end(), i->_id, DirectoryChunk::CompareBlock());
                    assert(b==blocks.cend() || b->_id != i->_id);
                    ArchiveDirectoryBlock newBlock = { i->_id, i->_pendingCommitPtr, newBlockSize };
                    blocks.insert(b, newBlock);

                }
            }

                //  everything is allocated... we need to write the blocks to the data file
                //  sort by pending commit ptr for convenience
            std::sort(_pendingBlocks.begin(), _pendingBlocks.end(), 
                [](const PendingCommit& lhs, const PendingCommit& rhs) { return lhs._pendingCommitPtr < rhs._pendingCommitPtr; });
            {
                BasicFile dataFile;
                bool good = false;
                TRY {
                    dataFile = BasicFile(_mainFileName.c_str(), "r+b");
                    good = true;
                } CATCH (...) {} CATCH_END
                if (!good) {
                    dataFile = BasicFile(_mainFileName.c_str(), "wb");
                }
                for (auto i=_pendingBlocks.begin(); i!=_pendingBlocks.end(); ++i) {
                    dataFile.Seek(i->_pendingCommitPtr, SEEK_SET);
                    dataFile.Write(AsPointer(i->_data->cbegin()), 1, i->_data->size());
                }
            }

                // write the new directory file (including the blocks list and spanning heap)
            {
                ChunkFileHeader fileHeader;
                XlZeroMemory(fileHeader);
                fileHeader._magic = MagicHeader;
                fileHeader._fileVersionNumber = 0;
                XlCopyString(fileHeader._buildVersion, dimof(fileHeader._buildVersion), _buildVersionString);
                XlCopyString(fileHeader._buildDate, dimof(fileHeader._buildDate), _buildDateString);
                fileHeader._chunkCount = 1;

                auto flattenedHeap = spanningHeap.Flatten();
            
                ChunkHeader chunkHeader(
                    ChunkType_ArchiveDirectory, 0, "ArchiveCache", unsigned(sizeof(DirectoryChunk) + blocks.size() * sizeof(ArchiveDirectoryBlock) + flattenedHeap.second));
                chunkHeader._fileOffset = sizeof(ChunkFileHeader) + sizeof(ChunkHeader);

                DirectoryChunk chunkData;
                chunkData._blockCount = (unsigned)blocks.size();
                chunkData._spanningHeapSize = (unsigned)flattenedHeap.second;

                    // Write blank header data to the file
                    //  note that there's a potential problem here, because
                    //  we don't truncate the file before writing this. So if there is
                    //  some data in there (but invalid data), then it will remain in the file
                directoryFile.Seek(0, SEEK_SET);
                directoryFile.Write(&fileHeader, sizeof(fileHeader), 1);
                directoryFile.Write(&chunkHeader, sizeof(chunkHeader), 1);
                directoryFile.Write(&chunkData, sizeof(chunkData), 1);
                directoryFile.Write(AsPointer(blocks.begin()), sizeof(ArchiveDirectoryBlock), blocks.size());
                directoryFile.Write(flattenedHeap.first.get(), 1, flattenedHeap.second);
            }
        }

        #if defined(ARCHIVE_CACHE_ATTACHED_STRINGS)
            {
                        //  read the old string table, and then merge it
                        //  with the new. This will destroy and re-write the entire debugging file in one go.
                char debugFilename[MaxPath];
                XlCombineString(debugFilename, dimof(debugFilename), _mainFileName.c_str(), ".debug");
                    
                std::vector<std::pair<uint64, std::string>> attachedStrings;
                TRY {
                    BasicFile debugFile(debugFilename, "rb");
                    auto chunkTable = LoadChunkTable(debugFile);
                    auto chunk = FindChunk(debugFilename, chunkTable, ChunkType_ArchiveAttachments, 0);

                    AttachedStringChunk hdr;
                    debugFile.Seek(chunk._fileOffset, SEEK_SET);
                    debugFile.Read(&hdr, sizeof(hdr), 1);

                    std::vector<AttachedStringChunk::Block> attachedBlocks;
                    attachedStrings.resize(hdr._blockCount);
                    debugFile.Read(AsPointer(attachedBlocks.begin()), sizeof(AttachedStringChunk::Block), hdr._blockCount);
                    auto startPt = debugFile.TellP();
                    for (auto b=attachedBlocks.cbegin(); b!=attachedBlocks.cend(); ++b) {
                        debugFile.Seek(startPt + b->_start, SEEK_SET);
                        std::string t; t.resize(b->_size);
                        debugFile.Read(AsPointer(t.begin()), 1, b->_size);
                        attachedStrings.push_back(std::make_pair(b->_id, t));
                    }
                } CATCH (...) {
                } CATCH_END

                        // merge in the new strings
                for (auto i=_pendingBlocks.begin(); i!=_pendingBlocks.end(); ++i) {
                    auto c = LowerBound(attachedStrings, i->_id);
                    if (c!=attachedStrings.end() && c->first == i->_id) {
                        c->second = i->_attachedString;
                    } else {
                        attachedStrings.insert(c, std::make_pair(i->_id, i->_attachedString));
                    }
                }

                        // write the new debugging file
                TRY {
                    SimpleChunkFileWriter debugFile(1, _buildVersionString, _buildDateString, std::make_tuple(debugFilename, "wb", 0));
                    debugFile.BeginChunk(ChunkType_ArchiveAttachments, 0, "ArchiveAttachments");

                    AttachedStringChunk hdr;
                    hdr._blockCount = (unsigned)attachedStrings.size();
                    debugFile.Write(&hdr, sizeof(AttachedStringChunk), 1);

                    unsigned offset = 0;
                    for (auto b=attachedStrings.cbegin(); b!=attachedStrings.cend(); ++b) {
                        AttachedStringChunk::Block block;
                        block._id = b->first;
                        block._start = offset;
                        block._size = (unsigned)b->second.size();
                        offset += block._size;
                        debugFile.Write(&block, sizeof(block), 1);
                    }

                    for (auto b=attachedStrings.cbegin(); b!=attachedStrings.cend(); ++b) {
                        debugFile.Write(AsPointer(b->second.begin()), sizeof(std::string::value_type), b->second.size());
                    }
                } CATCH (...) {
                } CATCH_END
            }
        #endif

        for (const auto& i:_pendingBlocks)
            i._onFlush();

            // clear all pending block (now that they're flushed to disk)
        _pendingBlocks.clear();
    }
    
    auto ArchiveCache::GetMetrics() const -> Metrics
    {
        using namespace Serialization::ChunkFile;

            // We need to open the file and get metrics information
            // for the blocks contained within
        ////////////////////////////////////////////////////////////////////////////////////
        std::vector<ArchiveDirectoryBlock> fileBlocks;
        TRY {
            BasicFile directoryFile(_directoryFileName.c_str(), "rb");

            auto chunkTable = LoadChunkTable(directoryFile);
            auto chunk = FindChunk(_directoryFileName.c_str(), chunkTable, ChunkType_ArchiveDirectory, 0);

            directoryFile.Seek(chunk._fileOffset, SEEK_SET);
            DirectoryChunk dirHdr;
            directoryFile.Read(&dirHdr, sizeof(dirHdr), 1);

            fileBlocks.resize(dirHdr._blockCount);
            directoryFile.Read(AsPointer(fileBlocks.begin()), sizeof(ArchiveDirectoryBlock), dirHdr._blockCount);
        } CATCH (...) {
        } CATCH_END

        ////////////////////////////////////////////////////////////////////////////////////
        #if defined(ARCHIVE_CACHE_ATTACHED_STRINGS)
            std::vector<AttachedStringChunk::Block> attachedBlocks;
            BasicFile debugFile;
            unsigned debugFileStartPoint = 0;
            TRY {
                char debugFilename[MaxPath];
                XlCombineString(debugFilename, dimof(debugFilename), _mainFileName.c_str(), ".debug");
                debugFile = BasicFile(debugFilename, "rb");

                auto chunkTable = LoadChunkTable(debugFile);
                auto chunk = FindChunk(_directoryFileName.c_str(), chunkTable, ChunkType_ArchiveAttachments, 0);

                debugFile.Seek(chunk._fileOffset, SEEK_SET);
                AttachedStringChunk dirHdr;
                debugFile.Read(&dirHdr, sizeof(dirHdr), 1);

                attachedBlocks.resize(dirHdr._blockCount);
                debugFile.Read(AsPointer(attachedBlocks.begin()), sizeof(AttachedStringChunk::Block), dirHdr._blockCount);
                debugFileStartPoint = (unsigned)debugFile.TellP();
            } CATCH (...) {
            } CATCH_END
        #endif

        ////////////////////////////////////////////////////////////////////////////////////
        std::vector<BlockMetrics> blocks;
        unsigned usedSpace = 0;
        for (auto b=fileBlocks.cbegin(); b!=fileBlocks.cend(); ++b) {
            BlockMetrics metrics;
            metrics._id = b->_id;
            metrics._offset = b->_start;
            metrics._size = b->_size;

            #if defined(ARCHIVE_CACHE_ATTACHED_STRINGS)
                auto s = std::lower_bound(attachedBlocks.cbegin(), attachedBlocks.cend(), 
                    b->_id, AttachedStringChunk::CompareBlock());
                if (s != attachedBlocks.cend() && s->_id == b->_id) {
                    TRY {
                        std::string t;
                        t.resize(s->_size);
                        debugFile.Seek(s->_start + debugFileStartPoint, SEEK_SET);
                        debugFile.Read(AsPointer(t.begin()), 1, s->_size);
                        metrics._attachedString = t;
                    } CATCH (...) {
                    } CATCH_END
                }
            #endif

            blocks.push_back(metrics);
            usedSpace += metrics._size;
        }

        ////////////////////////////////////////////////////////////////////////////////////
        for (auto p=_pendingBlocks.cbegin(); p!=_pendingBlocks.cend(); ++p) {

            BlockMetrics newMetrics;
            newMetrics._id = p->_id;
            newMetrics._size = (unsigned)p->_data->size();
            newMetrics._offset = ~unsigned(0x0);
            #if defined(ARCHIVE_CACHE_ATTACHED_STRINGS)
                newMetrics._attachedString = p->_attachedString;
            #endif

            auto b = std::find_if(blocks.begin(), blocks.end(),
                [=](const BlockMetrics& t) { return t._id == newMetrics._id; });
            if (b != blocks.end()) {
                *b = newMetrics;
            } else {
                blocks.push_back(newMetrics);
            }
        }

        ////////////////////////////////////////////////////////////////////////////////////
        Metrics result;
        result._blocks = std::move(blocks);
        result._usedSpace = usedSpace;
        result._allocatedFileSize = unsigned(Utility::GetFileSize(_mainFileName.c_str()));
        return result;
    }

    ArchiveCache::ArchiveCache(
        const char archiveName[],
        const char buildVersionString[],
        const char buildDateString[]) 
    : _mainFileName(archiveName)
    , _buildVersionString(buildVersionString)
    , _buildDateString(buildDateString)
    , _cachedBlockListValid(false)
    {
        _directoryFileName = _mainFileName + ".dir";

            // (make sure the directory provided exists)
        char dirName[MaxPath];
        XlDirname(dirName, dimof(dirName), _mainFileName.c_str());
        CreateDirectoryRecursive(dirName);
    }

    ArchiveCache::~ArchiveCache() 
    {
        TRY {
            FlushToDisk();
        } CATCH (const std::exception& e) {
            LogWarning << "Suppressing exception in ArchiveCache::~ArchiveCache: " << e.what();
        } CATCH (...) {
            LogWarning << "Suppressing unknown exception in ArchiveCache::~ArchiveCache.";
        } CATCH_END
    }
}

