// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

namespace COLLADAFW
{
    class Node;
    class Geometry;

    template<class T> class Array;
    class MaterialBinding;
    typedef Array<MaterialBinding> MaterialBindingArray;
}

namespace RenderCore { namespace ColladaConversion
{
    class NascentRawGeometry;
    class NascentModelCommandStream;
    class TableOfObjects;
    class JointReferences;
    class NascentSkeleton;

    NascentRawGeometry Convert(const COLLADAFW::Geometry* geometry);

    bool IsUseful(  const COLLADAFW::Node& node, const TableOfObjects& objects,
                    const JointReferences& skeletonReferences);

    void FindInstancedSkinControllers(   
        const COLLADAFW::Node& node, const TableOfObjects& objects,
        JointReferences& results);

    void InstantiateControllers(
        NascentModelCommandStream& stream, 
        const COLLADAFW::Node& node, 
        const TableOfObjects& accessableObjects, 
        TableOfObjects& destinationForNewObjects);

    void PushNode(
        NascentSkeleton& skeleton,
        const COLLADAFW::Node& node, const TableOfObjects& accessableObjects,
        const JointReferences& skeletonReferences);

    void PushNode(   
        NascentModelCommandStream& stream,
        const COLLADAFW::Node& node, const TableOfObjects& accessableObjects,
        const JointReferences& skeletonReferences);
}}

