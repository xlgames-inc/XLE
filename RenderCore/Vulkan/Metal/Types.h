// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../../Utility/IntrusivePtr.h"
#include "../../../Utility/Threading/ThreadingUtils.h"

namespace RenderCore { namespace Metal_Vulkan
{
	using DummyQuery = RefCountedObject;
    typedef intrusive_ptr<DummyQuery>            UnderlyingQuery;
}}
