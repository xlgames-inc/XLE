// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NascentCommandStream.h"

namespace ColladaConversion { class Node; class VisualScene; class URIResolveContext; class InstanceGeometry; class InstanceController; }

namespace RenderCore { namespace ColladaConversion
{
    class NascentSkeleton;
    class SkeletonRegistry;
    class TableOfObjects;
    class ImportConfiguration;

    void BuildFullSkeleton(
        NascentSkeleton& skeleton,
        const ::ColladaConversion::Node& node,
        SkeletonRegistry& skeletonReferences);

    void BuildMinimalSkeleton(
        NascentSkeleton& skeleton,
        const ::ColladaConversion::Node& node,
        SkeletonRegistry& skeletonReferences);

    class NascentGeometryObjects
    {
    public:
        std::vector<std::pair<ObjectGuid, NascentRawGeometry>> _rawGeos;
        std::vector<std::pair<ObjectGuid, NascentBoundSkinnedGeometry>> _skinnedGeos;

        unsigned GetGeo(ObjectGuid id);
        unsigned GetSkinnedGeo(ObjectGuid id);

        std::pair<Float3, Float3> CalculateBoundingBox
            (
                const NascentModelCommandStream& scene,
                const Float4x4* transformsBegin, 
                const Float4x4* transformsEnd
            );
    };

    NascentModelCommandStream::GeometryInstance InstantiateGeometry(
        const ::ColladaConversion::InstanceGeometry& instGeo,
        const ::ColladaConversion::Node& attachedNode,
        const ::ColladaConversion::URIResolveContext& resolveContext,
        NascentGeometryObjects& objects,
        SkeletonRegistry& nodeRefs,
        const ImportConfiguration& cfg);

    NascentModelCommandStream::SkinControllerInstance InstantiateController(
        const ::ColladaConversion::InstanceController& instGeo,
        const ::ColladaConversion::Node& attachedNode,
        const ::ColladaConversion::URIResolveContext& resolveContext,
        NascentGeometryObjects& objects,
        SkeletonRegistry& nodeRefs,
        const ImportConfiguration& cfg);

    void FindReferencedGeometries(
        const ::ColladaConversion::Node& node, 
        std::vector<unsigned>& instancedGeometries,
        std::vector<unsigned>& instancedControllers);

    void RegisterNodeBindingNames(NascentSkeleton& skeleton, const SkeletonRegistry& registry);
    void RegisterNodeBindingNames(NascentModelCommandStream& stream, const SkeletonRegistry& registry);
}}
