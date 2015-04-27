// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "PlatformRigUtil.h"
#include "../SceneEngine/LightDesc.h"
#include "../SceneEngine/SceneParser.h"

namespace PlatformRig
{
    /// <summary>Describes a lighting environment</summary>
    /// This contains all of the settings and properties required
    /// for constructing a basic lighting environment.
    /// This can be used to implement the ISceneParser functions that
    /// return lighting settings (like ISceneParser::GetLightDesc() and ISceneParser::GetGlobalLightingDesc())
    class EnvironmentSettings
    {
    public:
        std::vector<SceneEngine::LightDesc> _lights;
        SceneEngine::GlobalLightingDesc _globalLightingDesc;

        class ShadowProj
        {
        public:
            SceneEngine::LightDesc _light;
            DefaultShadowFrustumSettings _shadowFrustumSettings;
        };
        std::vector<ShadowProj> _shadowProj;
    };

    /// <summary>Simple & partial implementation of the ISceneParser interface<summary>
    /// This provides implementations of the basic lighting related interfaces of
    /// ISceneParser that will hook into an EnvironmentSettings object.
    /// Derived classes should implement the accessor GetEnvSettings().
    class BasicSceneParser : public SceneEngine::ISceneParser
    {
    public:
        unsigned GetShadowProjectionCount() const;
        SceneEngine::ShadowProjectionDesc GetShadowProjectionDesc(
            unsigned index, 
            const RenderCore::Techniques::ProjectionDesc& mainSceneProjectionDesc) const;

        unsigned                            GetLightCount() const;
        const SceneEngine::LightDesc&       GetLightDesc(unsigned index) const;
        SceneEngine::GlobalLightingDesc     GetGlobalLightingDesc() const;

    protected:
        virtual const EnvironmentSettings&  GetEnvSettings() const = 0;
    };

    SceneEngine::LightDesc DefaultDominantLight();
    SceneEngine::GlobalLightingDesc DefaultGlobalLightingDesc();
    EnvironmentSettings DefaultEnvironmentSettings();
}

