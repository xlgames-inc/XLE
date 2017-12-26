// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsCore.h"
#include "IntermediateAssets.h"
#include "../Utility/Threading/LockFree.h"
#include <memory>
#include <thread>
#include <functional>

namespace Assets 
{
    class QueuedCompileOperation : public ::Assets::CompileFuture
    {
    public:
        uint64 _typeCode;
        ::Assets::ResChar _initializer0[MaxPath];
        ::Assets::ResChar _initializer1[MaxPath];

        const ::Assets::IntermediateAssets::Store* _destinationStore;
		unsigned _compilerIndex;
    };

    /// <summary>Used by the compiler types to manage background operations</summary>
    class CompilationThread
    {
    public:
        void Push(std::shared_ptr<QueuedCompileOperation> op);
        void StallOnPendingOperations(bool cancelAll);

        CompilationThread(std::function<void(QueuedCompileOperation&)> compileOp);
        ~CompilationThread();
    protected:
        std::thread _thread;
        XlHandle _events[2];
        volatile bool _workerQuit;
        using Queue = LockFree::FixedSizeQueue<std::weak_ptr<QueuedCompileOperation>, 256>;
        Queue _queue;
        Queue _delayedQueue;
        std::function<void(QueuedCompileOperation&)> _compileOp;

        void ThreadFunction();
    };
}

