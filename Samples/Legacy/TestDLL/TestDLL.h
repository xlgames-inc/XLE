// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Core/Prefix.h"
#include <utility>

#if defined(PROJECT_TEST_DLL)
	#define TESTDLL_API dll_export
#else
	#define TESTDLL_API dll_import
#endif

namespace Samples
{
	TESTDLL_API void	Startup();
	TESTDLL_API void	Shutdown();
	TESTDLL_API std::pair<const char*, const char*>     GetVersionInformation();

	TESTDLL_API void* AllocateBlock(size_t size);
	TESTDLL_API void  DeallocateBlock(void* blk);
}

