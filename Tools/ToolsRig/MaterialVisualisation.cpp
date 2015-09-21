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
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../RenderCore/Techniques/ResourceBox.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/InputLayout.h"

#include "../../RenderCore/Assets/ModelRunTime.h"
#include "../../RenderCore/Assets/ModelRunTimeInternal.h"

#include "../../Assets/IntermediateAssets.h"

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

        Metal::VertexBuffer _cubeBuffer;
        Metal::VertexBuffer _sphereBuffer;
        unsigned _cubeVCount;
        unsigned _sphereVCount;

        CachedVisGeo(const Desc&);
    };

    CachedVisGeo::CachedVisGeo(const Desc&)
    {
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
        void ExecuteScene(  Metal::DeviceContext* context, 
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

        void ExecuteShadowScene(    Metal::DeviceContext* context, 
                                    SceneEngine::LightingParserContext& parserContext, 
                                    const SceneEngine::SceneParseSettings& parseSettings,
                                    unsigned index, unsigned techniqueIndex) const
        {
            ExecuteScene(context, parserContext, parseSettings, techniqueIndex);
        }

        void Draw(  Metal::DeviceContext& metalContext, 
                    SceneEngine::LightingParserContext& parserContext,
                    unsigned techniqueIndex) const
        {
            TRY
            {
                if (techniqueIndex!=3)
                    metalContext.Bind(Techniques::CommonResources()._defaultRasterizer);

                    // disable blending to avoid problem when rendering single component stuff 
                    //  (ie, nodes that output "float", not "float4")
                metalContext.Bind(Techniques::CommonResources()._blendOpaque);
            
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

                } else if (geoType == MaterialVisSettings::GeometryType::Model) {

                    DrawModel(metalContext, parserContext, techniqueIndex);

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
                    } else return;
                
                    metalContext.Bind(Metal::Topology::TriangleList);
                    metalContext.Draw(count);

                }
            } 
            CATCH(const ::Assets::Exceptions::InvalidAsset& e) { parserContext.Process(e); }
            CATCH(const ::Assets::Exceptions::PendingAsset& e) { parserContext.Process(e); }
            CATCH_END
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

    static void BindVertexBuffer(
        Metal::DeviceContext& metalContext,
        const RenderCore::Assets::ModelScaffold& scaffold,
        RenderCore::Assets::VertexData& vb, unsigned vertexStride)
    {
        auto buffer = std::make_unique<uint8[]>(vb._size);
        
        {
            BasicFile inputFile(scaffold.Filename().c_str(), "rb");
            inputFile.Seek(scaffold.LargeBlocksOffset() + vb._offset, SEEK_SET);
            inputFile.Read(buffer.get(), vb._size, 1);
        }

        Metal::VertexBuffer metalVB(buffer.get(), vb._size);
        metalContext.Bind(MakeResourceList(metalVB), vertexStride);
    }

    static void BindVertexBuffer(
        Metal::DeviceContext& metalContext,
        const RenderCore::Assets::ModelScaffold& scaffold,
        RenderCore::Assets::VertexData& vb0, unsigned vertexStride0,
        RenderCore::Assets::VertexData& vb1, unsigned vertexStride1)
    {
        auto buffer = std::make_unique<uint8[]>(vb0._size + vb1._size);
        
        {
            BasicFile inputFile(scaffold.Filename().c_str(), "rb");
            inputFile.Seek(scaffold.LargeBlocksOffset() + vb0._offset, SEEK_SET);
            inputFile.Read(buffer.get(), vb0._size, 1);

            inputFile.Seek(scaffold.LargeBlocksOffset() + vb1._offset, SEEK_SET);
            inputFile.Read(PtrAdd(buffer.get(), vb0._size), vb1._size, 1);
        }

        Metal::VertexBuffer metalVB(buffer.get(), vb0._size + vb1._size);
        const Metal::VertexBuffer* vbs[] = { &metalVB, &metalVB };
        const unsigned strides[] = { vertexStride0, vertexStride1 };
        const unsigned offsets[] = { 0, vb0._size };
        metalContext.Bind(0, dimof(vbs), vbs, strides, offsets);
    }

    static void BindIndexBuffer(
        Metal::DeviceContext& metalContext,
        const RenderCore::Assets::ModelScaffold& scaffold,
        RenderCore::Assets::IndexData& ib)
    {
        auto buffer = std::make_unique<uint8[]>(ib._size);
        
        {
            BasicFile inputFile(scaffold.Filename().c_str(), "rb");
            inputFile.Seek(scaffold.LargeBlocksOffset() + ib._offset, SEEK_SET);
            inputFile.Read(buffer.get(), ib._size, 1);
        }

        Metal::IndexBuffer metalIB(buffer.get(), ib._size);
        metalContext.Bind(metalIB, (Metal::NativeFormat::Enum)ib._format);
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

        auto& compilers = ::Assets::Services::GetAsyncMan().GetIntermediateCompilers();
        auto& store = ::Assets::Services::GetAsyncMan().GetIntermediateStore();
        auto skinMarker = compilers.PrepareAsset(RenderCore::Assets::ModelScaffold::CompileProcessType, &modelFile, 1, store);
        auto skelMarker = compilers.PrepareAsset(RenderCore::Assets::SkeletonScaffold::CompileProcessType, &modelFile, 1, store);

        skinMarker->StallWhilePending();
        skelMarker->StallWhilePending();

        const auto& model = ::Assets::GetAsset<ModelScaffold>(skinMarker->_sourceID0);
        const auto& skeleton = ::Assets::GetAsset<SkeletonScaffold>(skelMarker->_sourceID0);

        SkeletonBinding skelBinding(
            skeleton.GetTransformationMachine().GetOutputInterface(),
            model.CommandStream().GetInputInterface());

        auto transformCount = skeleton.GetTransformationMachine().GetOutputMatrixCount();
        auto skelTransforms = std::make_unique<Float4x4[]>(transformCount);
        skeleton.GetTransformationMachine().GenerateOutputTransforms(
            skelTransforms.get(), transformCount,
            &skeleton.GetTransformationMachine().GetDefaultParameters());

        const auto& cmdStream = model.CommandStream();
        const auto& immData = model.ImmutableData();
        for (unsigned c = 0; c < cmdStream.GetGeoCallCount(); ++c) {
            const auto& geoCall = cmdStream.GetGeoCall(c);
            
            auto& rawGeo = immData._geos[geoCall._geoId];
            for (unsigned d = 0; d < rawGeo._drawCallsCount; ++d) {
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

                Metal::InputElementDesc metalVertInput[16];
                unsigned eleCount = BuildLowLevelInputAssembly(metalVertInput, dimof(metalVertInput),
                    rawGeo._vb._ia._elements, rawGeo._vb._ia._elementCount);

                IMaterialBinder::SystemConstants sysContants = _object->_systemConstants;
                sysContants._objectToWorld = skelTransforms[
                    skelBinding._modelJointIndexToMachineOutput[geoCall._transformMarker]];

                auto shaderProgram = _object->_materialBinder->Apply(
                    metalContext, parserContext, techniqueIndex,
                    _object->_parameters, sysContants, 
                    _object->_searchRules, std::make_pair(metalVertInput, eleCount));
                if (!shaderProgram) return;

                BindVertexBuffer(
                    metalContext, model, 
                    rawGeo._vb, rawGeo._vb._ia._vertexStride);
                BindIndexBuffer(metalContext, model, rawGeo._ib);

                metalContext.Bind((Metal::Topology::Enum)drawCall._topology);
                metalContext.DrawIndexed(drawCall._indexCount, drawCall._firstIndex, drawCall._firstVertex);
            }
        }

        for (unsigned c = 0; c < cmdStream.GetSkinCallCount(); ++c) {
            const auto& geoCall = cmdStream.GetSkinCall(c);
            
            auto& rawGeo = immData._boundSkinnedControllers[geoCall._geoId];
            for (unsigned d = 0; d < rawGeo._drawCallsCount; ++d) {
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

                Metal::InputElementDesc metalVertInput[16];
                unsigned eleCount = BuildLowLevelInputAssembly(metalVertInput, dimof(metalVertInput),
                    rawGeo._vb._ia._elements, rawGeo._vb._ia._elementCount, 0);
                eleCount += BuildLowLevelInputAssembly(
                    metalVertInput + eleCount, dimof(metalVertInput) - eleCount,
                    rawGeo._animatedVertexElements._ia._elements, rawGeo._animatedVertexElements._ia._elementCount, 1);

                IMaterialBinder::SystemConstants sysContants = _object->_systemConstants;
                sysContants._objectToWorld = skelTransforms[
                    skelBinding._modelJointIndexToMachineOutput[geoCall._transformMarker]];

                auto shaderProgram = _object->_materialBinder->Apply(
                    metalContext, parserContext, techniqueIndex,
                    _object->_parameters, sysContants, 
                    _object->_searchRules, std::make_pair(metalVertInput, eleCount));
                if (!shaderProgram) return;

                BindVertexBuffer(
                    metalContext, model, 
                    rawGeo._vb, rawGeo._vb._ia._vertexStride,
                    rawGeo._animatedVertexElements, rawGeo._animatedVertexElements._ia._vertexStride);
                BindIndexBuffer(metalContext, model, rawGeo._ib);

                metalContext.Bind((Metal::Topology::Enum)drawCall._topology);
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
        TRY {

                // if we need to reset the camera, do so now...
            if (settings._pendingCameraAlignToModel) {
                if (settings._geometryType == MaterialVisSettings::GeometryType::Model && !object._previewModelFile.empty()) {
                        // this is more tricky... when using a model, we have to get the bounding box for the model
                    using namespace RenderCore::Assets;
                    auto& compilers = ::Assets::Services::GetAsyncMan().GetIntermediateCompilers();
                    auto& store = ::Assets::Services::GetAsyncMan().GetIntermediateStore();
                    const ::Assets::ResChar* modelFile = object._previewModelFile.c_str();
                    auto skinMarker = compilers.PrepareAsset(RenderCore::Assets::ModelScaffold::CompileProcessType, &modelFile, 1, store);
                    const auto& model = ::Assets::GetAsset<ModelScaffold>(skinMarker->_sourceID0);
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
        CATCH (::Assets::Exceptions::PendingAsset& e) { parserContext.Process(e); }
        CATCH (::Assets::Exceptions::InvalidAsset& e) { parserContext.Process(e); }
        CATCH_END

        return false;
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
        IThreadContext* context, 
        SceneEngine::LightingParserContext& parserContext)
    {
        assert(context);
        Draw(*context, parserContext, *_pimpl->_settings, *_pimpl->_envSettings, *_pimpl->_object);
    }

    void MaterialVisLayer::RenderWidgets(
        IThreadContext* context, 
        const Techniques::ProjectionDesc& projectionDesc)
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

