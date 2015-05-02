// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Mutex.h"
#include "LockFree.h"
#include <vector>
#include <thread>

namespace Utility
{
    class CompletionThreadPool
    {
    public:
        template<class Fn, class... Args>
            void Enqueue(Fn&& fn, Args&&... args);

        CompletionThreadPool(unsigned threadCount);
        ~CompletionThreadPool();

    private:
        std::vector<std::thread> _workerThreads;
        
        Threading::Mutex _pendingsTaskLock;
        typedef std::function<void()> PendingTask;
        LockFree::FixedSizeQueue<PendingTask, 256> _pendingTasks;

        XlHandle _events[2];
        bool _workerQuit;

        void EnqueueInternal(PendingTask&& task);
    };

    template<class Fn, class... Args>
        void CompletionThreadPool::Enqueue(Fn&& fn, Args&&... args)
        {
            EnqueueInternal(std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...));
        }
}

