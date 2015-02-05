// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"

namespace RenderCore { class CameraDesc; }

namespace SceneEngine
{
    class LightingParserContext;
    class ProjectionDesc;
    class ShadowProjectionDesc;
    class GlobalLightingDesc;
    class LightDesc;

    class SceneParseSettings
    {
    public:
        struct Toggles
        {
            enum Enum
            {
                Opaque          = 1<<0, 
                Transparent     = 1<<1,
                NonTerrain      = 1<<2,
                Terrain         = 1<<3,
                TerrainLayers   = 1<<4
            };
            typedef unsigned BitField;
        };

        struct BatchFilter
        {
            enum Enum
            {
                General,
                Depth,
                Transparent
            };
        };

        BatchFilter::Enum       _batchFilter;
        Toggles::BitField       _toggles;

        SceneParseSettings(BatchFilter::Enum batchFilter, Toggles::BitField toggles)
            : _batchFilter(batchFilter), _toggles(toggles) {}
    };

    class ISceneParser
    {
    public:
        virtual RenderCore::CameraDesc  GetCameraDesc() const = 0;
        virtual void                    ExecuteScene(   
            RenderCore::Metal::DeviceContext* context, 
            LightingParserContext& parserContext, 
            const SceneParseSettings& parseSettings,
            unsigned techniqueIndex) const = 0;

        virtual unsigned                GetShadowProjectionCount() const = 0;
        virtual ShadowProjectionDesc    GetShadowProjectionDesc(
            unsigned index, const ProjectionDesc& mainSceneProjectionDesc) const = 0;

        virtual void                    ExecuteShadowScene( 
            RenderCore::Metal::DeviceContext* context, 
            LightingParserContext& parserContext, 
            const SceneParseSettings& parseSettings,
            unsigned frustumIndex, unsigned techniqueIndex) const = 0;

        virtual unsigned                GetLightCount() const = 0;
        virtual const LightDesc&        GetLightDesc(unsigned index) const = 0;

        virtual GlobalLightingDesc      GetGlobalLightingDesc() const = 0;

        virtual float                   GetTimeValue() const = 0;
    };

}

