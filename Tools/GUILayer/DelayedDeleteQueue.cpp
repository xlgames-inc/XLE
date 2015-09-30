// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DelayedDeleteQueue.h"

namespace LogUtilMethods
{
    void LogInfoF(const char format[], ...);
}

namespace GUILayer
{
    void DelayedDeleteQueue::Add(void* ptr, DeletionCallback^ callback)
    {
        _queue->Add(gcnew DeletablePtr(System::IntPtr(ptr), callback));
    }

    void DelayedDeleteQueue::FlushQueue()
    {
        auto count = _queue->Count;
        if (count > 0) {
            LogUtilMethods::LogInfoF("Destroying native objects that were released deterministically from cli code: %i", count);
            for each(auto i in _queue)
                (i->Item2)(i->Item1.ToPointer());
            _queue->Clear();
        }
    }

    static DelayedDeleteQueue::DelayedDeleteQueue()
    {
        _queue = gcnew DeletablePtrList;;
    }
}

