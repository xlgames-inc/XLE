// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "RawAnimationCurve.h"
#include "../Format.h"
#include "../../Math/Matrix.h"
#include "../../Math/Quaternion.h"
#include "../../Math/Interpolation.h"
#include "../../Utility/IteratorUtils.h"
#include "../../Core/Exceptions.h"

namespace RenderCore { namespace Assets
{
    static float LerpParameter(float A, float B, float input) { return (input - A) / (B-A); }

	template<typename OutType>
		class CurveElementDecompressor
		{
		public:
			const OutType& operator()(const void* data) const { return *(const OutType*)data; }
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
			Float4 operator()(const void* data) const
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
					return Float4(q.w/float(200), q.x/float(0x200), q.y/float(0x200), q.z/float(0x200));
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

	template<>
		class CurveElementDecompressor<Quaternion>
		{
		public:
			Quaternion operator()(const void* data) const
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
					// When this format is used for unnormalized quaternions (ie, we're expecting a normalize operation at some
					// point, possibly after an interpolation) then it won't matter too much -- because the magnitude is only
					// meaningful in relation to the magnitudes of other quaternions in the same form.
					return Quaternion(q.w/float(0x200), q.x/float(0x200), q.y/float(0x200), q.z/float(200));
				} else {
					return *(const Quaternion*)data;		// (note -- expecting w, x, y, z order here)
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
		class CurveElementDequantDecompressor
		{
		public:
			OutType operator()(const void* data) const;

			CurveElementDequantDecompressor(Format fmt, const CurveDequantizationBlock& dequantBlock)
			: _fmt(fmt), _dequantBlock(&dequantBlock) {}
		private:
			Format _fmt;
			const CurveDequantizationBlock* _dequantBlock;
		};

	template <>
		float CurveElementDequantDecompressor<float>::operator()(const void* data) const {
			assert(_fmt == Format::R16_UNORM);
			float result = _dequantBlock->_mins[0];
			auto* v = (const uint16*)data;
			if (_dequantBlock->_elementFlags & (1<<0)) result = LinearInterpolate(_dequantBlock->_mins[0], _dequantBlock->_maxs[0], float(*v++)/float(0xffff));
			return result;
		}

	template <>
		Float3 CurveElementDequantDecompressor<Float3>::operator()(const void* data) const {
			assert(_fmt == Format::R16_UNORM);
			Float3 result = _dequantBlock->_mins;
			auto* v = (const uint16*)data;
			// Dequantize each element separately
			if (_dequantBlock->_elementFlags & (1<<0)) result[0] = LinearInterpolate(_dequantBlock->_mins[0], _dequantBlock->_maxs[0], float(*v++)/float(0xffff));
			if (_dequantBlock->_elementFlags & (1<<1)) result[1] = LinearInterpolate(_dequantBlock->_mins[1], _dequantBlock->_maxs[1], float(*v++)/float(0xffff));
			if (_dequantBlock->_elementFlags & (1<<2)) result[2] = LinearInterpolate(_dequantBlock->_mins[2], _dequantBlock->_maxs[2], float(*v++)/float(0xffff));
			assert(isfinite(result[0]) && isfinite(result[1]) && isfinite(result[2]));
			assert(!isnan(result[0]) && !isnan(result[1]) && !isnan(result[2]));
			assert(result[0] == result[0] && result[1] == result[1] && result[2] == result[2]);
			return result;
		}

	template <>
		Float4 CurveElementDequantDecompressor<Float4>::operator()(const void* data) const {
			assert(_fmt == Format::R16_UNORM);
			Float4 result = _dequantBlock->_mins;
			auto* v = (const uint16*)data;
			// Dequantize each element separately
			if (_dequantBlock->_elementFlags & (1<<0)) result[0] = LinearInterpolate(_dequantBlock->_mins[0], _dequantBlock->_maxs[0], float(*v++)/float(0xffff));
			if (_dequantBlock->_elementFlags & (1<<1)) result[1] = LinearInterpolate(_dequantBlock->_mins[1], _dequantBlock->_maxs[1], float(*v++)/float(0xffff));
			if (_dequantBlock->_elementFlags & (1<<2)) result[2] = LinearInterpolate(_dequantBlock->_mins[2], _dequantBlock->_maxs[2], float(*v++)/float(0xffff));
			if (_dequantBlock->_elementFlags & (1<<3)) result[3] = LinearInterpolate(_dequantBlock->_mins[3], _dequantBlock->_maxs[3], float(*v++)/float(0xffff));
			assert(isfinite(result[0]) && isfinite(result[1]) && isfinite(result[2]) && isfinite(result[3]));
			assert(!isnan(result[0]) && !isnan(result[1]) && !isnan(result[2]) && !isnan(result[3]));
			assert(result[0] == result[0] && result[1] == result[1] && result[2] == result[2] && result[3] == result[3]);
			return result;
		}

	template <>
		Quaternion CurveElementDequantDecompressor<Quaternion>::operator()(const void* data) const {
			assert(0);
			return Quaternion();
		}

	template <>
		Float4x4 CurveElementDequantDecompressor<Float4x4>::operator()(const void* data) const {
			assert(0);
			return Float4x4();
		}

	template<typename OutType, typename Decomp>
        OutType        EvaluateCurve(	float evalTime, 
										IteratorRange<const float*> timeMarkers,
										IteratorRange<const void*> keyData,
										const CurveKeyDataDesc& keyDataDesc,
										CurveInterpolationType interpolationType,
										const Decomp& decomp) never_throws 
	{
		// reminder -- lower_bound returns a pointer to the first key that is not smaller than inputTime (eg, equal or larger)
		auto* key = std::lower_bound(timeMarkers.begin(), timeMarkers.end(), evalTime);

			// note -- clamping at start and end positions of the curve
		if (key == timeMarkers.end())
			return decomp(keyData.begin());

		--key;	// (back one, to the first key that is smaller)
		auto keyIndex = key-timeMarkers.begin();
		auto alpha = LerpParameter(key[0], key[1], evalTime);
		auto keyCount = keyData.size() / keyDataDesc._elementStride;

        if (interpolationType == CurveInterpolationType::Linear) {

			// (need at least one key greater than the interpolation point, to perform interpolation correctly)
			if ((key+1) >= timeMarkers.end())
				return decomp(PtrAdd(keyData.begin(), (keyCount-1) * keyDataDesc._elementStride));

            assert(key[1] >= key[0]);		// (validating sorting assumption)
            
            auto P0 = decomp(PtrAdd(keyData.begin(), keyIndex * keyDataDesc._elementStride));
            auto P1 = decomp(PtrAdd(keyData.begin(), (keyIndex+1) * keyDataDesc._elementStride));
            return SphericalInterpolate(P0, P1, alpha);

        } else if (interpolationType == CurveInterpolationType::Bezier) {

            assert(keyDataDesc._flags & CurveKeyDataDesc::Flags::HasInTangent);
			assert(keyDataDesc._flags & CurveKeyDataDesc::Flags::HasOutTangent);

			// (need at least one key greater than the interpolation point, to perform interpolation correctly)
			if ((key+1) >= timeMarkers.end())
				return decomp(PtrAdd(keyData.begin(), (keyCount-1) * keyDataDesc._elementStride));

			assert(key[1] >= key[0]);		// (validating sorting assumption)
            const auto inTangentOffset = BitsPerPixel(keyDataDesc._elementFormat)/8;
            const auto outTangentOffset = inTangentOffset + BitsPerPixel(keyDataDesc._elementFormat)/8;

            auto P0 = decomp(PtrAdd(keyData.begin(), keyIndex * keyDataDesc._elementStride));
            auto P1 = decomp(PtrAdd(keyData.begin(), (keyIndex+1) * keyDataDesc._elementStride));

			// This is a convention of the Collada format
			// (see Collada spec 1.4.1, page 4-4)
			//		the first control point is stored under the semantic "OUT_TANGENT" for P0
			//		and second control point is stored under the semantic "IN_TANGENT" for P1
            auto C0 = decomp(PtrAdd(keyData.begin(), keyIndex * keyDataDesc._elementStride + outTangentOffset));
			auto C1 = decomp(PtrAdd(keyData.begin(), (keyIndex+1) * keyDataDesc._elementStride + inTangentOffset));

            return SphericalBezierInterpolate(P0, C0, C1, P1, alpha);

		} else if (interpolationType == CurveInterpolationType::CatmullRom) {

			// (need at least one key greater than the interpolation point, to perform interpolation correctly)
			if ((key+2) >= timeMarkers.end())
				return decomp(PtrAdd(keyData.begin(), (keyCount-1) * keyDataDesc._elementStride));

			auto P0 = decomp(PtrAdd(keyData.begin(), keyIndex * keyDataDesc._elementStride));
            auto P1 = decomp(PtrAdd(keyData.begin(), (keyIndex+1) * keyDataDesc._elementStride));
			// (note the clamp here that can result in P0 == P0n1 at the start of the curve)
			auto P0n1 = decomp(PtrAdd(keyData.begin(), std::max(0, signed(keyIndex)-1) * keyDataDesc._elementStride));
			auto P1p1 = decomp(PtrAdd(keyData.begin(), (keyIndex+2) * keyDataDesc._elementStride));

			auto P0n1T = timeMarkers[std::max(0, signed(keyIndex)-1)];
			auto P1p1T = timeMarkers[keyIndex+2];

			return SphericalCatmullRomInterpolate(
				P0n1, P0, P1, P1p1, 
				(P0n1T - key[0]) / (key[1] - key[0]), (P1p1T - key[0]) / (key[1] - key[0]),
				alpha);

        } else if (interpolationType == CurveInterpolationType::Hermite) {
			// hermite version not implemented
			//  -- but it's similar to both the Bezier and Catmull Rom implementations, nad
			//		could be easily hooked up
			assert(0);      
		}

        return decomp(keyData.begin());
    }

	template<typename OutType>
        OutType        RawAnimationCurve::Calculate(float inputTime) const never_throws
    {
		if (_keyDataDesc._flags & CurveKeyDataDesc::Flags::Quantized) {
			// We should find a dequantization block at the start of the key data.
			// This will contain the reconstructed min & max, and other parameters that
			// help with dequantization.
			assert(_keyData.size() > sizeof(CurveDequantizationBlock));
			auto* dequantBlock = (const CurveDequantizationBlock*)_keyData.begin();
			return EvaluateCurve<OutType>(	
				inputTime, 
				MakeIteratorRange(_timeMarkers.begin(), _timeMarkers.end()),
				MakeIteratorRange(PtrAdd(_keyData.begin(), sizeof(CurveDequantizationBlock)), _keyData.end()),
				_keyDataDesc, _interpolationType,
				CurveElementDequantDecompressor<OutType>(_keyDataDesc._elementFormat, *dequantBlock));
		} else {
			return EvaluateCurve<OutType>(	
				inputTime, 
				MakeIteratorRange(_timeMarkers.begin(), _timeMarkers.end()),
				MakeIteratorRange(_keyData.begin(), _keyData.end()),
				_keyDataDesc, _interpolationType,
				CurveElementDecompressor<OutType>(_keyDataDesc._elementFormat));
		}
	}

    float       RawAnimationCurve::StartTime() const
    {
        if (_timeMarkers.empty()) { return FLT_MAX; }
        return _timeMarkers[0];
    }

    float       RawAnimationCurve::EndTime() const
    {
        if (_timeMarkers.empty()) return -FLT_MAX;
        return _timeMarkers[_timeMarkers.size()-1];
    }

    template float      RawAnimationCurve::Calculate(float inputTime) const never_throws;
    template Float3     RawAnimationCurve::Calculate(float inputTime) const never_throws;
    template Float4     RawAnimationCurve::Calculate(float inputTime) const never_throws;
    template Float4x4   RawAnimationCurve::Calculate(float inputTime) const never_throws;
	template Quaternion RawAnimationCurve::Calculate(float inputTime) const never_throws;

    RawAnimationCurve::RawAnimationCurve(   SerializableVector<float>&&	timeMarkers, 
											SerializableVector<uint8>&&   keyData,
											const CurveKeyDataDesc&	keyDataDesc,
											CurveInterpolationType	interpolationType)
    :       _timeMarkers(std::move(timeMarkers))
    ,       _keyData(std::move(keyData))
    ,       _keyDataDesc(keyDataDesc)
    ,       _interpolationType(interpolationType)
    {}

	RawAnimationCurve::~RawAnimationCurve() {}

	RawAnimationCurve::RawAnimationCurve(const RawAnimationCurve& copyFrom)
	: _timeMarkers(copyFrom._timeMarkers)
	, _keyData(copyFrom._keyData)
	, _keyDataDesc(copyFrom._keyDataDesc)
	, _interpolationType(copyFrom._interpolationType) 
	{}

	RawAnimationCurve& RawAnimationCurve::operator=(const RawAnimationCurve& copyFrom) 
	{
		_timeMarkers = copyFrom._timeMarkers;
		_keyData = copyFrom._keyData;
		_keyDataDesc = copyFrom._keyDataDesc;
		_interpolationType = copyFrom._interpolationType;
		return *this;
	}

}}

