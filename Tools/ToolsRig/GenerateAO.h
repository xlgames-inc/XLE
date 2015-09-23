// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Math/Vector.h"
#include "../../RenderCore/IThreadContext_Forward.h"
#include "../../RenderCore/Metal/Forward.h"
#include "../../Assets/AssetsCore.h"
#include <memory>

namespace RenderCore { namespace Assets 
{ 
    class ModelRenderer; class SharedStateSet; 
    class MeshToModel; class ModelScaffold; class MaterialScaffold; 
}}
namespace Assets { class DependencyValidation; class DirectorySearchRules; }

namespace ToolsRig
{
    /// <summary>Generates pre-calculated AO information for a model</summary>
    /// Actually, more specifically this can be used for calculating the fraction
    /// of the sky dome that is internally occluded by a static model. This can be
    /// used for blocking direct light from the skydome (which should be similar 
    /// to blocking indirect light bounced off nearby sources).
    class AoGen
    {
    public:
        float CalculateSkyDomeOcclusion(
            RenderCore::IThreadContext& threadContext,
            const RenderCore::Assets::ModelRenderer& renderer, 
            RenderCore::Assets::SharedStateSet& sharedStates, 
            const RenderCore::Assets::MeshToModel& meshToModel,
            const Float3& samplePoint);

        class Desc
        {
        public:
            float       _minDistance, _maxDistance;
            unsigned    _renderResolution;

            Desc(float minDistance, float maxDistance, unsigned renderRes)
                : _minDistance(minDistance), _maxDistance(maxDistance)
                , _renderResolution(renderRes) {}
            Desc() {}
        };

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const;
        
        AoGen(const Desc& settings);
        ~AoGen();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

    void CalculateVertexAO(
        RenderCore::IThreadContext& threadContext,
        const ::Assets::ResChar destinationFile[],
        AoGen& gen,
        const RenderCore::Assets::ModelScaffold& model,
        const RenderCore::Assets::MaterialScaffold& material,
        const ::Assets::DirectorySearchRules* searchRules);

}

