// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CompletionThreadPool.h"
#include "../../ConsoleRig/Log.h"
#include "../../Core/Exceptions.h"

namespace Utility
{
    void CompletionThreadPool::EnqueueInternal(PendingTask&& task)
    {
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
                                LogAlwaysError << "Suppressing exception in thread pool thread: " << e.what();
                            } CATCH(...) {
                                LogAlwaysError << "Suppressing unknown exception in thread pool thread.";
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
                }
            );
    }

    CompletionThreadPool::~CompletionThreadPool()
    {
        _workerQuit = true;
        XlSetEvent(this->_events[1]);   // should wake all threads
        for (auto&t : _workerThreads) t.join();
    }
}

