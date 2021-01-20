// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsCore.h"
#include "IArtifact.h"
#include "../Utility/Threading/LockFree.h"
#include "../OSServices/WinAPI/System_WinAPI.h"
#include <memory>
#include <thread>
#include <functional>

namespace Utility { class ThreadPool; }

namespace Assets 
{
    /// <summary>Used by the compiler types to manage background operations</summary>
    class CompilationThread
    {
    public:
        void Push(
			std::shared_ptr<::Assets::ArtifactFuture> future,
			std::function<void(::Assets::ArtifactFuture&)> operation);
        void StallOnPendingOperations(bool cancelAll);

        CompilationThread();
        ~CompilationThread();
    protected:
        std::thread _thread;
        OSServices::XlHandle _events[2];
        volatile bool _workerQuit;
		struct Element
		{
			std::weak_ptr<::Assets::ArtifactFuture> _future;
			std::function<void(::Assets::ArtifactFuture&)> _operation;
		};
        using Queue = LockFreeFixedSizeQueue<Element, 256>;
        Queue _queue;
        Queue _delayedQueue;

        void ThreadFunction();
    };

	void QueueCompileOperation(
		const std::shared_ptr<::Assets::ArtifactFuture>& future,
		std::function<void(::Assets::ArtifactFuture&)>&& operation);
		
}

