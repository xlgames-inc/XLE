// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Assets/ModelImmutableData.h"
#include "../Math/Matrix.h"

namespace RenderCore { namespace Assets 
{
	class ModelScaffold;
	class SkeletonBinding;
	class TransformationParameterSet;
}}

namespace FixedFunctionModel
{
	class PreparedAnimation;

	class MeshToModel
    {
    public:
        const Float4x4*         _skeletonOutput;
        unsigned                _skeletonOutputCount;
        const RenderCore::Assets::SkeletonBinding*  _skeletonBinding;

        Float4x4    GetMeshToModel(unsigned transformMarker) const;
        bool        IsGood() const { return _skeletonOutput != nullptr; }

        MeshToModel();
        MeshToModel(const Float4x4 skeletonOutput[], unsigned skeletonOutputCount,
                    const RenderCore::Assets::SkeletonBinding* binding = nullptr);
        MeshToModel(const PreparedAnimation& preparedAnim, const RenderCore::Assets::SkeletonBinding* binding = nullptr);
        MeshToModel(const RenderCore::Assets::ModelScaffold&);
    };

    class EmbeddedSkeletonPose
    {
    public:
        RenderCore::Assets::SkeletonBinding         _binding;
        std::vector<Float4x4>   _transforms;

        MeshToModel GetMeshToModel()
        {
            return MeshToModel(
                AsPointer(_transforms.cbegin()), 
                (unsigned)_transforms.size(), &_binding);
        }

        EmbeddedSkeletonPose(
            const RenderCore::Assets::ModelScaffold& scaffold, 
            const RenderCore::Assets::TransformationParameterSet* paramSet = nullptr);
        ~EmbeddedSkeletonPose();
        EmbeddedSkeletonPose(EmbeddedSkeletonPose&& moveFrom) never_throws;
        EmbeddedSkeletonPose& operator=(EmbeddedSkeletonPose&& moveFrom) never_throws;
    };
}

