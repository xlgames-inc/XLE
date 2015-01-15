// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"

namespace SceneEngine
{
    /// <summary>Interface to access surface height values for shader</summary>
    /// Some shaders need to know the height of the terrain surface. This interface
    /// can provide a way to request surface height values.
    class ISurfaceHeightsProvider
    {
    public:
        class Addressing
        {
        public:
            bool    _valid;

            Int3    _baseCoordinate;
            Int2    _minCoordOffset, _maxCoordOffset;
            float   _heightScale, _heightOffset;
        };

        typedef RenderCore::Metal::ShaderResourceView SRV;
        virtual SRV         GetSRV() = 0;
        virtual Addressing  GetAddress(Float2 minCoord, Float2 maxCoord) = 0;
        virtual bool        IsFloatFormat() const = 0;
    };
}

