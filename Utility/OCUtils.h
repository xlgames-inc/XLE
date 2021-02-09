// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#if !defined(__OBJC__)
	#error This file is only useful when used in ObjC++ code. Either the source file including this is not Objective-C++, or the correct compiler settings have not be enabled
#endif

#include "IntrusivePtr.h"
#import <Foundation/NSObjCRuntime.h>
#import <Foundation/NSObject.h>

namespace Utility
{
	template <typename T>
    	using OCPtr = intrusive_ptr<T>;

	using IdPtr = intrusive_ptr<NSObject>;
}

inline void intrusive_ptr_add_ref(id p) { [p retain]; }
inline void intrusive_ptr_release(id p) { [p release]; }

using namespace Utility;
