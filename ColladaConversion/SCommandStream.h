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

    void BuildSkeleton(
        NascentSkeleton& skeleton,
        const ::ColladaConversion::Node& node,
        SkeletonRegistry& skeletonReferences);

    NascentModelCommandStream::GeometryInstance InstantiateGeometry(
        const ::ColladaConversion::InstanceGeometry& instGeo,
        const ::ColladaConversion::Node& attachedNode,
        const ::ColladaConversion::URIResolveContext& resolveContext,
        TableOfObjects& accessableObjects,
        SkeletonRegistry& nodeRefs);

    NascentModelCommandStream::SkinControllerInstance InstantiateController(
        const ::ColladaConversion::InstanceController& instGeo,
        const ::ColladaConversion::Node& attachedNode,
        const ::ColladaConversion::URIResolveContext& resolveContext,
        TableOfObjects& accessableObjects,
        SkeletonRegistry& nodeRefs);
}}
