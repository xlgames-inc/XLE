// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"
#include "../RenderCore/Metal/TextureView.h"
#include "../Math/Vector.h"

namespace Utility { class ParameterBox; }
namespace RenderCore { namespace Techniques { class ParsingContext; } }

namespace SceneEngine
{
    class LightingParserContext;
    class ToneMapSettings;
    class ToneMapLuminanceResult;

    class LuminanceResult
    {
    public:
        using SRV = RenderCore::Metal::ShaderResourceView;
        SRV     _propertiesBuffer;
        SRV     _bloomBuffer;
        bool    _isGood;

        LuminanceResult();
        LuminanceResult(const SRV& propertiesBuffer, const SRV& bloomBuffer, bool isGood);
        ~LuminanceResult();
        LuminanceResult& operator=(LuminanceResult&&) never_throws;
        LuminanceResult(LuminanceResult&&) never_throws;

        LuminanceResult& operator=(const LuminanceResult&) = delete;
        LuminanceResult(const LuminanceResult&) = delete;
    };

    LuminanceResult ToneMap_SampleLuminance(
        RenderCore::Metal::DeviceContext& context, 
        LightingParserContext& parserContext,
        const ToneMapSettings& settings,
        const RenderCore::Metal::ShaderResourceView& inputResource,
        bool doAdapt = true);

    LuminanceResult ToneMap_SampleLuminance(
        RenderCore::Metal::DeviceContext& context, 
        RenderCore::Techniques::ParsingContext& parserContext,
        const ToneMapSettings& settings,
        const RenderCore::Metal::ShaderResourceView& inputResource,
        bool doAdapt = true);

    void ToneMap_Execute(
        RenderCore::Metal::DeviceContext& context, 
        RenderCore::Techniques::ParsingContext& parserContext, 
        const LuminanceResult& luminanceResult,
        const ToneMapSettings& settings,
        const RenderCore::Metal::ShaderResourceView& inputResource);

    class AtmosphereBlurSettings
    {
    public:
        float _blurStdDev;
        float _startDistance;
        float _endDistance;
    };

    void AtmosphereBlur_Execute(
        RenderCore::Metal::DeviceContext& context, LightingParserContext& parserContext,
        const AtmosphereBlurSettings& settings);

    class ToneMapSettings 
    { 
    public:
        struct Flags
        {
            enum Enum { EnableToneMap = 1<<0, EnableBloom = 1<<1 };
            typedef unsigned BitField;
        };
        Flags::BitField _flags;
        Float3  _bloomColor;
        float   _bloomBrightness;
        float   _bloomThreshold;
        float   _bloomRampingFactor;
        float   _bloomDesaturationFactor;
        float   _sceneKey;
	    float   _luminanceMin;
	    float   _luminanceMax;
	    float   _whitepoint;
        float   _bloomBlurStdDev;

        ToneMapSettings();
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
}
