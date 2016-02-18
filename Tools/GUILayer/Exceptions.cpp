// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Exceptions.h"
#include "MarshalString.h"

namespace GUILayer
{
	System::Exception^ Marshal(const std::exception& e)
	{
		// note --	when we do this marshalling step, we loose the callstack
		//			that we would normally get with a SEHException, but we gain
		//			the exception message from std::exception::what()
		//
		// It's difficult to grab the callstack from when the exception
		// occurred, because we've caught it using the c++ mechanisms, rather than
		// the SEH mechanisms.
		return gcnew System::Exception(clix::marshalString<clix::E_UTF8>(e.what()));
	}
}
