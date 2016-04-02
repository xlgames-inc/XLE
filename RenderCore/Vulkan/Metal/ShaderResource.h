// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Resource.h"
#include "Format.h"
#include "../../../Core/Prefix.h"

namespace RenderCore { namespace Metal_Vulkan
{
    class MipSlice
    {
    public:
        unsigned _mostDetailedMip;
        unsigned _mipLevels;
        MipSlice(unsigned mostDetailedMip = 0, unsigned mipLevels = 1) : _mostDetailedMip(mostDetailedMip), _mipLevels(mipLevels) {}
    };

    class ShaderResourceView
    {
    public:
        ShaderResourceView() {}
        ~ShaderResourceView() {}
		ShaderResourceView(Underlying::Resource*, NativeFormat::Enum = NativeFormat::Unknown) {}

        ShaderResourceView(const ShaderResourceView& cloneFrom) {}
        ShaderResourceView(ShaderResourceView&& moveFrom) never_throws {}
        ShaderResourceView& operator=(const ShaderResourceView& cloneFrom) {}
        ShaderResourceView& operator=(ShaderResourceView&& moveFrom) never_throws {}

		typedef Underlying::Resource*   UnderlyingResource;
		typedef Underlying::Resource*   UnderlyingType;
		UnderlyingType					GetUnderlying() const { return nullptr; }
		bool IsGood() const { return true; }
    };
}}

