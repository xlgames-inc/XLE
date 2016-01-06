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
#include "../../Assets/CompilerHelper.h"
#include "../../Assets/IntermediateAssets.h"
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
            float       _powerExaggerate;       // power applied to the final value to exaggerate the effect
            float       _duplicatesThreshold;   // typically around 1cm; vertices within this threshold are combined
            float       _samplePushOut;         // sample points are pushed out along the normal this distance

            Desc(   float minDistance, float maxDistance, 
                    unsigned renderRes, 
                    float powerExaggerate,
                    float duplicatesThreshold,
                    float samplePushOut)
                : _minDistance(minDistance), _maxDistance(maxDistance)
                , _renderResolution(renderRes), _powerExaggerate(powerExaggerate)
                , _duplicatesThreshold(duplicatesThreshold), _samplePushOut(samplePushOut) {}
            Desc() {}
        };

        const Desc& GetSettings() const;

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

    /// <summary>Compiler type for generating per-vertex AO supplement</summary>
    class AOSupplementCompiler : public ::Assets::IntermediateAssets::IAssetCompiler, public std::enable_shared_from_this<AOSupplementCompiler>
    {
    public:
        std::shared_ptr<::Assets::ICompileMarker> PrepareAsset(
            uint64 typeCode, 
            const ::Assets::ResChar* initializers[], unsigned initializerCount,
            const ::Assets::IntermediateAssets::Store& destinationStore);
        void StallOnPendingOperations(bool cancelAll);

            // When using with placements, this hash value is referenced by the
            // "supplements" string in data. If the hash value changes, the data
            // must also change.
        static const uint64 CompilerType = ConstHash64<'PER_', 'VERT', 'EX_A', 'O'>::Value;

        AOSupplementCompiler(std::shared_ptr<RenderCore::IThreadContext> threadContext);
        ~AOSupplementCompiler();
    protected:
        class Pimpl;
        std::shared_ptr<Pimpl> _pimpl;
        class PollingOp;

        class Marker;
    };

}

