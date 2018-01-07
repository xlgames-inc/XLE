// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsCore.h"
#include "IAssetCompiler.h"
#include "../Utility/Threading/LockFree.h"
#include <memory>
#include <thread>
#include <functional>

namespace Assets 
{
    /// <summary>Used by the compiler types to manage background operations</summary>
    class CompilationThread
    {
    public:
        void Push(
			std::shared_ptr<::Assets::CompileFuture> future,
			std::function<void(::Assets::CompileFuture&)> operation);
        void StallOnPendingOperations(bool cancelAll);

        CompilationThread();
        ~CompilationThread();
    protected:
        std::thread _thread;
        XlHandle _events[2];
        volatile bool _workerQuit;
		struct Element
		{
			std::weak_ptr<::Assets::CompileFuture> _future;
			std::function<void(::Assets::CompileFuture&)> _operation;
		};
        using Queue = LockFree::FixedSizeQueue<Element, 256>;
        Queue _queue;
        Queue _delayedQueue;

        void ThreadFunction();
    };
}

