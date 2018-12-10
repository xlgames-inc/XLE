// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../SceneEngine/LightingParser.h"
#include "../../SceneEngine/SceneParser.h"
#include <memory>

namespace SceneEngine { class ILightingParserDelegate; }

namespace Sample
{
    class BasicSceneParser : public SceneEngine::IScene
    {
    public:
		void ExecuteScene(
            RenderCore::IThreadContext& context, 
            SceneEngine::SceneExecuteContext& executeContext) const;

		BasicSceneParser();
		~BasicSceneParser();
	protected:
		class Model;
        std::unique_ptr<Model> _model;
	};


	class SampleLightingDelegate : public SceneEngine::ILightingParserDelegate
	{
	public:
        unsigned GetShadowProjectionCount() const override;
        SceneEngine::ShadowProjectionDesc GetShadowProjectionDesc(
			unsigned index, 
			const RenderCore::Techniques::ProjectionDesc& mainSceneProjectionDesc) const override;

        unsigned GetLightCount() const override;
        const SceneEngine::LightDesc& GetLightDesc(unsigned index) const override;

        SceneEngine::GlobalLightingDesc GetGlobalLightingDesc() const override;
        SceneEngine::ToneMapSettings GetToneMapSettings() const override;
        float GetTimeValue() const override;

		void Update(float deltaTime);

        SampleLightingDelegate();
        ~SampleLightingDelegate();

    protected:
        float _time;
    };

	RenderCore::Techniques::CameraDesc CalculateCameraDesc(Float2 rotations, float zoomFactor, float time);

}

