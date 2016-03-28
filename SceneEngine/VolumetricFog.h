// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "LightingParserContext.h"
#include "LightingParser.h"
#include "LightDesc.h"
#include "../Math/Vector.h"

namespace Utility { class ParameterBox; }

namespace SceneEngine
{
    class VolumetricFogMaterial
    {
    public:
        float _opticalThickness;
        float _noiseThicknessScale;       // set to 0. to disable this feature
        float _noiseSpeed;

        Float3 _sunInscatter;
        Float3 _ambientInscatter;

        float _ESM_C;
        float _shadowsBias;
        float _jitteringAmount;
    };

    VolumetricFogMaterial VolumetricFogMaterial_Default();

    class VolumetricFogConfig
    {
    public:
        class FogVolume
        {
        public:
            VolumetricFogMaterial _material;

            float _heightStart;
            float _heightEnd;
            Float3 _center;
            float _radius;

            FogVolume();
            FogVolume(const ParameterBox& params);
        };

        class Renderer
        {
        public:
            unsigned _blurredShadowSize;
            unsigned _shadowDownsample;
            unsigned _skipShadowFrustums;
            unsigned _maxShadowFrustums;
            UInt3 _gridDimensions;
            float _worldSpaceGridDepth;
            bool _enable;

            Renderer();
            Renderer(const ParameterBox& params);
        };

        std::vector<FogVolume> _volumes;
        Renderer _renderer;
    };

    class VolumetricFogManager
    {
    public:
        std::shared_ptr<ILightingParserPlugin> GetParserPlugin();

        void Load(const VolumetricFogConfig& cfg);
        void AddVolume(const VolumetricFogConfig::FogVolume& volume);

        VolumetricFogManager();
        ~VolumetricFogManager();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
        friend class VolumetricFogPlugin;
    };
}

