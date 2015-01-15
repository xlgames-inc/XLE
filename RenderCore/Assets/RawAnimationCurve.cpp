// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "RawAnimationCurve.h"
#include "../../Math/Matrix.h"
#include "../../Math/Interpolation.h"
#include "../../Core/Exceptions.h"

namespace RenderCore { namespace Assets
{

    template<> Metal::NativeFormat::Enum   RawAnimationCurve::ExpectedFormat<Float4x4>()        { return Metal::NativeFormat::Matrix4x4; }
    template<> Metal::NativeFormat::Enum   RawAnimationCurve::ExpectedFormat<Float4>()          { return Metal::NativeFormat::R32G32B32A32_FLOAT; }
    template<> Metal::NativeFormat::Enum   RawAnimationCurve::ExpectedFormat<Float3>()          { return Metal::NativeFormat::R32G32B32_FLOAT; }
    template<> Metal::NativeFormat::Enum   RawAnimationCurve::ExpectedFormat<float>()           { return Metal::NativeFormat::R32_FLOAT; }

    static float LerpParameter(float A, float B, float input)
    {
        return (input - A) / (B-A);
    }

    template<typename OutType>
        OutType        RawAnimationCurve::Calculate(float inputTime) const never_throws
    {
        assert(_positionFormat == ExpectedFormat<OutType>());

            // note -- clamping at start and end positions of the curve
        if (inputTime < _timeMarkers[0])
            return *(OutType*)_parameterData.get();

        if (_interpolationType == Linear) {

            for (unsigned c=0; c<(_keyCount-1); ++c) {
                if (inputTime < _timeMarkers[c+1]) {
                    assert(_timeMarkers[c+1] > _timeMarkers[c]);
                    float alpha = LerpParameter(_timeMarkers[c], _timeMarkers[c+1], inputTime);

                    const OutType& P0 = *(const OutType*)PtrAdd(_parameterData.get(), c * _elementSize);
                    const OutType& P1 = *(const OutType*)PtrAdd(_parameterData.get(), (c+1) * _elementSize);
                    return SphericalInterpolate(P0, P1, alpha);
                }
            }

        } else if (_interpolationType == Bezier || _interpolationType == Hermite) {

            assert(_inTangentFormat != Metal::NativeFormat::Unknown);
            assert(_outTangentFormat != Metal::NativeFormat::Unknown);

            const size_t inTangentOffset = Metal::BitsPerPixel(_positionFormat)/8;
            const size_t outTangentOffset = inTangentOffset + Metal::BitsPerPixel(_inTangentFormat)/8;

            if (_interpolationType == Bezier) {

                for (unsigned c=0; c<(_keyCount-1); ++c) {
                    if (inputTime < _timeMarkers[c+1]) {
                        assert(_timeMarkers[c+1] > _timeMarkers[c]);
                        float alpha = LerpParameter(_timeMarkers[c], _timeMarkers[c+1], inputTime);

                        const OutType& P0 = *(const OutType*)PtrAdd(_parameterData.get(), c * _elementSize);
                        const OutType& P1 = *(const OutType*)PtrAdd(_parameterData.get(), (c+1) * _elementSize);

                        const OutType& C0 = *(const OutType*)PtrAdd(_parameterData.get(), c * _elementSize + outTangentOffset);
                        const OutType& C1 = *(const OutType*)PtrAdd(_parameterData.get(), (c+1) * _elementSize + inTangentOffset);

                        return SphericalBezierInterpolate(P0, C0, C1, P1, alpha);
                    }
                }

            } else {
                assert(0);      // hermite version not implemented (though we could just convert on load in)
            }

        }

        return *(OutType*)PtrAdd(_parameterData.get(), (_keyCount-1) * _elementSize );
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

    void        RawAnimationCurve::Serialize(Serialization::NascentBlockSerializer& outputSerializer) const
    {
        Serialization::Serialize(outputSerializer, _keyCount);
        Serialization::Serialize(outputSerializer, _timeMarkers, _keyCount);
        Serialization::Serialize(outputSerializer, _parameterData);
        Serialization::Serialize(outputSerializer, _elementSize);
        Serialization::Serialize(outputSerializer, unsigned(_interpolationType));
        Serialization::Serialize(outputSerializer, unsigned(_positionFormat));
        Serialization::Serialize(outputSerializer, unsigned(_inTangentFormat));
        Serialization::Serialize(outputSerializer, unsigned(_outTangentFormat));
    }

    RawAnimationCurve::RawAnimationCurve(   size_t keyCount, 
                                            std::unique_ptr<float[], Serialization::BlockSerializerDeleter<float[]>>&&  timeMarkers, 
                                            DynamicArray<uint8, Serialization::BlockSerializerDeleter<uint8[]>>&&       keyPositions,
                                            size_t elementSize, InterpolationType interpolationType,
                                            Metal::NativeFormat::Enum positionFormat, Metal::NativeFormat::Enum inTangentFormat, 
                                            Metal::NativeFormat::Enum outTangentFormat)
    :       _keyCount(keyCount)
    ,       _timeMarkers(std::forward<std::unique_ptr<float[], Serialization::BlockSerializerDeleter<float[]>>>(timeMarkers))
    ,       _parameterData(std::forward<DynamicArray<uint8, Serialization::BlockSerializerDeleter<uint8[]>>>(keyPositions))
    ,       _elementSize(elementSize)
    ,       _interpolationType(interpolationType)
    ,       _positionFormat(positionFormat)
    ,       _inTangentFormat(inTangentFormat)
    ,       _outTangentFormat(outTangentFormat)
    {}

    RawAnimationCurve::RawAnimationCurve(RawAnimationCurve&& curve)
    :       _keyCount(curve._keyCount)
    ,       _timeMarkers(std::move(curve._timeMarkers))
    ,       _parameterData(std::move(curve._parameterData))
    ,       _elementSize(curve._elementSize)
    ,       _interpolationType(curve._interpolationType)
    ,       _positionFormat(curve._positionFormat)
    ,       _inTangentFormat(curve._inTangentFormat)
    ,       _outTangentFormat(curve._outTangentFormat)
    {}

    RawAnimationCurve::RawAnimationCurve(const RawAnimationCurve& copyFrom)
    :       _keyCount(copyFrom._keyCount)
    ,       _parameterData(DynamicArray<uint8, Serialization::BlockSerializerDeleter<uint8[]>>::Copy(copyFrom._parameterData))
    ,       _elementSize(copyFrom._elementSize)
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
        _elementSize = curve._elementSize;
        _interpolationType = curve._interpolationType;
        _positionFormat = curve._positionFormat;
        _inTangentFormat = curve._inTangentFormat;
        _outTangentFormat = curve._outTangentFormat;
        return *this;
    }

}}

