// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Format.h"
#include "../../Foreign/half-1.9.2/include/half.hpp"
#include "../Utility/IteratorUtils.h"
#include <utility>
#include <assert.h>

namespace RenderCore
{
	class VertexElementIterator
	{
	public:
		class ConstValue
		{
		public:
			template<typename Type> const Type& As(); 
			IteratorRange<void*> _data;
			RenderCore::Format _format = RenderCore::Format(0);
		};

		class Value : public ConstValue
		{
		public:
			template<typename Type> void operator=(const Type& input);
			Value() {}
			Value(IteratorRange<void*> data, RenderCore::Format format) : ConstValue{data, format} {}
		};

		bool operator==(const VertexElementIterator&);
		bool operator!=(const VertexElementIterator&);
		void operator++();
		friend size_t operator-(const VertexElementIterator& lhs, const VertexElementIterator& rhs);

		ConstValue operator*() const;
		Value operator*();
		ConstValue operator->() const;
		Value operator->();

		RenderCore::Format Format() const { return _format; }

		IteratorRange<void*> _data;
		size_t _stride = 0;
		RenderCore::Format _format = RenderCore::Format(0);

		VertexElementIterator() {}
		VertexElementIterator(std::nullptr_t) {}
		VertexElementIterator(IteratorRange<void*> data, size_t stride, RenderCore::Format fmt) : _data(data), _stride(stride), _format(fmt) {}
	};

	inline bool VertexElementIterator::operator==(const VertexElementIterator& other)
	{
		return _data.begin() == other._data.begin();
	}

	inline bool VertexElementIterator::operator!=(const VertexElementIterator& other)
	{
		return _data.begin() != other._data.begin();
	}

	inline void VertexElementIterator::operator++()
	{
		_data.first = PtrAdd(_data.first, _stride);
		assert(_data.begin() <= _data.end());
	}

	inline auto VertexElementIterator::operator*() const -> ConstValue
	{ 
		return ConstValue { 
			IteratorRange<void*>(_data.begin(), PtrAdd(_data.begin(), std::min(_data.size(), _stride))), 
			_format}; 
	}

	inline auto VertexElementIterator::operator*() -> Value
	{ 
		return Value { 
			IteratorRange<void*>(_data.begin(), PtrAdd(_data.begin(), std::min(_data.size(), _stride))), 
			_format}; 
	}

	inline auto VertexElementIterator::operator->() const -> ConstValue
	{ 
		return ConstValue { 
			IteratorRange<void*>(_data.begin(), PtrAdd(_data.begin(), std::min(_data.size(), _stride))), 
			_format}; 
	}

	inline auto VertexElementIterator::operator->() -> Value
	{ 
		return Value { 
			IteratorRange<void*>(_data.begin(), PtrAdd(_data.begin(), std::min(_data.size(), _stride))), 
			_format}; 
	}

	inline size_t operator-(const VertexElementIterator& lhs, const VertexElementIterator& rhs)
	{
		return ((const uint8_t*)lhs._data.begin() - (const uint8_t*)rhs._data.begin()) / lhs._stride;
	}

	template<typename Type> 
		const Type& VertexElementIterator::ConstValue::As()
	{
		assert(_data.size() >= sizeof(Type));
		return *(const Type*)_data.begin();
	}

	template<typename Type> 
		void VertexElementIterator::Value::operator=(const Type& input)
	{
		assert(_data.size() >= sizeof(Type));
		*(Type*)_data.begin() = input;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	inline IteratorRange<VertexElementIterator> MakeVertexIteratorRange(IteratorRange<void*> data, size_t stride, RenderCore::Format fmt)
	{
		return {
			VertexElementIterator{data, stride, fmt},
			VertexElementIterator{{data.end(), data.end()}, stride, fmt}};
	}

	inline IteratorRange<VertexElementIterator> MakeVertexIteratorRangeConst(IteratorRange<const void*> data, size_t stride, RenderCore::Format fmt)
	{
		auto castedData = MakeIteratorRange(const_cast<void*>(data.begin()), const_cast<void*>(data.end()));
		return {
			VertexElementIterator{castedData, stride, fmt},
			VertexElementIterator{{castedData.end(), castedData.end()}, stride, fmt}};
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	enum class VertexUtilComponentType { Float32, Float16, UNorm8, UNorm16, SNorm8, SNorm16 };
    static std::pair<VertexUtilComponentType, unsigned> BreakdownFormat(Format fmt);
    
	inline unsigned short AsFloat16(float input)
    {
        //
        //      Using "half" library
        //          http://sourceforge.net/projects/half/
        //
        //      It doesn't have vectorized conversions,
        //      and it looks like it doesn't support denormalized
        //      or overflowed numbers. But it has lots of rounding
        //      modes!
        //

        auto result = half_float::detail::float2half<std::round_to_nearest>(input);
        // assert(!isinf(half_float::detail::half2float(result)));
        return result;
    }

    inline float AsFloat32(unsigned short input)
    {
        return half_float::detail::half2float(input);
    }

	inline std::pair<VertexUtilComponentType, unsigned> BreakdownFormat(Format fmt)
    {
        if (fmt == Format::Unknown) return std::make_pair(VertexUtilComponentType::Float32, 0);

        auto componentType = VertexUtilComponentType::Float32;
        unsigned componentCount = GetComponentCount(GetComponents(fmt));

        auto type = GetComponentType(fmt);
        unsigned prec = GetComponentPrecision(fmt);

        switch (type) {
        case FormatComponentType::Float:
            assert(prec == 16 || prec == 32);
            componentType = (prec > 16) ? VertexUtilComponentType::Float32 : VertexUtilComponentType::Float16; 
            break;

        case FormatComponentType::UnsignedFloat16:
        case FormatComponentType::SignedFloat16:
            componentType = VertexUtilComponentType::Float16;
            break;

		case FormatComponentType::SNorm:
			componentType = (prec == 16) ? VertexUtilComponentType::SNorm16 : VertexUtilComponentType::SNorm8;
			break;

		case FormatComponentType::UNorm: 
        case FormatComponentType::UNorm_SRGB:
            assert(prec==8 || prec==16);
            componentType = (prec == 16) ? VertexUtilComponentType::UNorm16 : VertexUtilComponentType::UNorm8;
            break;
        default:
            assert(0);
        }

        return std::make_pair(componentType, componentCount);
    }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////

	inline void GetVertDataF32(
        float* dst, 
        const float* src, unsigned srcComponentCount)
    {
            // In Collada, the default for values not set is 0.f (or 1. for components 3 or greater)
        dst[0] = (srcComponentCount > 0) ? src[0] : 0.f;
        dst[1] = (srcComponentCount > 1) ? src[1] : 0.f;
        dst[2] = (srcComponentCount > 2) ? src[2] : 0.f;
        dst[3] = (srcComponentCount > 3) ? src[3] : 1.f;
    }

    inline void GetVertDataF16(
        float* dst, 
        const uint16_t* src, unsigned srcComponentCount)
    {
            // In Collada, the default for values not set is 0.f (or 1. for components 3 or greater)
        dst[0] = (srcComponentCount > 0) ? AsFloat32(src[0]) : 0.f;
        dst[1] = (srcComponentCount > 1) ? AsFloat32(src[1]) : 0.f;
        dst[2] = (srcComponentCount > 2) ? AsFloat32(src[2]) : 0.f;
        dst[3] = (srcComponentCount > 3) ? AsFloat32(src[3]) : 1.f;
    }

	inline float UNorm16AsFloat32(uint16_t value)	{ return value / float(0xffff); }
	inline float SNorm16AsFloat32(int16_t value)	{ return value / float(0x7fff); }

	inline void GetVertDataUNorm16(
		float* dst,
		const uint16_t* src, unsigned srcComponentCount)
	{
		// In Collada, the default for values not set is 0.f (or 1. for components 3 or greater)
		dst[0] = (srcComponentCount > 0) ? UNorm16AsFloat32(src[0]) : 0.f;
		dst[1] = (srcComponentCount > 1) ? UNorm16AsFloat32(src[1]) : 0.f;
		dst[2] = (srcComponentCount > 2) ? UNorm16AsFloat32(src[2]) : 0.f;
		dst[3] = (srcComponentCount > 3) ? UNorm16AsFloat32(src[3]) : 1.f;
	}

	inline void GetVertDataSNorm16(
		float* dst,
		const int16_t* src, unsigned srcComponentCount)
	{
		// In Collada, the default for values not set is 0.f (or 1. for components 3 or greater)
		dst[0] = (srcComponentCount > 0) ? SNorm16AsFloat32(src[0]) : 0.f;
		dst[1] = (srcComponentCount > 1) ? SNorm16AsFloat32(src[1]) : 0.f;
		dst[2] = (srcComponentCount > 2) ? SNorm16AsFloat32(src[2]) : 0.f;
		dst[3] = (srcComponentCount > 3) ? SNorm16AsFloat32(src[3]) : 1.f;
	}

	// static unsigned short AsFloat16_Fast(float input)
    // {
    //         //
    //         //      See stack overflow article:
    //         //          http://stackoverflow.com/questions/3026441/float32-to-float16
    //         //
    //         //      He suggests either using a table lookup or vectorising
    //         //      this code for further optimisation.
    //         //
    //     unsigned int fltInt32 = FloatBits(input);
    // 
    //     unsigned short fltInt16 = (fltInt32 >> 31) << 5;
    // 
    //     unsigned short tmp = (fltInt32 >> 23) & 0xff;
    //     tmp = (tmp - 0x70) & ((unsigned int)((int)(0x70 - tmp) >> 4) >> 27);
    // 
    //     fltInt16 = (fltInt16 | tmp) << 10;
    //     fltInt16 |= (fltInt32 >> 13) & 0x3ff;
    // 
    //     return fltInt16;
    // }

}
