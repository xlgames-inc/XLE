// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "NascentCommandStream.h"

namespace ColladaConversion { class Node; class VisualScene; class URIResolveContext; class InstanceGeometry; class InstanceController; }

namespace RenderCore { namespace ColladaConversion
{
    class NascentSkeleton;
    class SkeletonRegistry;
    class ImportConfiguration;
    class NascentRawGeometry;
    class NascentBoundSkinnedGeometry;

    void BuildSkeleton(
        NascentSkeleton& skeleton,
        const ::ColladaConversion::Node& node,
        SkeletonRegistry& skeletonReferences,
        int ignoreTransforms, bool fullSkeleton);

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
                IteratorRange<const Float4x4*> transforms
            ) const;

        friend std::ostream& operator<<(std::ostream&, const NascentGeometryObjects& geos);
    };

    NascentModelCommandStream::GeometryInstance InstantiateGeometry(
        const ::ColladaConversion::InstanceGeometry& instGeo,
        unsigned outputTransformIndex, const Float4x4& mergedTransform,
        const ::ColladaConversion::URIResolveContext& resolveContext,
        NascentGeometryObjects& objects,
        SkeletonRegistry& nodeRefs,
        const ImportConfiguration& cfg);

    NascentModelCommandStream::SkinControllerInstance InstantiateController(
        const ::ColladaConversion::InstanceController& instGeo,
        unsigned outputTransformIndex,
        const ::ColladaConversion::URIResolveContext& resolveContext,
        NascentGeometryObjects& objects,
        SkeletonRegistry& nodeRefs,
        const ImportConfiguration& cfg);

    class ReferencedGeometries
    {
    public:
        class AttachedObject
        {
        public:
            unsigned    _outputMatrixIndex;
            unsigned    _objectIndex;
        };
        std::vector<AttachedObject>   _meshes;
        std::vector<AttachedObject>   _skinControllers;

        void Gather(const ::ColladaConversion::Node& node, SkeletonRegistry& nodeRefs);
    };

    void RegisterNodeBindingNames(NascentSkeleton& skeleton, const SkeletonRegistry& registry);
    void RegisterNodeBindingNames(NascentModelCommandStream& stream, const SkeletonRegistry& registry);
}}
