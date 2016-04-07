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
    static VkBufferUsageFlags AsUsageFlags(BufferUploads::BindFlag::BitField bindFlags)
    {
        using namespace BufferUploads;
        VkBufferUsageFlags result = 0;
        if (bindFlags & BindFlag::VertexBuffer) result |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if (bindFlags & BindFlag::IndexBuffer) result |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (bindFlags & BindFlag::ConstantBuffer) result |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        if (bindFlags & BindFlag::DrawIndirectArgs) result |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

        // Other Vulkan flags:
        // VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        // VK_BUFFER_USAGE_TRANSFER_DST_BIT
        // VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
        // VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT
        // VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        return result;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class MemoryMap
    {
    public:
        void*       _data;

        MemoryMap(
            VkDevice dev, VkDeviceMemory memory, 
            VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
        MemoryMap();
        ~MemoryMap();

        MemoryMap(const MemoryMap&) = delete;
        MemoryMap& operator=(const MemoryMap&) = delete;
        MemoryMap(MemoryMap&&);
        MemoryMap& operator=(MemoryMap&&);

    private:
        VkDevice _dev;
        VkDeviceMemory _mem;

        void TryUnmap();
    };

    MemoryMap::MemoryMap(
        VkDevice dev, VkDeviceMemory memory, 
        VkDeviceSize offset, VkDeviceSize size)
    : _dev(dev), _mem(memory)
    {
        // There are many restrictions on this call -- see the Vulkan docs.
        // * we must ensure that the memory was allocated with VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        // * we must ensure that the memory was allocated with VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        //          (because we're not performing manual memory flushes)
        // * we must ensure that it is not being used by the GPU during the map
        auto res = vkMapMemory(dev, memory, offset, size, 0, &_data);
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failed while mapping device memory"));
    }

    void MemoryMap::TryUnmap()
    {
        vkUnmapMemory(_dev, _mem);
    }

    MemoryMap::MemoryMap() : _dev(nullptr), _mem(nullptr), _data(nullptr) {}

    MemoryMap::~MemoryMap()
    {
        TryUnmap();
    }

    MemoryMap::MemoryMap(MemoryMap&& moveFrom)
    {
        _data = moveFrom._data; moveFrom._data = nullptr;
        _dev = moveFrom._dev; moveFrom._dev = nullptr;
        _mem = moveFrom._mem; moveFrom._mem = nullptr;
    }

    MemoryMap& MemoryMap::operator=(MemoryMap&& moveFrom)
    {
        TryUnmap();
        _data = moveFrom._data; moveFrom._data = nullptr;
        _dev = moveFrom._dev; moveFrom._dev = nullptr;
        _mem = moveFrom._mem; moveFrom._mem = nullptr;
        return *this;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    Buffer::Buffer(
        const ObjectFactory& factory, const Desc& desc,
        const void* initData, size_t initDataSize)
    {
        if (desc._type != Desc::Type::LinearBuffer)
            Throw(::Exceptions::BasicLabel("Invalid desc passed to buffer constructor"));

        VkBufferCreateInfo buf_info = {};
        buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buf_info.pNext = nullptr;
        buf_info.usage = AsUsageFlags(desc._bindFlags);
        buf_info.size = desc._linearBufferDesc._sizeInBytes;
        buf_info.queueFamilyIndexCount = 0;
        buf_info.pQueueFamilyIndices = nullptr;
        buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;   // sharing between queues
        buf_info.flags = 0;     // flags for sparse buffers

        // set this flag to enable usage with "vkCmdUpdateBuffer"
        if (    (desc._cpuAccess & BufferUploads::CPUAccess::Write)
            ||  (desc._cpuAccess & BufferUploads::CPUAccess::WriteDynamic))
            buf_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        _underlying = factory.CreateBuffer(buf_info);

        VkMemoryRequirements mem_reqs = {};
        vkGetBufferMemoryRequirements(factory.GetDevice().get(), _underlying.get(), &mem_reqs);

        // todo -- relax memory requirements based on the flags in "desc"
        auto memoryRequirements = 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        auto memoryTypeIndex = factory.FindMemoryType(mem_reqs.memoryTypeBits, memoryRequirements);
        if (memoryTypeIndex >= 32)
            Throw(::Exceptions::BasicLabel("Could not find valid memory type for buffer"));

        auto devMem = factory.AllocateMemory(mem_reqs.size, memoryTypeIndex);

        if (initData && initDataSize) {
            MemoryMap map(factory.GetDevice().get(), devMem.get());
            std::memcpy(map._data, initData, std::min(initDataSize, (size_t)buf_info.size));
        }

        const VkDeviceSize offset = 0;
        auto res = vkBindBufferMemory(factory.GetDevice().get(), _underlying.get(), devMem.get(), offset);
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failed while binding a buffer to device memory"));
    }

    Buffer::Buffer()
    {
    }

    void    Buffer::Update(DeviceContext& context, const void* data, size_t byteCount)
    {
        assert(byteCount <= 65536);
        assert((byteCount & (4-1)) == 0);  // must be a multiple of 4
        assert(byteCount > 0 && data);
        vkCmdUpdateBuffer(
            context.GetPrimaryCommandList().get(),
            _underlying.get(), 0,
            byteCount, (const uint32_t*)data);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
    
    static BufferUploads::BufferDesc BuildDesc(
        BufferUploads::BindFlag::BitField bindingFlags, size_t byteCount, bool immutable=true)
    {
        using namespace BufferUploads;
        return CreateDesc(
            bindingFlags, 
            immutable ? 0 : CPUAccess::Read, 
            GPUAccess::Read, 
            LinearBufferDesc::Create(unsigned(byteCount)), 
            "buf");
    }


    VertexBuffer::VertexBuffer() {}
    VertexBuffer::VertexBuffer(const void* data, size_t byteCount)
    : VertexBuffer(GetDefaultObjectFactory(), data, byteCount)
    {}

    VertexBuffer::VertexBuffer(const ObjectFactory& factory, const void* data, size_t byteCount)
    : Buffer(factory, BuildDesc(BufferUploads::BindFlag::VertexBuffer, byteCount))
    {}

    IndexBuffer::IndexBuffer() {}
    IndexBuffer::IndexBuffer(const void* data, size_t byteCount)
    : IndexBuffer(GetDefaultObjectFactory(), data, byteCount)
    {}

    IndexBuffer::IndexBuffer(const ObjectFactory& factory, const void* data, size_t byteCount)
    : Buffer(factory, BuildDesc(BufferUploads::BindFlag::IndexBuffer, byteCount))
    {}

    ConstantBuffer::ConstantBuffer() {}
    ConstantBuffer::ConstantBuffer(const void* data, size_t byteCount, bool immutable)
    : ConstantBuffer(GetDefaultObjectFactory(), data, byteCount, immutable)
    {}

    ConstantBuffer::ConstantBuffer(const ObjectFactory& factory, const void* data, size_t byteCount, bool immutable)
    : Buffer(factory, BuildDesc(BufferUploads::BindFlag::ConstantBuffer, byteCount, immutable))
    {}

}}

