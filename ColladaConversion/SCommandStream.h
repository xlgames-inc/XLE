// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

namespace ColladaConversion { class Node; class VisualScene; class URIResolveContext; }

namespace RenderCore { namespace ColladaConversion
{
    class NascentSkeleton;
    class NodeReferences;
    class NascentModelCommandStream;
    class TableOfObjects;

    void BuildSkeleton(
        NascentSkeleton& skeleton,
        const ::ColladaConversion::Node& node,
        NodeReferences& skeletonReferences);

    void FindImportantNodes(
        NodeReferences& skeletonReferences,
        const ::ColladaConversion::VisualScene& scene);

    void InstantiateGeometry(
        NascentModelCommandStream& stream,
        const ::ColladaConversion::InstanceGeometry& instGeo,
        const ::ColladaConversion::Node& attachedNode,
        const ::ColladaConversion::URIResolveContext& resolveContext,
        TableOfObjects& accessableObjects,
        NodeReferences& nodeRefs);

    void InstantiateController(
        NascentModelCommandStream& stream,
        const ::ColladaConversion::InstanceController& instGeo,
        const ::ColladaConversion::Node& attachedNode,
        const ::ColladaConversion::URIResolveContext& resolveContext,
        TableOfObjects& accessableObjects,
        NodeReferences& nodeRefs);
}}
