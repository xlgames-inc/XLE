// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Buffer.h"
#include "DeviceContext.h"
#include "../../../Utility/PtrUtils.h"
#include <assert.h>

#include "IncludeAppleMetal.h"

namespace RenderCore { namespace Metal_AppleMetal
{
    void Buffer::Update(DeviceContext& context, const void* data, size_t dataSize, size_t writeOffset, UpdateFlags::BitField flags)
    {
        // DavidJ -- temporary hack to get around lack of correctly synchronized buffer updates. If we want to update
        //      the entire buffer, we can just create a new one
        if (writeOffset == 0 && dataSize == GetDesc()._linearBufferDesc._sizeInBytes) {
            _underlyingBuffer = GetObjectFactory().CreateBuffer(data, (unsigned)dataSize);
        } else {
            assert((GetDesc()._cpuAccess & CPUAccess::Write) != 0);
            id<MTLBuffer> buffer = (AplMtlBuffer*)GetBuffer().get();
            assert(buffer.storageMode == MTLStorageModeShared);
            void* dst = buffer.contents;
            memcpy(PtrAdd(dst, writeOffset), data, dataSize);
        }
    }

    Buffer::Buffer( ObjectFactory& factory, const ResourceDesc& desc,
                    IteratorRange<const void*> initData)
    : Resource(factory, desc, SubResourceInitData { initData, {0u, 0u, 0u} })
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
            data);
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

