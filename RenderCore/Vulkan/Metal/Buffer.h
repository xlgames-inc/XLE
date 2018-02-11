// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Resource.h"
#include "VulkanCore.h"
#include "../../../BufferUploads/IBufferUploads.h"
#include "../../../Core/Prefix.h"

namespace RenderCore { namespace Metal_Vulkan
{
    class ObjectFactory;
    class DeviceContext;

    class Buffer : public Resource
    {
	public:
		Buffer(
			const ObjectFactory& factory, const Desc& desc,
			IteratorRange<const void*> initData = {});
		Buffer();

		void    Update(DeviceContext& context, const void* data, size_t byteCount);

		using UnderlyingType = VkBuffer;
		UnderlyingType		GetUnderlying() const { return _underlyingBuffer.get(); }
		bool                IsGood() const { return _underlyingBuffer != nullptr; }
    };

	using ConstantBuffer = Buffer;

        ////////////////////////////////////////////////////////////////////////////////////////////////

    Buffer MakeVertexBuffer(ObjectFactory& factory, IteratorRange<const void*>);
    Buffer MakeIndexBuffer(ObjectFactory& factory, IteratorRange<const void*>);
    Buffer MakeConstantBuffer(ObjectFactory& factory, IteratorRange<const void*>, bool immutable = true);
	Buffer MakeConstantBuffer(ObjectFactory& factory, size_t size);

}}

