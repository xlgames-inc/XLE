// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "GenerateAO.h"
#include "../../BufferUploads/ResourceLocator.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../BufferUploads/DataPacket.h"
#include "../../SceneEngine/LightInternal.h"    // for shadow projection constants;
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../SceneEngine/LightingParser.h"   // for SetFrameGlobalStates
#include "../../RenderCore/Assets/ModelRunTime.h"
#include "../../RenderCore/Assets/ModelImmutableData.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../RenderCore/Assets/SharedStateSet.h"
#include "../../RenderCore/Assets/MeshDatabase.h"
#include "../../RenderCore/Metal/ShaderResource.h"
#include "../../RenderCore/Metal/RenderTargetView.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/CommonBindings.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/StringFormat.h"
#include "../../Math/Transformations.h"
#include "../../Math/ProjectionMath.h"

#include "../../Assets/ChunkFile.h"
#include "../../ConsoleRig/AttachableLibrary.h"
#include "../../RenderCore/Assets/AssetUtils.h"

namespace RenderCore { 
    extern char VersionString[];
    extern char BuildDateString[];
}

namespace ToolsRig
{
    using namespace RenderCore;
    using ModelScaffold = RenderCore::Assets::ModelScaffold;
    using MaterialScaffold = RenderCore::Assets::MaterialScaffold;
    using ModelRenderer = RenderCore::Assets::ModelRenderer;
    using SharedStateSet = RenderCore::Assets::SharedStateSet;
    using namespace RenderCore::Assets::GeoProc;

    class AoGen::Pimpl
    {
    public:
        Desc        _settings;

        using ResLocator = intrusive_ptr<BufferUploads::ResourceLocator>;
        using SRV = Metal::ShaderResourceView;
        using DSV = Metal::DepthStencilView;
        using UAV = Metal::UnorderedAccessView;
        ResLocator  _cubeLocator;
        DSV         _cubeDSV;
        SRV         _cubeSRV;

        ResLocator  _miniLocator;
        UAV         _miniUAV;

        const Metal::ComputeShader* _stepDownShader;
        std::shared_ptr<::Assets::DependencyValidation> _depVal;
    };

    static void SetupCubeMapShadowProjection(
        Metal::DeviceContext& metalContext,
        Techniques::ParsingContext& parserContext,
        float nearClip, float farClip);

    float AoGen::CalculateSkyDomeOcclusion(
        RenderCore::IThreadContext& threadContext,
        const ModelRenderer& renderer,
        RenderCore::Assets::SharedStateSet& sharedStates,
        const RenderCore::Assets::MeshToModel& meshToModel,
        const Float3& samplePoint)
    {
            //
            // We want to calculate a rough approximation for the amount of
            // the sky dome that is occluded at the given sample point.
            //
            // We can use this value to occlude indirect light (assuming that
            // geometry that occludes the sky dome will also occlude indirect
            // light -- which should be reasonable for indirect sources
            // out side of the model).
            //
            // But we can also use this to occlude direct light from the sky
            // dome.
            //
            // To generate the value, we need to render a half-cube-map (we
            // are only interested in illumination coming from above the equator)
            // of depths around the sample point.
            //
            // For each texel in the cube map, if there is a value in the depth
            // texture, then there must be a nearby occluder in that direction. 
            // We weight the texel's occlusion by it's solid angle and sum up
            // all of the occlusion.
            //

            //
            // To render the object, we can use the normal ModelRenderer object.
            // The ModelRenderer might be a bit heavy-weight (we could alternatively
            // just parse through the ModelScaffold (for example, like 
            // MaterialSceneParser::DrawModel)
            // But the ModelRenderer is perhaps the most future-proof approach.
            //
            // We can re-use the shadow rendering path for this (because it works
            // in the same way; with the geometry shader splitting the geometry
            // between viewports, depth-only output & alpha test support)
            //  -- and, after all, we are rendering a kind of shadow
            //

        auto metalContextPtr = Metal::DeviceContext::Get(threadContext);
        auto& metalContext = *metalContextPtr;
        SceneEngine::SavedTargets savedTargets(metalContext);

        metalContext.Clear(_pimpl->_cubeDSV, 1.f, 0u);
        metalContext.Bind(ResourceList<Metal::RenderTargetView, 0>(), &_pimpl->_cubeDSV);

            // configure rendering for the shadow shader
        const auto& settings = _pimpl->_settings;
        const auto& commonRes = Techniques::CommonResources();
        Metal::ViewportDesc viewport(
            0.f, 0.f, float(settings._renderResolution), float(settings._renderResolution), 
            0.f, 1.f);
        metalContext.Bind(viewport);
        metalContext.Bind(commonRes._defaultRasterizer);

        Techniques::TechniqueContext techniqueContext;
        techniqueContext._runtimeState.SetParameter((const utf8*)"SHADOW_CASCADE_MODE", 1u);            // arbitrary projection mode
        techniqueContext._runtimeState.SetParameter((const utf8*)"FRUSTUM_FILTER", 31u);                // enable writing to 5 frustums
        techniqueContext._runtimeState.SetParameter((const utf8*)"OUTPUT_SHADOW_PROJECTION_COUNT", 5u);
        Techniques::ParsingContext parserContext(techniqueContext);

        SceneEngine::SetFrameGlobalStates(metalContext);

            // We shouldn't need to fill in the "global transform" constant buffer
            // The model renderer will manage local transform constant buffers internally,
            // we should only need to set the shadow projection constants.
        
        SetupCubeMapShadowProjection(metalContext, parserContext, settings._minDistance, settings._maxDistance);

            // Render the model onto our cube map surface
        {
            auto captureMarker = sharedStates.CaptureState(metalContext, parserContext.GetStateSetResolver(), parserContext.GetStateSetEnvironment());
            TRY {
                RenderCore::Assets::DelayedDrawCallSet delayedDraws(typeid(ModelRenderer).hash_code());
                renderer.Prepare(
                    delayedDraws, sharedStates, AsFloat4x4(Float3(-samplePoint)), meshToModel);

                ModelRenderer::Sort(delayedDraws);
                for (unsigned c=0; c<unsigned(RenderCore::Assets::DelayStep::Max); ++c)
                    ModelRenderer::RenderPrepared(
                        RenderCore::Assets::ModelRendererContext(
                            metalContext, parserContext, Techniques::TechniqueIndex::ShadowGen),
                        sharedStates, delayedDraws, RenderCore::Assets::DelayStep(c));

            } CATCH(...) {
                savedTargets.ResetToOldTargets(metalContext);
                throw;
            } CATCH_END
            savedTargets.ResetToOldTargets(metalContext);
        }

            //
            // Now we have to read-back the results from the cube map,
            // and weight by the solid angle. Let's do this with a 
            // compute shader. We'll split each face into 16 squares, and
            // assign a separate thread to each. The output will be a 4x4x5
            // texture of floats
            //

        metalContext.BindCS(MakeResourceList(_pimpl->_cubeSRV));
        metalContext.BindCS(MakeResourceList(_pimpl->_miniUAV));

        // metalContext.Bind(*_pimpl->_stepDownShader);
        metalContext.Bind(
            ::Assets::GetAssetDep<Metal::ComputeShader>(
                "game/xleres/toolshelper/aogenprocess.sh:CubeMapStepDown:cs_*"));
        metalContext.Dispatch(1u);

        metalContext.UnbindCS<Metal::ShaderResourceView>(0, 1);
        metalContext.UnbindCS<Metal::UnorderedAccessView>(0, 1);

        auto& bufferUploads = RenderCore::Assets::Services::GetBufferUploads();
        auto readback = bufferUploads.Resource_ReadBack(*_pimpl->_miniLocator);

            // Note that we're currently using the full face for the side faces
            // (as opposed to just half of the face, which would correspond to
            // a hemisphere)
            // We could ignore some of the texels in "readback" to only use
            // the top half of the side faces.
        const float solidAngleFace = 4.f * gPI / 6.f;
        const float solidAngleTotal = 5.f * solidAngleFace;
        float occlusionTotal = 0.f;
        for (unsigned f=0; f<5; ++f) {
            auto pitches = readback->GetPitches(f);
            auto* d = (float*)readback->GetData(f);
            for (unsigned y=0; y<4; ++y)
                for (unsigned x=0; x<4; ++x)
                    occlusionTotal += PtrAdd(d, y*pitches._rowPitch)[x];
        }

            // Our final result is a proportion of the sampled sphere that is 
            // occluded.
        return occlusionTotal / solidAngleTotal;
    }

    AoGen::AoGen(const Desc& settings)
    {
            // _renderResolution must be a multiple of 4 -- this is required
            // for the step-down compute shader to work correctly.
        if ((settings._renderResolution%4)!=0)
            Throw(::Exceptions::BasicLabel("Working texture in AOGen must have dimensions that are a multiple of 4"));

        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_settings = settings;

        const unsigned cubeFaces = 5;

        using namespace BufferUploads;
        auto& bufferUploads = RenderCore::Assets::Services::GetBufferUploads();
        _pimpl->_cubeLocator = bufferUploads.Transaction_Immediate(
            CreateDesc( 
                BindFlag::DepthStencil | BindFlag::ShaderResource,
                0, GPUAccess::Read|GPUAccess::Write,
                TextureDesc::Plain2D(
                    settings._renderResolution, settings._renderResolution, 
                    Metal::NativeFormat::R24G8_TYPELESS, 1, cubeFaces),
                "AoGen"));
        _pimpl->_cubeDSV = Metal::DepthStencilView(_pimpl->_cubeLocator->GetUnderlying(), Metal::NativeFormat::D24_UNORM_S8_UINT, Metal::ArraySlice(cubeFaces));
        _pimpl->_cubeSRV = Metal::ShaderResourceView(_pimpl->_cubeLocator->GetUnderlying(), Metal::NativeFormat::R24_UNORM_X8_TYPELESS, cubeFaces);

        _pimpl->_miniLocator = bufferUploads.Transaction_Immediate(
            CreateDesc( 
                BindFlag::UnorderedAccess,
                0, GPUAccess::Write,
                TextureDesc::Plain2D(4, 4, Metal::NativeFormat::R32_FLOAT, 1, cubeFaces),
                "AoGenMini"));
        _pimpl->_miniUAV = Metal::UnorderedAccessView(_pimpl->_miniLocator->GetUnderlying());

        // _pimpl->_stepDownShader = &::Assets::GetAssetDep<Metal::ComputeShader>(
        //     "game/xleres/toolshelper/aogenprocess.sh:CubeMapStepDown:cs_*");

        _pimpl->_depVal = std::make_shared<::Assets::DependencyValidation>();
        // ::Assets::RegisterAssetDependency(_pimpl->_depVal, _pimpl->_stepDownShader->GetDependencyValidation());
    }

    auto AoGen::GetSettings() const -> const Desc& { return _pimpl->_settings; }

    AoGen::~AoGen() {}

    const std::shared_ptr<::Assets::DependencyValidation>& AoGen::GetDependencyValidation() const
    {
        return _pimpl->_depVal;
    }

    std::vector<unsigned> GetGeoList(const RenderCore::Assets::ModelImmutableData& immData)
    {
        std::vector<unsigned> result;
        const auto& cmdStream = immData._visualScene;
        for (unsigned c = 0; c < cmdStream.GetGeoCallCount(); ++c) {
            const auto& geoCall = cmdStream.GetGeoCall(c);
            result.push_back(geoCall._geoId);
        }

            // remove duplicates
        std::sort(result.begin(), result.end());
        auto newEnd = std::unique(result.begin(), result.end());
        result.erase(newEnd, result.end());

        return std::move(result);
    }
    
    static void WriteSupplementalModel(
        const ::Assets::ResChar destinationFile[],
        IteratorRange<std::pair<unsigned, MeshDatabase>*> meshes,
        const NativeVBSettings& nativeVbSettings);

    void CalculateVertexAO(
        RenderCore::IThreadContext& threadContext,
        const ::Assets::ResChar destinationFile[],
        AoGen& gen,
        const RenderCore::Assets::ModelScaffold& model,
        const RenderCore::Assets::MaterialScaffold& material,
        const ::Assets::DirectorySearchRules* searchRules)
    {
        //
        // For each vertex in the given model, calculate an ambient 
        // occlusion value.
        //
        // Note that we might have problems here when a single mesh is
        // repeated multiple times in the same model (eg, reflected
        // left and right version). Since the ao values is tied to the
        // vertex, the system will attempt to share the same value for
        // each copy (but only one copy will be correct).
        //
        // Also note that there might be problems where there are multiple
        // vertices at the same position. Ideally we want to do a single
        // sample in these cases (even though we still need to store 
        // separate values) because it would look strange if we saw large
        // divergence in AO values at one point.
        //
        // We could push the sample points some distance long the vertex
        // normals. We still want small variations in the surface to have an
        // effect on the AO -- so we can't go to far. We also don't want to 
        // push the same point into the inside of some other structure.
        // Some geometry will be double-sided -- but there should still be
        // a normal to push along.
        //
        // Also note that sometimes we may be generating the AO for vertices
        // that are actually invisible (eg, pixels at that vertex are rejected
        // by alpha testing). In those cases the vertex AO is only used by 
        // interpolation (and is likely to be close to 1.f, anyway).
        //

        // Go through the geo calls in the input scaffold, and find the 
        // mesh instances. We need to know the vertex positions and normals
        // in those meshes.

        const auto& settings = gen.GetSettings();
        const float duplicatesThreshold = settings._duplicatesThreshold;
        const float normalsPushOut      = settings._samplePushOut;
        const float powerExaggerate     = settings._powerExaggerate;

            // Setup the rendering objects we'll need
            // We're going to be use a ModelRenderer to do the rendering
            // for calculating the AO -- so we have to create that now.
        SharedStateSet sharedStates;
        auto renderer = std::make_unique<ModelRenderer>(
            model, material, ModelRenderer::Supplements(), sharedStates, searchRules);

            // we need to stall while pending...
        auto& asyncMan = ::Assets::Services::GetAsyncMan();
        auto& bufferUploads = RenderCore::Assets::Services::GetBufferUploads();
        for (;;) {
            auto state = renderer->TryResolve();
            if (state == ::Assets::AssetState::Ready) break;
            if (state == ::Assets::AssetState::Invalid)
                Throw(::Assets::Exceptions::InvalidAsset(model.Filename().c_str(), "Got invalid asset while performing AO gen"));
            
                // stall...!
            asyncMan.Update();
            bufferUploads.Update(threadContext);
        }

        RenderCore::Assets::MeshToModel meshToModel(model);

            // We're going to be reading the vertex data directly from the
            // file on disk. We'll use a memory mapped file to access that
            // so we need to open that now...
        std::vector<std::pair<unsigned, MeshDatabase>> meshes;
        
        {
            MemoryMappedFile file(model.Filename().c_str(), 0ull, MemoryMappedFile::Access::Read);

            const auto& immData = model.ImmutableData();
            auto geoIndicies = GetGeoList(immData);
            for (auto c=geoIndicies.cbegin(); c!=geoIndicies.cend(); ++c) {
                auto& rawGeo = immData._geos[*c];
                if (rawGeo._drawCalls.empty() || rawGeo._vb._size==0 || rawGeo._vb._ia._vertexStride==0) continue;

                auto vbStart = model.LargeBlocksOffset() + rawGeo._vb._offset;
                auto vbEnd = vbStart + rawGeo._vb._size;
                auto vertexCount = rawGeo._vb._size / rawGeo._vb._ia._vertexStride;

                MeshDatabase mesh;

                    // Material & index buffer are irrelevant.
                    // Find the normal and position streams, and add to
                    // our mesh database adapter.
                const auto& vbIA = rawGeo._vb._ia;
                for (unsigned e=0; e<unsigned(vbIA._elements.size()); ++e) {
                    const auto& ele = vbIA._elements[e];
                    if (    (XlEqStringI(ele._semanticName, "POSITION") && ele._semanticIndex == 0)
                        ||  (XlEqStringI(ele._semanticName, "NORMAL") && ele._semanticIndex == 0)) {

                        auto rawSource = CreateRawDataSource(
                            PtrAdd(file.GetData(), vbStart + ele._alignedByteOffset),
                            PtrAdd(file.GetData(), vbEnd),
                            vertexCount, vbIA._vertexStride, 
                            Metal::NativeFormat::Enum(ele._nativeFormat));

                        mesh.AddStream(
                            std::move(rawSource),
                            std::vector<unsigned>(),
                            vbIA._elements[e]._semanticName, vbIA._elements[e]._semanticIndex);
                    }
                }

                auto posElement = mesh.FindElement("POSITION");
                if (posElement == ~0u) {
                    LogWarning << "No vertex positions found in mesh! Cannot calculate AO for this mesh.";
                    continue;
                }

                    // Compress the positions stream so that identical positions are
                    // combined into one. 
                    // Once that is done, we need to be able to find all of the normals
                    // associated with each point.

                const auto& stream = mesh.GetStream(posElement);
                std::vector<unsigned> remapping;
                auto newSource = RemoveDuplicates(
                    remapping, stream.GetSourceData(), 
                    stream.GetVertexMap(), duplicatesThreshold);

                mesh.RemoveStream(posElement);
                posElement = mesh.AddStream(newSource, std::move(remapping), "POSITION", 0);

                auto nEle = mesh.FindElement("NORMAL");
                if (nEle == ~0u) {
                    LogWarning << "No vertex normals found in mesh! Cannot calculate AO for this mesh.";
                    continue;
                }

                const auto& pStream = mesh.GetStream(posElement);
                const auto& nStream = mesh.GetStream(nEle);
                std::vector<std::pair<unsigned, unsigned>> pn;
                pn.reserve(mesh.GetUnifiedVertexCount());
                for (unsigned q=0; q<mesh.GetUnifiedVertexCount(); ++q)
                    pn.push_back(
                        std::make_pair(pStream.UnifiedToStream(q), nStream.UnifiedToStream(q)));
            
                    // sort by position index
                std::sort(pn.begin(), pn.end(), CompareFirst<unsigned, unsigned>());

                    // find the final sample points
                std::vector<Float3> samplePoints;
                samplePoints.resize(pStream.GetSourceData().GetCount(), Float3(FLT_MAX, FLT_MAX, FLT_MAX));

                for (auto p=pn.cbegin(); p!=pn.cend();) {
                    auto p2 = p+1;
                    while (p2!=pn.cend() && p2->first == p->first) ++p2;

                    auto n = Zero<Float3>();
                    for (auto q=p; q<p2; ++q)
                        n += GetVertex<Float3>(nStream.GetSourceData(), q->second);
                    n = Normalize(n);

                    auto baseSamplePoint = GetVertex<Float3>(pStream.GetSourceData(), p->first);
                    auto samplePoint = baseSamplePoint + normalsPushOut * n;
                    samplePoints[p->first] = samplePoint;

                    p = p2;
                }

                    // Now we can actually perform the AO 
                    // calculation to generate the final occlusion values
                    // This should be the most expensive part.
                std::vector<uint8> aoValues;
                aoValues.reserve(samplePoints.size());
            
                LogInfo << "Starting AO gen of " << samplePoints.size() << " pts";
                for (size_t p=0; p!=samplePoints.size(); ++p) {
                    float skyDomeOcc =
                        gen.CalculateSkyDomeOcclusion(
                            threadContext, 
                            *renderer, sharedStates, meshToModel,
                            samplePoints[p]);

                        // CalculateSkyDomeOcclusion returned the quantity of the skydome that is occluded
                        // We want to write the complement of this value (1.0f - occlusion) to the vertex
                        // buffer.
                    skyDomeOcc = 1.f-skyDomeOcc;
                    skyDomeOcc = std::pow(skyDomeOcc, powerExaggerate);
                    auto finalValue = (uint8)Clamp(skyDomeOcc * float(0xff), 0.f, float(0xff));
                    aoValues.push_back(finalValue);

                    if ((p % 100)==0)
                        LogInfo << "Generated " << p << "/" << samplePoints.size() << " AO sample points";
                }
                LogInfo << "Finished AO gen";

                    // Note that when we using the vertex map here, it should 
                    // guaranteed us a final VB that matches the vertices from the input
                MeshDatabase meshSupp;
                meshSupp.AddStream(
                    CreateRawDataSource(
                        std::move(aoValues),
                        samplePoints.size(), sizeof(uint8),
                        Metal::NativeFormat::R8_UNORM),
                    std::vector<unsigned>(pStream.GetVertexMap()), // shares the same vertex map
                    "PER_VERTEX_AO", 0);

                meshes.push_back(std::make_pair(*c, std::move(meshSupp)));
            }
        }

        // Now we have a list of meshes containing supplemental vertex streams
        // we need to write these to a ModelSupplementImmutableData asset

        WriteSupplementalModel(destinationFile, MakeIteratorRange(meshes), NativeVBSettings { true });
    }

    static void WriteSupplementalModel(
        const ::Assets::ResChar destinationFile[],
        IteratorRange<std::pair<unsigned, MeshDatabase>*> meshes,
        const NativeVBSettings& nativeVbSettings)
    {
        std::vector<uint8> largeResourcesChunk;
        std::unique_ptr<uint8[]> scaffoldChunk;
        size_t scaffoldChunkSize = 0;

        {
            Serialization::NascentBlockSerializer suppArray;
            for (auto m=meshes.begin(); m!=meshes.end(); ++m) {
                ::Serialize(suppArray, m->first);

                NativeVBLayout vbLayout = BuildDefaultLayout(m->second, nativeVbSettings);
                auto nativeVB = m->second.BuildNativeVertexBuffer(vbLayout);

                auto vbOffset = largeResourcesChunk.size();
                auto vbSize = nativeVB.size();
                largeResourcesChunk.insert(largeResourcesChunk.end(), nativeVB.begin(), nativeVB.end());

                auto inputAssembly = RenderCore::Assets::CreateGeoInputAssembly(
                    vbLayout._elements, (unsigned)vbLayout._vertexStride);
                ::Serialize(
                    suppArray, 
                    RenderCore::Assets::VertexData { inputAssembly, unsigned(vbOffset), unsigned(vbSize) });
            }

            Serialization::NascentBlockSerializer supplementImmutableData;
            supplementImmutableData.SerializeSubBlock(suppArray);
            ::Serialize(supplementImmutableData, size_t(meshes.size()));

            scaffoldChunk = supplementImmutableData.AsMemoryBlock();
            scaffoldChunkSize = supplementImmutableData.Size();
        }
    
        {
            using namespace Serialization::ChunkFile;
            SimpleChunkFileWriter file(
                2, RenderCore::VersionString, RenderCore::BuildDateString,
                std::make_tuple(destinationFile, "wb", 0));

            file.BeginChunk(
                RenderCore::Assets::ChunkType_ModelScaffold, 
                0, "AOSupplement");
            file.Write(scaffoldChunk.get(), scaffoldChunkSize, 1);
            file.FinishCurrentChunk();

            file.BeginChunk(
                RenderCore::Assets::ChunkType_ModelScaffoldLargeBlocks, 
                0, "AOSupplement");
            file.Write(AsPointer(largeResourcesChunk.cbegin()), largeResourcesChunk.size(), 1);
            file.FinishCurrentChunk();
        }
    }


    static void SetupCubeMapShadowProjection(
        Metal::DeviceContext& metalContext,
        Techniques::ParsingContext& parserContext,
        float nearClip, float farClip)
    {
            // set 5 faces of the cubemap tr
        auto basicProj = PerspectiveProjection(
            -1.f, -1.f, 1.f, 1.f, nearClip, farClip,
            Techniques::GetDefaultClipSpaceType());

        Float4x4 cubeViewMatrices[6] = 
        {
            MakeCameraToWorld(Float3( 0.f,  0.f,  1.f), Float3( 1.f,  0.f,  0.f), Zero<Float3>()),
            MakeCameraToWorld(Float3( 1.f,  0.f,  0.f), Float3( 0.f,  0.f,  1.f), Zero<Float3>()),
            MakeCameraToWorld(Float3( 0.f,  1.f,  0.f), Float3( 0.f,  0.f,  1.f), Zero<Float3>()),
            MakeCameraToWorld(Float3(-1.f,  0.f,  0.f), Float3( 0.f,  0.f,  1.f), Zero<Float3>()),
            MakeCameraToWorld(Float3( 0.f, -1.f,  0.f), Float3( 0.f,  0.f,  1.f), Zero<Float3>()),
            MakeCameraToWorld(Float3( 0.f,  0.f, -1.f), Float3(-1.f,  0.f,  0.f), Zero<Float3>())
        };

        SceneEngine::CB_ArbitraryShadowProjection shadowProj;
        shadowProj._projectionCount = 5;
        for (unsigned c=0; c<dimof(cubeViewMatrices); ++c) {
            shadowProj._worldToProj[c] = Combine(InvertOrthonormalTransform(cubeViewMatrices[c]), basicProj);
            shadowProj._minimalProj[c] = ExtractMinimalProjection(shadowProj._worldToProj[c]);
        }

        parserContext.SetGlobalCB(
            metalContext, Techniques::TechniqueContext::CB_ShadowProjection,
            &shadowProj, sizeof(shadowProj));
    }


///////////////////////////////////////////////////////////////////////////////////////////////////

    class AOSupplementCompiler::Pimpl
    {
    public:
        std::shared_ptr<RenderCore::IThreadContext> _threadContext;
        std::unique_ptr<AoGen> _aoGen;
    };

    auto AOSupplementCompiler::PerformCompile(
        const ::Assets::ResChar modelFilename[], const ::Assets::ResChar materialFilename[],
        const ::Assets::ResChar destinationFile[]) -> CompileResult
    {
        const auto& model    = ::Assets::GetAssetComp<ModelScaffold>(modelFilename);
        const auto& material = ::Assets::GetAssetComp<MaterialScaffold>(materialFilename, modelFilename);
        auto searchRules     = ::Assets::DefaultDirectorySearchRules(modelFilename);

        CalculateVertexAO(
            *_pimpl->_threadContext, destinationFile,
            *_pimpl->_aoGen, model, material, &searchRules);

        using Store = ::Assets::IntermediateAssets::Store;
        return CompileResult 
            {
                std::vector<::Assets::DependentFileState>
                    {
                        Store::GetDependentFileState(model.Filename().c_str()),
                        Store::GetDependentFileState(material.Filename().c_str())
                    },
                MakeFileNameSplitter(model.Filename()).DriveAndPath().AsString()
            };
    }
    
    std::shared_ptr<::Assets::PendingCompileMarker> 
        AOSupplementCompiler::PrepareAsset(
            uint64 typeCode, 
            const ::Assets::ResChar* initializers[], unsigned initializerCount,
            const ::Assets::IntermediateAssets::Store& store)
    {
        if (initializerCount != 2 || !initializers[0][0] || !initializers[1][0]) 
            Throw(::Exceptions::BasicLabel("Expecting exactly 2 initializers in AOSupplementCompiler. Model filename first, then material filename"));

        const auto* modelFilename = initializers[0], *materialFilename = initializers[1];

            // build the intermediate name from our initializers
        using namespace ::Assets;
        ResChar intermediateName[MaxPath];
        store.MakeIntermediateName(intermediateName, modelFilename);
        StringMeldAppend(intermediateName)
            << "-" << MakeFileNameSplitter(materialFilename).File().AsString() << "-ao";

            // check for an existing asset that is up-to-date and can be used immediately...
        auto marker = CompilerHelper::CheckExistingAsset(store, intermediateName);
        if (marker) return marker;

            // if it doesn't exist, we have to perform a compile
            // then return a marker for the new asset
        auto deps = PerformCompile(modelFilename, materialFilename, intermediateName);
        return CompilerHelper::PrepareCompileMarker(store, intermediateName, deps);
    }

    void AOSupplementCompiler::StallOnPendingOperations(bool)
    {
        // everything is done in the foreground in this compiler; so nothing
        // to stall for
    }

    AOSupplementCompiler::AOSupplementCompiler(std::shared_ptr<RenderCore::IThreadContext> threadContext)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_threadContext = threadContext;
        ToolsRig::AoGen::Desc settings(
            0.001f, 10.f, 128,
            2.f, 0.01f, 0.02f);
        _pimpl->_aoGen = std::make_unique<ToolsRig::AoGen>(settings);
    }

    AOSupplementCompiler::~AOSupplementCompiler() {}

}

