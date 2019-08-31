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

        // METAL_TODO: Instead of allocating a new buffer and giving it to the GPU to blit from (which defeats the whole performance purpose of calling Update, although it might still be simpler to use in some cases, and at least it allows us to test the API...), we should have some kind of pool of buffers, or just a shared DynamicBuffer per context or something. Until we do that, it would be more efficient to just:
        // if (writeOffset == 0 && dataSize == GetDesc()._linearBufferDesc._sizeInBytes) {
        //     _underlyingBuffer = GetObjectFactory().CreateBuffer(data, (unsigned)dataSize);
        // }

        // METAL_TODO: This should be checking that we're not inside a render pass; this instead checks that we're not inside a subpass.
        if (context.HasEncoder()) {
            Throw(::Exceptions::BasicLabel("Buffer::Update synchronized can only be called between render passes."));
        }

        TBC::OCPtr<id> buffer = (AplMtlBuffer*)GetBuffer().get(); // id<MTLBuffer>
        // METAL_TODO: Implementing this for managed-mode buffers is pretty simple, but those only exist on macOS, so we probably never have them?
        assert([buffer.get() storageMode] == MTLStorageModeShared);

        assert(dataSize < MAX_INT);
        TBC::OCPtr<id> commandBuffer(context.RetrieveCommandBuffer());
        id<MTLBuffer> blitBuffer = [GetObjectFactory().CreateBuffer((const unsigned char *)data, (unsigned)dataSize).get() retain];
        [commandBuffer addCompletedHandler:^(id){ [blitBuffer release]; }];

        // METAL_TODO: The intended design (because it's the best granularity we can get with Vulkan) is to only allow blit updates at the end of, or between, rendering passes. But this allows them at the end of any subpass.
        context.OnDestroyEncoder([&context, blitBuffer, buffer, writeOffset, dataSize]() {
            context.CreateBlitCommandEncoder();
            id<MTLBlitCommandEncoder> encoder = context.GetBlitCommandEncoder();
            [encoder copyFromBuffer:blitBuffer sourceOffset:0 toBuffer:buffer destinationOffset:writeOffset size:dataSize];
            context.EndEncoding();
            context.DestroyBlitCommandEncoder();
        });
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

