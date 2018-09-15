
#include "FontRectanglePacking.h"
#include "../Utility/HeapUtils.h"
#include <vector>

namespace RenderOverlays
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class RectanglePacker_FontCharArray::SlotArray
	{
	public:
		unsigned			GetY0() const { return _y0; }
		unsigned			GetY1() const { return _y1; }
		unsigned			GetHeight() const { return _y1 - _y0; }

			//if there is not empty slot, return -1
		std::pair<unsigned, unsigned>	Allocate(unsigned width);
		void				Deallocate(unsigned x0, unsigned x1);

		SlotArray(unsigned y0, unsigned y1, unsigned width);
		~SlotArray();

	public:
		unsigned         _y0, _y1;
		SpanningHeap<unsigned> _heap;
	};

	std::pair<unsigned, unsigned> RectanglePacker_FontCharArray::SlotArray::Allocate(unsigned width)
	{
		// Find the empty slot that will best fit a character of this width
		auto x = _heap.Allocate(width);
		if (x == ~0u) return {0,0};
		return {x, x+width};
	}

	void RectanglePacker_FontCharArray::SlotArray::Deallocate(unsigned x0, unsigned x1)
	{
		_heap.Deallocate(x0, x1-x0);
	}

	RectanglePacker_FontCharArray::SlotArray::SlotArray(unsigned y0, unsigned y1, unsigned width) : _heap(width), _y0(y0), _y1(y1) {}
	RectanglePacker_FontCharArray::SlotArray::~SlotArray() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

	static unsigned AlignSlotHeight(unsigned h) { return (h + 3) & ~0x3; }

	auto RectanglePacker_FontCharArray::Allocate(UInt2 dims) -> Rectangle
	{
		assert(dims[0] > 0 && dims[1] > 0);

		// Try to find a matching slot array, and if we find it, just allocate directly from it
		auto slotArrayHeight = AlignSlotHeight(dims[1]);
		for (auto&a:_slotArrays)
			if (a.GetHeight() == slotArrayHeight) {
				auto allocation = a.Allocate(dims[0]);
				if (allocation.first != allocation.second)
					return {{allocation.first, a.GetY0()}, {allocation.second, a.GetY1()}};
			}

		// Try to allocate a heap char array of the correct height
		auto y0 = _heapOfSlotArrays.Allocate(slotArrayHeight);
		if (y0 != ~0u) {
			_slotArrays.emplace_back(SlotArray{y0, y0+slotArrayHeight, _width});
			auto&a = *(_slotArrays.end()-1);
			auto allocation = a.Allocate(dims[0]);
			if (allocation.first != allocation.second)
				return {{allocation.first, a.GetY0()}, {allocation.second, a.GetY1()}};
		}

		return {{0,0}, {0,0}};
	}

	void RectanglePacker_FontCharArray::Deallocate(const Rectangle& rect)
	{
		assert(0);	// not implemented
	}

	RectanglePacker_FontCharArray::RectanglePacker_FontCharArray(const UInt2& dims)
	: _width(dims[0]), _heapOfSlotArrays(dims[1])
	{
	}

	RectanglePacker_FontCharArray::~RectanglePacker_FontCharArray()
	{
	}

}

