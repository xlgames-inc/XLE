#pragma once

#include "../Math/Vector.h"
#include "../Utility/HeapUtils.h"
#include <utility>

namespace RenderOverlays
{
	class RectanglePacker_FontCharArray
	{
	public:
		using Rectangle = std::pair<UInt2, UInt2>;

		Rectangle Allocate(UInt2 dims);
		void Deallocate(const Rectangle& rect);

		RectanglePacker_FontCharArray(const UInt2& dims);
		~RectanglePacker_FontCharArray();
	private:
		class SlotArray;
		SimpleSpanningHeap _heapOfSlotArrays;
		std::vector<SlotArray> _slotArrays;
		unsigned _width;
	};
}