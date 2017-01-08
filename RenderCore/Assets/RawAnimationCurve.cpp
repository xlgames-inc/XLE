// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "RawAnimationCurve.h"
#include "../Format.h"
#include "../../Math/Matrix.h"
#include "../../Math/Interpolation.h"
#include "../../Core/Exceptions.h"

namespace RenderCore { namespace Assets
{
    static float LerpParameter(float A, float B, float input) { return (input - A) / (B-A); }

	template<typename OutType>
		class CurveElementDecompressor
		{
		public:
			using T = const OutType&;
			const OutType& operator()(const void* data) { return *(const OutType*)data; }
			CurveElementDecompressor(Format fmt);
		};

	CurveElementDecompressor<float>::CurveElementDecompressor(Format fmt)
	{
		assert(fmt == Format::R32_FLOAT
			|| fmt == Format::R32G32_FLOAT
			|| fmt == Format::R32G32B32_FLOAT
			|| fmt == Format::R32G32B32A32_FLOAT);
	}

	CurveElementDecompressor<Float3>::CurveElementDecompressor(Format fmt)
	{
		assert(fmt == Format::R32G32B32_FLOAT
			|| fmt == Format::R32G32B32A32_FLOAT);
	}

	CurveElementDecompressor<Float4x4>::CurveElementDecompressor(Format fmt)
	{
		assert(fmt == Format::Matrix4x4);
	}
		
	template<>
		class CurveElementDecompressor<Float4>
		{
		public:
			using T = const Float4;
			Float4 operator()(const void* data) 
			{ 
				if (_fmt == Format::R10G10B10A10_SNORM) {
					// Decompress 5 byte quaternion format
					// This is 4 10-bit signed values, in x,y,z,w form
					struct Q {
						int x : 10;
						int y : 10;
						int z : 10;
						int w : 10;
					};
					const auto& q = *(const Q*)data;
					// note --	min value should be -0x200, but max positive value is 0x1ff
					//			so, this calculation will never actually return +1.0f
					return Float4(q.x/float(0x200), q.y/float(0x200), q.z/float(0x200), q.w/float(200));
				} else {
					return *(const Float4*)data;
				}
			}

			CurveElementDecompressor(Format fmt) : _fmt(fmt)
			{
				assert(fmt == Format::R10G10B10A10_SNORM || fmt == Format::R32G32B32A32_FLOAT);
			}
		private:
			Format _fmt;
		};

    template<typename OutType>
        OutType        RawAnimationCurve::Calculate(float inputTime) const never_throws
    {
		CurveElementDecompressor<OutType> decomp(_positionFormat);
		using T = decltype(decomp)::T;

		// reminder -- lower_bound returns a pointer to the first key that is not smaller than inputTime (eg, equal or larger)
		auto* key = std::lower_bound(_timeMarkers.get(), &_timeMarkers[_keyCount], inputTime);

			// note -- clamping at start and end positions of the curve
		if (key == _timeMarkers.get())
			return decomp(_parameterData.get());

		--key;	// (back one, to the first key that is smaller)
		auto keyIndex = key-_timeMarkers.get();
		float alpha = LerpParameter(key[0], key[1], inputTime);

        if (_interpolationType == Linear) {

			// (need at least one key greater than the interpolation point, to perform interpolation correctly)
			if ((key+1) >= &_timeMarkers[_keyCount])
				return decomp(PtrAdd(_parameterData.get(), (_keyCount-1)*_elementStride));

            assert(key[1] >= key[0]);		// (validating sorting assumption)
            
            T P0 = decomp(PtrAdd(_parameterData.get(), keyIndex * _elementStride));
            T P1 = decomp(PtrAdd(_parameterData.get(), (keyIndex+1) * _elementStride));
            return SphericalInterpolate(P0, P1, alpha);

        } else if (_interpolationType == Bezier) {

            assert(_inTangentFormat != Format::Unknown);
            assert(_outTangentFormat != Format::Unknown);

			// (need at least one key greater than the interpolation point, to perform interpolation correctly)
			if ((key+1) >= &_timeMarkers[_keyCount])
				return decomp(PtrAdd(_parameterData.get(), (_keyCount-1) * _elementStride));

			assert(key[1] >= key[0]);		// (validating sorting assumption)
            const size_t inTangentOffset = BitsPerPixel(_positionFormat)/8;
            const size_t outTangentOffset = inTangentOffset + BitsPerPixel(_inTangentFormat)/8;

            T P0 = decomp(PtrAdd(_parameterData.get(), keyIndex * _elementStride));
            T P1 = decomp(PtrAdd(_parameterData.get(), (keyIndex+1) * _elementStride));

			// This is a convention of the Collada format
			// (see Collada spec 1.4.1, page 4-4)
			//		the first control point is stored under the semantic "OUT_TANGENT" for P0
			//		and second control point is stored under the semantic "IN_TANGENT" for P1
            T C0 = decomp(PtrAdd(_parameterData.get(), keyIndex * _elementStride + outTangentOffset));
            T C1 = decomp(PtrAdd(_parameterData.get(), (keyIndex+1) * _elementStride + inTangentOffset));

            return SphericalBezierInterpolate(P0, C0, C1, P1, alpha);

		} else if (_interpolationType == CatmullRom) {

			// (need at least one key greater than the interpolation point, to perform interpolation correctly)
			if ((key+2) >= &_timeMarkers[_keyCount])
				return decomp(PtrAdd(_parameterData.get(), (_keyCount-1) * _elementStride));

			T P0 = decomp(PtrAdd(_parameterData.get(), keyIndex * _elementStride));
            T P1 = decomp(PtrAdd(_parameterData.get(), (keyIndex+1) * _elementStride));
			// (note the clamp here that can result in P0 == P0n1 at the start of the curve)
			T P0n1 = decomp(PtrAdd(_parameterData.get(), std::max(0, signed(keyIndex)-1) * _elementStride));
			T P1p1 = decomp(PtrAdd(_parameterData.get(), (keyIndex+2) * _elementStride));

			float P0n1T = _timeMarkers[std::max(0, signed(keyIndex)-1)];
			float P1p1T = _timeMarkers[keyIndex+2];

			return SphericalCatmullRomInterpolate(
				P0n1, P0, P1, P1p1, 
				(P0n1T - key[0]) / (key[1] - key[0]), (P1p1T - key[0]) / (key[1] - key[0]),
				alpha);

        } else if (_interpolationType == Hermite) {
			// hermite version not implemented
			//  -- but it's similar to both the Bezier and Catmull Rom implementations, nad
			//		could be easily hooked up
			assert(0);      
		}

        return decomp(_parameterData.get());
    }

    float       RawAnimationCurve::StartTime() const
    {
        if (!_keyCount) {
            return FLT_MAX;
        }
        return _timeMarkers[0];
    }

    float       RawAnimationCurve::EndTime() const
    {
        if (!_keyCount) {
            return -FLT_MAX;
        }
        return _timeMarkers[_keyCount-1];
    }

    template float      RawAnimationCurve::Calculate(float inputTime) const never_throws;
    template Float3     RawAnimationCurve::Calculate(float inputTime) const never_throws;
    template Float4     RawAnimationCurve::Calculate(float inputTime) const never_throws;
    template Float4x4   RawAnimationCurve::Calculate(float inputTime) const never_throws;

    RawAnimationCurve::RawAnimationCurve(   size_t keyCount, 
                                            std::unique_ptr<float[], BlockSerializerDeleter<float[]>>&&  timeMarkers, 
                                            DynamicArray<uint8, BlockSerializerDeleter<uint8[]>>&&       keyPositions,
                                            size_t elementSize, InterpolationType interpolationType,
                                            Format positionFormat, Format inTangentFormat,
                                            Format outTangentFormat)
    :       _keyCount(keyCount)
    ,       _timeMarkers(std::forward<std::unique_ptr<float[], BlockSerializerDeleter<float[]>>>(timeMarkers))
    ,       _parameterData(std::forward<DynamicArray<uint8, BlockSerializerDeleter<uint8[]>>>(keyPositions))
    ,       _elementStride(elementSize)
    ,       _interpolationType(interpolationType)
    ,       _positionFormat(positionFormat)
    ,       _inTangentFormat(inTangentFormat)
    ,       _outTangentFormat(outTangentFormat)
    {}

    RawAnimationCurve::RawAnimationCurve(RawAnimationCurve&& curve)
    :       _keyCount(curve._keyCount)
    ,       _timeMarkers(std::move(curve._timeMarkers))
    ,       _parameterData(std::move(curve._parameterData))
    ,       _elementStride(curve._elementStride)
    ,       _interpolationType(curve._interpolationType)
    ,       _positionFormat(curve._positionFormat)
    ,       _inTangentFormat(curve._inTangentFormat)
    ,       _outTangentFormat(curve._outTangentFormat)
    {}

    RawAnimationCurve::RawAnimationCurve(const RawAnimationCurve& copyFrom)
    :       _keyCount(copyFrom._keyCount)
    ,       _parameterData(DynamicArray<uint8, BlockSerializerDeleter<uint8[]>>::Copy(copyFrom._parameterData))
    ,       _elementStride(copyFrom._elementStride)
    ,       _interpolationType(copyFrom._interpolationType)
    ,       _positionFormat(copyFrom._positionFormat)
    ,       _inTangentFormat(copyFrom._inTangentFormat)
    ,       _outTangentFormat(copyFrom._outTangentFormat)
    {
        _timeMarkers.reset(new float[_keyCount]);
        std::copy(copyFrom._timeMarkers.get(), &copyFrom._timeMarkers[_keyCount], _timeMarkers.get());
    }

    RawAnimationCurve& RawAnimationCurve::operator=(RawAnimationCurve&& curve)
    {
        _keyCount = curve._keyCount;
        _timeMarkers = std::move(curve._timeMarkers);
        _parameterData = std::move(curve._parameterData);
        _elementStride = curve._elementStride;
        _interpolationType = curve._interpolationType;
        _positionFormat = curve._positionFormat;
        _inTangentFormat = curve._inTangentFormat;
        _outTangentFormat = curve._outTangentFormat;
        return *this;
    }

}}

