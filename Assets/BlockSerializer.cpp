// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "BlockSerializer.h"
#include "../Utility/MemoryUtils.h"

namespace Serialization
{

        ////////////////////////////////////////////////////////////

    template<typename Type>
        void PushBack(std::vector<uint8>& buffer, const Type& type)
    {
        std::copy(  (const uint8*)&type, (const uint8*)PtrAdd(&type, sizeof(Type)), 
                    std::back_inserter(buffer));
    }

    void NascentBlockSerializer::PushBackPointer(size_t value)
    {
        PushBack(_memory, value);
    }

    void    NascentBlockSerializer::PushBackPlaceholder(SpecialBuffer::Enum specialBuffer)
    {
        if (specialBuffer == SpecialBuffer::String) {
            _memory.insert(_memory.end(), sizeof(std::string), 0);
        } else if (specialBuffer == SpecialBuffer::VertexBuffer || specialBuffer == SpecialBuffer::IndexBuffer) {
            assert(0);
        } else if (specialBuffer == SpecialBuffer::Vector) {
            _memory.insert(_memory.end(), sizeof(std::vector<unsigned, BlockSerializerAllocator<unsigned>>), 0);
        } else if (specialBuffer == SpecialBuffer::UniquePtr) {
            _memory.insert(_memory.end(), sizeof(std::unique_ptr<void, BlockSerializerDeleter<void>>), 0);
        } else if (specialBuffer == SpecialBuffer::Unknown) {
            PushBackPointer(0);
        }
    }

    void    NascentBlockSerializer::SerializeSpecialBuffer( 
                    SpecialBuffer::Enum specialBuffer, 
                    const void* begin, const void* end)
    {
        InternalPointer newPointer;
        newPointer._pointerOffset    = _memory.size();
        newPointer._subBlockOffset   = _trailingSubBlocks.size();
        newPointer._subBlockSize     = ptrdiff_t(end) - ptrdiff_t(begin);
        newPointer._specialBuffer    = specialBuffer;
        _internalPointers.push_back(newPointer);

        std::copy((const uint8*)begin, (const uint8*)end, std::back_inserter(_trailingSubBlocks));

            //
            //      =<>=    Write blank space for this special buffer   =<>=
            //
        PushBackPlaceholder(specialBuffer);
    }

    void    NascentBlockSerializer::SerializeValue(uint8     value)
    {
        std::copy(  (const uint8*)&value, (const uint8*)PtrAdd(&value, sizeof(value)), 
                    std::back_inserter(_memory));
    }

    void    NascentBlockSerializer::SerializeValue(uint16    value)
    {
        std::copy(  (const uint8*)&value, (const uint8*)PtrAdd(&value, sizeof(value)), 
                    std::back_inserter(_memory));
    }

    void    NascentBlockSerializer::SerializeValue(uint32    value)
    {
        std::copy(  (const uint8*)&value, (const uint8*)PtrAdd(&value, sizeof(value)), 
                    std::back_inserter(_memory));
    }

    void    NascentBlockSerializer::SerializeValue(uint64    value)
    {
        std::copy(  (const uint8*)&value, (const uint8*)PtrAdd(&value, sizeof(value)), 
                    std::back_inserter(_memory));
    }

    void    NascentBlockSerializer::SerializeValue(float     value)
    {
        std::copy(  (const uint8*)&value, (const uint8*)PtrAdd(&value, sizeof(value)), 
                    std::back_inserter(_memory));
    }

    void NascentBlockSerializer::PushBackRaw(const void* data, size_t size)
    {
        std::copy(  (const uint8*)data, (const uint8*)PtrAdd(data, size), 
                    std::back_inserter(_memory));
    }

    void NascentBlockSerializer::PushBackRaw_SubBlock(const void* data, size_t size)
    {
        std::copy(  (const uint8*)data, (const uint8*)PtrAdd(data, size), 
                    std::back_inserter(_trailingSubBlocks));
    }

    void NascentBlockSerializer::PushBackInternalPointer(const InternalPointer& ptr)
    {
        _internalPointers.push_back(ptr);
    }

    void    NascentBlockSerializer::SerializeSubBlock(NascentBlockSerializer& subBlock, SpecialBuffer::Enum specialBuffer)
    {
            //
            //      Merge in the block we've just serialised, and write
            //      an internal pointer record for it.
            //

        InternalPointer ptr;
        ptr._pointerOffset   = _memory.size();
        ptr._subBlockOffset  = _trailingSubBlocks.size();
        ptr._subBlockSize    = subBlock._memory.size();
        ptr._specialBuffer   = specialBuffer;
        PushBackInternalPointer(ptr);

        PushBackPlaceholder(specialBuffer);

            //
            //      All of the internal pointer records should be merged in
            //      on an offset, also.
            //

        for (   auto    i =  subBlock._internalPointers.cbegin(); 
                        i != subBlock._internalPointers.cend(); ++i) {
            InternalPointer p     = *i;
            if (p._pointerOffset & PtrFlagBit) {
                p._pointerOffset     = (p._pointerOffset&PtrMask) + subBlock._memory.size() + ptr._subBlockOffset;
            } else {
                p._pointerOffset    +=  ptr._subBlockOffset;
            }
                //
                //      Because the pointer itself is in the "subblock" part, we
                //      need tag it some way, to prevent confusion with pointers
                //      in the "memory" part. Let's just set it negative.
                //
            p._pointerOffset     |= PtrFlagBit;
            p._subBlockOffset    +=  ptr._subBlockOffset + subBlock._memory.size();
            PushBackInternalPointer(p);
        }

        PushBackRaw_SubBlock(AsPointer(subBlock._memory.begin()), subBlock._memory.size());
        PushBackRaw_SubBlock(AsPointer(subBlock._trailingSubBlocks.begin()), subBlock._trailingSubBlocks.size());
    }

    void NascentBlockSerializer::SerializeValue(const std::string& value)
    {
        SerializeSpecialBuffer(SpecialBuffer::String, AsPointer(value.begin()), AsPointer(value.end()));
    }

    class Header
    {
    public:
        size_t  _rawMemorySize;
        size_t  _internalPointerCount;
    };
    
    std::unique_ptr<uint8[]>      NascentBlockSerializer::AsMemoryBlock()
    {
        std::unique_ptr<uint8[]> result = std::make_unique<uint8[]>(
            sizeof(Header)
            + _memory.size()
            + _trailingSubBlocks.size()
            + _internalPointers.size() * sizeof(InternalPointer));

        ((Header*)result.get())->_rawMemorySize = _memory.size() + _trailingSubBlocks.size();
        ((Header*)result.get())->_internalPointerCount = _internalPointers.size();

        std::copy(  AsPointer(_memory.begin()), AsPointer(_memory.end()),
                    PtrAdd(result.get(), sizeof(Header)));

        std::copy(  AsPointer(_trailingSubBlocks.begin()), AsPointer(_trailingSubBlocks.end()),
                    PtrAdd(result.get(), _memory.size() + sizeof(Header)));

        InternalPointer* d = (InternalPointer*)PtrAdd(result.get(), _memory.size() + _trailingSubBlocks.size() + sizeof(Header));
        for (auto i=_internalPointers.cbegin(); i!=_internalPointers.cend(); ++i, ++d) {
            *d = *i;
                //      pointers in the subblock part are marked as negative... But what about zero? 
                //      It could be in the memory part, or the subblock part
            if (d->_pointerOffset & PtrFlagBit) {
                d->_pointerOffset = (d->_pointerOffset&PtrMask) + _memory.size();
            }
            d->_subBlockOffset += _memory.size();
        }

        return result;
    }

    NascentBlockSerializer::NascentBlockSerializer()
    {
    }

    NascentBlockSerializer::~NascentBlockSerializer()
    {

    }

    #undef new

    void        Block_Initialize(void* block, const void* base)
    {
        if (!base) { base = block; }

        const Header& h = *(const Header*)block;
        const NascentBlockSerializer::InternalPointer* ptrTable = 
            (const NascentBlockSerializer::InternalPointer*)PtrAdd(block, sizeof(Header)+h._rawMemorySize);

        for (unsigned c=0; c<h._internalPointerCount; ++c) {
            const NascentBlockSerializer::InternalPointer& ptr = ptrTable[c];
            if (ptr._specialBuffer == NascentBlockSerializer::SpecialBuffer::Unknown) {
                *(size_t*)PtrAdd(block, sizeof(Header)+ptr._pointerOffset) = 
                    ptr._subBlockOffset + size_t(base) + sizeof(Header);
            } else if ( ptr._specialBuffer == NascentBlockSerializer::SpecialBuffer::VertexBuffer
                ||      ptr._specialBuffer == NascentBlockSerializer::SpecialBuffer::IndexBuffer) {
                assert(0);
            } else if (ptr._specialBuffer == NascentBlockSerializer::SpecialBuffer::String) {
                new(PtrAdd(block, sizeof(Header)+ptr._pointerOffset)) std::string(
                    (const char*)(ptr._subBlockOffset + size_t(base) + sizeof(Header)),
                    ptr._subBlockSize);
            } else if (ptr._specialBuffer == NascentBlockSerializer::SpecialBuffer::Vector) {
                size_t* o = (size_t*)PtrAdd(block, sizeof(Header)+ptr._pointerOffset);
                #if (STL_ACTIVE == STL_MSVC) && (_ITERATOR_DEBUG_LEVEL != 0)
                        //  in debug, to produce a valid vector, we need to add one of these proxy
                        //  objects. it requires some extra memory management; but fortunately it's
                        //  just debug builds!
                        //  Note that this requires that the caller correctly call the destructors
                        //  on the returned objects to make sure the vector destructor deletes this
                        //  object!
                    auto proxy = std::make_unique<std::_Container_proxy>();
                    proxy->_Mycont = (std::_Container_base12*)o;
                    *o++ = size_t(proxy.release());
                #endif
                o[0] = (ptr._subBlockOffset + size_t(base) + sizeof(Header));
                o[1] = (ptr._subBlockOffset + ptr._subBlockSize + size_t(base) + sizeof(Header));
                o[2] = (ptr._subBlockOffset + ptr._subBlockSize + size_t(base) + sizeof(Header));
                *(unsigned*)(&o[3]) = 1;
            } else if (ptr._specialBuffer == NascentBlockSerializer::SpecialBuffer::UniquePtr) {
                size_t* o = (size_t*)PtrAdd(block, sizeof(Header)+ptr._pointerOffset);
                o[0] = (ptr._subBlockOffset + size_t(base) + sizeof(Header));
                *(unsigned*)(&o[1]) = 1;
            }
        }
    }

    #if !defined(DEBUG_NEW)
        #define new DEBUG_NEW
    #endif

    const void*       Block_GetFirstObject(const void* blockStart)
    {
        return PtrAdd(blockStart, sizeof(Header));
    }

    size_t          Block_GetSize(const void* block)
    {
        const Header& h = *(const Header*)block;
        return h._rawMemorySize + h._internalPointerCount * sizeof(NascentBlockSerializer::InternalPointer) + sizeof(Header);
    }

    std::unique_ptr<uint8[]>  Block_Duplicate(const void* block)
    {
        size_t size = Block_GetSize(block);
        std::unique_ptr<uint8[]> result = std::make_unique<uint8[]>(size);
        XlCopyMemory(result.get(), block, size);
        return std::move(result);
    }



}

