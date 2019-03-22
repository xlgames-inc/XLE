// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Buffer.h"
#include "ObjectFactory.h"
#include "DeviceContext.h"
#include "PipelineLayout.h"

namespace RenderCore { namespace Metal_Vulkan
{

	void    Buffer::Update(DeviceContext& context, const void* data, size_t byteCount)
	{
        // hack to catch "LocalTransorm" buffer going through this path
        if (byteCount == 64) {
            context.GetActiveCommandList().PushConstants(
                context.GetPipelineLayout(DeviceContext::PipelineType::Graphics)->GetUnderlying(),
                VK_SHADER_STAGE_VERTEX_BIT,
                0, (uint32_t)byteCount, data);
            return;
        }

        // note --  there is a problem with vkCmdUpdateBuffer here... It can't normally
        //          be used during a render pass (but creating a new buffer and mapping
        //          it will work). However, creating a new buffer is still non ideal
        //          ... ideally we want to be using "push constants" for buffers that
        //          change during the render pass (or, at least, update all of the
        //          buffers before the render pass!).
        //          Probably we need to be much more strict with when we create and
        //          push data to all buffer and image types.
        const bool useUpdateBuffer = !context.IsInRenderPass();
        if (useUpdateBuffer) {
		    assert(IsGood());
            context.GetActiveCommandList().UpdateBuffer(_underlyingBuffer.get(), 0, byteCount, data);
        } else {
            Resource updatedRes(GetObjectFactory(context), GetDesc(), SubResourceInitData{ MakeIteratorRange(data, PtrAdd(data, byteCount)) });
            *(Resource*)this = std::move(updatedRes);

            // We ideally need to add a memory barrier to prevent reading from this buffer 
            // until the CPU write is completed... But we can't actually do this during a render pass!
            // VkBufferMemoryBarrier buf_barrier = {};
            // buf_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            // buf_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            // buf_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            // buf_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            // buf_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            // buf_barrier.buffer = GetUnderlying();
            // buf_barrier.offset = 0;
            // buf_barrier.size = VK_WHOLE_SIZE;
            // context.CmdPipelineBarrier(
            //     VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            //     0, 0, nullptr, 1, &buf_barrier, 0, nullptr);
        }
	}

	Buffer::Buffer(
		const ObjectFactory& factory, const Desc& desc,
		IteratorRange<const void*> initData)
	: Resource(factory, desc, SubResourceInitData{ initData })
	{
		if (desc._type != Desc::Type::LinearBuffer)
			Throw(::Exceptions::BasicLabel("Expecting linear buffer type"));
	}

	Buffer::Buffer() {}

///////////////////////////////////////////////////////////////////////////////////////////////////
    
    static ResourceDesc BuildDesc(BindFlag::BitField bindingFlags, size_t byteCount, bool immutable=true)
    {
        return CreateDesc(
            bindingFlags,
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

    Buffer MakeConstantBuffer(ObjectFactory& factory, IteratorRange<const void*> data, bool immutable)
    {
        return Buffer(
            factory,
            BuildDesc(BindFlag::ConstantBuffer, data.size(), immutable),
            data);
    }

	Buffer MakeConstantBuffer(ObjectFactory& factory, size_t size)
	{
		return Buffer(
            factory,
            BuildDesc(BindFlag::ConstantBuffer, size, false));
	}

}}

