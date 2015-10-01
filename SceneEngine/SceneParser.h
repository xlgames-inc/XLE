// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"

namespace RenderCore { namespace Techniques { class CameraDesc; class ProjectionDesc; } }

namespace SceneEngine
{
    class LightingParserContext;
    class ShadowProjectionDesc;
    class GlobalLightingDesc;
    class LightDesc;
    class ToneMapSettings;

    class SceneParseSettings
    {
    public:
        struct Toggles
        {
            enum Enum
            {
                NonTerrain  = 1<<0,
                Terrain     = 1<<1
            };
            typedef unsigned BitField;
        };

        enum class BatchFilter
        {
            General,            // general rendering batch
            PreDepth,           // objects that should get a pre-depth pass
            Transparent,        // transparent objects (particularly those that require some object based sorting)
            OITransparent,      // order independent transparent
            RayTracedShadows    // objects enabled for rendering into ray traced shadows
        };

        BatchFilter         _batchFilter;
        Toggles::BitField   _toggles;

        SceneParseSettings(BatchFilter batchFilter, Toggles::BitField toggles)
            : _batchFilter(batchFilter), _toggles(toggles) {}
    };

    class ISceneParser
    {
    public:
        virtual RenderCore::Techniques::CameraDesc  GetCameraDesc() const = 0;
        virtual void                    ExecuteScene(   
            RenderCore::Metal::DeviceContext* context, 
            LightingParserContext& parserContext, 
            const SceneParseSettings& parseSettings,
            unsigned techniqueIndex) const = 0;

        virtual unsigned                GetShadowProjectionCount() const = 0;
        virtual ShadowProjectionDesc    GetShadowProjectionDesc(
            unsigned index, const RenderCore::Techniques::ProjectionDesc& mainSceneProjectionDesc) const = 0;

        virtual void                    ExecuteShadowScene( 
            RenderCore::Metal::DeviceContext* context, 
            LightingParserContext& parserContext, 
            const SceneParseSettings& parseSettings,
            unsigned frustumIndex, unsigned techniqueIndex) const = 0;

        virtual unsigned                GetLightCount() const = 0;
        virtual const LightDesc&        GetLightDesc(unsigned index) const = 0;

        virtual GlobalLightingDesc      GetGlobalLightingDesc() const = 0;
        virtual ToneMapSettings         GetToneMapSettings() const = 0;

        virtual float                   GetTimeValue() const = 0;

        virtual ~ISceneParser();
    };

}

