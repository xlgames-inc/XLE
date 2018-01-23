// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DelayedDeleteQueue.h"
#include "../../ConsoleRig/Log.h"
#include <msclr\lock.h>

namespace GUILayer
{
    void DelayedDeleteQueue::Add(void* ptr, DeletionCallback^ callback)
    {
        msclr::lock l(_queue);
        _queue->Add(gcnew DeletablePtr(System::IntPtr(ptr), callback));
    }

    void DelayedDeleteQueue::FlushQueue()
    {
        // swap with a new list, so the list doesn't get modified by another thread as we're deleting items
        auto flip = gcnew DeletablePtrList;
        {
            msclr::lock l(_queue);
            auto t = _queue;
            _queue = flip;
            flip = t;
        }

        auto count = flip->Count;
        if (count > 0) {
            Log(Verbose) << "Destroying native objects that were released indeterministically from cli code: " << count << std::endl;
            for each(auto i in flip)
                (i->Item2)(i->Item1.ToPointer());
            flip->Clear();
        }
    }

    static DelayedDeleteQueue::DelayedDeleteQueue()
    {
        _queue = gcnew DeletablePtrList;
    }
}

