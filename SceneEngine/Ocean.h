// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCOre/Metal/DeviceContext.h"
#include "../Math/Matrix.h"

namespace SceneEngine
{
    class LightingParserContext;
    void Ocean_Execute( RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext, 
                        RenderCore::Metal::ShaderResourceView& depthBufferSRV);

    void FFT_DoDebugging(RenderCore::Metal::DeviceContext* context);

    class OceanSettings
    {
    public:
        float       _windAngle[2];
        float       _windVelocity[2];
        float       _physicalDimensions;
        unsigned    _gridDimensions;
        float       _strengthConstantXY;
        float       _strengthConstantZ;
        float       _detailNormalsStrength;
        float       _spectrumFade;
        float       _scaleAgainstWind[2];
        float       _suppressionFactor[2];
        float       _gridShiftSpeed;
        float       _baseHeight;

        OceanSettings();
    };

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
        float       _dummy[2];

        OceanLightingSettings();
    };

    extern OceanSettings GlobalOceanSettings;
    extern OceanLightingSettings GlobalOceanLightingSettings;

    extern RenderCore::Metal::ShaderResourceView OceanReflectionResource;
    extern Float4x4 OceanWorldToReflection;

    class OceanMaterialConstants
    {
    public:
        float _physicalWidth, _physicalHeight;
        float _strengthConstantXY;
        float _strengthConstantZ;
        float _shallowGridPhysicalDimension;
        float _baseHeight;
        float _dummy[2];
    };

    OceanMaterialConstants BuildOceanMaterialConstants(
        const OceanSettings& oceanSettings, float shallowGridPhysicalDimension);
}

