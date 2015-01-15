// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"
#include "../Math/Vector.h"

namespace SceneEngine
{
    class LightingParserContext;

    void ToneMap_SampleLuminance(
        RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext, 
        RenderCore::Metal::ShaderResourceView& inputResource, int sampleCount);

    void ToneMap_Execute(
        RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext, 
        RenderCore::Metal::ShaderResourceView& inputResource, int sampleCount);

    void AtmosphereBlur_Execute(
        RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext);

    class ToneMapSettings 
    { 
    public:
        Float3  _bloomScale;
        float   _bloomThreshold;
        float   _bloomRampingFactor;
        float   _bloomDesaturationFactor;
        float   _sceneKey;
	    float   _luminanceMin;
	    float   _luminanceMax;
	    float   _whitepoint;
        float   _dummy[2];
    };

    class ColorGradingSettings
    {
    public:
        float _sharpenAmount;
        float _minInput;
        float _gammaInput;
        float _maxInput;
        float _minOutput;
        float _maxOutput;
        float _brightness;
        float _contrast;
        float _saturation;
        Float3 _filterColor;
        float _filterColorDensity;
        float _grain;
        Float4 _selectiveColor;
        float _selectiveColorCyans;
        float _selectiveColorMagentas;
        float _selectiveColorYellows;
        float _selectiveColorBlacks;
    };

    extern ToneMapSettings      GlobalToneMapSettings;
    extern ColorGradingSettings GlobalColorGradingSettings;
}
