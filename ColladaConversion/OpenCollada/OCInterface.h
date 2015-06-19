// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Math/Vector.h"
#include "../../Math/Matrix.h"

namespace COLLADAFW
{
    class Node;
    class Geometry;

    template<class T> class Array;
    class MaterialBinding;
    typedef Array<MaterialBinding> MaterialBindingArray;

    template<class T> class PointerArray;
    class Transformation;
    typedef PointerArray<Transformation> TransformationPointerArray;
}

namespace COLLADABU { namespace Math
{
    class Matrix4; class Vector3;
}}

namespace RenderCore { namespace Assets { class NascentTransformationMachine; }}

namespace RenderCore { namespace ColladaConversion
{
    class NascentRawGeometry;
    class NascentModelCommandStream;
    class TableOfObjects;
    class SkeletonRegistry;
    class NascentSkeleton;

    NascentRawGeometry Convert(const COLLADAFW::Geometry* geometry);

    bool IsUseful(  const COLLADAFW::Node& node, const TableOfObjects& objects,
                    const SkeletonRegistry& skeletonReferences);

    void FindInstancedSkinControllers(   
        const COLLADAFW::Node& node, const TableOfObjects& objects,
        SkeletonRegistry& results);

    void InstantiateControllers(
        NascentModelCommandStream& stream, 
        const COLLADAFW::Node& node, 
        const TableOfObjects& accessableObjects, 
        TableOfObjects& destinationForNewObjects);

    void PushNode(
        NascentSkeleton& skeleton,
        const COLLADAFW::Node& node, const TableOfObjects& accessableObjects,
        SkeletonRegistry& skeletonReferences);

    void PushNode(   
        NascentModelCommandStream& stream,
        const COLLADAFW::Node& node, const TableOfObjects& accessableObjects,
        SkeletonRegistry& skeletonReferences);

    Float4x4 AsFloat4x4(const COLLADABU::Math::Matrix4& matrix);
    Float3 AsFloat3(const COLLADABU::Math::Vector3& vector);


    unsigned PushTransformations(
        RenderCore::Assets::NascentTransformationMachine& dst,
        const COLLADAFW::TransformationPointerArray& transformations, 
        const char nodeName[]);
}}

