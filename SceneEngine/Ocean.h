// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/DeviceContext.h"
#include "../Math/Matrix.h"

namespace SceneEngine
{
    class LightingParserContext;
    class DeepOceanSimSettings;
    class OceanLightingSettings;

    /// Entry point for ocean rendering
    /// Draws the surface of the ocean, according to the given settings.
    void Ocean_Execute(
        RenderCore::Metal::DeviceContext* context,
        LightingParserContext& parserContext,
        const DeepOceanSimSettings& settings,
        const OceanLightingSettings& lightingSettings,
        RenderCore::Metal::ShaderResourceView& depthBufferSRV);

    void FFT_DoDebugging(RenderCore::Metal::DeviceContext* context);

    class OceanLightingSettings
    {
    public:
        Float3      _specularReflectionBrightness;
        float       _foamBrightness;
        Float3      _opticalThickness;
        float       _skyReflectionBrightness;

        float       _specularPower;
        float       _upwellingScale;
        float       _refractiveIndex;
        float       _reflectionBumpScale;

        float       _detailNormalFrequency;
        float       _specularityFrequency;
        float       _matSpecularMin, _matSpecularMax;

        float       _matRoughness;
        unsigned    _dummy[3];

        OceanLightingSettings();
    };

    extern RenderCore::Metal::ShaderResourceView OceanReflectionResource;
    extern Float4x4 OceanWorldToReflection;
}

