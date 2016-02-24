// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

namespace SceneEngine
{
    class GradientFlagsSettings
    {
    public:
        bool    _enable;
        float   _elementSpacing;
        float   _slopeThresholds[3];

        explicit GradientFlagsSettings(
            bool enable = false,
            float elementSpacing = 2.f,
            float slope0Threshold = .08f,
            float slope1Threshold = 0.4f,
            float slope2Threshold = 1.75f);
    };
}

