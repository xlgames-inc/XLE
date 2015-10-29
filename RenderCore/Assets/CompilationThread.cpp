// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CompilationThread.h"

namespace RenderCore { namespace Assets 
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
                if (o) _compileOp(*o);
                _queue.pop();
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

}}

