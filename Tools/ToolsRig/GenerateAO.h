// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Math/Vector.h"
#include "../../RenderCore/Metal/Forward.h"

namespace RenderCore { namespace Assets { class ModelRenderer; class SharedStateSet; } }

namespace ToolsRig
{
    class AoGen
    {
    public:
        float CalculateSkyDomeOcclusion(
            RenderCore::Metal::DeviceContext& devContext,
            const RenderCore::Assets::ModelRenderer& renderer, 
            RenderCore::Assets::SharedStateSet& sharedStates, 
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
        
        AoGen(const Desc& settings);
        ~AoGen();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    }; 
}

