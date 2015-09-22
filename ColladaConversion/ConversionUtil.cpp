// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "ConversionUtil.h"
#include "NascentCommandStream.h"
#include "NascentRawGeometry.h"
#include "NascentAnimController.h"
#include "../Assets/Assets.h"       // (for RegisterFileDependency)
#include "../ConsoleRig/Log.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/Streams/StreamDOM.h"
#include "../Utility/Streams/StreamFormatter.h"
#include "../Utility/StringUtils.h"
#include "../Foreign/half-1.9.2/include/half.hpp"


namespace RenderCore { namespace ColladaConversion
{
    bool ImportCameras = true;

    ImportConfiguration::ImportConfiguration(const ::Assets::ResChar filename[])
    {
        TRY 
        {
            size_t fileSize = 0;
            auto sourceFile = LoadFileAsMemoryBlock(filename, &fileSize);
            InputStreamFormatter<utf8> formatter(
                MemoryMappedInputStream(sourceFile.get(), PtrAdd(sourceFile.get(), fileSize)));
            Document<InputStreamFormatter<utf8>> doc(formatter);

            _resourceBindings = BindingConfig(doc.Element(u("Resources")));
            _constantsBindings = BindingConfig(doc.Element(u("Constants")));
            _vertexSemanticBindings = BindingConfig(doc.Element(u("VertexSemantics")));

        } CATCH(...) {
            LogWarning << "Problem while loading configuration file (" << filename << "). Using defaults.";
        } CATCH_END

        _depVal = std::make_shared<::Assets::DependencyValidation>();
        RegisterFileDependency(_depVal, filename);
    }
    ImportConfiguration::ImportConfiguration() {}
    ImportConfiguration::~ImportConfiguration()
    {}

    BindingConfig::BindingConfig(const DocElementHelper<InputStreamFormatter<utf8>>& source)
    {
        auto bindingRenames = source.Element(u("Rename"));
        if (bindingRenames) {
            auto child = bindingRenames.FirstAttribute();
            for (; child; child = child.Next())
                if (child)
                    _exportNameToBinding.push_back(
                        std::make_pair(child.Name(), child.Value()));
        }

        auto bindingSuppress = source.Element(u("Suppress"));
        if (bindingSuppress) {
            auto child = bindingSuppress.FirstAttribute();
            for (; child; child = child.Next())
                _bindingSuppressed.push_back(child.Name());
        }
    }

    BindingConfig::BindingConfig() {}
    BindingConfig::~BindingConfig() {}

    std::basic_string<utf8> BindingConfig::AsNative(const utf8* inputStart, const utf8* inputEnd) const
    {
            //  we need to define a mapping between the names used by the max exporter
            //  and the native XLE shader names. The meaning might not match perfectly
            //  but let's try to get as close as possible
        auto i = std::find_if(
            _exportNameToBinding.cbegin(), _exportNameToBinding.cend(),
            [=](const std::pair<String, String>& e) 
            {
                return  ptrdiff_t(e.first.size()) == (inputEnd - inputStart)
                    &&  std::equal(e.first.cbegin(), e.first.cend(), inputStart);
            });

        if (i != _exportNameToBinding.cend()) 
            return i->second;
        return std::basic_string<utf8>(inputStart, inputEnd);
    }

    bool BindingConfig::IsSuppressed(const utf8* inputStart, const utf8* inputEnd) const
    {
        auto i = std::find_if(
            _bindingSuppressed.cbegin(), _bindingSuppressed.cend(),
            [=](const String& e) 
            {
                return  ptrdiff_t(e.size()) == (inputEnd - inputStart)
                    &&  std::equal(e.cbegin(), e.cend(), inputStart);
            });

        return (i != _bindingSuppressed.cend());
    }

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

    static float Float16AsFloat32(unsigned short input)
    {
        return half_float::detail::half2float(input);
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
                            const Assets::VertexElement& elementDesc, 
                            const Float4x4& localToWorld)
    {
            //
            //      We need to have an explicit aligned byte offset for this call. If we
            //      pass in ~unsigned(0x0), it means "pack tightly after the previous element",
            //      But since we don't know the previous elements, we can't be sure
            //
        assert(elementDesc._startOffset != ~unsigned(0x0));
        for (size_t c=0; c<vertexCount; ++c) {
            const void* v    = PtrAdd(vertexData, vertexStride*c + elementDesc._startOffset);
            Float3 position  = Truncate(AsFloat4(v, Metal::NativeFormat::Enum(elementDesc._nativeFormat)));
            assert(!isinf(position[0]) && !isinf(position[1]) && !isinf(position[2]));
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

    Assets::VertexElement FindPositionElement(const Assets::VertexElement elements[], size_t elementCount)
    {
        for (unsigned c=0; c<elementCount; ++c)
            if (elements[c]._semanticIndex == 0 && !XlCompareStringI(elements[c]._semanticName, "POSITION"))
                return elements[c];
        return Assets::VertexElement();
    }

}}

