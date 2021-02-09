// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Buffer.h"
#include "DeviceContext.h"
#include "../../../Utility/PtrUtils.h"
#include "../../Core/Exceptions.h"
#include <assert.h>

#include "IncludeAppleMetal.h"

namespace RenderCore { namespace Metal_AppleMetal
{
    void Buffer::Update(DeviceContext& context, const void* data, size_t dataSize, size_t writeOffset, UpdateFlags::BitField flags)
    {
        if (flags & UpdateFlags::UnsynchronizedWrite) {
            assert((GetDesc()._cpuAccess & CPUAccess::Write) != 0);
            id<MTLBuffer> buffer = (AplMtlBuffer*)GetBuffer().get();
            assert(buffer.storageMode == MTLStorageModeShared);
            void* dst = buffer.contents;
            memcpy(PtrAdd(dst, writeOffset), data, dataSize);
            return;
        }

        // NOTE: This implementation is designed to be bulletproof, and to work exaxtly the same way on GL, Apple Metal, and Vulkan, for cases where updating a buffer synchronously is convenient. If you're naively using it as an optimization (which sort of works in GL-only code), it's actually making your code a lot slower; what you want is to use unsynchronized writes, and add your own sync on top--or use Magnesium::DynamicBuffer, which gives you allocations out of a big shared buffer and wraps all of that up for you.

        if (context.InRenderPass()) {
            Throw(::Exceptions::BasicLabel("Buffer::Update synchronized can only be called between render passes."));
        }

        IdPtr buffer = (AplMtlBuffer*)GetBuffer().get(); // id<MTLBuffer>
        // METAL_TODO: Implementing this for managed-mode buffers is simple, but those only exist on macOS, so we probably never have them.
        assert([buffer.get() storageMode] == MTLStorageModeShared);

        assert(dataSize < MAX_INT);
        IdPtr commandBuffer(context.RetrieveCommandBuffer());
        id<MTLBuffer> blitBuffer = [GetObjectFactory().CreateBuffer((const unsigned char *)data, (unsigned)dataSize).get() retain];
        [commandBuffer addCompletedHandler:^(id){ [blitBuffer release]; }];

        context.CreateBlitCommandEncoder();
        id<MTLBlitCommandEncoder> encoder = context.GetBlitCommandEncoder();
        [encoder copyFromBuffer:blitBuffer sourceOffset:0 toBuffer:buffer destinationOffset:writeOffset size:dataSize];
        context.EndEncoding();
        context.DestroyBlitCommandEncoder();
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

