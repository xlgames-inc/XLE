// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Resource.h"
#include "../../../Utility/IntrusivePtr.h"

namespace RenderCore { class Resource; enum class Format; }
namespace RenderCore { namespace Metal_Vulkan
{
    class DeviceContext;

    class SubResourceSlice
    {
    public:
        unsigned _arraySize;
        unsigned _firstArraySlice;
        unsigned _mipMapIndex;
        SubResourceSlice(unsigned arraySize=0, unsigned firstArraySlice=0, unsigned mipMapIndex=0) : _arraySize(arraySize), _firstArraySlice(firstArraySlice), _mipMapIndex(mipMapIndex) {}
    };
    
    class RenderTargetView
    {
    public:
		RenderTargetView(DeviceContext& context) {}
		RenderTargetView(UnderlyingResourcePtr, Format = Format(0)) {}
        RenderTargetView() {}
        ~RenderTargetView() {}

        RenderTargetView(const RenderTargetView& cloneFrom) {}
        RenderTargetView(RenderTargetView&& moveFrom) never_throws {}
        RenderTargetView& operator=(const RenderTargetView& cloneFrom) {}
        RenderTargetView& operator=(RenderTargetView&& moveFrom) never_throws {}

		typedef Resource*   UnderlyingType;
		UnderlyingType					GetUnderlying() const { return nullptr; }
		bool IsGood() const { return true; }
    };

    class DepthStencilView
    {
    public:
        DepthStencilView(DeviceContext& context) {}
		DepthStencilView(UnderlyingResourcePtr) {}
        DepthStencilView() {}
        ~DepthStencilView() {}

        DepthStencilView(const DepthStencilView& cloneFrom) {}
        DepthStencilView(DepthStencilView&& moveFrom) never_throws {}
        DepthStencilView& operator=(const DepthStencilView& cloneFrom) {}
        DepthStencilView& operator=(DepthStencilView&& moveFrom) never_throws {}

		typedef Resource*   UnderlyingType;
		UnderlyingType					GetUnderlying() const { return nullptr; }
		bool IsGood() const { return true; }
    };

    class UnorderedAccessView
    {
    public:
        struct Flags
        {
            enum Enum
            {
                AttachedCounter = 1<<0
            };
            typedef unsigned BitField;
        };
        UnorderedAccessView() {}
        ~UnorderedAccessView() {}
		UnorderedAccessView(UnderlyingResourcePtr) {}

        UnorderedAccessView(const UnorderedAccessView& cloneFrom) {}
        UnorderedAccessView(UnorderedAccessView&& moveFrom) never_throws {}
        UnorderedAccessView& operator=(const UnorderedAccessView& cloneFrom) {}
        UnorderedAccessView& operator=(UnorderedAccessView&& moveFrom) never_throws {}

		typedef Resource*   UnderlyingType;
		UnderlyingType					GetUnderlying() const { return nullptr; }
		bool IsGood() const { return true; }
    };
    
}}

