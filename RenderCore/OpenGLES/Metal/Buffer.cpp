// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Buffer.h"
#include "DeviceContext.h"
#include "../../../Utility/PtrUtils.h"
#include <assert.h>

#include "IncludeGLES.h"

namespace RenderCore { namespace Metal_OpenGLES
{
    void Buffer::Update(DeviceContext& context, const void* data, size_t dataSize, size_t writeOffset, UpdateFlags::BitField flags)
    {
        if (!(flags & UpdateFlags::UnsynchronizedWrite) && context.InRenderPass()) {
            Throw(::Exceptions::BasicLabel("Buffer::Update synchronized can only be called between render passes."));
        }

        if (GetBuffer()) {
            auto bindTarget = AsBufferTarget(GetDesc()._bindFlags);
            glBindBuffer(bindTarget, GetBuffer()->AsRawGLHandle());

            // METAL_TODO: Should unsynchronized writes on APPPORTABLE or on pre-GLES300 actually be errors instead of falling back to synchronized writes?
            #if !APPORTABLE
                if ((flags & UpdateFlags::UnsynchronizedWrite) && (context.GetFeatureSet() & FeatureSet::GLES300)) {
                    auto glFlags = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT;
                    if (flags & UpdateFlags::UnsynchronizedWrite)
                        glFlags |= GL_MAP_UNSYNCHRONIZED_BIT;
                    void* mappedData = glMapBufferRange(bindTarget, writeOffset, dataSize, glFlags);
                    std::memcpy(mappedData, data, dataSize);
                    glUnmapBuffer(bindTarget);
                } else
            #endif
            {
                glBufferSubData(bindTarget, writeOffset, dataSize, data);
            }
        } else {
            auto size = std::min(ptrdiff_t(_constantBuffer.size()) - ptrdiff_t(writeOffset), ptrdiff_t(dataSize));
            if (size > 0)
                std::copy((const uint8_t*)data, (const uint8_t*)data + size, _constantBuffer.begin() + writeOffset);
        }
    }

    Buffer::Buffer( ObjectFactory& factory, const ResourceDesc& desc,
                    IteratorRange<const void*> initData)
    : Resource(factory, desc, SubResourceInitData { initData, {0u, 0u, 0u} })
    {}

    Buffer::Buffer(const intrusive_ptr<OpenGL::Buffer>& underlying)
    {
        _underlyingBuffer = underlying;
    }

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
    
    Buffer MakeIndexBuffer(ObjectFactory& factory, IteratorRange<const void*> data)
    {
        return Buffer(
            factory,
            BuildDesc(BindFlag::IndexBuffer, data.size(), true),
            data);
    }

    Buffer MakeConstantBuffer(ObjectFactory& factory, IteratorRange<const void*> data)
    {
        return Buffer(
            factory,
            BuildDesc(BindFlag::ConstantBuffer, data.size(), true),
            data);
    }
}}

