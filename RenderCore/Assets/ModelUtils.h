// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ModelRunTime.h"
#include "ModelImmutableData.h"

namespace RenderCore { namespace Assets 
{
    class EmbeddedSkeletonPose
    {
    public:
        SkeletonBinding         _binding;
        std::vector<Float4x4>   _transforms;

        MeshToModel GetMeshToModel()
        {
            return MeshToModel(
                AsPointer(_transforms.cbegin()), 
                (unsigned)_transforms.size(), &_binding);
        }

        EmbeddedSkeletonPose(
            const ModelScaffold& scaffold, 
            const TransformationParameterSet* paramSet = nullptr);
        ~EmbeddedSkeletonPose();
        EmbeddedSkeletonPose(EmbeddedSkeletonPose&& moveFrom) never_throws;
        EmbeddedSkeletonPose& operator=(EmbeddedSkeletonPose&& moveFrom) never_throws;
    };
}}

