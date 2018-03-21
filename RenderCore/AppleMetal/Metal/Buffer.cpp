// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Buffer.h"
#include "DeviceContext.h"
#include "../../../Utility/PtrUtils.h"
#include <assert.h>

namespace RenderCore { namespace Metal_AppleMetal
{
    void Buffer::Update(DeviceContext& context, const void* data, size_t dataSize, size_t writeOffset, UpdateFlags::BitField flags)
    {
        /* KenD -- Metal TODO -- implement updating buffer (required for DynamicGeoBuffer and other cases like non-tracking particle emitters) */
        //assert(0);
    }

    Buffer::Buffer( ObjectFactory& factory, const ResourceDesc& desc,
                    const void* initData, size_t initDataSize)
    : Resource(factory, desc, SubResourceInitData { {initData, PtrAdd(initData, initDataSize)}, {0u, 0u, 0u} })
    {}

    Buffer::Buffer() {}
    Buffer::~Buffer() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    static ResourceDesc BuildDesc(BindFlag::BitField bindingFlags, size_t byteCount, bool immutable=true)
    {
        return CreateDesc(
            bindingFlags | (immutable ? 0 : BindFlag::TransferDst),
            immutable ? 0 : CPUAccess::Write,
            GPUAccess::Read,
            LinearBufferDesc::Create(unsigned(byteCount)),
            "buf");
    }

    Buffer MakeVertexBuffer(ObjectFactory& factory, IteratorRange<const void*> data)
    {
        return Buffer(
            factory,
            BuildDesc(BindFlag::VertexBuffer, data.size(), true),
            data.begin(),
            data.size());
    }
    
    Resource MakeIndexBuffer(ObjectFactory& factory, IteratorRange<const void*> data)
    {
        return Resource(
            factory,
            BuildDesc(BindFlag::IndexBuffer, data.size(), true),
            SubResourceInitData { data, {0u, 0u, 0u} });
    }

    Resource MakeConstantBuffer(ObjectFactory& factory, IteratorRange<const void*> data)
    {
        return Resource(
            factory,
            BuildDesc(BindFlag::ConstantBuffer, data.size(), true),
            SubResourceInitData { data, {0u, 0u, 0u} });
    }
}}

