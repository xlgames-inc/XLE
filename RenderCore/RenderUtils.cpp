// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RenderUtils.h"
#include "Resource.h"
#include "Metal/Shader.h"

#include "../../Utility/StringFormat.h"


namespace RenderCore
{
    namespace Exceptions
    {
        GenericFailure::GenericFailure(const char what[]) : ::Exceptions::BasicLabel(what) {}

        AllocationFailure::AllocationFailure(const char what[]) 
        : GenericFailure(what) 
        {}
    }

    SharedPkt::SharedPkt(MiniHeap::Allocation alloc, size_t size)
    : Allocation(alloc), _size(size)
    {
            // Careful --   first initialization never addrefs!
            //              this is because allocations will return an 
            //              object with reference count of 1
    }

    SharedPkt::SharedPkt(const SharedPkt& cloneFrom)
    : Allocation(cloneFrom), _size(cloneFrom._size)
    {
        GetHeap().AddRef(*this);
    }

    SharedPkt::~SharedPkt()
    {
        if (_allocation != nullptr) {
            GetHeap().Release(*this);
        }
    }

    SharedPkt MakeSharedPkt(size_t size)
    {
        auto& heap = SharedPkt::GetHeap();
        return SharedPkt(heap.Allocate(size), size);
    }

    SharedPkt MakeSharedPkt(const void* begin, const void* end)
    {
        auto& heap = SharedPkt::GetHeap();
        auto size = size_t(ptrdiff_t(end) - ptrdiff_t(begin));
        SharedPkt pkt(heap.Allocate(size), size);
        if (pkt.begin()) {
            XlCopyMemory(pkt.begin(), begin, size);
        }
        return std::move(pkt);
    }

    MiniHeap& SharedPkt::GetHeap()
    {
            // \todo -- this has to be done in a DLL safe way.
            //          There should be a single heap like this
            //          that is used by the whole system
        static MiniHeap MainHeap;
        return MainHeap;
    }

}


