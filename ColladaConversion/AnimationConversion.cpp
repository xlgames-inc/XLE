// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS   // warning C4996: 'std::_Copy_impl': Function call with parameters that may be unsafe
#pragma warning(disable:4244) // 4244: '=' : conversion from 'const double' to 'float', possible loss of data

#include "../RenderCore/Assets/RawAnimationCurve.h"
#include "../RenderCore/RenderUtils.h"

#include "ColladaConversion.h"
#pragma warning(push)
#pragma warning(disable:4201)       // nonstandard extension used : nameless struct/union
#pragma warning(disable:4245)       // conversion from 'int' to 'const COLLADAFW::SamplerID', signed/unsigned mismatch
#pragma warning(disable:4512)       // assignment operator could not be generated
    #include <COLLADAFWFloatOrDoubleArray.h>
    #include <COLLADAFWAnimation.h>
    #include <COLLADAFWAnimationCurve.h>
#pragma warning(pop)


namespace RenderCore { namespace ColladaConversion
{
    using ::Assets::Exceptions::FormatError;

    static std::unique_ptr<float[]>         AsFloats(const COLLADAFW::FloatOrDoubleArray& input)
    {
        size_t count = input.getValuesCount();
        if (input.getType() == COLLADAFW::FloatOrDoubleArray::DATA_TYPE_DOUBLE) {
            std::unique_ptr<float[]> result = std::make_unique<float[]>(count);
            for (unsigned c=0; c<count; ++c) 
                result[c] = float((*input.getDoubleValues())[c]);
            return result;
        } else if (input.getType() == COLLADAFW::FloatOrDoubleArray::DATA_TYPE_FLOAT) {
            std::unique_ptr<float[]> result = std::make_unique<float[]>(count);
            for (unsigned c=0; c<count; ++c) 
                result[c] = (*input.getFloatValues())[c];
            return result;
        }

        return nullptr;
    }

    /*static Float4x4 AsFloat4x4(const double* inputValues)
    {
        Float4x4 result;
        for (unsigned j=0; j<4; ++j)
            for (unsigned i=0; i<4; ++i)
                result(i,j) = float(inputValues[(i*4)+j]);      // (careful about ordering of input matrix)
        return result;
    }

    static Float4x4 AsFloat4x4(const float* inputValues)
    {
        Float4x4 result;
        for (unsigned j=0; j<4; ++j)
            for (unsigned i=0; i<4; ++i)
                result(i,j) = inputValues[(i*4)+j];  // (careful about ordering of input matrix)
        return result;
    }

    static std::unique_ptr<Float4x4[]>         AsFloat4x4s(const COLLADAFW::FloatOrDoubleArray& input)
    {
        const size_t valuesPerMatrix = sizeof(Float4x4)/sizeof(Float4x4::value_type);
        size_t count = input.getValuesCount();
        if (input.getType() == COLLADAFW::FloatOrDoubleArray::DATA_TYPE_DOUBLE) {

            std::unique_ptr<Float4x4[]> result = std::make_unique<Float4x4[]>(count/valuesPerMatrix);
            for (unsigned c=0; c<count/valuesPerMatrix; ++c) 
                result[c] = AsFloat4x4(&(*input.getDoubleValues())[c*valuesPerMatrix]);
            return result;

        } else if (input.getType() == COLLADAFW::FloatOrDoubleArray::DATA_TYPE_FLOAT) {

            std::unique_ptr<Float4x4[]> result = std::make_unique<Float4x4[]>(count/valuesPerMatrix);
            for (unsigned c=0; c<count/valuesPerMatrix; ++c) 
                result[c] = AsFloat4x4(&(*input.getFloatValues())[c*valuesPerMatrix]);
            return result;

        }

        return nullptr;
    }*/

    Assets::RawAnimationCurve      Convert(const COLLADAFW::Animation& animation)
    {
        using namespace COLLADAFW;
        if (animation.getAnimationType() != Animation::ANIMATION_CURVE) {
            ThrowException(FormatError("Formula animations not currently supported. Failure in animation (%s)", animation.getName().c_str()));
        }

        const AnimationCurve* curve = objectSafeCast<const AnimationCurve>(&animation);
        if (curve->getInPhysicalDimension() != PHYSICAL_DIMENSION_TIME) {
            ThrowException(FormatError("Input physical dimension for animation is not \"time\" in animation (%s). Only time animations are supported.", animation.getName().c_str()));
        }

        Assets::RawAnimationCurve::InterpolationType interpolationType = Assets::RawAnimationCurve::Linear;
        switch (curve->getInterpolationType()) {
        case AnimationCurve::INTERPOLATION_LINEAR:      interpolationType = Assets::RawAnimationCurve::Linear;  break;
        case AnimationCurve::INTERPOLATION_BEZIER:      interpolationType = Assets::RawAnimationCurve::Bezier;  break;
        case AnimationCurve::INTERPOLATION_HERMITE:     interpolationType = Assets::RawAnimationCurve::Hermite; break;
        default: ThrowException(FormatError("Interpolation type for animation (%s) is not linear, bezier or hermite. Only these interpolation types are currently supported.", animation.getName().c_str()));
        }

        size_t keyCount = curve->getKeyCount();
        if (!keyCount) {
            ThrowException(FormatError("Zero key count in animation (%s). Invalid input data.", animation.getName().c_str()));
        }

        using namespace Metal;
        NativeFormat::Enum positionFormat    = NativeFormat::Unknown;
        NativeFormat::Enum inTangentFormat   = NativeFormat::Unknown;
        NativeFormat::Enum outTangentFormat  = NativeFormat::Unknown;
            
        size_t outDimension = curve->getOutDimension();
        switch (outDimension) {
        case 1:     positionFormat = NativeFormat::R32_FLOAT; break;
        case 3:     positionFormat = NativeFormat::R32G32B32_FLOAT; break;
        case 4:     positionFormat = NativeFormat::R32G32B32A32_FLOAT; break;
        case 16:    positionFormat = NativeFormat::Matrix4x4; break;
        default:    ThrowException(FormatError("Out dimension is animation (%s) is invalid (%i). Expected 1, 3, 4 or 16.", animation.getName().c_str(), outDimension));
        }

        if (!curve->getInTangentValues().empty()) {
            inTangentFormat = positionFormat;
        }

        if (!curve->getOutTangentValues().empty()) {
            outTangentFormat = positionFormat;
        }

        const size_t positionSize =     Metal::BitsPerPixel(positionFormat)/8;
        const size_t inTangentSize =    Metal::BitsPerPixel(inTangentFormat)/8;
        const size_t elementSize = positionSize + inTangentSize + Metal::BitsPerPixel(outTangentFormat)/8;

            //
            //      We want the position and tangent values to be stored interleaved in the same buffer
            //      So let's build that buffer. Note that we will always use floats here... (so it may
            //      require conversion if the input collada data is not in floats)
            //
            //      Note that if we need to transpose the matrix input, then it must be done here!
            //
        DynamicArray<uint8, Serialization::BlockSerializerDeleter<uint8[]>> interleavedData(
            std::unique_ptr<uint8[], Serialization::BlockSerializerDeleter<uint8[]>>(new uint8[elementSize * keyCount]), 
            elementSize * keyCount);
        for (unsigned c=0; c<keyCount; ++c) {
            uint8* destination = PtrAdd(interleavedData.get(), c*elementSize);

            if (curve->getOutputValues().getType() == FloatOrDoubleArray::DATA_TYPE_FLOAT) {
                std::copy(
                    &(*curve->getOutputValues().getFloatValues())[c*outDimension],
                    &(*curve->getOutputValues().getFloatValues())[(c+1)*outDimension],
                    (float*)destination);
            } else if (curve->getOutputValues().getType() == FloatOrDoubleArray::DATA_TYPE_DOUBLE) {
                std::copy(
                    &(*curve->getOutputValues().getDoubleValues())[c*outDimension],
                    &(*curve->getOutputValues().getDoubleValues())[(c+1)*outDimension],
                    (float*)destination);
            }

            if (inTangentFormat != NativeFormat::Unknown) {
                uint8* inTangentDestination = PtrAdd(destination, positionSize);

                if (curve->getInTangentValues().getType() == FloatOrDoubleArray::DATA_TYPE_FLOAT) {
                    std::copy(
                        &(*curve->getInTangentValues().getFloatValues())[c*outDimension],
                        &(*curve->getInTangentValues().getFloatValues())[(c+1)*outDimension],
                        (float*)inTangentDestination);
                } else if (curve->getInTangentValues().getType() == FloatOrDoubleArray::DATA_TYPE_DOUBLE) {
                    std::copy(
                        &(*curve->getInTangentValues().getDoubleValues())[c*outDimension],
                        &(*curve->getInTangentValues().getDoubleValues())[(c+1)*outDimension],
                        (float*)inTangentDestination);
                }
            }

            if (outTangentFormat != NativeFormat::Unknown) {
                uint8* outTangentDestination = PtrAdd(destination, positionSize + inTangentSize);

                if (curve->getOutTangentValues().getType() == FloatOrDoubleArray::DATA_TYPE_FLOAT) {
                    std::copy(
                        &(*curve->getOutTangentValues().getFloatValues())[c*outDimension],
                        &(*curve->getOutTangentValues().getFloatValues())[(c+1)*outDimension],
                        (float*)outTangentDestination);
                } else if (curve->getOutTangentValues().getType() == FloatOrDoubleArray::DATA_TYPE_DOUBLE) {
                    std::copy(
                        &(*curve->getOutTangentValues().getDoubleValues())[c*outDimension],
                        &(*curve->getOutTangentValues().getDoubleValues())[(c+1)*outDimension],
                        (float*)outTangentDestination);
                }
            }
        }

            //      Record all of the time markers and position values (for each key in the input)

        assert(curve->getInputValues().getValuesCount() == keyCount);
        std::unique_ptr<float[], Serialization::BlockSerializerDeleter<float[]>> timeMarkers;
        timeMarkers.reset(AsFloats(curve->getInputValues()).release());

        return Assets::RawAnimationCurve(
            keyCount, std::move(timeMarkers), std::move(interleavedData), 
            elementSize, interpolationType,
            positionFormat, inTangentFormat, outTangentFormat);
    }


}}

