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
    class ISurfaceHeightsProvider;

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ShallowSurface
    {
    public:
        class Config
        {
        public:
            float       _gridPhysicalSize;
            unsigned    _simGridDims;
            unsigned    _simGridCount;
            float       _baseHeight;
            bool        _usePipeModel;

            float       _rainQuantity;
            float       _evaporationConstant;
            float       _pressureConstant;

            Config();
        };

        class LightingConfig
        {
        public:
            unsigned _opticalThicknessColor;
            float _opticalThicknessScalar;
            unsigned _foamColor;
            float _specular;
            float _roughness;
            float _refractiveIndex;
            float _upwellingScale;
            float _skyReflectionScale;

            LightingConfig();
        };

        void RenderDebugging(
            RenderCore::Metal::DeviceContext& metalContext,
            LightingParserContext& parserContext,
            unsigned techniqueIndex,
            unsigned skyProjType, bool refractionsEnable);

        void UpdateSimulation(
            RenderCore::Metal::DeviceContext& metalContext,
            LightingParserContext& parserContext,
            ISurfaceHeightsProvider* surfaceHeights);

        ShallowSurface(
            const Float2 triangleList[], size_t stride,
            size_t ptCount,
            const Config& settings,
            const LightingConfig& lightingSettings);
        ~ShallowSurface();

    protected:
        void MaybeCreateGrid(
            RasterizationSurface& mask,
            Int2 gridCoords);

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
            unsigned techniqueIndex,
            ISurfaceHeightsProvider* surfaceHeights);

        ShallowSurfaceManager();
        ~ShallowSurfaceManager();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

}

