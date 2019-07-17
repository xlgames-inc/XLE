// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

// Since DelayedDeleteQueue is a ref class, we can't include the declaration in modules other than GUILayer.
// They will reference the class via the import table of GUILayer; a declaration here ends up causing a 
// duplicate symbol error.
#if defined(COMPILING_GUILAYER)

#include <msclr/gcroot.h>

namespace GUILayer
{
	public delegate void DeletionCallback(System::IntPtr);

    public ref class DelayedDeleteQueue sealed
    {
    public:
        static void Add(System::IntPtr ptr, DeletionCallback^ callback);
        static void FlushQueue();
    };
}

#endif
