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

#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/Drawables.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/BasicDelegates.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/Assets/ModelRunTime.h"
#include "../../RenderCore/Assets/AssetUtils.h"
#include "../../RenderCore/Assets/SimpleModelRenderer.h"
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

    class MaterialSceneParser : public VisSceneParser
    {
    public:
        void ExecuteScene(
            RenderCore::IThreadContext& context,
			RenderCore::Techniques::ParsingContext& parserContext,
            SceneEngine::LightingParserContext& lightingParserContext,
            const SceneEngine::SceneParseSettings& parseSettings,
            SceneEngine::PreparedScene& preparedPackets,
            unsigned techniqueIndex) const 
        {
            using BF = SceneEngine::SceneParseSettings::BatchFilter;
            if (    parseSettings._batchFilter == BF::PreDepth
                ||  parseSettings._batchFilter == BF::General
                ||  parseSettings._batchFilter == BF::DMShadows) {

                Draw(context, parserContext, techniqueIndex);
            }
        }

		virtual void PrepareScene(
            RenderCore::IThreadContext& context, 
			RenderCore::Techniques::ParsingContext& parserContext,
            SceneEngine::PreparedScene& preparedPackets) const
		{
			// if we need to reset the camera, do so now...
			if (_settings->_pendingCameraAlignToModel) {
				if (_settings->_geometryType == MaterialVisSettings::GeometryType::Model && !_object->_previewModelFile.empty()) {
						// this is more tricky... when using a model, we have to get the bounding box for the model
					using namespace RenderCore::Assets;
					const ::Assets::ResChar* modelFile = _object->_previewModelFile.c_str();
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

        bool HasContent(const SceneEngine::SceneParseSettings& parseSettings) const
        {
            using BF = SceneEngine::SceneParseSettings::BatchFilter;
            return (    parseSettings._batchFilter == BF::PreDepth
                ||      parseSettings._batchFilter == BF::General
                ||      parseSettings._batchFilter == BF::DMShadows);
        }

        void Draw(  IThreadContext& threadContext, 
                    RenderCore::Techniques::ParsingContext& parserContext,
                    unsigned techniqueIndex) const
        {
			auto& metalContext = *Metal::DeviceContext::Get(threadContext);
            if (techniqueIndex!=3)
                metalContext.Bind(Techniques::CommonResources()._defaultRasterizer);

                // disable blending to avoid problem when rendering single component stuff 
                //  (ie, nodes that output "float", not "float4")
            metalContext.Bind(Techniques::CommonResources()._blendOpaque);

			VariantArray drawables;

			auto usi = std::make_shared<UniformsStreamInterface>();
			usi->BindConstantBuffer(0, {Techniques::ObjectCB::LocalTransform});
            
            auto geoType = _settings->_geometryType;
            if (geoType == MaterialVisSettings::GeometryType::Plane2D) {

                const Internal::Vertex3D    vertices[] = 
                {
                    { Float3(0.f, -1.f, -1.f),  Float3(1.f, 0.f, 0.f), Float2(0.f, 1.f), Float4(1.f, 0.f, 0.f, 1.f) },
                    { Float3(0.f, -1.f,  1.f),  Float3(1.f, 0.f, 0.f), Float2(0.f, 0.f), Float4(1.f, 0.f, 0.f, 1.f) },
					{ Float3(0.f,  1.f, -1.f),  Float3(1.f, 0.f, 0.f), Float2(1.f, 1.f), Float4(1.f, 0.f, 0.f, 1.f) },
                    { Float3(0.f,  1.f,  1.f),  Float3(1.f, 0.f, 0.f), Float2(1.f, 0.f), Float4(1.f, 0.f, 0.f, 1.f) }
                };

				auto& drawable = *drawables.Allocate<MaterialSceneParserDrawable>();
				drawable._techniqueConfig = "xleres/techniques/illum.tech";
				drawable._material = nullptr;
				drawable._geo = std::make_shared<Techniques::DrawableGeo>();
				drawable._geo->_vertexStreams[0]._resource = RenderCore::Assets::CreateStaticVertexBuffer(*threadContext.GetDevice(), MakeIteratorRange(vertices));
				drawable._geo->_vertexStreams[0]._vertexElements = Vertex3D_MiniInputLayout;
				drawable._geo->_vertexStreamCount = 1;
				drawable._drawFn = (Techniques::Drawable::ExecuteDrawFn*)&MaterialSceneParserDrawable::DrawFn;
				drawable._topology = Topology::TriangleStrip;
				drawable._vertexCount = (unsigned)dimof(vertices);
				drawable._uniformsInterface = usi;

            } else if (geoType == MaterialVisSettings::GeometryType::Model) {

                drawables = DrawModel();

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

				auto& drawable = *drawables.Allocate<MaterialSceneParserDrawable>();
				drawable._techniqueConfig = "xleres/techniques/illum.tech";
				drawable._material = nullptr;
				drawable._geo = std::make_shared<Techniques::DrawableGeo>();
				drawable._geo->_vertexStreams[0]._resource = vb;
				drawable._geo->_vertexStreams[0]._vertexElements = Vertex3D_MiniInputLayout;
				drawable._geo->_vertexStreamCount = 1;
				drawable._drawFn = (Techniques::Drawable::ExecuteDrawFn*)&MaterialSceneParserDrawable::DrawFn;
				drawable._topology = Topology::TriangleList;
				drawable._vertexCount = count;
				drawable._uniformsInterface = usi;

            }

			Techniques::SequencerTechnique seqTechnique;
			seqTechnique._techniqueDelegate = std::make_shared<RenderCore::Techniques::TechniqueDelegate_Basic>();
			seqTechnique._materialDelegate = std::make_shared<RenderCore::Techniques::MaterialDelegate_Basic>();

			auto& techUSI = RenderCore::Techniques::TechniqueContext::GetGlobalUniformsStreamInterface();
			for (unsigned c=0; c<techUSI._cbBindings.size(); ++c)
				seqTechnique._sequencerUniforms.emplace_back(std::make_pair(techUSI._cbBindings[c]._hashName, std::make_shared<RenderCore::Techniques::GlobalCBDelegate>(c)));

			ParameterBox seqShaderSelectors;

			for (auto d=drawables.begin(); d!=drawables.end(); ++d)
				RenderCore::Techniques::Draw(
					threadContext, 
					parserContext,
					techniqueIndex,
					seqTechnique,
					&seqShaderSelectors,
					*(Techniques::Drawable*)d.get());
        }

        VariantArray DrawModel() const;

        MaterialSceneParser(
            const std::shared_ptr<MaterialVisSettings>& settings,
            const std::shared_ptr<VisEnvSettings>& envSettings,
            const std::shared_ptr<MaterialVisObject>& object)
        : VisSceneParser(settings->_camera, envSettings), _settings(settings), _object(object) {}

    protected:
        std::shared_ptr<MaterialVisSettings>  _settings;
        std::shared_ptr<MaterialVisObject>    _object;
    };

	std::shared_ptr<SceneEngine::ISceneParser> CreateMaterialVisSceneParser(
		const std::shared_ptr<MaterialVisSettings>& settings,
        const std::shared_ptr<VisEnvSettings>& envSettings,
        const std::shared_ptr<MaterialVisObject>& object)
	{
		return std::make_shared<MaterialSceneParser>(settings, envSettings, object);
	}
    
///////////////////////////////////////////////////////////////////////////////////////////////////

    VariantArray MaterialSceneParser::DrawModel() const
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
		if (_object->_previewModelFile.empty()) return {};

        using namespace RenderCore::Assets;
        auto modelFile = MakeStringSection(_object->_previewModelFile);
        uint64 boundMaterial = _object->_previewMaterialBinding;

        auto modelFuture = ::Assets::MakeAsset<SimpleModelRenderer>(modelFile);
		auto state = modelFuture->StallWhilePending();

		if (state != ::Assets::AssetState::Ready)
			return {};

		auto& model = *modelFuture->Actualize();
		return model.BuildDrawables(boundMaterial);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    VisCameraSettings AlignCameraToBoundingBox(float verticalFieldOfView, const std::pair<Float3, Float3>& box);

	static SceneEngine::RenderingQualitySettings::LightingModel AsLightingModel(DrawPreviewLightingType lightingType)
	{
		switch (lightingType) {
		case DrawPreviewLightingType::Deferred:
			return SceneEngine::RenderingQualitySettings::LightingModel::Deferred;
		case DrawPreviewLightingType::Forward:
			return SceneEngine::RenderingQualitySettings::LightingModel::Forward;
		default:
		case DrawPreviewLightingType::NoLightingParser:
			return SceneEngine::RenderingQualitySettings::LightingModel::Direct;
		}
	}

    bool MaterialVisLayer::Draw(
        IThreadContext& context,
        RenderCore::Techniques::ParsingContext& parserContext,
        DrawPreviewLightingType lightingType,
		SceneEngine::ISceneParser& sceneParser)
    {
		std::shared_ptr<SceneEngine::ILightingParserPlugin> lightingPlugins[] = {
			std::make_shared<SceneEngine::LightingParserStandardPlugin>()
		};
        SceneEngine::RenderingQualitySettings qualSettings{
			UInt2(context.GetStateDesc()._viewportDimensions[0], context.GetStateDesc()._viewportDimensions[1]),
			AsLightingModel(lightingType),
			MakeIteratorRange(lightingPlugins)};

        SceneEngine::LightingParser_ExecuteScene(
            context, parserContext,
            sceneParser, sceneParser.GetCameraDesc(), qualSettings);

        return true;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class MaterialVisLayer::Pimpl
    {
    public:
		std::shared_ptr<SceneEngine::ISceneParser> _sceneParser;
		DrawPreviewLightingType _lightingType = DrawPreviewLightingType::NoLightingParser;
    };

    auto MaterialVisLayer::GetInputListener() -> std::shared_ptr<IInputListener>
        { return nullptr; }

    void MaterialVisLayer::RenderToScene(
        IThreadContext& context, 
        RenderCore::Techniques::ParsingContext& parserContext)
    {
        Draw(context, parserContext, _pimpl->_lightingType, *_pimpl->_sceneParser);
    }

    void MaterialVisLayer::RenderWidgets(
        IThreadContext& context, 
        Techniques::ParsingContext& parsingContext)
    {}

    void MaterialVisLayer::SetActivationState(bool) {}

	void MaterialVisLayer::SetLightingType(DrawPreviewLightingType newType)
	{
		_pimpl->_lightingType = newType;
	}

    MaterialVisLayer::MaterialVisLayer(
		std::shared_ptr<SceneEngine::ISceneParser> sceneParser)
    {
        _pimpl = std::make_unique<Pimpl>();
		_pimpl->_sceneParser = sceneParser;
    }

    MaterialVisLayer::~MaterialVisLayer()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    MaterialVisSettings::MaterialVisSettings()
    {
        _camera = std::make_shared<VisCameraSettings>();
        _camera->_position = Float3(-5.f, 0.f, 0.f);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////


	std::pair<DrawPreviewResult, std::string> DrawPreview(
        RenderCore::IThreadContext& context,
        const RenderCore::Techniques::TechniqueContext& techContext,
		RenderCore::Techniques::AttachmentPool* attachmentPool,
		RenderCore::Techniques::FrameBufferPool* frameBufferPool,
        PreviewGeometry geometry,
		ToolsRig::MaterialVisObject& visObject)
    {
        using namespace ToolsRig;

        try 
        {
            auto visSettings = std::make_shared<MaterialVisSettings>();
            visSettings->_camera = std::make_shared<VisCameraSettings>();
            visSettings->_camera->_position = Float3(-4, 0, 0);  // note that the position of the camera affects the apparent color of normals when previewing world space normals

			auto visObjCopy = std::make_shared<ToolsRig::MaterialVisObject>();

            // Select the geometry type to use.
            // In the "chart" mode, we are just going to run a pixel shader for every
            // output pixel, so we want to use a pretransformed quad covering the viewport
            switch (geometry) {
            case PreviewGeometry::Plane2D:
            case PreviewGeometry::Chart:
                visSettings->_geometryType = MaterialVisSettings::GeometryType::Plane2D;
                visObjCopy->_parameters._matParams.SetParameter(u("GEO_PRETRANSFORMED"), "1");
                break;

            case PreviewGeometry::Box:
                visSettings->_geometryType = MaterialVisSettings::GeometryType::Cube;
                break;

            default:
            case PreviewGeometry::Sphere:
                visSettings->_geometryType = MaterialVisSettings::GeometryType::Sphere;
                break;

            case PreviewGeometry::Model:
                visSettings->_geometryType = MaterialVisSettings::GeometryType::Model;
                break;
            };

            // Align the camera if we're drawing with a model...
            if (geometry == PreviewGeometry::Model && !visObject._previewModelFile.empty()) {
                auto model = ::Assets::MakeAsset<RenderCore::Assets::ModelScaffold>(
                    visObject._previewModelFile.c_str());
                model->StallWhilePending();
                *visSettings->_camera = ToolsRig::AlignCameraToBoundingBox(
                    visSettings->_camera->_verticalFieldOfView, 
                    model->Actualize()->GetStaticBoundingBox());
            }

			auto sceneParser = CreateMaterialVisSceneParser(visSettings, std::make_shared<VisEnvSettings>(), visObjCopy);

            RenderCore::Techniques::ParsingContext parserContext(techContext, attachmentPool, frameBufferPool);
            bool result = ToolsRig::MaterialVisLayer::Draw(
                context, parserContext, 
                DrawPreviewLightingType::NoLightingParser, *sceneParser);
            if (result)
                return std::make_pair(DrawPreviewResult::Success, std::string());

            if (parserContext.HasPendingAssets()) return std::make_pair(DrawPreviewResult::Pending, std::string());
        }
        catch (::Assets::Exceptions::InvalidAsset& e) { return std::make_pair(DrawPreviewResult::Error, e.what()); }
        catch (::Assets::Exceptions::PendingAsset& e) { return std::make_pair(DrawPreviewResult::Pending, e.Initializer()); }

        return std::make_pair(DrawPreviewResult::Error, std::string());
    }

}

