// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

namespace GUILayer
{
    public ref class DelayedDeleteQueue
    {
    public:
        delegate void DeletionCallback(void*);

        static void Add(void* ptr, DeletionCallback^ callback);
        static void FlushQueue();

    private:
        using DeletablePtr = System::Tuple<System::IntPtr, DeletionCallback^>;
        using DeletablePtrList = System::Collections::Generic::List<DeletablePtr^>;
        static DeletablePtrList^ _queue;
        static DelayedDeleteQueue();
    };
}

