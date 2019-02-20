// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialVisualisation.h"
#include "VisualisationUtils.h"
#include "VisualisationGeo.h"

#include "../../SceneEngine/LightingParser.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/LightingParserStandardPlugin.h"
#include "../../SceneEngine/SceneParser.h"
#include "../../PlatformRig/BasicSceneParser.h"

#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/Drawables.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/BasicDelegates.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/Assets/AssetUtils.h"
#include "../../RenderCore/Assets/SimpleModelRenderer.h"
#include "../../RenderCore/Assets/ModelScaffold.h"
#include "../../RenderCore/IThreadContext.h"

#include "../../Assets/Assets.h"
#include "../../ConsoleRig/ResourceBox.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/VariantUtils.h"


namespace ToolsRig
{
    using namespace RenderCore;

///////////////////////////////////////////////////////////////////////////////////////////////////

    class CachedVisGeo
    {
    public:
        class Desc {};

        IResourcePtr _cubeBuffer;
        IResourcePtr _sphereBuffer;
        unsigned _cubeVCount;
        unsigned _sphereVCount;

        CachedVisGeo(const Desc&);
    };

    CachedVisGeo::CachedVisGeo(const Desc&)
    {
        auto sphereGeometry = BuildGeodesicSphere();
        _sphereBuffer = RenderCore::Assets::CreateStaticVertexBuffer(MakeIteratorRange(sphereGeometry));
        _sphereVCount = unsigned(sphereGeometry.size());
        auto cubeGeometry = BuildCube();
        _cubeBuffer = RenderCore::Assets::CreateStaticVertexBuffer(MakeIteratorRange(cubeGeometry));
        _cubeVCount = unsigned(cubeGeometry.size());
    }

	class MaterialSceneParserDrawable : public Techniques::Drawable
	{
	public:
		Topology	_topology;
		unsigned	_vertexCount;

		static void DrawFn(
			Metal::DeviceContext& metalContext,
			Techniques::ParsingContext& parserContext,
			const MaterialSceneParserDrawable& drawable, const Metal::BoundUniforms& boundUniforms,
			const Metal::ShaderProgram&)
		{
			if (boundUniforms._boundUniformBufferSlots[3] != 0) {
				ConstantBufferView cbvs[] = {
					Techniques::MakeLocalTransformPacket(
						Identity<Float4x4>(), 
						ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld))};
				boundUniforms.Apply(metalContext, 3, UniformsStream{MakeIteratorRange(cbvs)});
			}

			assert(!drawable._geo->_ib);
			metalContext.Bind(drawable._topology);
			metalContext.Draw(drawable._vertexCount);
		}
	};

    class MaterialVisualizationScene : public SceneEngine::IScene
    {
    public:
		void PrepareScene() const
		{
			// if we need to reset the camera, do so now...
			if (_settings->_pendingCameraAlignToModel) {
				if (_settings->_geometryType == MaterialVisSettings::GeometryType::Model && !_settings->_previewModelFile.empty()) {
						// this is more tricky... when using a model, we have to get the bounding box for the model
					using namespace RenderCore::Assets;
					const ::Assets::ResChar* modelFile = _settings->_previewModelFile.c_str();
					auto modelFuture = ::Assets::MakeAsset<ModelScaffold>(modelFile);
					modelFuture->StallWhilePending();
					auto model = modelFuture->TryActualize();
					if (model)
						*_settings->_camera = AlignCameraToBoundingBox(_settings->_camera->_verticalFieldOfView, model->GetStaticBoundingBox());
				} else {
						// just reset camera to the default
					*_settings->_camera = VisCameraSettings();
					_settings->_camera->_position = Float3(-5.f, 0.f, 0.f);
				}

				_settings->_pendingCameraAlignToModel = false;
			}
		}

        void Draw(  IThreadContext& threadContext, 
                    SceneEngine::SceneExecuteContext& executeContext,
                    IteratorRange<Techniques::DrawablesPacket** const> pkts) const
        {
			auto& metalContext = *Metal::DeviceContext::Get(threadContext);

                // disable blending to avoid problem when rendering single component stuff 
                //  (ie, nodes that output "float", not "float4")
            metalContext.Bind(Techniques::CommonResources()._blendOpaque);

			auto usi = std::make_shared<UniformsStreamInterface>();
			usi->BindConstantBuffer(0, {Techniques::ObjectCB::LocalTransform});

            auto geoType = _settings->_geometryType;
            if (geoType == MaterialVisSettings::GeometryType::Plane2D) {

                const Internal::Vertex3D    vertices[] = 
                {
                    { Float3(-1.f, -1.f, 0.f),  Float3(0.f, 0.f, 1.f), Float2(0.f, 1.f), Float4(1.f, 0.f, 0.f, 1.f) },
                    { Float3( 1.f, -1.f, 0.f),  Float3(0.f, 0.f, 1.f), Float2(1.f, 1.f), Float4(1.f, 0.f, 0.f, 1.f) },
                    { Float3(-1.f,  1.f, 0.f),  Float3(0.f, 0.f, 1.f), Float2(0.f, 0.f), Float4(1.f, 0.f, 0.f, 1.f) },
                    { Float3( 1.f,  1.f, 0.f),  Float3(0.f, 0.f, 1.f), Float2(1.f, 0.f), Float4(1.f, 0.f, 0.f, 1.f) }
                };

				auto& drawable = *pkts[unsigned(RenderCore::Techniques::BatchFilter::General)]->_drawables.Allocate<MaterialSceneParserDrawable>();
				drawable._material = _material.get();
				drawable._geo = std::make_shared<Techniques::DrawableGeo>();
				drawable._geo->_vertexStreams[0]._resource = RenderCore::Assets::CreateStaticVertexBuffer(*threadContext.GetDevice(), MakeIteratorRange(vertices));
				drawable._geo->_vertexStreams[0]._vertexElements = Vertex3D_MiniInputLayout;
				drawable._geo->_vertexStreamCount = 1;
				drawable._drawFn = (Techniques::Drawable::ExecuteDrawFn*)&MaterialSceneParserDrawable::DrawFn;
				drawable._topology = Topology::TriangleStrip;
				drawable._vertexCount = (unsigned)dimof(vertices);
				drawable._uniformsInterface = usi;

            } else if (geoType == MaterialVisSettings::GeometryType::Model) {

                DrawModel(pkts);

            } else {

                unsigned count = 0;
                const auto& cachedGeo = ConsoleRig::FindCachedBox2<CachedVisGeo>();
				RenderCore::IResourcePtr vb;
                if (geoType == MaterialVisSettings::GeometryType::Sphere) {
					vb = cachedGeo._sphereBuffer;
                    count = cachedGeo._sphereVCount;
                } else if (geoType == MaterialVisSettings::GeometryType::Cube) {
					vb = cachedGeo._cubeBuffer;
                    count = cachedGeo._cubeVCount;
                } else return;

				auto& drawable = *pkts[unsigned(RenderCore::Techniques::BatchFilter::General)]->_drawables.Allocate<MaterialSceneParserDrawable>();
				drawable._material = _material.get();
				drawable._geo = std::make_shared<Techniques::DrawableGeo>();
				drawable._geo->_vertexStreams[0]._resource = vb;
				drawable._geo->_vertexStreams[0]._vertexElements = Vertex3D_MiniInputLayout;
				drawable._geo->_vertexStreamCount = 1;
				drawable._drawFn = (Techniques::Drawable::ExecuteDrawFn*)&MaterialSceneParserDrawable::DrawFn;
				drawable._topology = Topology::TriangleList;
				drawable._vertexCount = count;
				drawable._uniformsInterface = usi;

            }
        }

		virtual void ExecuteScene(
            RenderCore::IThreadContext& threadContext,
			SceneEngine::SceneExecuteContext& executeContext) const
		{
			for (unsigned v=0; v<executeContext.GetViews().size(); ++v) {
				RenderCore::Techniques::DrawablesPacket* pkts[unsigned(RenderCore::Techniques::BatchFilter::Max)];
				for (unsigned c=0; c<unsigned(RenderCore::Techniques::BatchFilter::Max); ++c)
					pkts[c] = executeContext.GetDrawablesPacket(v, RenderCore::Techniques::BatchFilter(c));

				Draw(threadContext, executeContext, MakeIteratorRange(pkts));
			}
		}

        void DrawModel(IteratorRange<Techniques::DrawablesPacket** const> pkts) const;

        MaterialVisualizationScene(
            const std::shared_ptr<MaterialVisSettings>& settings)
        : _settings(settings) 
		{
			_material = std::make_shared<RenderCore::Techniques::Material>();
			XlCopyString(_material->_techniqueConfig, "xleres/techniques/illum.tech");
		}

    protected:
        std::shared_ptr<MaterialVisSettings>  _settings;
		std::shared_ptr<RenderCore::Techniques::Material> _material;
    };
    
///////////////////////////////////////////////////////////////////////////////////////////////////

	class MaterialFilterDelegate : public RenderCore::Assets::SimpleModelRenderer::IPreDrawDelegate
	{
	public:
		virtual bool OnDraw( 
			RenderCore::Metal::DeviceContext& metalContext, RenderCore::Techniques::ParsingContext&,
			const RenderCore::Techniques::Drawable&,
			uint64_t materialGuid, unsigned drawCallIdx) override
		{
			// Note that we're rejecting other draw calls very late in the pipeline here. But it
			// helps avoid extra overhead in the more common cases
			return materialGuid == _activeMaterial;
		}

		MaterialFilterDelegate(uint64_t activeMaterial) : _activeMaterial(activeMaterial) {}
	private:
		uint64_t _activeMaterial;
	};

    void MaterialVisualizationScene::DrawModel(IteratorRange<Techniques::DrawablesPacket** const> pkts) const
    {
            // This mode is a little more complex than the others. We want to
            // load the geometry data for a model and render all the parts of
            // that model that match a certain material assignment.
            // We're going to do this without a ModelRenderer object... So
            // we have to parse the ModelScaffold data directly
            //
            // It's a little complex. But it's a good way to test the internals of
            // the ModelScaffold class -- because we go through all the parts and
            // use each aspect of the model pipeline. 
		if (_settings->_previewModelFile.empty()) return;

        using namespace RenderCore::Assets;
        auto modelFile = MakeStringSection(_settings->_previewModelFile);
        uint64 boundMaterial = _settings->_previewMaterialBinding;

        auto modelFuture = ::Assets::MakeAsset<SimpleModelRenderer>(modelFile);
		auto state = modelFuture->StallWhilePending();

		if (state != ::Assets::AssetState::Ready)
			return;

		auto& model = *modelFuture->Actualize();
		return model.BuildDrawables(pkts, Identity<Float4x4>(), std::make_shared<MaterialFilterDelegate>(boundMaterial));
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	class MaterialVisLayer::Pimpl
    {
    public:
		std::shared_ptr<SceneEngine::IScene> _scene;
		std::shared_ptr<SceneEngine::ILightingParserDelegate> _lightingParserDelegate;
		std::shared_ptr<VisCameraSettings>_camera;
		DrawPreviewLightingType _lightingType = DrawPreviewLightingType::Deferred;
    };

    VisCameraSettings AlignCameraToBoundingBox(float verticalFieldOfView, const std::pair<Float3, Float3>& box);

	static SceneEngine::RenderSceneSettings::LightingModel AsLightingModel(DrawPreviewLightingType lightingType)
	{
		switch (lightingType) {
		case DrawPreviewLightingType::Deferred:
			return SceneEngine::RenderSceneSettings::LightingModel::Deferred;
		case DrawPreviewLightingType::Forward:
			return SceneEngine::RenderSceneSettings::LightingModel::Forward;
		default:
		case DrawPreviewLightingType::Direct:
			return SceneEngine::RenderSceneSettings::LightingModel::Direct;
		}
	}

    bool MaterialVisLayer::Draw(
        IThreadContext& context,
		const RenderCore::IResourcePtr& renderTarget,
        RenderCore::Techniques::ParsingContext& parserContext,
        DrawPreviewLightingType lightingType,
		SceneEngine::IScene& sceneParser,
		const SceneEngine::ILightingParserDelegate& lightingParserDelegate,
		const RenderCore::Techniques::CameraDesc& cameraDesc)
    {
		std::shared_ptr<SceneEngine::ILightingParserPlugin> lightingPlugins[] = {
			std::make_shared<SceneEngine::LightingParserStandardPlugin>()
		};
        SceneEngine::RenderSceneSettings qualSettings{
			AsLightingModel(lightingType),
			&lightingParserDelegate,
			MakeIteratorRange(lightingPlugins)};

        SceneEngine::LightingParser_ExecuteScene(
            context, renderTarget, parserContext,
            sceneParser, cameraDesc, qualSettings);

        return true;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    void MaterialVisLayer::Render(
        IThreadContext& context,
		const RenderCore::IResourcePtr& renderTarget,
        RenderCore::Techniques::ParsingContext& parserContext)
    {
        Draw(context, renderTarget, parserContext, 
			_pimpl->_lightingType, 
			*_pimpl->_scene, *_pimpl->_lightingParserDelegate,
			AsCameraDesc(*_pimpl->_camera));
    }

	void MaterialVisLayer::SetLightingType(DrawPreviewLightingType newType)
	{
		_pimpl->_lightingType = newType;
	}

    MaterialVisLayer::MaterialVisLayer(
		const std::shared_ptr<SceneEngine::IScene>& scene,
		const std::shared_ptr<SceneEngine::ILightingParserDelegate>& lightingParserDelegate,
		const std::shared_ptr<VisCameraSettings>& camera)
    {
        _pimpl = std::make_unique<Pimpl>();
		_pimpl->_scene = scene;
		_pimpl->_lightingParserDelegate = lightingParserDelegate;
		_pimpl->_camera = camera;
    }

    MaterialVisLayer::~MaterialVisLayer()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

	std::shared_ptr<SceneEngine::IScene> CreateScene(const std::shared_ptr<MaterialVisSettings>& visObject)
	{
		auto result = std::make_shared<MaterialVisualizationScene>(visObject);
		result->PrepareScene();
		return result;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	std::pair<DrawPreviewResult, std::string> DrawPreview(
        RenderCore::IThreadContext& context,
		const RenderCore::IResourcePtr& renderTarget,
        RenderCore::Techniques::ParsingContext& parserContext,
		const std::shared_ptr<MaterialVisSettings>& visObject,
		const std::shared_ptr<VisEnvSettings>& envSettings,
		DrawPreviewLightingType lightingType)
    {
        using namespace ToolsRig;

        try 
        {
            // Align the camera if we're drawing with a model...
            if (visObject->_geometryType == MaterialVisSettings::GeometryType::Model && !visObject->_previewModelFile.empty()) {
                auto model = ::Assets::MakeAsset<RenderCore::Assets::ModelScaffold>(
                    visObject->_previewModelFile.c_str());
                model->StallWhilePending();
                *visObject->_camera = ToolsRig::AlignCameraToBoundingBox(
                    visObject->_camera->_verticalFieldOfView, 
                    model->Actualize()->GetStaticBoundingBox());
            }

			MaterialVisualizationScene scene(visObject);
			auto future = ::Assets::MakeAsset<PlatformRig::EnvironmentSettings>(envSettings->_envConfigFile);
			future->StallWhilePending();
			PlatformRig::BasicLightingParserDelegate lightingParserDelegate(future->Actualize());

			scene.PrepareScene();

            bool result = ToolsRig::MaterialVisLayer::Draw(
                context, renderTarget, parserContext, 
                lightingType, 
				scene, lightingParserDelegate,
				AsCameraDesc(*visObject->_camera));
            if (parserContext.HasInvalidAssets())
				return std::make_pair(DrawPreviewResult::Error, "Invalid assets encountered");
			if (parserContext.HasErrorString())
				return std::make_pair(DrawPreviewResult::Error, parserContext._stringHelpers->_errorString);
            if (parserContext.HasPendingAssets()) return std::make_pair(DrawPreviewResult::Pending, std::string());
			if (result)
                return std::make_pair(DrawPreviewResult::Success, std::string());
        }
        catch (::Assets::Exceptions::InvalidAsset& e) { return std::make_pair(DrawPreviewResult::Error, e.what()); }
        catch (::Assets::Exceptions::PendingAsset& e) { return std::make_pair(DrawPreviewResult::Pending, e.Initializer()); }

        return std::make_pair(DrawPreviewResult::Error, std::string());
    }

}

