// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Buffer.h"
#include "ObjectFactory.h"
#include "DeviceContext.h"

namespace RenderCore { namespace Metal_Vulkan
{

	void    Buffer::Update(DeviceContext& context, const void* data, size_t byteCount)
	{
        // note --  there is a problem wiht vkCmdUpdateBuffer here... It can't normally
        //          be used during a render pass (but creating a new buffer and mapping
        //          it will work). However, creating a new buffer is still non ideal
        //          ... ideally we want to be using "push constants" for buffers that
        //          change during the render pass (or, at least, update all of the command
        //          buffers before the render pass!)
        const bool useUpdateBuffer = false;
        if (constant_expression<useUpdateBuffer>::result()) {
		    assert(IsGood());
		    assert(byteCount <= 65536);
		    assert((byteCount & (4 - 1)) == 0);  // must be a multiple of 4
		    assert(byteCount > 0 && data);
		    vkCmdUpdateBuffer(
			    context.GetCommandList(),
			    _underlyingBuffer.get(), 0,
			    byteCount, (const uint32_t*)data);
        } else {
            Resource updatedRes(GetObjectFactory(context), GetDesc(), SubResourceInitData{ data, byteCount });
            *(Resource*)this = std::move(updatedRes);
        }
	}

	Buffer::Buffer(
		const ObjectFactory& factory, const Desc& desc,
		const void* initData, size_t initDataSize)
	: Resource(factory, desc, SubResourceInitData{ initData, initDataSize })
	{
		if (desc._type != Desc::Type::LinearBuffer)
			Throw(::Exceptions::BasicLabel("Expecting linear buffer type"));
	}

	Buffer::Buffer() {}

///////////////////////////////////////////////////////////////////////////////////////////////////
    
    static BufferUploads::BufferDesc BuildDesc(
        BufferUploads::BindFlag::BitField bindingFlags, size_t byteCount, bool immutable=true)
    {
        using namespace BufferUploads;
        return CreateDesc(
            bindingFlags | (immutable ? 0 : BindFlag::TransferDst), 
            immutable ? 0 : CPUAccess::Write, 
            GPUAccess::Read, 
            LinearBufferDesc::Create(unsigned(byteCount)), 
            "buf");
    }


    VertexBuffer::VertexBuffer() {}
    VertexBuffer::VertexBuffer(const void* data, size_t byteCount)
    : VertexBuffer(GetObjectFactory(), data, byteCount)
    {}

    VertexBuffer::VertexBuffer(const ObjectFactory& factory, const void* data, size_t byteCount)
    : Buffer(factory, BuildDesc(BufferUploads::BindFlag::VertexBuffer, byteCount, data != nullptr), data, byteCount)
    {}

    IndexBuffer::IndexBuffer() {}
    IndexBuffer::IndexBuffer(const void* data, size_t byteCount)
    : IndexBuffer(GetObjectFactory(), data, byteCount)
    {}

    IndexBuffer::IndexBuffer(const ObjectFactory& factory, const void* data, size_t byteCount)
    : Buffer(factory, BuildDesc(BufferUploads::BindFlag::IndexBuffer, byteCount, data != nullptr), data, byteCount)
    {}

    ConstantBuffer::ConstantBuffer() {}
    ConstantBuffer::ConstantBuffer(const void* data, size_t byteCount, bool immutable)
    : ConstantBuffer(GetObjectFactory(), data, byteCount, immutable)
    {}

    ConstantBuffer::ConstantBuffer(const ObjectFactory& factory, const void* data, size_t byteCount, bool immutable)
    : Buffer(factory, BuildDesc(BufferUploads::BindFlag::ConstantBuffer, byteCount, immutable && (data != nullptr)), data, byteCount)
    {}

}}

