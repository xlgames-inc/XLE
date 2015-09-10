// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Fluid.h"  // only for FluidDebuggingMode
#include "../RenderCore/Metal/Forward.h"
#include "../Math/Vector.h"

namespace SceneEngine
{
    class LightingParserContext;

    class CloudsForm2D
    {
    public:
        struct Settings
        {
                // diffusion
            float       _viscosity;
            float       _condensedDiffusionRate;
            float       _vaporDiffusionRate;
            float       _temperatureDiffusionRate;
            int         _diffusionMethod;

                // advection
            int         _advectionMethod;
            unsigned    _advectionSteps;
            int         _interpolationMethod;

                // forces
            int         _enforceIncompressibilityMethod;
            float       _vorticityConfinement;
            float       _buoyancyAlpha;
            float       _buoyancyBeta;

                // condensation
            float       _condensationSpeed;
            float       _temperatureChangeSpeed;

            Settings();
        };

        void Tick(float deltaTime, const Settings& settings);
        void AddVapor(UInt2 coords, float amount);
        void AddVelocity(UInt2 coords, Float2 vel);

        UInt2 GetDimensions() const;

        void RenderDebugging(
            RenderCore::Metal::DeviceContext& metalContext,
            LightingParserContext& parserContext,
            FluidDebuggingMode debuggingMode = FluidDebuggingMode::Density);

        CloudsForm2D(UInt2 dimensions);
        ~CloudsForm2D();

    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };
}


