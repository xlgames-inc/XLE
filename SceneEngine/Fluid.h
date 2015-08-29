// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"
#include "../Math/Vector.h"
#include <memory>

namespace SceneEngine
{
    class LightingParserContext;

    enum class FluidDebuggingMode
    {
        Density, Velocity, Temperature
    };

    class ReferenceFluidSolver2D
    {
    public:
        struct Settings
        {
            float _deltaTime;
            float _viscosity;
            float _diffusionRate;

            Settings();
        };

        void Tick(const Settings& settings);
        void AddDensity(UInt2 coords, float amount);
        void AddVelocity(UInt2 coords, Float2 vel);

        UInt2 GetDimensions() const;

        void RenderDebugging(
            RenderCore::Metal::DeviceContext& metalContext,
            LightingParserContext& parserContext,
            FluidDebuggingMode debuggingMode = FluidDebuggingMode::Density);

        ReferenceFluidSolver2D(UInt2 dimensions);
        ~ReferenceFluidSolver2D();

    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

    class FluidSolver2D
    {
    public:
        struct Settings
        {
            float _viscosity;
            float _diffusionRate;
            float _tempDiffusion;
            int _diffusionMethod;
            int _advectionMethod;
            unsigned _advectionSteps;
            int _enforceIncompressibilityMethod;
            float _buoyancyAlpha;
            float _buoyancyBeta;
            float _addDensity;
            float _addTemperature;
            float _vorticityConfinement;

            Settings();
        };

        void Tick(float deltaTime, const Settings& settings);
        void AddDensity(UInt2 coords, float amount);
        void AddVelocity(UInt2 coords, Float2 vel);
        void AddTemperature(UInt2 coords, float amount);

        UInt2 GetDimensions() const;

        void RenderDebugging(
            RenderCore::Metal::DeviceContext& metalContext,
            LightingParserContext& parserContext,
            FluidDebuggingMode debuggingMode = FluidDebuggingMode::Density);

        FluidSolver2D(UInt2 dimensions);
        ~FluidSolver2D();

    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };
}

