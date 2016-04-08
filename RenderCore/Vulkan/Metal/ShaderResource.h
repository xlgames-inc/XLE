// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Format.h"
#include "Resource.h"
#include "VulkanCore.h"
#include "../../../Core/Prefix.h"

namespace RenderCore { namespace Metal_Vulkan
{
    class ObjectFactory;
    class SamplerState;

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
        ShaderResourceView();
        ~ShaderResourceView();
		ShaderResourceView(const ObjectFactory& factory, VkImage image, NativeFormat::Enum = NativeFormat::Unknown);
        ShaderResourceView(VkImage image, NativeFormat::Enum = NativeFormat::Unknown);
        ShaderResourceView(Underlying::Resource* res, NativeFormat::Enum = NativeFormat::Unknown);

		using UnderlyingType = VkImageView;
        using UnderlyingResource = VkImage;
		UnderlyingType			GetUnderlying() const { return _underlying.get(); }
		bool                    IsGood() const { return _underlying != nullptr; }

        VkImageLayout _layout;

        const SamplerState&     GetSampler() const;

    private:
        VulkanSharedPtr<VkImageView> _underlying;
    };
}}

