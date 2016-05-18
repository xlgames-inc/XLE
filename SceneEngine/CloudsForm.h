// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Fluid.h"  // only for FluidDebuggingMode
#include "../RenderCore/Metal/Forward.h"
#include "../RenderCore/IThreadContext_Forward.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../Math/Vector.h"

namespace RenderCore { namespace Techniques { class ProjectionDesc; }}

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

                // inputs
            float       _crossWindSpeed;
            float       _inputVapor;
            float       _inputTemperature;
            float       _inputUpdraft;

                // condensation
            float       _condensationSpeed;
            float       _temperatureChangeSpeed;
            float       _evaporateThreshold;

                // environment
            float       _airTemperature;    // Kelvin
            float       _relativeHumidity;
            float       _altitudeMin;       // KM
            float       _altitudeMax;       // KM
            float       _lapseRate;         // Kelvin/KM
            int         _obstructionType;

            Settings();
        };

        void Tick(float deltaTime, const Settings& settings);
        void AddVapor(UInt2 coords, float amount);
        void AddVelocity(UInt2 coords, Float2 vel);
        void OnMouseMove(Float2 coords);

        UInt2 GetDimensions() const;

        void RenderDebugging(
            RenderCore::Metal::DeviceContext& metalContext,
            LightingParserContext& parserContext,
            FluidDebuggingMode debuggingMode = FluidDebuggingMode::Density);

        void RenderWidgets(
            RenderCore::IThreadContext& context, 
            RenderCore::Techniques::ParsingContext& parserContext);

        CloudsForm2D(UInt2 dimensions);
        ~CloudsForm2D();

    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        void OldTick(float deltaTime, const Settings& settings);
    };
}


