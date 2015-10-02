// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ModelUtils.h"

namespace RenderCore { namespace Assets 
{
    EmbeddedSkeletonPose::EmbeddedSkeletonPose(
        const ModelScaffold& scaffold, 
        const TransformationParameterSet* paramSet)
    {
        auto& transMachine = scaffold.EmbeddedSkeleton();
        auto transformCount = transMachine.GetOutputMatrixCount();
        _transforms.resize(transformCount);
        transMachine.GenerateOutputTransforms(
            AsPointer(_transforms.begin()), transformCount,
            paramSet ? paramSet : &transMachine.GetDefaultParameters());

        _binding = SkeletonBinding(
            transMachine.GetOutputInterface(),
            scaffold.CommandStream().GetInputInterface());
    }

    EmbeddedSkeletonPose::~EmbeddedSkeletonPose() {}

    EmbeddedSkeletonPose::EmbeddedSkeletonPose(EmbeddedSkeletonPose&& moveFrom) never_throws
    : _binding(moveFrom._binding)
    , _transforms(moveFrom._transforms)
    {}

    EmbeddedSkeletonPose& EmbeddedSkeletonPose::operator=(EmbeddedSkeletonPose&& moveFrom) never_throws
    {
        _binding = moveFrom._binding;
        _transforms = moveFrom._transforms;
        return *this;
    }

}}

