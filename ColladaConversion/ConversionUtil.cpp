// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "ConversionUtil.h"
#include "ColladaConversion.h"
#include "ModelCommandStream.h"
#include "TableOfObjects.h"
#include "../Assets/Assets.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/Streams/StreamDOM.h"
#include "../Utility/Streams/StreamFormatter.h"

namespace RenderCore { namespace ColladaConversion
{
    ImportConfiguration::ImportConfiguration(const ::Assets::ResChar filename[])
    {
        TRY 
        {
            size_t fileSize = 0;
            auto sourceFile = LoadFileAsMemoryBlock(filename, &fileSize);
            InputStreamFormatter<utf8> formatter(
                MemoryMappedInputStream(sourceFile.get(), PtrAdd(sourceFile.get(), fileSize)));
            Document<InputStreamFormatter<utf8>> doc(formatter);

            auto bindingRenames = doc.Element(u("BindingRenames"));
            if (bindingRenames) {
                auto child = bindingRenames.FirstAttribute();
                for (; child; child = child.Next())
                    if (child)
                        _exportNameToBinding.push_back(
                            std::make_pair(child.Name(), child.Value()));
            }

            auto bindingSuppress = doc.Element(u("BindingSuppress"));
            if (bindingSuppress) {
                auto child = bindingSuppress.FirstAttribute();
                for (; child; child = child.Next())
                    _suppressed.push_back(child.Name());
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

    bool ImportConfiguration::IsSuppressed(const utf8* inputStart, const utf8* inputEnd) const
    {
        auto i = std::find_if(
            _suppressed.cbegin(), _suppressed.cend(),
            [=](const String& e) 
            {
                return  ptrdiff_t(e.size()) == (inputEnd - inputStart)
                    &&  std::equal(e.cbegin(), e.cend(), inputStart);
            });

        return (i != _suppressed.cend());
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

            const NascentBoundSkinnedGeometry* controller = objects.Get<NascentBoundSkinnedGeometry>(inst._id);
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
}}

