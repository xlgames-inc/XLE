// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/IThreadContext_Forward.h"

namespace RenderCore { namespace Techniques { class CameraDesc; class ProjectionDesc; class ParsingContext; } }

namespace SceneEngine
{
    class LightingParserContext;
    class ShadowProjectionDesc;
    class GlobalLightingDesc;
    class LightDesc;
    class ToneMapSettings;
    class PreparedScene;

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
            General,                // general rendering batch
            PreDepth,               // objects that should get a pre-depth pass
            Transparent,            // transparent objects (particularly those that require some object based sorting)
            OITransparent,          // order independent transparent
            TransparentPreDepth,    // pre-depth pass for objects considered "transparent" (ie, opaque parts of transparent objects)
            DMShadows,              // depth map shadows
            RayTracedShadows        // objects enabled for rendering into ray traced shadows
        };

        BatchFilter         _batchFilter;
        Toggles::BitField   _toggles;
        unsigned            _projectionIndex;

        SceneParseSettings(BatchFilter batchFilter, Toggles::BitField toggles=~0u, unsigned projectionIndex=0)
        : _batchFilter(batchFilter), _toggles(toggles), _projectionIndex(projectionIndex) {}
    };

    class ISceneParser
    {
    public:
        virtual auto GetCameraDesc() const -> RenderCore::Techniques::CameraDesc = 0;
        virtual void ExecuteScene(
            RenderCore::IThreadContext& context,
			RenderCore::Techniques::ParsingContext& parserContext,
            LightingParserContext& lightingParserContext, 
            const SceneParseSettings& parseSettings,
            PreparedScene& preparedPackets,
            unsigned techniqueIndex) const = 0;
        virtual void PrepareScene(
            RenderCore::IThreadContext& context, 
			RenderCore::Techniques::ParsingContext& parserContext,
            PreparedScene& preparedPackets) const = 0;
        virtual bool HasContent(const SceneParseSettings& parseSettings) const = 0;

        using ProjectionDesc    = RenderCore::Techniques::ProjectionDesc;
        using ShadowProjIndex   = unsigned;
        using LightIndex        = unsigned;

        virtual ShadowProjIndex GetShadowProjectionCount() const = 0;
        virtual auto            GetShadowProjectionDesc(ShadowProjIndex index, const ProjectionDesc& mainSceneProj) const
            -> ShadowProjectionDesc = 0;

        virtual LightIndex  GetLightCount() const = 0;
        virtual auto        GetLightDesc(LightIndex index) const -> const LightDesc& = 0;
        virtual auto        GetGlobalLightingDesc() const -> GlobalLightingDesc = 0;
        virtual auto        GetToneMapSettings() const -> ToneMapSettings = 0;

        virtual float       GetTimeValue() const = 0;

        virtual ~ISceneParser();
    };

}

