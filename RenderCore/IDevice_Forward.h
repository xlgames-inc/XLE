// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Prefix.h"
#include <memory>

namespace RenderCore
{
    class IPresentationChain;
    class IDevice;
    class IResource;

    using Resource = IResource;
    using ResourcePtr = std::shared_ptr<IResource>;
    using IResourcePtr = std::shared_ptr<IResource>;
    class ResourceDesc;
    class PresentationChainDesc;
    class SubResourceInitData;
    class PresentationChainDesc;
    class SubResourceId;

    using Base_PresentationChain = IPresentationChain;
    using Base_Device = IDevice;
    using Base_Resource = IResource;
}

