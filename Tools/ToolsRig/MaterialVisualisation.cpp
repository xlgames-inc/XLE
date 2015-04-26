// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialVisualisation.h"
#include "VisualisationUtils.h"
#include "VisualisationGeo.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/LightingParser.h"
#include "../../SceneEngine/SceneParser.h"
#include "../../SceneEngine/LightDesc.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/TechniqueMaterial.h"
#include "../../RenderCore/Techniques/CommonBindings.h"
#include "../../RenderCore/Techniques/ResourceBox.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/IThreadContext.h"
#include "../../Math/Transformations.h"

namespace ToolsRig
{

///////////////////////////////////////////////////////////////////////////////////////////////////

    class VisSceneParser : public SceneEngine::ISceneParser
    {
    public:
        RenderCore::Techniques::CameraDesc  GetCameraDesc() const
        {
            RenderCore::Techniques::CameraDesc result;
            result._cameraToWorld = MakeCameraToWorld(
                Normalize(_settings->_focus - _settings->_position),
                Float3(0.f, 0.f, 1.f), _settings->_position);
            result._farClip = _settings->_farClip;
            result._nearClip = _settings->_nearClip;
            result._verticalFieldOfView = Deg2Rad(_settings->_verticalFieldOfView);
            result._temporaryMatrix = Identity<Float4x4>();
            return result;
        }

        unsigned GetShadowProjectionCount() const { return 0; }
        SceneEngine::ShadowProjectionDesc GetShadowProjectionDesc(unsigned index, const RenderCore::Techniques::ProjectionDesc& mainSceneProjectionDesc) const 
            { return SceneEngine::ShadowProjectionDesc(); }

        unsigned                        GetLightCount() const { return 1; }
        const SceneEngine::LightDesc&   GetLightDesc(unsigned index) const
        {
            static SceneEngine::LightDesc light = DefaultDominantLight();
            return light;
        }

        SceneEngine::GlobalLightingDesc GetGlobalLightingDesc() const
        {
            return DefaultGlobalLightingDesc();
        }

        float GetTimeValue() const { return 0.f; }

        VisSceneParser(std::shared_ptr<VisCameraSettings> settings)
            : _settings(std::move(settings)) {}
        ~VisSceneParser() {}

    protected:
        std::shared_ptr<VisCameraSettings> _settings;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    class CachedVisGeo
    {
    public:
        class Desc {};

        RenderCore::Metal::VertexBuffer _cubeBuffer;
        RenderCore::Metal::VertexBuffer _sphereBuffer;
        unsigned _cubeVCount;
        unsigned _sphereVCount;

        CachedVisGeo(const Desc&);
    };

    CachedVisGeo::CachedVisGeo(const Desc&)
    {
        using namespace RenderCore;
        auto sphereGeometry = BuildGeodesicSphere();
        _sphereBuffer = Metal::VertexBuffer(AsPointer(sphereGeometry.begin()), sphereGeometry.size() * sizeof(Internal::Vertex3D));
        _sphereVCount = unsigned(sphereGeometry.size());
        auto cubeGeometry = BuildCube();
        _cubeBuffer = Metal::VertexBuffer(AsPointer(cubeGeometry.begin()), cubeGeometry.size() * sizeof(Internal::Vertex3D));
        _cubeVCount = unsigned(cubeGeometry.size());
    }

    class MaterialSceneParser : public VisSceneParser
    {
    public:
        void ExecuteScene(  RenderCore::Metal::DeviceContext* context, 
                            SceneEngine::LightingParserContext& parserContext, 
                            const SceneEngine::SceneParseSettings& parseSettings,
                            unsigned techniqueIndex) const 
        {
            if (    parseSettings._batchFilter == SceneEngine::SceneParseSettings::BatchFilter::Depth
                ||  parseSettings._batchFilter == SceneEngine::SceneParseSettings::BatchFilter::General) {

                    // draw here
                Draw(*context, parserContext, techniqueIndex);
            }
        }

        void ExecuteShadowScene(    RenderCore::Metal::DeviceContext* context, 
                                    SceneEngine::LightingParserContext& parserContext, 
                                    const SceneEngine::SceneParseSettings& parseSettings,
                                    unsigned index, unsigned techniqueIndex) const
        {
            ExecuteScene(context, parserContext, parseSettings, techniqueIndex);
        }

        void Draw(  RenderCore::Metal::DeviceContext& metalContext, 
                    SceneEngine::LightingParserContext& parserContext,
                    unsigned techniqueIndex) const
        {
            using namespace RenderCore;
            metalContext.Bind(RenderCore::Techniques::CommonResources()._defaultRasterizer);

                // disable blending to avoid problem when rendering single component stuff 
                //  (ie, nodes that output "float", not "float4")
            metalContext.Bind(RenderCore::Techniques::CommonResources()._blendOpaque);
            
            auto geoType = _settings->_geometryType;
            if (geoType == MaterialVisSettings::GeometryType::Plane2D) {

                auto shaderProgram = _object->_materialBinder->Apply(
                    metalContext, parserContext, techniqueIndex,
                    _object->_parameters, _object->_systemConstants, 
                    _object->_searchRules, Vertex2D_InputLayout);
                if (!shaderProgram) return;

                const Internal::Vertex2D    vertices[] = 
                {
                    { Float2(-1.f, -1.f),  Float2(0.f, 0.f) },
                    { Float2( 1.f, -1.f),  Float2(1.f, 0.f) },
                    { Float2(-1.f,  1.f),  Float2(0.f, 1.f) },
                    { Float2( 1.f,  1.f),  Float2(1.f, 1.f) }
                };

                Metal::VertexBuffer vertexBuffer(vertices, sizeof(vertices));
                metalContext.Bind(MakeResourceList(vertexBuffer), sizeof(Internal::Vertex2D), 0);
                metalContext.Bind(Metal::Topology::TriangleStrip);
                metalContext.Draw(dimof(vertices));

            } else {

                auto shaderProgram = _object->_materialBinder->Apply(
                    metalContext, parserContext, techniqueIndex,
                    _object->_parameters, _object->_systemConstants, 
                    _object->_searchRules, Vertex3D_InputLayout);
                if (!shaderProgram) return;
            
                unsigned count;
                const auto& cachedGeo = Techniques::FindCachedBox2<CachedVisGeo>();
                if (geoType == MaterialVisSettings::GeometryType::Sphere) {
                    metalContext.Bind(MakeResourceList(cachedGeo._sphereBuffer), sizeof(Internal::Vertex3D), 0);    
                    count = cachedGeo._sphereVCount;
                } else if (geoType == MaterialVisSettings::GeometryType::Cube) {
                    metalContext.Bind(MakeResourceList(cachedGeo._cubeBuffer), sizeof(Internal::Vertex3D), 0);    
                    count = cachedGeo._cubeVCount;
                }
                
                metalContext.Bind(Metal::Topology::TriangleList);
                metalContext.Draw(count);

            }
        }

        MaterialSceneParser(
            const MaterialVisSettings& settings,
            const MaterialVisObject& object)
        : VisSceneParser(settings._camera), _settings(&settings), _object(&object) {}

    protected:
        const MaterialVisSettings*  _settings;
        const MaterialVisObject*    _object;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    bool MaterialVisLayer::Draw(
        RenderCore::IThreadContext& context,
        SceneEngine::LightingParserContext& parserContext,
        const MaterialVisSettings& settings,
        const MaterialVisObject& object)
    {
        using namespace RenderCore;
        using namespace RenderCore::Metal;

        TRY {

            MaterialSceneParser sceneParser(settings, object);
            SceneEngine::RenderingQualitySettings qualSettings(context.GetStateDesc()._viewportDimensions);

            if (settings._lightingType == MaterialVisSettings::LightingType::NoLightingParser) {
                
                auto metalContext = RenderCore::Metal::DeviceContext::Get(context);
                SceneEngine::LightingParser_SetupScene(
                    metalContext.get(), parserContext, 
                    &sceneParser, sceneParser.GetCameraDesc(),
                    qualSettings);
                sceneParser.Draw(*metalContext.get(), parserContext, 0);
                
            } else if (settings._lightingType == MaterialVisSettings::LightingType::Deferred) {
                qualSettings._lightingModel = SceneEngine::RenderingQualitySettings::LightingModel::Deferred;
                SceneEngine::LightingParser_ExecuteScene(context, parserContext, sceneParser, qualSettings);
            } else if (settings._lightingType == MaterialVisSettings::LightingType::Forward) {
                qualSettings._lightingModel = SceneEngine::RenderingQualitySettings::LightingModel::Forward;
                SceneEngine::LightingParser_ExecuteScene(context, parserContext, sceneParser, qualSettings);
            }

            return true;
        }
        CATCH (::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
        CATCH (::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
        CATCH_END

        return false;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class MaterialVisLayer::Pimpl
    {
    public:
        std::shared_ptr<MaterialVisSettings> _settings;
        std::shared_ptr<MaterialVisObject> _object;
    };

    auto MaterialVisLayer::GetInputListener() -> std::shared_ptr<IInputListener>
        { return nullptr; }

    void MaterialVisLayer::RenderToScene(
        RenderCore::IThreadContext* context, 
        SceneEngine::LightingParserContext& parserContext)
    {
        assert(context);
        Draw(*context, parserContext, *_pimpl->_settings, *_pimpl->_object);
    }

    void MaterialVisLayer::RenderWidgets(
        RenderCore::IThreadContext* context, 
        const RenderCore::Techniques::ProjectionDesc& projectionDesc)
    {}

    void MaterialVisLayer::SetActivationState(bool) {}

    MaterialVisLayer::MaterialVisLayer(
        std::shared_ptr<MaterialVisSettings> settings,
        std::shared_ptr<MaterialVisObject> object)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_settings = settings;
        _pimpl->_object = object;
    }

    MaterialVisLayer::~MaterialVisLayer()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    MaterialVisSettings::MaterialVisSettings()
    {
        _geometryType = GeometryType::Sphere;
        _lightingType = LightingType::NoLightingParser;
        _camera = std::make_shared<VisCameraSettings>();
        _camera->_position = Float3(-5.f, 0.f, 0.f);
    }

    MaterialVisObject::MaterialVisObject()
    {
    }

    MaterialVisObject::~MaterialVisObject()
    {
    }

}

