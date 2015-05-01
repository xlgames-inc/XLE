// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IBufferUploads.h"
#include "../Core/Types.h"
#include <vector>

namespace BufferUploads
{
    typedef int64 TimeMarker;

    namespace UploadDataType
    {
        enum Enum
        {
            Texture, Vertex, Index,
            Max
        };
    }

    struct AssemblyLineMetrics
    {
        unsigned _transactionCount, _temporaryTransactionsAllocated, _longTermTransactionsAllocated;
        unsigned _queuedCreates, _queuedUploads, _queuedStagingCreates, _queuedPrepares;
        unsigned _queuedPeakCreates, _queuedPeakUploads, _queuedPeakStagingCreates, _queuedPeakPrepares;
        unsigned _queuedBytes[UploadDataType::Max];
        AssemblyLineMetrics();
    };

    struct AssemblyLineRetirement
    {
        BufferDesc _desc;
        TimeMarker _requestTime, _retirementTime;
    };

    struct CommandListMetrics
    {
        unsigned _bytesUploaded[UploadDataType::Max];
        unsigned _bytesCreated[UploadDataType::Max];
        unsigned _bytesUploadedDuringCreation[UploadDataType::Max];
        unsigned _bytesUploadTotal;
        unsigned _countCreations[UploadDataType::Max];
        unsigned _countDeviceCreations[UploadDataType::Max];
        unsigned _countUploaded[UploadDataType::Max];
        unsigned _contextOperations, _nonContextOperations, _deviceCreateOperations;
        AssemblyLineMetrics _assemblyLineMetrics;
        AssemblyLineRetirement _retirements[16];
        unsigned _retirementCount;
        std::vector<AssemblyLineRetirement> _retirementsOverflow;
        TimeMarker _resolveTime, _commitTime;
        TimeMarker _waitTime, _processingStart, _processingEnd;
        TimeMarker _framePriorityStallTime;
        unsigned _batchedCopyBytes, _batchedCopyCount;
        unsigned _wakeCount, _frameId;

        buffer_upload_dll_export CommandListMetrics();
        buffer_upload_dll_export CommandListMetrics(const CommandListMetrics& cloneFrom);
        buffer_upload_dll_export const CommandListMetrics& operator=(const CommandListMetrics& cloneFrom);

        unsigned RetirementCount() const                                { return unsigned(_retirementCount + _retirementsOverflow.size()); }
        const AssemblyLineRetirement& Retirement(unsigned index) const  { if (index<_retirementCount) {return _retirements[index];} return _retirementsOverflow[index-_retirementCount]; }
    };

        /////////////////////////////////////////////////

    struct PoolMetrics
    {
        BufferDesc _desc;
        unsigned _currentSize, _peakSize;
        unsigned _topMostAge;
        unsigned _recentDeviceCreateCount;
        unsigned _recentPoolCreateCount;
        unsigned _recentReleaseCount;
        unsigned _totalRealSize, _totalCreateSize, _totalCreateCount;
    };

    struct BatchedHeapMetrics
    {
        std::vector<unsigned> _markers;
        unsigned _allocatedSpace, _unallocatedSpace;
        unsigned _heapSize;
        unsigned _largestFreeBlock;
        unsigned _spaceInReferencedCountedBlocks;
        unsigned _referencedCountedBlockCount;
    };

    struct BatchingSystemMetrics
    {
        std::vector<BatchedHeapMetrics> _heaps;
    };

    struct PoolSystemMetrics
    {
        std::vector<PoolMetrics> _resourcePools;
        std::vector<PoolMetrics> _stagingPools;
        BatchingSystemMetrics _batchingSystemMetrics;
    };
}

