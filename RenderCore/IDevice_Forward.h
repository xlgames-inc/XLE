// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Prefix.h"
#include <memory>

        // // // //      Flexible interfaces configuration      // // // //
#define FLEX_USE_VTABLE_PresentationChain    1
#define FLEX_USE_VTABLE_Device               1
#define FLEX_USE_VTABLE_Resource_            1

namespace RenderCore
{
    #define FLEX_INTERFACE PresentationChain
#include "FlexForward.h"
    #undef FLEX_INTERFACE
    #define FLEX_INTERFACE Device
#include "FlexForward.h"
    #undef FLEX_INTERFACE
    #define FLEX_INTERFACE Resource_
#include "FlexForward.h"
    #undef FLEX_INTERFACE

	using Resource = IResource_;
    using IResource = IResource_;
	using ResourcePtr = std::shared_ptr<IResource>;
    using IResourcePtr = std::shared_ptr<IResource>;
	class ResourceDesc;
	class SubResourceInitData;
	class PresentationChainDesc;
	class SubResourceId;
}
