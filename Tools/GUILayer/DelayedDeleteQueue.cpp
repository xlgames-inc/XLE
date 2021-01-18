// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DelayedDeleteQueue.h"
#include "../../OSServices/Log.h"
#include <msclr/lock.h>
#include <msclr/auto_gcroot.h>

namespace GUILayer
{
	using DeletablePtr = System::Tuple<System::IntPtr, DeletionCallback^>;
    using DeletablePtrList = System::Collections::Generic::List<DeletablePtr^>;

	static bool static_hasQueue = false;
	class QueueContainer
	{
	public:
		msclr::auto_gcroot<DeletablePtrList^> _queue = gcnew DeletablePtrList;
		QueueContainer() { static_hasQueue = true; }
		~QueueContainer() { static_hasQueue = false; }
	};

	static QueueContainer static_queue;

    void DelayedDeleteQueue::Add(System::IntPtr ptr, DeletionCallback^ callback)
    {
		if (static_hasQueue) {
			msclr::lock l(static_queue._queue.get());
			static_queue._queue->Add(gcnew DeletablePtr(ptr, callback));
		} else {
			callback(ptr);
		}
    }

    void DelayedDeleteQueue::FlushQueue()
    {
        // swap with a new list, so the list doesn't get modified by another thread as we're deleting items
        msclr::auto_gcroot<DeletablePtrList^> flip = gcnew DeletablePtrList;
        {
            msclr::lock l(static_queue._queue.get());
            static_queue._queue.swap(flip);
        }

        auto count = flip->Count;
        if (count > 0) {
            Log(Verbose) << "Destroying native objects that were released indeterministically from cli code: " << count << std::endl;
            for each(auto i in flip.get())
                (i->Item2)(i->Item1);
            flip->Clear();
        }
    }
}
