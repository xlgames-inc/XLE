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
	using DeletablePtr = System::Tuple<System::IntPtr, DeletionCallback^>;
    using DeletablePtrList = System::Collections::Generic::List<DeletablePtr^>;
    static msclr::gcroot<DeletablePtrList^> static_queue = gcnew DeletablePtrList;

    void DelayedDeleteQueue::Add(System::IntPtr ptr, DeletionCallback^ callback)
    {
        msclr::lock l(static_queue);
        static_queue->Add(gcnew DeletablePtr(ptr, callback));
    }

    void DelayedDeleteQueue::FlushQueue()
    {
        // swap with a new list, so the list doesn't get modified by another thread as we're deleting items
        auto flip = gcnew DeletablePtrList;
        {
            msclr::lock l(static_queue);
            auto t = static_queue;
            static_queue = flip;
            flip = t;
        }

        auto count = flip->Count;
        if (count > 0) {
            Log(Verbose) << "Destroying native objects that were released indeterministically from cli code: " << count << std::endl;
            for each(auto i in flip)
                (i->Item2)(i->Item1);
            flip->Clear();
        }
    }
}
