// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RenderUtils.h"
#include "Resource.h"
#include "Metal/Shader.h"

#include "../../ConsoleRig/GlobalServices.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/MemoryUtils.h"


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
            _allocation = nullptr;      // getting a wierd case were the destructor for one object was called twice...? Unclear why
        }
    }

    SharedPkt MakeSharedPktSize(size_t size)
    {
        auto& heap = SharedPkt::GetHeap();
        return SharedPkt(heap.Allocate((unsigned)size), size);
    }

    SharedPkt MakeSharedPkt(const void* begin, const void* end)
    {
        auto& heap = SharedPkt::GetHeap();
        auto size = size_t(ptrdiff_t(end) - ptrdiff_t(begin));
        SharedPkt pkt(heap.Allocate((unsigned)size), size);
        if (pkt.begin()) {
            XlCopyMemory(pkt.begin(), begin, size);
        }
        return std::move(pkt);
    }

    MiniHeap& SharedPkt::GetHeap()
    {
        static MiniHeap* MainHeap = nullptr;
        if (!MainHeap) {
                // initialize our global from the global services
                // this will ensure that the same object will be used across multiple DLLs
            static auto Fn_GetStorage = ConstHash64<'gets', 'hare', 'dpkt', 'heap'>::Value;
            auto& services = ConsoleRig::GlobalServices::GetCrossModule()._services;
            if (!services.Has<MiniHeap*()>(Fn_GetStorage)) {
                auto newMiniHeap = std::make_shared<MiniHeap>();
                services.Add(Fn_GetStorage,
                    [newMiniHeap]() { return newMiniHeap.get(); });
                MainHeap = newMiniHeap.get();
            } else {
                MainHeap = services.Call<MiniHeap*>(Fn_GetStorage);
            }
        }

        return *MainHeap;
    }

}


