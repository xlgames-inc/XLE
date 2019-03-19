// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CompletionThreadPool.h"
#include "ThreadLocalPtr.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/SystemUtils.h"
#include "../../Core/Exceptions.h"
#include <functional>

namespace Utility
{
    static thread_local std::function<void()> s_threadPoolYieldFunction;

    void CompletionThreadPool::EnqueueBasic(PendingTask&& task)
    {
        assert(IsGood());
        _pendingTasks.push_overflow(std::forward<PendingTask>(task));

            // set event should wake one thread -- and that thread should
            // then take over and execute the task
        XlSetEvent(_events[0]);
    }

    CompletionThreadPool::CompletionThreadPool(unsigned threadCount)
    {
            // once event is an "auto-reset" event, which should wake a single thread
            // another event is a "manual-reset" event. This should 
        _events[0] = XlCreateEvent(false);
        _events[1] = XlCreateEvent(true);
        _workerQuit = false;

        for (unsigned i = 0; i<threadCount; ++i)
            _workerThreads.emplace_back(
                [this]
                {
                    SetYieldToPoolFunction([this]() {
                        bool gotTask = false;
                        std::function<void()> task;

                        {
                            ScopedLock(this->_pendingsTaskLock);
                            std::function<void()>* t = nullptr;
                            if (_pendingTasks.try_front(t)) {
                                task = std::move(*t);
                                _pendingTasks.pop();
                                gotTask = true;
                            }
                        }

                        // Attempt a short wait if we didn't get a task
                        if (!gotTask) {
                            XlWaitForMultipleSyncObjects(
                                2, this->_events,
                                false, 1, true);

                            {
                               ScopedLock(this->_pendingsTaskLock);
                                std::function<void()>* t = nullptr;
                                if (_pendingTasks.try_front(t)) {
                                    task = std::move(*t);
                                    _pendingTasks.pop();
                                    gotTask = true;
                                }
                            }
                        }

                        if (gotTask) {
                            TRY
                            {
                                task();
                            } CATCH(const std::exception& e) {
                                Log(Error) << "Suppressing exception in thread pool thread: " << e.what() << std::endl;
								(void)e;
                            } CATCH(...) {
                                Log(Error) << "Suppressing unknown exception in thread pool thread." << std::endl;
                            } CATCH_END
                        }
                    });

                    while (!this->_workerQuit) {
                        bool gotTask = false;
                        std::function<void()> task;

                        {
                                // note that _pendingTasks is safe for multiple pushing threads,
                                // but not safe for multiple popping threads. So we have to
                                // lock to prevent more than one thread from attempt to pop
                                // from it at the same time.
                            ScopedLock(this->_pendingsTaskLock);

                            std::function<void()>*t = nullptr;
                            if (_pendingTasks.try_front(t)) {
                                task = std::move(*t);
                                _pendingTasks.pop();
                                gotTask = true;
                            }
                        }

                        if (gotTask) {
                                // if we got this far, we can execute the task....
                            TRY
                            {
                                task();
                            } CATCH(const std::exception& e) {
                                Log(Error) << "Suppressing exception in thread pool thread: " << e.what() << std::endl;
								(void)e;
                            } CATCH(...) {
                                Log(Error) << "Suppressing unknown exception in thread pool thread." << std::endl;
                            } CATCH_END

                                // That that when using completion routines, we want to attempt to
                                // distribute the tasks evenly between threads (so that the completion
                                // routines will also be distributed evenly between threads.). To achieve
                                // this, let's not attempt to search for another task immediately... Instead
                                // when after we complete a task, let's encourage this thread to go back into
                                // a stall (unless all of our threads are saturated)
                            Threading::YieldTimeSlice();
                            continue;
                        }

                            // Wait for the event with the "alertable" flag set true
                            // note -- this is why we can't use std::condition_variable
                            //      (because threads waiting on a condition variable won't
                            //      be woken to execute completion routines)
                        XlWaitForMultipleSyncObjects(
                            2, this->_events,
                            false, XL_INFINITE, true);
                    }

                    SetYieldToPoolFunction(nullptr);
                }
            );
    }

    CompletionThreadPool::~CompletionThreadPool()
    {
        _workerQuit = true;
        XlSetEvent(_events[1]);   // trigger a manual reset event should wake all threads (and keep them awake)
        for (auto&t : _workerThreads) t.join();

        XlCloseSyncObject(_events[0]);
        XlCloseSyncObject(_events[1]);
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    void ThreadPool::EnqueueBasic(PendingTask&& task)
    {
        assert(IsGood());
        _pendingTasks.push_overflow(std::forward<PendingTask>(task));
        _pendingTaskVariable.notify_one();
    }

    ThreadPool::ThreadPool(unsigned threadCount)
    {
        _workerQuit = false;

        for (unsigned i = 0; i<threadCount; ++i)
            _workerThreads.emplace_back(
                [this]
                {
                    SetYieldToPoolFunction([this]() {
                        std::function<void()> task;
                        {
                            std::unique_lock<decltype(this->_pendingTaskLock)> autoLock(this->_pendingTaskLock);
                            if (this->_workerQuit) return;

                            std::function<void()>*t = nullptr;
                            if (!_pendingTasks.try_front(t)) {
                                this->_pendingTaskVariable.wait(autoLock);
                                if (this->_workerQuit) return;
                                if (!_pendingTasks.try_front(t))
                                    return;
                            }

                            task = std::move(*t);
                            _pendingTasks.pop();
                        }

                        TRY
                        {
                            task();
                        } CATCH(const std::exception& e) {
                            Log(Error) << "Suppressing exception in thread pool thread: " << e.what() << std::endl;
                            (void)e;
                        } CATCH(...) {
                            Log(Error) << "Suppressing unknown exception in thread pool thread." << std::endl;
                        } CATCH_END
                    });

                    for (;;) {
                        std::function<void()> task;

                        {
                                // note that _pendingTasks is safe for multiple pushing threads,
                                // but not safe for multiple popping threads. So we have to
                                // lock to prevent more than one thread from attempt to pop
                                // from it at the same time.
                            std::unique_lock<decltype(this->_pendingTaskLock)> autoLock(this->_pendingTaskLock);
                            if (this->_workerQuit) break;

                            std::function<void()>*t = nullptr;
                            if (!_pendingTasks.try_front(t)) {
                                this->_pendingTaskVariable.wait(autoLock);
                                if (this->_workerQuit) break;
                                if (!_pendingTasks.try_front(t))
                                    continue;
                            }

                            task = std::move(*t);
                            _pendingTasks.pop();
                        }

                        TRY
                        {
                            task();
                        } CATCH(const std::exception& e) {
                            Log(Error) << "Suppressing exception in thread pool thread: " << e.what() << std::endl;
                            (void)e;
                        } CATCH(...) {
                            Log(Error) << "Suppressing unknown exception in thread pool thread." << std::endl;
                        } CATCH_END
                    }

                    SetYieldToPoolFunction(nullptr);
                }
            );
    }

    ThreadPool::~ThreadPool()
    {
        _workerQuit = true;
        _pendingTaskVariable.notify_all();
        for (auto&t : _workerThreads) t.join();
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

#if !FEATURE_THREAD_LOCAL_KEYWORD
    static thread_local_ptr<std::function<void()>> s_threadPoolYieldFunction;

    void YieldToPool()
    {
        auto* yieldFn = s_threadPoolYieldFunction.get();
        if (yieldFn) {
            (*yieldFn)();
        } else {
            Threading::YieldTimeSlice();
        }
    }

    void SetYieldToPoolFunction(const std::function<void()>& yieldToPoolFunction)
    {
        s_threadPoolYieldFunction.allocate(yieldToPoolFunction);
    }
#else
    static thread_local std::function<void()> s_threadPoolYieldFunction;

    void YieldToPool()
    {
        if (s_threadPoolYieldFunction) {
            s_threadPoolYieldFunction();
        } else {
            Threading::YieldTimeSlice();
        }
    }

    void SetYieldToPoolFunction(const std::function<void()>& yieldToPoolFunction)
    {
        s_threadPoolYieldFunction = yieldToPoolFunction;
    }

#endif

}

