// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderCore/IThreadContext_Forward.h"
#include "../../SceneEngine/SceneParser.h"
#include <memory>

namespace SceneEngine { class TerrainManager; class PlacementsManager; }

namespace Sample
{
    class PlayerCharacter;
    class EnvironmentSceneParser : public SceneEngine::ISceneParser
    {
    public:
        void PrepareFrame(RenderCore::IThreadContext& context);
        void Update(float deltaTime);

        typedef SceneEngine::ShadowProjectionDesc   ShadowProjectionDesc;
        typedef SceneEngine::LightingParserContext  LightingParserContext;
        typedef SceneEngine::SceneParseSettings     SceneParseSettings;
        typedef SceneEngine::LightDesc              LightDesc;
        typedef SceneEngine::GlobalLightingDesc     GlobalLightingDesc;

        RenderCore::Techniques::CameraDesc GetCameraDesc() const;

        void ExecuteScene(   
            RenderCore::Metal::DeviceContext* context, 
            LightingParserContext& parserContext, 
            const SceneParseSettings& parseSettings,
            unsigned techniqueIndex) const;

        unsigned GetShadowProjectionCount() const;
        ShadowProjectionDesc GetShadowProjectionDesc(
            unsigned index, const RenderCore::Techniques::ProjectionDesc& mainSceneProjectionDesc) const;
        void ExecuteShadowScene( 
            RenderCore::Metal::DeviceContext* context, 
            LightingParserContext& parserContext, 
            const SceneParseSettings& parseSettings,
            unsigned frustumIndex, unsigned techniqueIndex) const;

        unsigned GetLightCount() const;
        const LightDesc& GetLightDesc(unsigned index) const;

        GlobalLightingDesc GetGlobalLightingDesc() const;
        float GetTimeValue() const;

        std::shared_ptr<PlayerCharacter> GetPlayerCharacter();
        std::shared_ptr<SceneEngine::TerrainManager> GetTerrainManager();
        std::shared_ptr<SceneEngine::PlacementsManager> GetPlacementManager();
        std::shared_ptr<RenderCore::Techniques::CameraDesc> GetCameraPtr();

        EnvironmentSceneParser();
        ~EnvironmentSceneParser();

    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

}

