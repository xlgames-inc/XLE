// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TestPlatformScene.h"
#include "../Shared/SampleGlobals.h"
#include "../../PlatformRig/PlatformRigUtil.h"
#include "../../SceneEngine/LightDesc.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"

#include "../../Tools/ToolsRig/VisualisationUtils.h"

#include "../../ConsoleRig/Console.h"
#include "../../Utility/Profiling/CPUProfiler.h"

namespace Sample
{
    class TestPlatformSceneParser::Pimpl
    {
    public:
        std::shared_ptr<ToolsRig::VisCameraSettings> _camera;
        PlatformRig::EnvironmentSettings _envSettings;

        float _time;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    void TestPlatformSceneParser::PrepareFrame(RenderCore::IThreadContext& context)
    {
    }

    void TestPlatformSceneParser::ExecuteScene(   
        RenderCore::Metal::DeviceContext* context, 
        LightingParserContext& parserContext, 
        const SceneParseSettings& parseSettings,
        unsigned techniqueIndex) const
    {
        CPUProfileEvent pEvnt("ExecuteScene", g_cpuProfiler);

        if (    parseSettings._batchFilter == SceneParseSettings::BatchFilter::General
            ||  parseSettings._batchFilter == SceneParseSettings::BatchFilter::Depth) {

        }
    }

    void TestPlatformSceneParser::ExecuteShadowScene( 
        RenderCore::Metal::DeviceContext* context, 
        LightingParserContext& parserContext, 
        const SceneParseSettings& parseSettings,
        unsigned frustumIndex, unsigned techniqueIndex) const 
    {
        CPUProfileEvent pEvnt("ExecuteShadowScene", g_cpuProfiler);

        if (Tweakable("DoShadows", true)) {
            SceneParseSettings settings = parseSettings;
            settings._toggles &= ~SceneParseSettings::Toggles::Terrain;
            ExecuteScene(context, parserContext, settings, techniqueIndex);
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    RenderCore::Techniques::CameraDesc TestPlatformSceneParser::GetCameraDesc() const  {  return ToolsRig::AsCameraDesc(*_pimpl->_camera); }
    float TestPlatformSceneParser::GetTimeValue() const {  return _pimpl->_time; }
    std::shared_ptr<ToolsRig::VisCameraSettings> TestPlatformSceneParser::GetCameraPtr() { return _pimpl->_camera; }
    void TestPlatformSceneParser::Update(float deltaTime) { _pimpl->_time += deltaTime;  }
    const PlatformRig::EnvironmentSettings& TestPlatformSceneParser::GetEnvSettings() const { return _pimpl->_envSettings; }

    TestPlatformSceneParser::TestPlatformSceneParser()
    {
        auto pimpl = std::make_unique<Pimpl>();
        pimpl->_time = 0.f;

        pimpl->_camera = std::make_shared<ToolsRig::VisCameraSettings>();
        pimpl->_camera->_nearClip = 0.5f;
        pimpl->_camera->_farClip = 6000.f;
        pimpl->_camera->_position = Float3(-10.f, 0.f, 0.f);

        pimpl->_envSettings = PlatformRig::DefaultEnvironmentSettings();

        _pimpl = std::move(pimpl);
    }

    TestPlatformSceneParser::~TestPlatformSceneParser()
    {}


}

