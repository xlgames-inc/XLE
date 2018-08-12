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

	static bool static_hasQueue = false;
	class QueueContainer
	{
	public:
		msclr::gcroot<DeletablePtrList^> _queue = gcnew DeletablePtrList;
		QueueContainer() { static_hasQueue = true; }
		~QueueContainer() { static_hasQueue = false; }
	};

	static QueueContainer static_queue;

    void DelayedDeleteQueue::Add(System::IntPtr ptr, DeletionCallback^ callback)
    {
		if (static_hasQueue) {
			msclr::lock l(static_queue._queue);
			static_queue._queue->Add(gcnew DeletablePtr(ptr, callback));
		} else {
			callback(ptr);
		}
    }

    void DelayedDeleteQueue::FlushQueue()
    {
        // swap with a new list, so the list doesn't get modified by another thread as we're deleting items
        auto flip = gcnew DeletablePtrList;
        {
            msclr::lock l(static_queue._queue);
            auto t = static_queue._queue;
            static_queue._queue = flip;
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
