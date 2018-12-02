// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"
#include "../Math/Matrix.h"

namespace Utility { class ParameterBox; }
namespace RenderCore { namespace Techniques { class ParsingContext; }}
namespace RenderCore { class IThreadContext; }

namespace SceneEngine
{
    class DeepOceanSimSettings;
    class OceanLightingSettings;

    /// Entry point for ocean rendering
    /// Draws the surface of the ocean, according to the given settings.
    void Ocean_Execute(
        RenderCore::IThreadContext& threadContext,
		RenderCore::Techniques::ParsingContext& parserContext,
        ISceneParser& sceneParser,
        const DeepOceanSimSettings& settings,
        const OceanLightingSettings& lightingSettings,
        const RenderCore::Metal::ShaderResourceView& depthBufferSRV);

    void FFT_DoDebugging(RenderCore::Metal::DeviceContext* context);

    class OceanLightingSettings
    {
    public:
        Float3      _opticalThickness;
        float       _foamBrightness;
        float       _skyReflectionBrightness;

        float       _upwellingScale;
        float       _refractiveIndex;
        float       _reflectionBumpScale;

        float       _detailNormalFrequency;
        float       _specularityFrequency;
        float       _matSpecularMin, _matSpecularMax;

        float       _matRoughness;
        unsigned    _dummy[3];

        OceanLightingSettings();
        OceanLightingSettings(const Utility::ParameterBox& params);
    };

    extern RenderCore::Metal::ShaderResourceView OceanReflectionResource;
    extern Float4x4 OceanWorldToReflection;

    class WaterNoiseTexture
    {
    public:
        class Desc
        {
        public:
            float _hgrid, _gain, _lacunarity;
            unsigned _octaves;
            Desc(float hgrid, float gain, float lacunarity, unsigned octaves);
            Desc();
        };

        std::unique_ptr<RenderCore::Metal::ShaderResourceView> _srv;

        WaterNoiseTexture(const Desc& desc);
    };
}

