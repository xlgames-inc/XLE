// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "NascentCommandStream.h"
#include "../Utility/StringUtils.h"

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
        const ::ColladaConversion::Node& sceneRoot,
        StringSection<utf8> rootNode,
        SkeletonRegistry& skeletonReferences,
        bool fullSkeleton);

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
        unsigned levelOfDetail,
        const ::ColladaConversion::URIResolveContext& resolveContext,
        NascentGeometryObjects& objects,
        SkeletonRegistry& nodeRefs,
        const ImportConfiguration& cfg);

    NascentModelCommandStream::SkinControllerInstance InstantiateController(
        const ::ColladaConversion::InstanceController& instGeo,
        unsigned outputTransformIndex, unsigned levelOfDetail,
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
            unsigned    _levelOfDetail;
        };
        std::vector<AttachedObject>   _meshes;
        std::vector<AttachedObject>   _skinControllers;

        bool Gather(const ::ColladaConversion::Node& sceneRoot, StringSection<utf8> rootNode, SkeletonRegistry& nodeRefs);

        void FindSkinJoints(
            const ::ColladaConversion::VisualScene& scene, 
            const ::ColladaConversion::URIResolveContext& resolveContext, 
            SkeletonRegistry& nodeRefs);

    private:
        void Gather(const ::ColladaConversion::Node& node, SkeletonRegistry& nodeRefs, bool terminateOnLODNodes = false);
    };

    void RegisterNodeBindingNames(NascentSkeleton& skeleton, const SkeletonRegistry& registry);
    void RegisterNodeBindingNames(NascentModelCommandStream& stream, const SkeletonRegistry& registry);
}}
