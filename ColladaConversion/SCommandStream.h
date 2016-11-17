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
	class NascentGeometryObjects;
	class NascentModelCommandStream;
    class SkeletonRegistry;
    class ImportConfiguration;

    void BuildSkeleton(
        NascentSkeleton& skeleton,
        const ::ColladaConversion::Node& sceneRoot,
        StringSection<utf8> rootNode,
        SkeletonRegistry& skeletonReferences,
        bool fullSkeleton);

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
