// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ColladaConversion.h"
#include "../Utility/StringFormat.h"
#include "../Utility/Streams/Stream.h"
#include "../Utility/StringUtils.h"
#include "../ConsoleRig/OutputStream.h"
#include <half.hpp>
#include <stdarg.h>

namespace RenderCore { namespace ColladaConversion
{
    bool ImportCameras = true;

    unsigned int FloatBits(float input)
    {
            // (or just use a reinterpret cast)
        union Converter { float f; unsigned int i; };
        Converter c; c.f = input; 
        return c.i;
    }

    unsigned int FloatBits(double input)
    {
            // (or just use a reinterpret cast)
        union Converter { float f; unsigned int i; };
        Converter c; c.f = float(input); 
        return c.i;
    }

    unsigned short AsFloat16(float input)
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

        return half_float::detail::float2half<std::round_to_nearest>(input);
    }

    float Float16AsFloat32(unsigned short input)
    {
        return half_float::detail::half2float(input);
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


    void AddToBoundingBox(  std::pair<Float3, Float3>& boundingBox,
                            const Float3& localPosition, const Float4x4& localToWorld)
    {
        Float3 transformedPosition = Truncate(localToWorld * Expand(localPosition, 1.f));

        boundingBox.first[0]    = std::min(transformedPosition[0], boundingBox.first[0]);
        boundingBox.first[1]    = std::min(transformedPosition[1], boundingBox.first[1]);
        boundingBox.first[2]    = std::min(transformedPosition[2], boundingBox.first[2]);
        boundingBox.second[0]   = std::max(transformedPosition[0], boundingBox.second[0]);
        boundingBox.second[1]   = std::max(transformedPosition[1], boundingBox.second[1]);
        boundingBox.second[2]   = std::max(transformedPosition[2], boundingBox.second[2]);
    }

    static float UNormAsFloat32(unsigned char unormValue)
    {
        return float(unormValue) / 255.f;
    }

    static Float4 AsFloat4(const void* rawData, Metal::NativeFormat::Enum nativeFormat)
    {   
            //
            //      todo -- this needs to move to the metal layer, so it can use
            //              platform specific formats
            //
        using namespace Metal::NativeFormat;
        switch (nativeFormat) {
        case R32G32B32A32_FLOAT:    return *(const Float4*)rawData;
        case R32G32B32_FLOAT:       return Float4(((const float*)rawData)[0], ((const float*)rawData)[1], ((const float*)rawData)[2], 0.f);
        case R32G32_FLOAT:          return Float4(((const float*)rawData)[0], ((const float*)rawData)[1], 0.f, 1.f);
        case R32_FLOAT:             return Float4(((const float*)rawData)[0], 0.f, 0.f, 1.f);

        case R10G10B10A2_UNORM:
        case R10G10B10A2_UINT:
        case R11G11B10_FLOAT:       
        case B5G6R5_UNORM:
        case B5G5R5A1_UNORM:        assert(0); return Float4(0,0,0,1);  // requires some custom adjustments (these are uncommon uses, anyway)

        case R16G16B16A16_FLOAT:    return Float4(Float16AsFloat32(((const unsigned short*)rawData)[0]), Float16AsFloat32(((const unsigned short*)rawData)[1]), Float16AsFloat32(((const unsigned short*)rawData)[2]), Float16AsFloat32(((const unsigned short*)rawData)[3]));
        case R16G16_FLOAT:          return Float4(Float16AsFloat32(((const unsigned short*)rawData)[0]), Float16AsFloat32(((const unsigned short*)rawData)[1]), 0.f, 1.f);
        case R16_FLOAT:             return Float4(Float16AsFloat32(((const unsigned short*)rawData)[0]), 0.f, 0.f, 1.f);

        case B8G8R8A8_UNORM:
        case R8G8B8A8_UNORM:        return Float4(UNormAsFloat32(((const unsigned char*)rawData)[0]), UNormAsFloat32(((const unsigned char*)rawData)[1]), UNormAsFloat32(((const unsigned char*)rawData)[2]), UNormAsFloat32(((const unsigned char*)rawData)[3]));
        case R8G8_UNORM:            return Float4(UNormAsFloat32(((const unsigned char*)rawData)[0]), UNormAsFloat32(((const unsigned char*)rawData)[1]), 0.f, 1.f);
        case R8_UNORM:              return Float4(UNormAsFloat32(((const unsigned char*)rawData)[0]), 0.f, 0.f, 1.f);
        
        case B8G8R8X8_UNORM:        return Float4(UNormAsFloat32(((const unsigned char*)rawData)[0]), UNormAsFloat32(((const unsigned char*)rawData)[1]), UNormAsFloat32(((const unsigned char*)rawData)[2]), 1.f);
        }

        assert(0);
        return Float4(0,0,0,1);
    }

    void AddToBoundingBox(  std::pair<Float3, Float3>& boundingBox,
                            const void* vertexData, size_t vertexStride, size_t vertexCount,
                            const Metal::InputElementDesc& elementDesc, 
                            const Float4x4& localToWorld)
    {
            //
            //      We need to have an explicit aligned byte offset for this call. If we
            //      pass in ~unsigned(0x0), it means "pack tightly after the previous element",
            //      But since we don't know the previous elements, we can't be sure
            //
        assert(elementDesc._alignedByteOffset != ~unsigned(0x0));
        for (size_t c=0; c<vertexCount; ++c) {
            const void* v    = PtrAdd(vertexData, vertexStride*c + elementDesc._alignedByteOffset);
            Float3 position  = Truncate(AsFloat4(v, elementDesc._nativeFormat));
            AddToBoundingBox(boundingBox, position, localToWorld);
        }
    }

    std::pair<Float3, Float3>       InvalidBoundingBox()
    {
        const Float3 mins(      std::numeric_limits<Float3::value_type>::max(),
                                std::numeric_limits<Float3::value_type>::max(),
                                std::numeric_limits<Float3::value_type>::max());
        const Float3 maxs(      -std::numeric_limits<Float3::value_type>::max(),
                                -std::numeric_limits<Float3::value_type>::max(),
                                -std::numeric_limits<Float3::value_type>::max());
        return std::make_pair(mins, maxs);
    }

    Metal::InputElementDesc FindPositionElement(const Metal::InputElementDesc elements[], size_t elementCount)
    {
        for (unsigned c=0; c<elementCount; ++c)
            if (elements[c]._semanticIndex == 0 && !XlCompareString(elements[c]._semanticName.c_str(), "POSITION"))
                return elements[c];
        return Metal::InputElementDesc();
    }

}}

