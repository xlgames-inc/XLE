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

            {
                auto bindingRenames = doc.Element(u("BindingRenames"));
                if (bindingRenames) {
                    auto child = bindingRenames.FirstAttribute();
                    for (; child; child = child.Next())
                        if (child)
                            _exportNameToBinding.push_back(
                                std::make_pair(child.Name(), child.Value()));
                }
            }

            {
                auto bindingSuppress = doc.Element(u("BindingSuppress"));
                if (bindingSuppress) {
                    auto child = bindingSuppress.FirstAttribute();
                    for (; child; child = child.Next())
                        _bindingSuppressed.push_back(child.Name());
                }
            }

            {
                auto bindingRenames = doc.Element(u("VertexSemanticRename"));
                if (bindingRenames) {
                    auto child = bindingRenames.FirstAttribute();
                    for (; child; child = child.Next())
                        if (child)
                            _vertexSemanticRename.push_back(
                                std::make_pair(child.Name(), child.Value()));
                }
            }

            {
                auto bindingSuppress = doc.Element(u("VertexSemanticSuppress"));
                if (bindingSuppress) {
                    auto child = bindingSuppress.FirstAttribute();
                    for (; child; child = child.Next())
                        _vertexSemanticSuppressed.push_back(child.Name());
                }
            }

        } CATCH(...) {
            LogWarning << "Problem while loading configuration file (" << filename << "). Using defaults.";
        } CATCH_END

        _depVal = std::make_shared<::Assets::DependencyValidation>();
        RegisterFileDependency(_depVal, filename);
    }
    ImportConfiguration::ImportConfiguration() {}
    ImportConfiguration::~ImportConfiguration()
    {}

    std::basic_string<utf8> ImportConfiguration::AsNativeBinding(const utf8* inputStart, const utf8* inputEnd) const
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

    bool ImportConfiguration::IsBindingSuppressed(const utf8* inputStart, const utf8* inputEnd) const
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

    std::basic_string<utf8> ImportConfiguration::AsNativeVertexSemantic(const utf8* inputStart, const utf8* inputEnd) const
    {
            //  we need to define a mapping between the names used by the max exporter
            //  and the native XLE shader names. The meaning might not match perfectly
            //  but let's try to get as close as possible
        auto i = std::find_if(
            _vertexSemanticRename.cbegin(), _vertexSemanticRename.cend(),
            [=](const std::pair<String, String>& e) 
            {
                return  ptrdiff_t(e.first.size()) == (inputEnd - inputStart)
                    &&  std::equal(e.first.cbegin(), e.first.cend(), inputStart);
            });

        if (i != _vertexSemanticRename.cend()) 
            return i->second;
        return std::basic_string<utf8>(inputStart, inputEnd);
    }

    bool ImportConfiguration::IsVertexSemanticSuppressed(const utf8* inputStart, const utf8* inputEnd) const
    {
        auto i = std::find_if(
            _vertexSemanticSuppressed.cbegin(), _vertexSemanticSuppressed.cend(),
            [=](const String& e) 
            {
                return  ptrdiff_t(e.size()) == (inputEnd - inputStart)
                    &&  std::equal(e.cbegin(), e.cend(), inputStart);
            });

        return (i != _vertexSemanticSuppressed.cend());
    }


    std::pair<Float3, Float3> CalculateBoundingBox
        (
            const NascentModelCommandStream& scene,
            const TableOfObjects& objects,
            const Float4x4* transformsBegin, 
            const Float4x4* transformsEnd
        )
    {
            //
            //      For all the parts of the model, calculate the bounding box.
            //      We just have to go through each vertex in the model, and
            //      transform it into model space, and calculate the min and max values
            //      found;
            //
        using namespace ColladaConversion;
        auto result = InvalidBoundingBox();
        // const auto finalMatrices = 
        //     _skeleton.GetTransformationMachine().GenerateOutputTransforms(
        //         _animationSet.BuildTransformationParameterSet(0.f, nullptr, _skeleton, _objects));

            //
            //      Do the unskinned geometry first
            //

        for (auto i=scene._geometryInstances.cbegin(); i!=scene._geometryInstances.cend(); ++i) {
            const NascentModelCommandStream::GeometryInstance& inst = *i;

            const NascentRawGeometry*  geo = objects.GetByIndex<NascentRawGeometry>(inst._id);
            if (!geo) continue;

            Float4x4 localToWorld = Identity<Float4x4>();
            if ((transformsBegin + inst._localToWorldId) < transformsEnd)
                localToWorld = *(transformsBegin + inst._localToWorldId);

            const void*         vertexBuffer = geo->_vertices.get();
            const unsigned      vertexStride = geo->_mainDrawInputAssembly._vertexStride;

            Metal::InputElementDesc positionDesc = FindPositionElement(
                AsPointer(geo->_mainDrawInputAssembly._vertexInputLayout.begin()),
                geo->_mainDrawInputAssembly._vertexInputLayout.size());

            if (positionDesc._nativeFormat != Metal::NativeFormat::Unknown && vertexStride) {
                AddToBoundingBox(
                    result, vertexBuffer, vertexStride, 
                    geo->_vertices.size() / vertexStride, positionDesc, localToWorld);
            }
        }

            //
            //      Now also do the skinned geometry. But use the default pose for
            //      skinned geometry (ie, don't apply the skinning transforms to the bones).
            //      Obvious this won't give the correct result post-animation.
            //

        for (auto i=scene._skinControllerInstances.cbegin(); i!=scene._skinControllerInstances.cend(); ++i) {
            const NascentModelCommandStream::SkinControllerInstance& inst = *i;

            const NascentBoundSkinnedGeometry* controller = objects.GetByIndex<NascentBoundSkinnedGeometry>(inst._id);
            if (!controller) continue;

            Float4x4 localToWorld = Identity<Float4x4>();
            if ((transformsBegin + inst._localToWorldId) < transformsEnd)
                localToWorld = *(transformsBegin + inst._localToWorldId);

                //  We can't get the vertex position data directly from the vertex buffer, because
                //  the "bound" object is already using an opaque hardware object. However, we can
                //  transform the local space bounding box and use that.

            const unsigned indices[][3] = 
            {
                {0,0,0}, {0,1,0}, {1,0,0}, {1,1,0},
                {0,0,1}, {0,1,1}, {1,0,1}, {1,1,1}
            };

            const Float3* A = (const Float3*)&controller->_localBoundingBox.first;
            for (unsigned c=0; c<dimof(indices); ++c) {
                Float3 position(A[indices[c][0]][0], A[indices[c][1]][1], A[indices[c][2]][2]);
                AddToBoundingBox(result, position, localToWorld);
            }
        }

        return result;
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

