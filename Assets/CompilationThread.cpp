// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CompilationThread.h"
#if defined(XLE_HAS_CONSOLE_RIG)
    #include "../ConsoleRig/Log.h"
#endif

namespace Assets 
{
    void CompilationThread::StallOnPendingOperations(bool cancelAll)
    {
        if (!_workerQuit) {
            _workerQuit = true;
            XlSetEvent(_events[1]);   // trigger a manual reset event should wake all threads (and keep them awake)
            _thread.join();
        }
    }
    
    void CompilationThread::Push(std::shared_ptr<QueuedCompileOperation> op)
    {
        if (!_workerQuit) {
            _queue.push_overflow(std::move(op));
            XlSetEvent(_events[0]);
        }
    }

    void CompilationThread::ThreadFunction()
    {
        while (!_workerQuit) {
            std::weak_ptr<QueuedCompileOperation>* op;
            if (_queue.try_front(op)) {
                auto o = op->lock();
                TRY
                {
                    if (o) _compileOp(*o);
                    _queue.pop();
                }
                CATCH (const ::Assets::Exceptions::PendingAsset&)
                {
                    // We need to stall on a pending asset while compiling
                    // All we can do is delay the request, and try again later.
                    // Let's move the request into a separate queue, so that
                    // new request get processed first.
                    _queue.pop();
                    _delayedQueue.push(o);
                }
                CATCH (const std::exception& e)
                {
                    #if defined(XLE_HAS_CONSOLE_RIG)
                        LogWarning << "Got exception while in asset compilation thread" << std::endl;
                        LogWarning << "Asset: " << o->Initializer() << std::endl;
                        LogWarning << "    " << e.what() << std::endl;
                    #endif
                    _queue.pop();
					(void)e;
                }
                CATCH_END
            } else if (_delayedQueue.try_front(op)) {
                
                    // do a short sleep first, do avoid too much
                    // trashing while processing delayed items. Note
                    // that if any new request comes in during this Sleep,
                    // then we won't handle that request in a prompt manner.
                Threading::Sleep(1);
                auto o = op->lock();
                TRY
                {
                    if (o) _compileOp(*o);
                    _delayedQueue.pop();
                }
                CATCH (const ::Assets::Exceptions::PendingAsset&)
                {
                    // We need to stall on a pending asset while compiling
                    // All we can do is delay the request, and try again later.
                    // Let's move the request into a separate queue, so that
                    // new request get processed first.
                    _delayedQueue.pop();
                    _delayedQueue.push(o);
                }
                CATCH (const std::exception& e)
                {
                    #if defined(XLE_HAS_CONSOLE_RIG)
                        LogWarning << "Got exception while in asset compilation thread" << std::endl;
                        LogWarning << "Asset: " << o->Initializer() << std::endl;
                        LogWarning << "    " << e.what() << std::endl;
                    #endif
                    _delayedQueue.pop();
					(void)e;
                }
                CATCH_END
            } else {
                XlWaitForMultipleSyncObjects(
                    2, this->_events,
                    false, XL_INFINITE, true);
            }
        }
    }

    CompilationThread::CompilationThread(std::function<void(QueuedCompileOperation&)> compileOp)
    : _compileOp(std::move(compileOp))
    {
        _events[0] = XlCreateEvent(false);
        _events[1] = XlCreateEvent(true);
        _workerQuit = false;

        _thread = std::thread(std::bind(&CompilationThread::ThreadFunction, this));
    }

    CompilationThread::~CompilationThread()
    {
        StallOnPendingOperations(true);
        XlCloseSyncObject(_events[0]);
        XlCloseSyncObject(_events[1]);
    }

}

