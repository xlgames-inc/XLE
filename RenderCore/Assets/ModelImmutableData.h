// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ModelScaffoldInternal.h"
#include "SkeletonScaffoldInternal.h"
#include "AnimationScaffoldInternal.h"
#include "Material.h"
#include <vector>
#include <utility>

namespace RenderCore { namespace Assets
{
        
    #pragma pack(push)
    #pragma pack(1)

////////////////////////////////////////////////////////////////////////////////////////////
    //      i m m u t a b l e   d a t a         //

    class ModelImmutableData
    {
    public:
        ModelCommandStream          _visualScene;
        
        RawGeometry*                _geos;
        size_t                      _geoCount;
        BoundSkinnedGeometry*       _boundSkinnedControllers;
        size_t                      _boundSkinnedControllerCount;

        TransformationMachine       _embeddedSkeleton;
        Float4x4*                   _defaultTransforms;
        size_t                      _defaultTransformCount;        

        std::pair<Float3, Float3>   _boundingBox;

        ModelImmutableData() = delete;
        ~ModelImmutableData();
    };

    class MaterialImmutableData
    {
    public:
        SerializableVector<std::pair<MaterialGuid, ResolvedMaterial>> _materials;
        SerializableVector<std::pair<MaterialGuid, std::string>> _materialNames;
    };

    class ModelSupplementImmutableData
    {
    public:
        SupplementGeo*  _geos;
        size_t          _geoCount;
    };

    #pragma pack(pop)

////////////////////////////////////////////////////////////////////////////////////////////
    //      b i n d i n g s         //

    class AnimationSetBinding
    {
    public:
        AnimationSetBinding(const AnimationSet::OutputInterface&            output,
                            const TransformationMachine::InputInterface&    input);

        std::vector<unsigned>   _animDriverToMachineParameter;
    };

    class SkeletonBinding
    {
    public:
        SkeletonBinding(    const TransformationMachine::OutputInterface&   output,
                            const ModelCommandStream::InputInterface&       input);

        std::vector<unsigned>   _modelJointIndexToMachineOutput;
        std::vector<Float4x4>   _modelJointIndexToInverseBindMatrix;
    };
}}
