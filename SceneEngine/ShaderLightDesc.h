// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Math/Vector.h"

namespace SceneEngine
{
    namespace ShaderLightDesc
    {
        struct Ambient
        { 
            Float3      _ambientColour; 
            float       _skyReflectionScale; 
            float       _skyReflectionBlurriness; 
            unsigned    _dummy[3];
        };

        struct RangeFog
        {
            Float3      _rangeFogInscatter; unsigned _dummy1;
            Float3      _rangeFogOpticalThickness; unsigned _dummy2;
        };

        struct VolumeFog
        {
            float       _opticalThickness;
            float       _heightStart;
            float       _heightEnd;  unsigned _enableFlag;
            Float3      _sunInscatter; unsigned _dummy1;
            Float3      _ambientInscatter; unsigned _dummy2;
        };

        class Light
        {
        public:
                // Note that this structure is larger than it needs to be
                // for some light types. Only some types need the full 
                // orientation matrix.
                // It seems like we would end up wasting shader constants
                // if we want to store a large number of lights for forward
                // rendering.
            Float3 _position;           float _cutoffRange;
            Float3 _diffuse;            float _sourceRadiusX;
            Float3 _specular;           float _sourceRadiusY;
            Float3 _orientationX;       float _diffuseWideningMin;
            Float3 _orientationY;       float _diffuseWideningMax;
            Float3 _orientationZ;       unsigned _dummy;
        };

        class BasicEnvironment
        {
        public:
            Ambient    _ambient;
            RangeFog   _rangeFog;
            VolumeFog  _volumeFog;
            Light      _dominant[1];
        };
    }
}

