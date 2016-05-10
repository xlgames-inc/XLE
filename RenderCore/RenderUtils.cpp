// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RenderUtils.h"
#include "ResourceDesc.h"
#include "Types.h"
#include "Format.h"

#include "../../ConsoleRig/GlobalServices.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/MemoryUtils.h"


namespace RenderCore
{
    const TextureViewWindow::SubResourceRange TextureViewWindow::All = SubResourceRange{0, Unlimited};

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

	ResourceDesc::ResourceDesc()
	{
		_type = Type::Unknown;
		_bindFlags = _cpuAccess = _gpuAccess = _allocationRules = 0;
		_name[0] = '\0';
		XlZeroMemory(_textureDesc);
	}


	namespace GlobalInputLayouts
    {
        namespace Detail
        {
            static const unsigned AppendAlignedElement = ~unsigned(0x0);
            InputElementDesc P2CT_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, Format::R32G32_FLOAT   ),
                InputElementDesc( "COLOR",      0, Format::R8G8B8A8_UNORM ),
                InputElementDesc( "TEXCOORD",   0, Format::R32G32_FLOAT   )
            };

            InputElementDesc P2C_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, Format::R32G32_FLOAT   ),
                InputElementDesc( "COLOR",      0, Format::R8G8B8A8_UNORM )
            };

            InputElementDesc PCT_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, Format::R32G32B32_FLOAT),
                InputElementDesc( "COLOR",      0, Format::R8G8B8A8_UNORM ),
                InputElementDesc( "TEXCOORD",   0, Format::R32G32_FLOAT   )
            };

            InputElementDesc P_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, Format::R32G32B32_FLOAT)
            };

            InputElementDesc PC_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, Format::R32G32B32_FLOAT),
                InputElementDesc( "COLOR",      0, Format::R8G8B8A8_UNORM )
            };

            InputElementDesc PT_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, Format::R32G32B32_FLOAT),
                InputElementDesc( "TEXCOORD",   0, Format::R32G32_FLOAT   )
            };

            InputElementDesc PN_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, Format::R32G32B32_FLOAT),
                InputElementDesc( "NORMAL",   0, Format::R32G32B32_FLOAT )
            };

            InputElementDesc PNT_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, Format::R32G32B32_FLOAT),
                InputElementDesc( "NORMAL",   0, Format::R32G32B32_FLOAT ),
                InputElementDesc( "TEXCOORD",   0, Format::R32G32_FLOAT )
            };

            InputElementDesc PNTT_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, Format::R32G32B32_FLOAT),
                InputElementDesc( "NORMAL",   0, Format::R32G32B32_FLOAT ),
                InputElementDesc( "TEXCOORD",   0, Format::R32G32_FLOAT ),
                InputElementDesc( "TEXTANGENT",   0, Format::R32G32B32_FLOAT ),
                InputElementDesc( "TEXBITANGENT",   0, Format::R32G32B32_FLOAT )
            };
        }

        InputLayout P2CT = std::make_pair(Detail::P2CT_Elements, dimof(Detail::P2CT_Elements));
        InputLayout P2C = std::make_pair(Detail::P2C_Elements, dimof(Detail::P2C_Elements));
        InputLayout PCT = std::make_pair(Detail::PCT_Elements, dimof(Detail::PCT_Elements));
        InputLayout P = std::make_pair(Detail::P_Elements, dimof(Detail::P_Elements));
        InputLayout PC = std::make_pair(Detail::PC_Elements, dimof(Detail::PC_Elements));
        InputLayout PT = std::make_pair(Detail::PT_Elements, dimof(Detail::PT_Elements));
        InputLayout PN = std::make_pair(Detail::PN_Elements, dimof(Detail::PN_Elements));
        InputLayout PNT = std::make_pair(Detail::PNT_Elements, dimof(Detail::PNT_Elements));
        InputLayout PNTT = std::make_pair(Detail::PNTT_Elements, dimof(Detail::PNTT_Elements));
    }

    unsigned CalculateVertexStride(
        const InputElementDesc* start, const InputElementDesc* end,
        unsigned slot)
    {
            // note --  Assuming vertex elements are densely packed (which
            //          they usually are).
            //          We could also use the "_alignedByteOffset" member
            //          to find out where the element begins and ends)
        unsigned result = 0;
        for (auto i=start; i<end; ++i) {
            if (i->_inputSlot == slot) {
                assert(i->_alignedByteOffset == (result/8) || i->_alignedByteOffset == ~unsigned(0x0));
                result += BitsPerPixel(i->_nativeFormat);
            }
        }
        return result / 8;
    }

    unsigned HasElement(const InputElementDesc* begin, const InputElementDesc* end, const char elementSemantic[])
    {
        unsigned result = 0;
        for (auto i = begin; i != end; ++i) {
            if (!XlCompareStringI(i->_semanticName.c_str(), elementSemantic)) {
                assert((result & (1 << i->_semanticIndex)) == 0);
                result |= (1 << i->_semanticIndex);
            }
        }
        return result;
    }

    unsigned FindElement(const InputElementDesc* begin, const InputElementDesc* end, const char elementSemantic[], unsigned semanticIndex)
    {
        for (auto i = begin; i != end; ++i)
            if (i->_semanticIndex == semanticIndex && !XlCompareStringI(i->_semanticName.c_str(), elementSemantic))
                return unsigned(i - begin);
        return ~0u;
    }

}


