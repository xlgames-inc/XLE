// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/AssetsCore.h"
#include "../Utility/Threading/Mutex.h"
#include "../Utility/UTFUtils.h"
#include "../Core/Types.h"

#include <memory>
#include <vector>
#include <string>
#include <functional>

#define ARCHIVE_CACHE_ATTACHED_STRINGS

namespace Assets
{
    class ArchiveDirectoryBlock;

    class ArchiveCache
    {
    public:
        using BlockAndSize = ::Assets::Blob;

        void            Commit(
			uint64_t id, const BlockAndSize& data, 
			const std::string& attachedStringName, const std::string& attachedString, 
			std::function<void()>&& onFlush);
        BlockAndSize    TryOpenFromCache(uint64_t id);
        bool            HasItem(uint64_t id) const;
        void            FlushToDisk();
        
        class BlockMetrics
        {
        public:
            uint64_t _id;
            unsigned _offset, _size;
            std::string _attachedString;
        };
        class Metrics
        {
        public:
            unsigned _allocatedFileSize;
            unsigned _usedSpace;
            std::vector<BlockMetrics> _blocks;
        };

        /// <summary>Return profiling related breakdown</summary>
        /// Designed to be used for profiling archive usage and stats.
        Metrics GetMetrics() const;

        ArchiveCache(const char archiveName[], const char buildVersionString[], const char buildDateString[]);
        ~ArchiveCache();

        ArchiveCache(const ArchiveCache&) = delete;
        ArchiveCache& operator=(const ArchiveCache&) = delete;
        ArchiveCache(ArchiveCache&&) = delete;
        ArchiveCache& operator=(ArchiveCache&&) = delete;

    protected:
        class PendingCommit
        {
        public:
            uint64_t          _id = 0;
            BlockAndSize    _data;
            unsigned        _pendingCommitPtr = 0;      // (only used during FlushToDisk)
            std::function<void()> _onFlush;

            #if defined(ARCHIVE_CACHE_ATTACHED_STRINGS)
				std::string		_attachedStringName;
                std::string     _attachedString;    // used for appending debugging/profiling information. user defined format
            #endif

            PendingCommit() {}
            PendingCommit(uint64_t id, const BlockAndSize& data, const std::string& attachedStringName, const std::string& attachedString, std::function<void()>&& onFlush);
            PendingCommit(PendingCommit&& moveFrom);
            PendingCommit& operator=(PendingCommit&& moveFrom);

        private:
            PendingCommit(const PendingCommit&);
            PendingCommit& operator=(const PendingCommit&);
        };

        mutable Threading::Mutex _pendingBlocksLock;
        std::vector<PendingCommit> _pendingBlocks;
        std::basic_string<utf8> _mainFileName, _directoryFileName;

        const char*     _buildVersionString;
        const char*     _buildDateString;

        class ComparePendingCommit
        {
        public:
            bool operator()(const PendingCommit& lhs, uint64_t rhs) { return lhs._id < rhs; }
            bool operator()(uint64_t lhs, const PendingCommit& rhs) { return lhs < rhs._id; }
            bool operator()(const PendingCommit& lhs, const PendingCommit& rhs) { return lhs._id < rhs._id; }
        };

        mutable std::vector<ArchiveDirectoryBlock> _cachedBlockList;
        mutable bool _cachedBlockListValid;
        const std::vector<ArchiveDirectoryBlock>* GetBlockList() const;
    };


}
