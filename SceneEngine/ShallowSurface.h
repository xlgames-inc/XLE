// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "LightingParserContext.h"
#include "../RenderCore/Metal/Forward.h"
#include "../Math/Vector.h"
#include <vector>

namespace SceneEngine
{
    class RasterizationSurface;

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ShallowSurface
    {
    public:
        class Config
        {
        public:
            float _cellPhysicalSize;
            unsigned _simGridDims;
        };

        void RenderDebugging(
            RenderCore::Metal::DeviceContext& metalContext,
            LightingParserContext& parserContext,
            unsigned techniqueIndex);

        ShallowSurface(
            const Float2 triangleList[], size_t stride,
            size_t ptCount,
            const Config& settings);
        ~ShallowSurface();

    protected:
        void MaybeCreateGrid(
            RasterizationSurface& mask,
            Int2 mins, Int2 maxs,
            Float2 physicalMins, Float2 physicalMaxs,
            float physicalHeight);

        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ShallowSurfaceManager
    {
    public:
        void Add(std::shared_ptr<ShallowSurface> surface);
        void Clear();

        void RenderDebugging(
            RenderCore::Metal::DeviceContext& metalContext,
            LightingParserContext& parserContext,
            unsigned techniqueIndex);

        ShallowSurfaceManager();
        ~ShallowSurfaceManager();
    protected:
        std::vector<std::shared_ptr<ShallowSurface>> _surfaces;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

}

