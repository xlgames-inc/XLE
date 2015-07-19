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
#include "../../SceneEngine/DualContour.h"
#include "../../SceneEngine/DualContourRender.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../RenderCore/Techniques/ResourceBox.h"

#include "../../Tools/ToolsRig/VisualisationUtils.h"

#include "../../ConsoleRig/Console.h"
#include "../../Utility/Profiling/CPUProfiler.h"
#include "../../Math/Noise.h"
#include "../../Math/Geometry.h"

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

    namespace Test
    {
        using namespace SceneEngine;

        class DualContourTest
        {
        public:
            class Desc
            {
            public:
                unsigned _gridDims;
                Desc(unsigned gridDims) : _gridDims(gridDims) {}
            };

            DualContourMesh _mesh;

            DualContourTest(const Desc& desc);
        };

        static float SimplexFBM(Float3 pos, float hgrid, float gain, float lacunarity, int octaves)
        {
            float total = 0.0f;
	        float frequency = 1.0f/(float)hgrid;
	        float amplitude = 1.f;
        
	        for (int i = 0; i < octaves; ++i) {
		        total += SimplexNoise(Float3(pos * frequency)) * amplitude;
		        frequency *= lacunarity;
		        amplitude *= gain;
	        }
        
	        return total;
        }

        class TestDensityFunction : public IVolumeDensityFunction
        {
        public:
            Boundary    GetBoundary() const
            {
                return std::make_pair(Float3(-10.f, -10.f, -10.f), Float3(10.f, 10.f, 10.f));
            }

            float       GetDensity(const Float3& pt) const
            {
                float basic = 1.f - (Magnitude(pt) / 9.f);
                // return basic;

                static Float3 posMult(1.f, 1.f, 0.25f);
                static float hgrid = 1.f;
                static float gain = 0.5f;
                static float lacunarity = 2.1042f;
                static unsigned octaves = 4;
                
                // return SimplexFBM(
                //     Float3(pt[0] * posMult[0], pt[1] * posMult[1], pt[2] * posMult[2]),
                //     hgrid, gain, lacunarity, octaves);
                
                auto spherical = CartesianToSpherical(pt);
                return basic + .33f * SimplexFBM(
                    Float3(spherical[0] * posMult[0], spherical[1] * posMult[1], spherical[2] * posMult[2]),
                    hgrid, gain, lacunarity, octaves);
            }

            Float3      GetNormal(const Float3& pt) const
            {
                static float range = 0.15f;
                float x0 = GetDensity(pt + Float3(-range, 0.f, 0.f));
                float x1 = GetDensity(pt + Float3( range, 0.f, 0.f));
                float y0 = GetDensity(pt + Float3(0.f, -range, 0.f));
                float y1 = GetDensity(pt + Float3(0.f,  range, 0.f));
                float z0 = GetDensity(pt + Float3(0.f, 0.f, -range));
                float z1 = GetDensity(pt + Float3(0.f, 0.f,  range));
                return Normalize(Float3(x0-x1, y0-y1, z0-z1));
            }
        };

        DualContourTest::DualContourTest(const Desc& desc)
        {
            _mesh = DualContourMesh_Build(desc._gridDims, TestDensityFunction());
        }
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

            auto& box = RenderCore::Techniques::FindCachedBox2<Test::DualContourTest>(
                Tweakable("GridDims", 64));
            DualContourMesh_DebuggingRender(context, parserContext, techniqueIndex, box._mesh);
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

