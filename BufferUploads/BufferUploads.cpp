// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Metrics.h"
#include "IBufferUploads.h"
#include "../Utility/MemoryUtils.h"
#include "../Core/Prefix.h"  // for dimof
#include <algorithm>

namespace BufferUploads
{
    CommandListMetrics::CommandListMetrics()
    {
        XlZeroMemory(_bytesUploaded);
        XlZeroMemory(_bytesCreated);
        XlZeroMemory(_stagingBytesUsed);
        XlZeroMemory(_countCreations);
        XlZeroMemory(_countDeviceCreations);
        XlZeroMemory(_countUploaded);
        _bytesUploadTotal = _contextOperations = _deviceCreateOperations = 0;
        _resolveTime = _commitTime = 0;
        _waitTime = _processingStart = _processingEnd = 0;
        _framePriorityStallTime = 0;
        _batchedCopyBytes = _batchedCopyCount = 0;
        _wakeCount = 0;
        _frameId = 0;
        _retirementCount = 0;
    }

    CommandListMetrics::CommandListMetrics(const CommandListMetrics& cloneFrom)
    {
        this->operator=(cloneFrom);
    }

    const CommandListMetrics& CommandListMetrics::operator=(const CommandListMetrics& cloneFrom)
    {
        std::copy(cloneFrom._bytesUploaded, &cloneFrom._bytesUploaded[dimof(cloneFrom._bytesUploaded)], _bytesUploaded);
        std::copy(cloneFrom._bytesCreated, &cloneFrom._bytesCreated[dimof(cloneFrom._bytesCreated)], _bytesCreated);
        std::copy(cloneFrom._stagingBytesUsed, &cloneFrom._stagingBytesUsed[dimof(cloneFrom._stagingBytesUsed)], _stagingBytesUsed);
        _bytesUploadTotal = cloneFrom._bytesUploadTotal;
        std::copy(cloneFrom._countCreations, &cloneFrom._countCreations[dimof(cloneFrom._countCreations)], _countCreations);
        std::copy(cloneFrom._countDeviceCreations, &cloneFrom._countDeviceCreations[dimof(cloneFrom._countDeviceCreations)], _countDeviceCreations);
        std::copy(cloneFrom._countUploaded, &cloneFrom._countUploaded[dimof(cloneFrom._bytesUploaded)], _countUploaded);
        _contextOperations = cloneFrom._contextOperations;
        _deviceCreateOperations = cloneFrom._deviceCreateOperations;
        _assemblyLineMetrics = cloneFrom._assemblyLineMetrics;
        _retirementCount = cloneFrom._retirementCount;
        std::copy(cloneFrom._retirements, &cloneFrom._retirements[std::min(unsigned(dimof(cloneFrom._retirements)), cloneFrom._retirementCount)], _retirements);
        _retirementsOverflow = cloneFrom._retirementsOverflow;
        _resolveTime = cloneFrom._resolveTime;
        _commitTime = cloneFrom._commitTime;
        _waitTime = cloneFrom._waitTime; _processingStart = cloneFrom._processingStart; _processingEnd = cloneFrom._processingEnd;
        _framePriorityStallTime = cloneFrom._framePriorityStallTime;
        _batchedCopyBytes = cloneFrom._batchedCopyBytes; _batchedCopyCount = cloneFrom._batchedCopyCount;
        _wakeCount = cloneFrom._wakeCount; _frameId = cloneFrom._frameId;
        return *this;
    }

    AssemblyLineMetrics::AssemblyLineMetrics()
    {
        _transactionCount = _temporaryTransactionsAllocated = _longTermTransactionsAllocated = _queuedPrepareStaging = _queuedTransferStagingToFinal = _queuedCreateFromDataPacket = 0;
        _peakPrepareStaging = _peakTransferStagingToFinal = _peakCreateFromDataPacket = 0;
        XlZeroMemory(_queuedBytes);
    }
}

