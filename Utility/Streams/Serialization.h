// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Core/Exceptions.h"
#include <memory>
#include <vector>

namespace Utility
{
        ////////////////////////////////////////////////////

    #pragma warning(push)
    #pragma warning(disable:4702)

    /// <summary>And STL allocator that will suppress deallocation when memory comes from a part of a larger heap block<summary>
    /// When we serialize an object in via the block serializer, we can loading it into a single heap block.
    /// However, objects loaded new can have STL containers and objects (like vectors and strings). In this case,
    /// the memory used by the container is a just a part of the larger heap block. When the container is
    /// destroyed, it will attempt to free it's memory. In the of a normal container, the memory is it's own
    /// unique heap block, and the normal deallocation functions can be used. But for our serialized containers,
    /// the memory is not a unique heap block. It is just a internal part of much larger block. In this case, we
    /// must suppress the deallocation.
    /// The BlockSerializerAllocator does exactly that. For vectors and containers that have been serialized in,
    /// we suppress the deallocation step.
    template<typename Type>
        class BlockSerializerAllocator : public std::allocator<Type>
    {
    public:
        pointer allocate(size_type n, std::allocator<void>::const_pointer ptr= 0)
        {
            if (_fromFixedStorage) {
                ThrowException(std::invalid_argument("Cannot allocate from a BlockSerializerAllocator than has been serialized in from a fixed block"));
                return nullptr;
            }
            return std::allocator<Type>::allocate(n, ptr);
        }

        void deallocate(pointer p, size_type n)
        {
            if (!_fromFixedStorage) {
                std::allocator<Type>::deallocate(p, n);
            }
        }

        template<class _Other>
		    struct rebind
		    {
		        typedef BlockSerializerAllocator<_Other> other;
		    };

        BlockSerializerAllocator()                                                  : _fromFixedStorage(0) {}
        explicit BlockSerializerAllocator(unsigned fromFixedStorage)                : _fromFixedStorage(fromFixedStorage) {}
        BlockSerializerAllocator(const std::allocator<Type>& copyFrom)              : _fromFixedStorage(0) {}
        BlockSerializerAllocator(std::allocator<Type>&& moveFrom)                   : _fromFixedStorage(0) {}
        BlockSerializerAllocator(const BlockSerializerAllocator<Type>& copyFrom)    : _fromFixedStorage(0) {}
        BlockSerializerAllocator(BlockSerializerAllocator<Type>&& moveFrom)         : _fromFixedStorage(moveFrom._fromFixedStorage) {}
    private:
        unsigned    _fromFixedStorage;
    };

    #pragma warning(pop)

        ////////////////////////////////////////////////////

    template<typename Type>
        class BlockSerializerDeleter : public std::default_delete<Type>
    {
    public:
        void operator()(Type *_Ptr) const
        {
            if (!_fromFixedStorage) {
                std::default_delete<Type>::operator()(_Ptr);
            }
        }

        BlockSerializerDeleter()                                                : _fromFixedStorage(0) {}
        explicit BlockSerializerDeleter(unsigned fromFixedStorage)              : _fromFixedStorage(fromFixedStorage) {}
        BlockSerializerDeleter(const std::default_delete<Type>& copyFrom)       : _fromFixedStorage(0) {}
        BlockSerializerDeleter(std::default_delete<Type>&& moveFrom)            : _fromFixedStorage(0) {}
        BlockSerializerDeleter(const BlockSerializerDeleter<Type>& copyFrom)    : _fromFixedStorage(copyFrom._fromFixedStorage) {}
        BlockSerializerDeleter(BlockSerializerDeleter<Type>&& moveFrom)         : _fromFixedStorage(moveFrom._fromFixedStorage) {}
    private:
        unsigned    _fromFixedStorage;
    };

        ////////////////////////////////////////////////////

    template<typename Type>
        class BlockSerializerDeleter<Type[]> : public std::default_delete<Type[]>
    {
    public:
        void operator()(Type *_Ptr) const
        {
            if (!_fromFixedStorage) {
                std::default_delete<Type[]>::operator()(_Ptr);
            }
        }

        BlockSerializerDeleter()                                                : _fromFixedStorage(0) {}
        explicit BlockSerializerDeleter(unsigned fromFixedStorage)              : _fromFixedStorage(fromFixedStorage) {}
        BlockSerializerDeleter(const std::default_delete<Type[]>& copyFrom)     : _fromFixedStorage(0) {}
        BlockSerializerDeleter(std::default_delete<Type[]>&& moveFrom)          : _fromFixedStorage(0) {}
        BlockSerializerDeleter(const BlockSerializerDeleter<Type[]>& copyFrom)  : _fromFixedStorage(copyFrom._fromFixedStorage) {}
        BlockSerializerDeleter(BlockSerializerDeleter<Type[]>&& moveFrom)       : _fromFixedStorage(moveFrom._fromFixedStorage) {}
    private:
        unsigned    _fromFixedStorage;
    };

        ////////////////////////////////////////////////////

    template<typename Type>
        using SerializableVector = std::vector<Type, BlockSerializerAllocator<Type>>;
}

template<typename Serializer, typename Object>
    void Serialize(Serializer& serializer, const Object& obj);

using namespace Utility;

