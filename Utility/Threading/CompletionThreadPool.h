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
#include <functional>

namespace Utility
{
    /** <summary>Temporarily yield execution of this thread to whatever pool manages it</summary>
     * 
     * Operations running on a thread pool thread should normally not use busy loops or
     * long locks waiting for mutexes. When a thread pool operation is stalled for some
     * synchronization primitive, the entire worker thread becomes stalled. Since there
     * are a finite number of worker threads, this can result in a deadlock were all
     * worker threads are stalled waiting on some pool operation that can never execute.
     * 
     * Rather than stalling or yielding worker thread time, we should instead attempt to 
     * find some other operation that can take over this worker thread temporarily.
     * 
     * When run on a thread pool worker thread, YieldToPool does exactly that. It does not
     * stall, but it will attempt pop another operation from the pending queue. It will
     * return execution back to the caller after this operation has completed, so that the
     * original operation can resume from where it left off.
     * 
     * When run on some other thread, it will just yield back to the OS.
    */
    void YieldToPool();

    class CompletionThreadPool
    {
    public:
        template<class Fn, class... Args>
            void Enqueue(Fn&& fn, Args&&... args);

		void EnqueueBasic(std::function<void()>&& task);

        bool IsGood() const { return !_workerThreads.empty(); }

        CompletionThreadPool(unsigned threadCount);
        ~CompletionThreadPool();

        CompletionThreadPool(const CompletionThreadPool&) = delete;
        CompletionThreadPool& operator=(const CompletionThreadPool&) = delete;
        CompletionThreadPool(CompletionThreadPool&&) = delete;
        CompletionThreadPool& operator=(CompletionThreadPool&&) = delete;
    private:
        std::vector<std::thread> _workerThreads;
        
        Threading::Mutex _pendingsTaskLock;
        typedef std::function<void()> PendingTask;
        LockFree::FixedSizeQueue<PendingTask, 256> _pendingTasks;

        XlHandle _events[2];
        volatile bool _workerQuit;
    };

    template<class Fn, class... Args>
        void CompletionThreadPool::Enqueue(Fn&& fn, Args&&... args)
        {
			EnqueueBasic(std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...));
        }

    class ThreadPool
    {
    public:
        template<class Fn, class... Args>
            void Enqueue(Fn&& fn, Args&&... args);

		void EnqueueBasic(std::function<void()>&& task);

        bool IsGood() const { return !_workerThreads.empty(); }

        ThreadPool(unsigned threadCount);
        ~ThreadPool();

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;
    private:
        std::vector<std::thread> _workerThreads;

        Threading::Conditional _pendingTaskVariable;
        Threading::Mutex _pendingTaskLock;
        typedef std::function<void()> PendingTask;
        LockFree::FixedSizeQueue<PendingTask, 256> _pendingTasks;

        volatile bool _workerQuit;
    };

    template<class Fn, class... Args>
        void ThreadPool::Enqueue(Fn&& fn, Args&&... args)
        {
			EnqueueBasic(std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...));
        }
}

using namespace Utility;
