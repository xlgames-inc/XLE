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
#include "../../SceneEngine/PreparedScene.h"

#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/InputLayout.h"

#include "../../RenderCore/Assets/ModelRunTime.h"
#include "../../RenderCore/Assets/ModelScaffoldInternal.h"
#include "../../RenderCore/Assets/ModelImmutableData.h"
#include "../../RenderCore/Assets/ModelRendererInternal.h"      // for BuildLowLevelInputAssembly
#include "../../RenderCore/Assets/AssetUtils.h"

#include "../../Assets/IFileSystem.h"
#include "../../Assets/Assets.h"
#include "../../ConsoleRig/ResourceBox.h"

#include "../../RenderCore/IThreadContext.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Math/Transformations.h"

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

    class MaterialSceneParser : public VisSceneParser
    {
    public:
        void ExecuteScene(  
            RenderCore::IThreadContext& context,
            SceneEngine::LightingParserContext& parserContext,
            const SceneEngine::SceneParseSettings& parseSettings,
            SceneEngine::PreparedScene& preparedPackets,
            unsigned techniqueIndex) const 
        {
            using BF = SceneEngine::SceneParseSettings::BatchFilter;
            if (    parseSettings._batchFilter == BF::PreDepth
                ||  parseSettings._batchFilter == BF::General
                ||  parseSettings._batchFilter == BF::DMShadows) {

                auto metalContext = RenderCore::Metal::DeviceContext::Get(context);
                Draw(*metalContext, parserContext, techniqueIndex);
            }
        }

        bool HasContent(const SceneEngine::SceneParseSettings& parseSettings) const
        {
            using BF = SceneEngine::SceneParseSettings::BatchFilter;
            return (    parseSettings._batchFilter == BF::PreDepth
                ||      parseSettings._batchFilter == BF::General
                ||      parseSettings._batchFilter == BF::DMShadows);
        }

        void Draw(  Metal::DeviceContext& metalContext, 
                    SceneEngine::LightingParserContext& parserContext,
                    unsigned techniqueIndex) const
        {
            if (techniqueIndex!=3)
                metalContext.Bind(Techniques::CommonResources()._defaultRasterizer);

                // disable blending to avoid problem when rendering single component stuff 
                //  (ie, nodes that output "float", not "float4")
            metalContext.Bind(Techniques::CommonResources()._blendOpaque);
            
            auto geoType = _settings->_geometryType;
            if (geoType == MaterialVisSettings::GeometryType::Plane2D) {

                const Internal::Vertex3D    vertices[] = 
                {
                    { Float3(-1.f, -1.f, 0.f),  Float3(0.f, 0.f, 1.f), Float2(0.f, 1.f), Float4(1.f, 0.f, 0.f, 1.f) },
                    { Float3( 1.f, -1.f, 0.f),  Float3(0.f, 0.f, 1.f), Float2(1.f, 1.f), Float4(1.f, 0.f, 0.f, 1.f) },
                    { Float3(-1.f,  1.f, 0.f),  Float3(0.f, 0.f, 1.f), Float2(0.f, 0.f), Float4(1.f, 0.f, 0.f, 1.f) },
                    { Float3( 1.f,  1.f, 0.f),  Float3(0.f, 0.f, 1.f), Float2(1.f, 0.f), Float4(1.f, 0.f, 0.f, 1.f) }
                };

                auto vertexBuffer = Metal::MakeVertexBuffer(Metal::GetObjectFactory(), MakeIteratorRange(vertices));
				VertexBufferView vbvs[] = {&vertexBuffer};

				auto shaderProgram = _object->_materialBinder->Apply(
                    metalContext, parserContext, techniqueIndex,
                    _object->_parameters, _object->_systemConstants, 
                    _object->_searchRules, Vertex3D_InputLayout,
					MakeIteratorRange(vbvs));
                if (!shaderProgram) return;

                metalContext.Bind(Topology::TriangleStrip);
                metalContext.Draw(dimof(vertices));

            } else if (geoType == MaterialVisSettings::GeometryType::Model) {

                DrawModel(metalContext, parserContext, techniqueIndex);

            } else {

                unsigned count;
                const auto& cachedGeo = ConsoleRig::FindCachedBox2<CachedVisGeo>();
				VertexBufferView vbv;
                if (geoType == MaterialVisSettings::GeometryType::Sphere) {
					vbv = cachedGeo._sphereBuffer;
                    count = cachedGeo._sphereVCount;
                } else if (geoType == MaterialVisSettings::GeometryType::Cube) {
					vbv = cachedGeo._cubeBuffer;
                    count = cachedGeo._cubeVCount;
                } else return;

				auto shaderProgram = _object->_materialBinder->Apply(
                    metalContext, parserContext, techniqueIndex,
                    _object->_parameters, _object->_systemConstants, 
                    _object->_searchRules, Vertex3D_InputLayout,
					MakeIteratorRange(&vbv, &vbv+1));
                if (!shaderProgram) return;
                
                metalContext.Bind(Topology::TriangleList);
                metalContext.Draw(count);

            }
        }

        void DrawModel(
            Metal::DeviceContext& metalContext, 
            SceneEngine::LightingParserContext& parserContext,
            unsigned techniqueIndex) const;

        MaterialSceneParser(
            const MaterialVisSettings& settings,
            const VisEnvSettings& envSettings,
            const MaterialVisObject& object)
        : VisSceneParser(settings._camera, std::move(envSettings)), _settings(&settings), _object(&object) {}

    protected:
        const MaterialVisSettings*  _settings;
        const MaterialVisObject*    _object;
    };

    static Metal::Buffer LoadVertexBuffer(
        const RenderCore::Assets::ModelScaffold& scaffold,
        RenderCore::Assets::VertexData& vb)
    {
        auto buffer = std::make_unique<uint8[]>(vb._size);
		{
            auto inputFile = scaffold.OpenLargeBlocks();
            inputFile->Seek(vb._offset, Utility::FileSeekAnchor::Current);
            inputFile->Read(buffer.get(), vb._size, 1);
        }
		return Metal::MakeVertexBuffer(
			Metal::GetObjectFactory(),
			MakeIteratorRange(buffer.get(), PtrAdd(buffer.get(), vb._size)));
    }

    static Metal::Buffer LoadIndexBuffer(
        const RenderCore::Assets::ModelScaffold& scaffold,
        RenderCore::Assets::IndexData& ib)
    {
        auto buffer = std::make_unique<uint8[]>(ib._size);
        {
            auto inputFile = scaffold.OpenLargeBlocks();
            inputFile->Seek(ib._offset, Utility::FileSeekAnchor::Current);
            inputFile->Read(buffer.get(), ib._size, 1);
        }
		return Metal::MakeIndexBuffer(
			Metal::GetObjectFactory(),
			MakeIteratorRange(buffer.get(), PtrAdd(buffer.get(), ib._size)));
    }

    void MaterialSceneParser::DrawModel(  
        Metal::DeviceContext& metalContext, 
        SceneEngine::LightingParserContext& parserContext,
        unsigned techniqueIndex) const
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
        if (_object->_previewModelFile.empty()) return;

        using namespace RenderCore::Assets;
        const ::Assets::ResChar* modelFile = _object->_previewModelFile.c_str();
        const uint64 boundMaterial = _object->_previewMaterialBinding;

        auto modelFuture = ::Assets::MakeAsset<ModelScaffold>(modelFile);
		modelFuture->StallWhilePending();
		auto& model = *modelFuture->Actualize();

        // const auto& skeletonScaff = ::Assets::GetAssetComp<SkeletonScaffold>(modelFile);
        // skeletonScaff.StallWhilePending();
        // const auto& skeleton = skeletonScaff.GetTransformationMachine();
        const auto& skeleton = model.EmbeddedSkeleton();

        SkeletonBinding skelBinding(
            skeleton.GetOutputInterface(),
            model.CommandStream().GetInputInterface());

        auto transformCount = skeleton.GetOutputMatrixCount();
        auto skelTransforms = std::make_unique<Float4x4[]>(transformCount);
        skeleton.GenerateOutputTransforms(skelTransforms.get(), transformCount, &skeleton.GetDefaultParameters());

        const auto& cmdStream = model.CommandStream();
        const auto& immData = model.ImmutableData();
        for (unsigned c = 0; c < cmdStream.GetGeoCallCount(); ++c) {
            const auto& geoCall = cmdStream.GetGeoCall(c);
            
            auto& rawGeo = immData._geos[geoCall._geoId];
            for (unsigned d = 0; d < unsigned(rawGeo._drawCalls.size()); ++d) {
                const auto& drawCall = rawGeo._drawCalls[d];

                    // reject geometry that doesn't match the material
                    // binding that we want
                if (boundMaterial != 0
                    && geoCall._materialGuids[drawCall._subMaterialIndex] != boundMaterial)
                    continue;

                    // now we have at least once piece of geometry
                    // that we want to render... We need to bind the material,
                    // index buffer and vertex buffer and topology
                    // then we just execute the draw command

                InputElementDesc metalVertInput[16];
                unsigned eleCount = BuildLowLevelInputAssembly(metalVertInput, dimof(metalVertInput),
                    rawGeo._vb._ia._elements);

                IMaterialBinder::SystemConstants sysContants = _object->_systemConstants;
                auto machineOutput = skelBinding.ModelJointToMachineOutput(geoCall._transformMarker);
                if (machineOutput < transformCount) {
                    sysContants._objectToWorld = skelTransforms[machineOutput];
                } else {
                    sysContants._objectToWorld = Identity<Float4x4>();
                }

				auto vb = LoadVertexBuffer(model, rawGeo._vb);
				VertexBufferView vbv { &vb };

                auto shaderProgram = _object->_materialBinder->Apply(
                    metalContext, parserContext, techniqueIndex,
                    _object->_parameters, sysContants, 
                    _object->_searchRules, MakeIteratorRange(metalVertInput, metalVertInput+eleCount),
					MakeIteratorRange(&vbv, &vbv+1));
                if (!shaderProgram) return;

                auto ib = LoadIndexBuffer(model, rawGeo._ib);
				metalContext.Bind(ib, rawGeo._ib._format);

                metalContext.Bind(drawCall._topology);
                metalContext.DrawIndexed(drawCall._indexCount, drawCall._firstIndex, drawCall._firstVertex);
            }
        }

        for (unsigned c = 0; c < cmdStream.GetSkinCallCount(); ++c) {
            const auto& geoCall = cmdStream.GetSkinCall(c);
            
            auto& rawGeo = immData._boundSkinnedControllers[geoCall._geoId];
            for (unsigned d = 0; d < unsigned(rawGeo._drawCalls.size()); ++d) {
                const auto& drawCall = rawGeo._drawCalls[d];

                    // reject geometry that doesn't match the material
                    // binding that we want
                if (boundMaterial != 0
                    && geoCall._materialGuids[drawCall._subMaterialIndex] != boundMaterial)
                    continue;

                    // now we have at least once piece of geometry
                    // that we want to render... We need to bind the material,
                    // index buffer and vertex buffer and topology
                    // then we just execute the draw command

                InputElementDesc metalVertInput[16];
                unsigned eleCount = BuildLowLevelInputAssembly(metalVertInput, dimof(metalVertInput),
                    rawGeo._vb._ia._elements, 0);
                eleCount += BuildLowLevelInputAssembly(
                    metalVertInput + eleCount, dimof(metalVertInput) - eleCount,
                    rawGeo._animatedVertexElements._ia._elements, 1);

                IMaterialBinder::SystemConstants sysContants = _object->_systemConstants;
                sysContants._objectToWorld = skelTransforms[
                    skelBinding.ModelJointToMachineOutput(geoCall._transformMarker)];

				auto vb0 = LoadVertexBuffer(model, rawGeo._vb);
				auto vb1 = LoadVertexBuffer(model, rawGeo._animatedVertexElements);
				VertexBufferView vbvs[] = { &vb0, &vb1 };

                auto shaderProgram = _object->_materialBinder->Apply(
                    metalContext, parserContext, techniqueIndex,
                    _object->_parameters, sysContants, 
                    _object->_searchRules, MakeIteratorRange(metalVertInput, metalVertInput+eleCount),
					MakeIteratorRange(vbvs));
                if (!shaderProgram) return;

				auto ib = LoadIndexBuffer(model, rawGeo._ib);
                metalContext.Bind(ib, rawGeo._ib._format);

                metalContext.Bind(drawCall._topology);
                metalContext.DrawIndexed(drawCall._indexCount, drawCall._firstIndex, drawCall._firstVertex);
            }
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    VisCameraSettings AlignCameraToBoundingBox(float verticalFieldOfView, const std::pair<Float3, Float3>& box);

    bool MaterialVisLayer::Draw(
        IThreadContext& context,
        SceneEngine::LightingParserContext& parserContext,
        const MaterialVisSettings& settings,
        const VisEnvSettings& envSettings,
        const MaterialVisObject& object)
    {
            // if we need to reset the camera, do so now...
        if (settings._pendingCameraAlignToModel) {
            if (settings._geometryType == MaterialVisSettings::GeometryType::Model && !object._previewModelFile.empty()) {
                    // this is more tricky... when using a model, we have to get the bounding box for the model
                using namespace RenderCore::Assets;
                const ::Assets::ResChar* modelFile = object._previewModelFile.c_str();
                auto modelFuture = ::Assets::MakeAsset<ModelScaffold>(modelFile);
                modelFuture->StallWhilePending();
				const auto& model = *modelFuture->Actualize();
                *settings._camera = AlignCameraToBoundingBox(settings._camera->_verticalFieldOfView, model.GetStaticBoundingBox());
            } else {
                    // just reset camera to the default
                *settings._camera = VisCameraSettings();
                settings._camera->_position = Float3(-5.f, 0.f, 0.f);
            }

            settings._pendingCameraAlignToModel = false;
        }

        MaterialSceneParser sceneParser(settings, envSettings, object);
        sceneParser.Prepare();
        SceneEngine::RenderingQualitySettings qualSettings(context.GetStateDesc()._viewportDimensions);

        if (settings._lightingType == MaterialVisSettings::LightingType::NoLightingParser) {
                
            auto metalContext = Metal::DeviceContext::Get(context);
            auto marker = SceneEngine::LightingParser_SetupScene(
                *metalContext.get(), parserContext, 
                &sceneParser);
            SceneEngine::LightingParser_SetGlobalTransform(
                *metalContext.get(), parserContext, 
                SceneEngine::BuildProjectionDesc(sceneParser.GetCameraDesc(), qualSettings._dimensions));
            CATCH_ASSETS_BEGIN
                SceneEngine::ReturnToSteadyState(*metalContext.get());
                SceneEngine::SetFrameGlobalStates(*metalContext.get());
            CATCH_ASSETS_END(parserContext)
            sceneParser.Draw(*metalContext.get(), parserContext, 0);
                
        } else if (settings._lightingType == MaterialVisSettings::LightingType::Deferred) {
            qualSettings._lightingModel = SceneEngine::RenderingQualitySettings::LightingModel::Deferred;
            SceneEngine::LightingParser_ExecuteScene(
                context, parserContext, 
                sceneParser, sceneParser.GetCameraDesc(), qualSettings);
        } else if (settings._lightingType == MaterialVisSettings::LightingType::Forward) {
            qualSettings._lightingModel = SceneEngine::RenderingQualitySettings::LightingModel::Forward;
            SceneEngine::LightingParser_ExecuteScene(
                context, parserContext, sceneParser, sceneParser.GetCameraDesc(), qualSettings);
        }

        return true;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class MaterialVisLayer::Pimpl
    {
    public:
        std::shared_ptr<MaterialVisSettings> _settings;
        std::shared_ptr<VisEnvSettings> _envSettings;
        std::shared_ptr<MaterialVisObject> _object;
    };

    auto MaterialVisLayer::GetInputListener() -> std::shared_ptr<IInputListener>
        { return nullptr; }

    void MaterialVisLayer::RenderToScene(
        IThreadContext& context, 
        SceneEngine::LightingParserContext& parserContext)
    {
        Draw(context, parserContext, *_pimpl->_settings, *_pimpl->_envSettings, *_pimpl->_object);
    }

    void MaterialVisLayer::RenderWidgets(
        IThreadContext& context, 
        Techniques::ParsingContext& parsingContext)
    {}

    void MaterialVisLayer::SetActivationState(bool) {}

    MaterialVisLayer::MaterialVisLayer(
        std::shared_ptr<MaterialVisSettings> settings,
        std::shared_ptr<VisEnvSettings> envSettings,
        std::shared_ptr<MaterialVisObject> object)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_settings = std::move(settings);
        _pimpl->_envSettings = std::move(envSettings);
        _pimpl->_object = std::move(object);
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
        _pendingCameraAlignToModel = false;
    }

    MaterialVisObject::MaterialVisObject()
    {
        _previewMaterialBinding = 0;
    }

    MaterialVisObject::~MaterialVisObject()
    {
    }

}

