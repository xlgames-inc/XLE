// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "LightingParserContext.h"
#include "LightDesc.h"

namespace SceneEngine
{
    void VolumetricFog_Build(       RenderCore::Metal::DeviceContext* context, 
                                    LightingParserContext& lightingParserContext,
                                    bool useMsaaSamplers, 
                                    ProcessedShadowFrustum& shadowFrustum);

    void VolumetricFog_Resolve(     RenderCore::Metal::DeviceContext* context, 
                                    LightingParserContext& lightingParserContext,
                                    unsigned samplingCount, bool useMsaaSamplers, bool flipDirection,
                                    ProcessedShadowFrustum& shadowFrustum);


    class VolumetricFogMaterial
    {
    public:
        float _density;
        float _noiseDensityScale;       // set to 0. to disable this feature
        float _noiseSpeed;

        float _heightStart;
        float _heightEnd;

        Float3 _forwardColour;
        Float3 _backColour;

        float _forwardBrightness;
        float _backBrightness;
        
        float _ESM_C;
        float _shadowsBias;
        float _jitteringAmount;
    };

    extern VolumetricFogMaterial GlobalVolumetricFogMaterial;
}

