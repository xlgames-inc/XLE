// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Utility/IteratorUtils.h"
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
    class InputElementDesc;
	using InputLayout = IteratorRange<const InputElementDesc*>;

	enum class Topology;
	enum class AddressMode;
	enum class FilterMode;
	enum class Comparison;
	enum class CullMode;
	enum class FillMode;
	enum class Blend;
	enum class BlendOp;
	enum class StencilOp;
}
