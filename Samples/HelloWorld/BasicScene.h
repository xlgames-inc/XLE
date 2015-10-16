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
        typedef SceneEngine::ToneMapSettings        ToneMapSettings;

        RenderCore::Techniques::CameraDesc GetCameraDesc() const;

        void ExecuteScene(
            RenderCore::Metal::DeviceContext* context, 
            LightingParserContext& parserContext, 
            const SceneParseSettings& parseSettings,
            unsigned techniqueIndex) const;
        bool HasContent(const SceneParseSettings& parseSettings) const;

        unsigned GetShadowProjectionCount() const;
        ShadowProjectionDesc GetShadowProjectionDesc(unsigned index, const RenderCore::Techniques::ProjectionDesc& mainSceneProjectionDesc) const;
        unsigned GetLightCount() const;
        const LightDesc& GetLightDesc(unsigned index) const;

        GlobalLightingDesc GetGlobalLightingDesc() const;
        ToneMapSettings GetToneMapSettings() const;
        float GetTimeValue() const;

        BasicSceneParser();
        ~BasicSceneParser();

    protected:
        class Model;
        std::unique_ptr<Model> _model;
        float _time;
    };

}

