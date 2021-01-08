// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Types_Forward.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/Streams/SerializationUtils.h"
#include "../../Core/Types.h"
#include <memory>

namespace RenderCore { namespace Assets
{
	struct CurveKeyDataDesc
	{
		struct Flags { enum BitValue { Quantized = 1<<0, HasInTangent = 1<<1, HasOutTangent = 1<<2 }; using BitField = unsigned; };
		Flags::BitField	_flags;
		unsigned		_elementStride;
		Format			_elementFormat;
	};

	struct CurveDequantizationBlock
	{
		unsigned _elementFlags;
		float _mins[4], _maxs[4];
	};

	enum class CurveInterpolationType : unsigned { Linear, Bezier, Hermite, CatmullRom };

    class RawAnimationCurve 
    {
    public:
        template<typename Serializer>
            void        SerializeMethod(Serializer& outputSerializer) const;

        float       StartTime() const;
        float       EndTime() const;

        template<typename OutType>
            OutType        Calculate(float inputTime) const never_throws;

		RawAnimationCurve(  SerializableVector<float>&&	timeMarkers, 
                            SerializableVector<uint8>&& keyData,
							const CurveKeyDataDesc&	keyDataDesc,
                            CurveInterpolationType	interpolationType);
        RawAnimationCurve(RawAnimationCurve&& moveFrom) = default;
        RawAnimationCurve& operator=(RawAnimationCurve&& moveFrom) = default;
		RawAnimationCurve(const RawAnimationCurve& copyFrom);
		RawAnimationCurve& operator=(const RawAnimationCurve& copyFrom);
		~RawAnimationCurve();

    protected:
        SerializableVector<float>	_timeMarkers;
        SerializableVector<uint8>	_keyData;
        CurveKeyDataDesc			_keyDataDesc;
		CurveInterpolationType		_interpolationType;
    };

    template<typename Serializer>
        void        RawAnimationCurve::SerializeMethod(Serializer& outputSerializer) const
    {
        SerializationOperator(outputSerializer, _timeMarkers);
        SerializationOperator(outputSerializer, _keyData);
        SerializationOperator(outputSerializer, _keyDataDesc._flags);
		SerializationOperator(outputSerializer, _keyDataDesc._elementStride);
        SerializationOperator(outputSerializer, unsigned(_keyDataDesc._elementFormat));
		SerializationOperator(outputSerializer, unsigned(_interpolationType));
    }

}}





