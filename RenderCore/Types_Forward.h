// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>

// Extremely awkwardly, CLR doesn't work correctly when forward declarating 
// "Format". But it's legal to forward declare an "enum class" in recent C++
// standards... Let's just add a switch based on the CLR define.
#if defined(_MANAGED)
    #include "Format.h"
#else
    namespace RenderCore { enum class Format; }
#endif

namespace RenderCore 
{
    class Resource;
	using ResourcePtr = std::shared_ptr<Resource>;

    class InputElementDesc;
	using InputLayout = std::pair<const InputElementDesc*, size_t>;
}
