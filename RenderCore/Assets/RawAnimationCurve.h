// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Metal/Format.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/Streams/Serialization.h"
#include "../../Core/Types.h"
#include <memory>

namespace RenderCore { namespace Assets
{
    class RawAnimationCurve 
    {
    public:
        enum InterpolationType { Linear, Bezier, Hermite };

        RawAnimationCurve(  size_t keyCount, 
                            std::unique_ptr<float[], BlockSerializerDeleter<float[]>>&&  timeMarkers, 
                            DynamicArray<uint8, BlockSerializerDeleter<uint8[]>>&&       keyPositions,
                            size_t elementSize, InterpolationType interpolationType,
                            Metal::NativeFormat::Enum positionFormat, Metal::NativeFormat::Enum inTangentFormat, 
                            Metal::NativeFormat::Enum outTangentFormat);
        RawAnimationCurve(RawAnimationCurve&& curve);
        RawAnimationCurve(const RawAnimationCurve& copyFrom);
        RawAnimationCurve& operator=(RawAnimationCurve&& curve);

        template<typename Serializer>
            void        Serialize(Serializer& outputSerializer) const;

        float       StartTime() const;
        float       EndTime() const;

        template<typename OutType>
            OutType        Calculate(float inputTime) const never_throws;

    protected:
        size_t                          _keyCount;
        std::unique_ptr<float[], BlockSerializerDeleter<float[]>>    _timeMarkers;
        DynamicArray<uint8, BlockSerializerDeleter<uint8[]>>         _parameterData;
        size_t                          _elementSize;
        InterpolationType               _interpolationType;

        Metal::NativeFormat::Enum       _positionFormat;
        Metal::NativeFormat::Enum       _inTangentFormat;
        Metal::NativeFormat::Enum       _outTangentFormat;

        template<typename OutType>
            static Metal::NativeFormat::Enum   ExpectedFormat();
    };

    template<typename Serializer>
        void        RawAnimationCurve::Serialize(Serializer& outputSerializer) const
    {
        ::Serialize(outputSerializer, _keyCount);
        ::Serialize(outputSerializer, _timeMarkers, _keyCount);
        ::Serialize(outputSerializer, _parameterData);
        ::Serialize(outputSerializer, _elementSize);
        ::Serialize(outputSerializer, unsigned(_interpolationType));
        ::Serialize(outputSerializer, unsigned(_positionFormat));
        ::Serialize(outputSerializer, unsigned(_inTangentFormat));
        ::Serialize(outputSerializer, unsigned(_outTangentFormat));
    }

}}





