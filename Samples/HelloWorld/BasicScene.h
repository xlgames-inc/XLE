// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../SceneEngine/SceneParser.h"
#include <memory>

namespace RenderCore { namespace Assets { class ModelRenderer; class SharedStateSet; } }

namespace Sample
{

    class BasicSceneParser : public SceneEngine::ISceneParser
    {
    public:
        void PrepareFrame(RenderCore::Metal::DeviceContext* context);
        void Update(float deltaTime);

        typedef SceneEngine::ShadowProjectionDesc   ShadowProjectionDesc;
        typedef SceneEngine::LightingParserContext  LightingParserContext;
        typedef SceneEngine::SceneParseSettings     SceneParseSettings;
        typedef SceneEngine::LightDesc              LightDesc;
        typedef SceneEngine::GlobalLightingDesc     GlobalLightingDesc;
        typedef SceneEngine::ProjectionDesc         ProjectionDesc;

        RenderCore::CameraDesc GetCameraDesc() const;

        void ExecuteScene(
            RenderCore::Metal::DeviceContext* context, 
            LightingParserContext& parserContext, 
            const SceneParseSettings& parseSettings,
            unsigned techniqueIndex) const;

        unsigned GetShadowProjectionCount() const;
        ShadowProjectionDesc GetShadowProjectionDesc(unsigned index, const ProjectionDesc& mainSceneProjectionDesc) const;
        void ExecuteShadowScene( 
            RenderCore::Metal::DeviceContext* context, 
            LightingParserContext& parserContext, 
            const SceneParseSettings& parseSettings,
            unsigned frustumIndex, unsigned techniqueIndex) const;

        unsigned GetLightCount() const;
        const LightDesc& GetLightDesc(unsigned index) const;

        GlobalLightingDesc GetGlobalLightingDesc() const;
        float GetTimeValue() const;

        BasicSceneParser();
        ~BasicSceneParser();

    protected:
        class Model;
        std::unique_ptr<Model> _model;
        float _time;
    };

}

