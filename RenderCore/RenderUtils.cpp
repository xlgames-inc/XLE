// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RenderUtils.h"
#include "ResourceDesc.h"
#include "Types.h"
#include "Format.h"

#include "IDevice.h"
#include "IThreadContext.h"
#include "IAnnotator.h"

#include "../ConsoleRig/GlobalServices.h"
#include "../ConsoleRig/Log.h"
#include "../ConsoleRig/AttachablePtr.h"
#include "../Utility/StringFormat.h"
#include "../Utility/MemoryUtils.h"


namespace RenderCore
{
    const TextureViewDesc::SubResourceRange TextureViewDesc::All = SubResourceRange{0, Unlimited};

    namespace Exceptions
    {
        GenericFailure::GenericFailure(const char what[]) : ::Exceptions::BasicLabel(what) {}

        AllocationFailure::AllocationFailure(const char what[]) 
        : GenericFailure(what) 
        {}
    }
    
    class SubFrameHeap
    {
    public:
        void OnFrameBarrier()
        {
            _writeMarker = _data.data();
            ++_resetId;
            _logMsg = true;
        }
        
        unsigned GetResetId() const { return _resetId; }
        
        void* Allocate(size_t size)
        {
            if (PtrAdd(_writeMarker, size) > AsPointer(_data.end())) {
                if (_logMsg) {
                    Log(Warning) << "Overran subframe heap with allocation of size (" << size << ")" << std::endl;
                    _logMsg = false;
                }
                return nullptr;
            }
            
            void* result = _writeMarker;
            _writeMarker += size;
            return result;
        }
        
        SubFrameHeap()
        : _data(256*1024, 0)
        {
            _writeMarker = _data.data();
            _resetId = 0;
            _logMsg = true;
        }
        
        ~SubFrameHeap() {}
    private:
        std::vector<uint8_t> _data;
        uint8_t* _writeMarker;
        unsigned _resetId;
        bool _logMsg;
    };
    
    static SubFrameHeap& GetSubFrameHeap()
    {
        static SubFrameHeap s_instance;
        return s_instance;
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
        if (_allocation != nullptr && _marker != ~0u) {
            GetHeap().AddRef(*this);
        }
    }

    SharedPkt::~SharedPkt()
    {
        if (_marker == ~0u) {
            // subframe allocation
        } else if (_allocation != nullptr) {
            GetHeap().Release(*this);
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
        return pkt;
    }
    
    SharedPkt MakeSubFramePktSize(size_t size)
    {
        auto* allocation = GetSubFrameHeap().Allocate(size);
        if (!allocation)
            return MakeSharedPktSize(size);   // fall back to (slower) shared pkt
        return SharedPkt({allocation, ~0u}, size);
    }
    
    SharedPkt MakeSubFramePkt(const void* begin, const void* end)
    {
        auto size = size_t(ptrdiff_t(end) - ptrdiff_t(begin));
        auto* allocation = GetSubFrameHeap().Allocate(size);
        if (!allocation)
            return MakeSharedPkt(begin, end);   // fall back to (slower) shared pkt
        SharedPkt pkt({allocation, ~0u}, size);
        if (pkt.begin()) {
            XlCopyMemory(pkt.begin(), begin, size);
        }
        return pkt;
    }
    
    void ResetSubFrameHeap()
    {
        GetSubFrameHeap().OnFrameBarrier();
    }

    MiniHeap& SharedPkt::GetHeap()
    {
        static MiniHeap* MainHeap = nullptr;
        if (!MainHeap) {
                // initialize our global from the global services
                // this will ensure that the same object will be used across multiple DLLs
            static auto Fn_GetStorage = ConstHash64<'gets', 'hare', 'dpkt', 'heap'>::Value;
            auto& services = ConsoleRig::CrossModule::GetInstance()._services;
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

        InputLayout P2CT = MakeIteratorRange(Detail::P2CT_Elements);
        InputLayout P2C = MakeIteratorRange(Detail::P2C_Elements);
        InputLayout PCT = MakeIteratorRange(Detail::PCT_Elements);
        InputLayout P = MakeIteratorRange(Detail::P_Elements);
        InputLayout PC = MakeIteratorRange(Detail::PC_Elements);
        InputLayout PT = MakeIteratorRange(Detail::PT_Elements);
        InputLayout PN = MakeIteratorRange(Detail::PN_Elements);
        InputLayout PNT = MakeIteratorRange(Detail::PNT_Elements);
        InputLayout PNTT = MakeIteratorRange(Detail::PNTT_Elements);
    }

    unsigned CalculateVertexStrideForSlot(IteratorRange<const InputElementDesc*> range, unsigned slot)
    {
            // note --  Assuming vertex elements are densely packed (which
            //          they usually are).
            //          We could also use the "_alignedByteOffset" member
            //          to find out where the element begins and ends)
        unsigned result = 0;
        for (auto i=range.begin(); i<range.end(); ++i) {
            if (i->_inputSlot == slot) {
                assert(i->_alignedByteOffset == (result/8) || i->_alignedByteOffset == ~unsigned(0x0));
                result += BitsPerPixel(i->_nativeFormat);
            }
        }
        return result / 8;
    }

	std::vector<unsigned> CalculateVertexStrides(IteratorRange<const InputElementDesc*> layout)
	{
		std::vector<unsigned> result;
		for (auto& a:layout) {
			if (result.size() <= a._inputSlot)
				result.resize(a._inputSlot + 1, 0);
			unsigned& stride = result[a._inputSlot];
			auto bytes = BitsPerPixel(a._nativeFormat) / 8;
			if (a._alignedByteOffset == ~0u) {
				stride = stride + bytes;
			} else {
				stride = std::max(stride, a._alignedByteOffset + bytes);
			}
		}
		return result;
	}

    unsigned HasElement(IteratorRange<const InputElementDesc*> range, const char elementSemantic[])
    {
        unsigned result = 0;
        for (auto i = range.begin(); i != range.end(); ++i) {
            if (!XlCompareStringI(i->_semanticName.c_str(), elementSemantic)) {
                assert((result & (1 << i->_semanticIndex)) == 0);
                result |= (1 << i->_semanticIndex);
            }
        }
        return result;
    }

    unsigned FindElement(IteratorRange<const InputElementDesc*> range, const char elementSemantic[], unsigned semanticIndex)
    {
        for (auto i = range.begin(); i != range.end(); ++i)
            if (i->_semanticIndex == semanticIndex && !XlCompareStringI(i->_semanticName.c_str(), elementSemantic))
                return unsigned(i - range.begin());
        return ~0u;
    }

	bool HasElement(IteratorRange<const MiniInputElementDesc*> elements, uint64 semanticHash)
	{
		for (const auto&e:elements)
			if (e._semanticHash == semanticHash)
				return true;
		return false;
	}

	unsigned CalculateVertexStride(IteratorRange<const MiniInputElementDesc*> elements, bool enforceAlignment)
	{
        // note -- following alignment rules suggested by Apple in OpenGL ES guide
        //          each element should be aligned to a multiple of 4 bytes (or a multiple of
        //          it's component size, whichever is larger).
        //
        if (elements.empty()) return 0;
		unsigned result = 0;
        unsigned largestComponentPrecision = 0;
        unsigned basicAlignment = 32;   // (ie, 4 bytes in # of bits)
        for (auto i=elements.begin(); i<elements.end(); ++i) {
            auto componentPrecision = GetComponentPrecision(i->_nativeFormat);
            assert((result % componentPrecision) == 0);
            largestComponentPrecision = std::max(largestComponentPrecision, componentPrecision);
            auto size = BitsPerPixel(i->_nativeFormat);
            result += size;
            if ((size % basicAlignment) != 0)
                result += basicAlignment - (size % basicAlignment);   // add padding required by basic alignment restriction
        }
        if (enforceAlignment) {
            assert(!largestComponentPrecision || (result % largestComponentPrecision) == 0);      // ensure second and subsequent vertices will be aligned
        }
        return result / 8;
	}

    unsigned CalculatePrimitiveCount(Topology topology, unsigned vertexCount, unsigned drawCallCount)
    {
        switch (topology) {
		case Topology::TriangleList:
			return vertexCount / 3;
		case Topology::TriangleStrip:
			return vertexCount - 2 * drawCallCount;
		case Topology::LineList:
			return vertexCount / 2;
		case Topology::LineStrip:
			return vertexCount - 1 * drawCallCount;
		case Topology::PointList:
			return vertexCount;
		default:
			return 0;
	    }
    }

    IDevice::~IDevice() {}
    IThreadContext::~IThreadContext() {}
    IPresentationChain::~IPresentationChain() {}
    IResource::~IResource() {}
    IAnnotator::~IAnnotator() {}

}


