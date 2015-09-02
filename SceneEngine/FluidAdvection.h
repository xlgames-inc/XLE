// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

namespace SceneEngine
{
    enum class AdvectionMethod { ForwardEuler, ForwardEulerDiv, RungeKutta, MacCormackRK4 };
    enum class AdvectionInterpolationMethod { Bilinear, MonotonicCubic };

    class AdvectionSettings
    {
    public:
        AdvectionMethod _method;
        AdvectionInterpolationMethod _interpolation;
        unsigned _subSteps;
    };

    template<typename Field, typename VelField>
        void PerformAdvection(
            Field dstValues, Field srcValues, 
            VelField velFieldT0, VelField velFieldT1,
            float deltaTime, const AdvectionSettings& settings);
}

