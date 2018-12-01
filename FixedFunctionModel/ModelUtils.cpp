// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ModelUtils.h"
#include "../RenderCore/Assets/ModelScaffold.h"

namespace FixedFunctionModel 
{
    EmbeddedSkeletonPose::EmbeddedSkeletonPose(
        const RenderCore::Assets::ModelScaffold& scaffold, 
        const RenderCore::Assets::TransformationParameterSet* paramSet)
    {
        auto& transMachine = scaffold.EmbeddedSkeleton();
        auto transformCount = transMachine.GetOutputMatrixCount();
        _transforms.resize(transformCount);
        transMachine.GenerateOutputTransforms(
            AsPointer(_transforms.begin()), transformCount,
            paramSet ? paramSet : &transMachine.GetDefaultParameters());

        _binding = RenderCore::Assets::SkeletonBinding(
            transMachine.GetOutputInterface(),
            scaffold.CommandStream().GetInputInterface());
    }

    EmbeddedSkeletonPose::~EmbeddedSkeletonPose() {}

    EmbeddedSkeletonPose::EmbeddedSkeletonPose(EmbeddedSkeletonPose&& moveFrom) never_throws
    : _binding(std::move(moveFrom._binding))
    , _transforms(std::move(moveFrom._transforms))
    {}

    EmbeddedSkeletonPose& EmbeddedSkeletonPose::operator=(EmbeddedSkeletonPose&& moveFrom) never_throws
    {
        _binding = std::move(moveFrom._binding);
        _transforms = std::move(moveFrom._transforms);
        return *this;
    }

}

