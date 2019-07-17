// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "OverlayPrimitives.h"
#include "../Core/Types.h"
#include <assert.h>
#include <algorithm>

namespace RenderOverlays
{

	inline float FloatBits(uint32 i) { return *(float*)&i; }
	static const float FP_INFINITY = FloatBits(0x7F800000);
	static const float FP_NEG_INFINITY = FloatBits(0xFF800000);

	const ColorB ColorB::White(0xff, 0xff, 0xff, 0xff);
    const ColorB ColorB::Black(0x0, 0x0, 0x0, 0xff);
    const ColorB ColorB::Red(0xff, 0x0, 0x0, 0xff);
    const ColorB ColorB::Green(0x0, 0xff, 0x0, 0xff);
    const ColorB ColorB::Blue(0x0, 0x0, 0xff, 0xff);
    const ColorB ColorB::Zero(0x0, 0x0, 0x0, 0x0);

	// --------------------------------------------------------------------------
	// Quad
	// --------------------------------------------------------------------------

	Quad Quad::Empty()
	{
		Quad q;
		q.min[0] = FP_INFINITY;
		q.min[1] = FP_INFINITY;
		q.max[0] = FP_NEG_INFINITY;
		q.max[1] = FP_NEG_INFINITY;
		return q;
	}

	Quad Quad::MinMax(float minX, float minY, float maxX, float maxY)
	{
		Quad q;
		q.min[0] = minX;
		q.min[1] = minY;
		q.max[0] = maxX;
		q.max[1] = maxY;
		return q;
	}


	Quad Quad::MinMax(const Float2& min, const Float2& max)
	{
		Quad q;
		q.min = min;
		q.max = max;
		return q;
	}

	Quad Quad::CenterExtent(const Float2& center, const Float2& extent)
	{
		Quad q;
		q.min = center - extent;
		q.max = center + extent;
		return q;
	}

	bool Quad::operator == (const Quad& v) const
	{
		return min == v.min && max == v.max;
	}

	bool Quad::operator != (const Quad& v) const
	{
		return min != v.min || max != v.max;
	}

	Float2 Quad::Center() const
	{
		return Float2(0.5f * (min[0] + max[0]), 0.5f * (min[1] + max[1]));
	}

	Float2 Quad::Extent() const
	{
		return max - Center();
	}

}


