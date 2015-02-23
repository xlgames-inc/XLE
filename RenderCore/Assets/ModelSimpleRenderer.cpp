// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "ModelSimple.h"
#include "ModelRunTimeInternal.h"
#include "SharedStateSet.h"
#include "AssetUtils.h"

#include "../Metal/InputLayout.h"
#include "../Metal/DeviceContext.h"
#include "../Metal/DeviceContextImpl.h"
#include "../Metal/Buffer.h"
#include "../RenderUtils.h"
#include "../../SceneEngine/Techniques.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/VegetationSpawn.h"
#include "../../SceneEngine/ResourceBox.h"

#include "../../Math/Matrix.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../ConsoleRig/Log.h"

#include "../DX11/Metal/IncludeDX11.h"

namespace RenderCore { namespace Assets { namespace Simple 
{

    ////////////////////////////////////////////////////////////////////////////////////////////////

    class ModelRenderer::Pimpl
    {
    public:
        class Mesh
        {
        public:
                // each mesh within the model can have a different vertex input interface
                //  (and so needs a different technique interface)
            int         _id;
            unsigned    _vbOffset, _ibOffset;
            unsigned    _vertexStride;
            Metal::NativeFormat::Enum       _indexFormat;
            std::vector<ScaffoldDrawCall>   _drawCalls;
        };

        std::vector<Metal::DeferredShaderResource*> _boundTextures;
        size_t                  _texturesPerMaterial;
        
            //  Ordered by draw calls, as we encounter them while rendering
            //  each mesh.
        std::vector<unsigned>   _resourcesIndices;
        std::vector<unsigned>   _techniqueInterfaceIndices;
        std::vector<unsigned>   _shaderNameIndices;
        std::vector<unsigned>   _materialParameterBoxIndices;
        std::vector<unsigned>   _geoParameterBoxIndices;
        std::vector<unsigned>   _constantBufferIndices;

        Metal::VertexBuffer     _vertexBuffer;
        Metal::IndexBuffer      _indexBuffer;
        std::vector<Mesh>       _meshes;
        std::vector<Metal::ConstantBuffer>  _constantBuffers;

        Pimpl() {}
    };

    static const std::string StringSpawnedInstance("SPAWNED_INSTANCE");
    
    void    ModelRenderer::Render(
            Metal::DeviceContext* context, 
            SceneEngine::LightingParserContext& parserContext,
            unsigned techniqueIndex,
            const SharedStateSet& sharedStateSet,
            const Float4x4& modelToWorld, unsigned vegetationSpawnObjectIndex)
    {
            //  use the transformation machine to build all of the transformations we'll
            //  need. Then render all of the meshes.
        auto& scaffold = _scaffold->_lods[_scaffoldLODIndex];
        auto* transMachine = (TransformationMachine*)Serialization::Block_GetFirstObject(scaffold._transformMachine.get());

        Float4x4 transforms[512];
        transMachine->GenerateOutputTransforms(transforms, dimof(transforms), nullptr);

            // At the moment, all models using "triangle list"
            //  ... though I suppose we would need to use patches for tessellated geometry
        // context->Bind(Metal::Topology::TriangleList);

        static Metal::ConstantBuffer localTransformBuffer(nullptr, sizeof(LocalTransformConstants));     // this should go into some kind of global resource heap
        const Metal::ConstantBuffer* pkts[] = { &localTransformBuffer, nullptr };

        unsigned currentResourceIndex = ~unsigned(0x0);
        unsigned currentConstantBufferIndex = ~unsigned(0x0);
        Metal::BoundUniforms* currentBoundUniforms = nullptr;

        if (vegetationSpawnObjectIndex)
            parserContext.GetTechniqueContext()._runtimeState.SetParameter(StringSpawnedInstance, 1);

        TRY
        {

            unsigned drawCallIndex = 0;
            for (auto i=scaffold._meshCalls.cbegin(); i!=scaffold._meshCalls.cend(); ++i) {

                if (!i->_materialId) continue;
            
                auto cm = std::find_if(_pimpl->_meshes.cbegin(), _pimpl->_meshes.cend(), [=](const Pimpl::Mesh& m) { return m._id == i->_meshId; });
                if (cm == _pimpl->_meshes.cend()) continue; // some meshes can be excluded if they are not renderable meshes

                auto& localToModel = transforms[i->_transformMarker];
                // auto localToModel = Identity<Float4x4>();
                auto trans = MakeLocalTransform(Combine(localToModel, modelToWorld), ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld));
                localTransformBuffer.Update(*context, &trans, sizeof(trans));

                context->Bind(_pimpl->_indexBuffer, cm->_indexFormat, cm->_ibOffset);
                context->Bind(ResourceList<Metal::VertexBuffer, 1>(std::make_tuple(std::ref(_pimpl->_vertexBuffer))), cm->_vertexStride, cm->_vbOffset);

                for (auto d = cm->_drawCalls.cbegin(); d!=cm->_drawCalls.cend(); ++d, ++drawCallIndex) {

                    auto techInterfIndex        = _pimpl->_techniqueInterfaceIndices[drawCallIndex];
                    auto shaderNameIndex        = _pimpl->_shaderNameIndices[drawCallIndex];
                    auto materialParamIndex     = _pimpl->_materialParameterBoxIndices[drawCallIndex];
                    auto geoParamIndex          = _pimpl->_geoParameterBoxIndices[drawCallIndex];

                    auto* boundUniforms = sharedStateSet.BeginVariation(
                        context, parserContext.GetTechniqueContext(), techniqueIndex,
                        shaderNameIndex, techInterfIndex, geoParamIndex, materialParamIndex);

                    auto resourceIndex          = _pimpl->_resourcesIndices[drawCallIndex];
                    auto constantBufferIndex    = _pimpl->_constantBufferIndices[drawCallIndex];

                    if (boundUniforms != currentBoundUniforms || resourceIndex != currentResourceIndex || constantBufferIndex != currentConstantBufferIndex) {
                        if (boundUniforms) {
                            const Metal::ShaderResourceView* srvs[16];
                            assert(_pimpl->_texturesPerMaterial <= dimof(srvs));
                            for (unsigned c=0; c<_pimpl->_texturesPerMaterial; c++) {
                                auto* t = _pimpl->_boundTextures[resourceIndex * _pimpl->_texturesPerMaterial + c];
                                srvs[c] = t?(&t->GetShaderResource()):nullptr;
                            }
                            pkts[1] = &_pimpl->_constantBuffers[constantBufferIndex];
                            boundUniforms->Apply(
                                *context, parserContext.GetGlobalUniformsStream(),
                                RenderCore::Metal::UniformsStream(nullptr, pkts, dimof(pkts), srvs, _pimpl->_texturesPerMaterial));
                        }

                        currentResourceIndex = resourceIndex;
                        currentConstantBufferIndex = constantBufferIndex;
                        currentBoundUniforms = boundUniforms;
                    }
            
                    if (vegetationSpawnObjectIndex) {
                        SceneEngine::VegetationSpawn_DrawInstances(context, parserContext, vegetationSpawnObjectIndex, d->_indexCount, d->_firstIndex, 0);
                    } else 
                        context->DrawIndexed(d->_indexCount, d->_firstIndex, 0);
                }
            }

        } CATCH (...) {
            if (vegetationSpawnObjectIndex)
                parserContext.GetTechniqueContext()._runtimeState.SetParameter(StringSpawnedInstance, 0);
            throw;
        } CATCH_END

        if (vegetationSpawnObjectIndex)
            parserContext.GetTechniqueContext()._runtimeState.SetParameter(StringSpawnedInstance, 0);
    }

    class ModelRenderer::SortedModelDrawCalls::Entry
    {
    public:
        unsigned        _shaderVariationHash;

        ModelRenderer*  _renderer;
        unsigned        _drawCallIndex;
        Float4x4        _modelToWorld;

        unsigned        _indexCount, _firstIndex;
        const ModelRenderer::Pimpl::Mesh * _cm;
    };

    void ModelRenderer::SortedModelDrawCalls::Reset() 
    {
        _entries.erase(_entries.begin(), _entries.end());
    }

    ModelRenderer::SortedModelDrawCalls::SortedModelDrawCalls() 
    {
        _entries.reserve(10*1000);
    }

    ModelRenderer::SortedModelDrawCalls::~SortedModelDrawCalls() {}

    bool CompareDrawCall(const ModelRenderer::SortedModelDrawCalls::Entry& lhs, const ModelRenderer::SortedModelDrawCalls::Entry& rhs)
    {
        if (lhs._shaderVariationHash == rhs._shaderVariationHash) {
            if (lhs._renderer == rhs._renderer) {
                if (lhs._cm == rhs._cm) {
                    return lhs._drawCallIndex < rhs._drawCallIndex;
                }
                return lhs._cm < rhs._cm;
            }
            return lhs._renderer < rhs._renderer;
        }
        return lhs._shaderVariationHash < rhs._shaderVariationHash; 
    }

    void    ModelRenderer::Prepare(SortedModelDrawCalls& dest, const SharedStateSet& sharedStateSet, const Float4x4& modelToWorld)
    {
            //      After culling; submit all of the draw-calls in this mesh to a list to be sorted
        auto& scaffold = _scaffold->_lods[_scaffoldLODIndex];
        auto* transMachine = (TransformationMachine*)Serialization::Block_GetFirstObject(scaffold._transformMachine.get());
        Float4x4 transforms[512];
        transMachine->GenerateOutputTransforms(transforms, dimof(transforms), nullptr);

        for (auto i=scaffold._meshCalls.cbegin(); i!=scaffold._meshCalls.cend(); ++i) {
            if (!i->_materialId) continue;

            auto cm = std::find_if(_pimpl->_meshes.cbegin(), _pimpl->_meshes.cend(), [=](const Pimpl::Mesh& m) { return m._id == i->_meshId; });
            if (cm == _pimpl->_meshes.cend()) continue;

            auto& localToModel = transforms[i->_transformMarker];
            auto localToWorld = Combine(localToModel, modelToWorld);

            unsigned drawCallIndex = 0;
            for (auto d = cm->_drawCalls.cbegin(); d!=cm->_drawCalls.cend(); ++d, ++drawCallIndex) {
                auto techInterfIndex        = _pimpl->_techniqueInterfaceIndices[drawCallIndex];
                auto shaderNameIndex        = _pimpl->_shaderNameIndices[drawCallIndex];
                auto materialParamIndex     = _pimpl->_materialParameterBoxIndices[drawCallIndex];
                auto geoParamIndex          = _pimpl->_geoParameterBoxIndices[drawCallIndex];

                SortedModelDrawCalls::Entry entry;
                entry._drawCallIndex = drawCallIndex;
                entry._renderer = this;
                entry._modelToWorld = localToWorld;
                entry._shaderVariationHash = techInterfIndex ^ (geoParamIndex << 12) ^ (materialParamIndex << 15) ^ (shaderNameIndex << 24);  // simple hash of these indices. Note that collisions might be possible
                entry._indexCount = d->_indexCount;
                entry._firstIndex = d->_firstIndex;
                entry._cm = &(*cm);

                dest._entries.push_back(entry);
            }
        }
    }

    void ModelRenderer::RenderPrepared(
        SortedModelDrawCalls& drawCalls, Metal::DeviceContext* context, 
        SceneEngine::LightingParserContext& parserContext,unsigned techniqueIndex,
        const SharedStateSet& sharedStateSet)
    {
        if (drawCalls._entries.empty()) return;

        LocalTransformConstants localTrans;
        localTrans._localSpaceView = Float3(0.f, 0.f, 0.f);
        localTrans._localNegativeLightDirection = Float3(0.f, 0.f, 0.f);
        
        static Metal::ConstantBuffer localTransformBuffer(nullptr, sizeof(LocalTransformConstants));     // this should go into some kind of global resource heap
        const Metal::ConstantBuffer* pkts[] = { &localTransformBuffer, nullptr };

        std::sort(drawCalls._entries.begin(), drawCalls._entries.end(), CompareDrawCall);

        const ModelRenderer::Pimpl::Mesh* currentMesh = nullptr;
        RenderCore::Metal::BoundUniforms* boundUniforms = nullptr;
        unsigned currentVariationHash = ~unsigned(0x0);
        unsigned currentResourceIndex = ~unsigned(0x0);
        unsigned currentConstantBufferIndex = ~unsigned(0x0);

        for (auto d=drawCalls._entries.cbegin(); d!=drawCalls._entries.cend(); ++d) {
            auto& renderer = *d->_renderer;

                // Note -- at the moment, shader variation hash is the sorting priority.
                //          This reduces the shader changes to a minimum. It also means we
                //          do the work in "BeginVariation" to resolve the variation
                //          as rarely as possible. However, we could pre-resolve all of the
                //          variations that we're going to need and use another value as the
                //          sorting priority instead... That might reduce the API thrashing
                //          in some cases.
            if (currentVariationHash != d->_shaderVariationHash) {
                auto techInterfIndex = renderer._pimpl->_techniqueInterfaceIndices[d->_drawCallIndex];
                auto shaderNameIndex = renderer._pimpl->_shaderNameIndices[d->_drawCallIndex];
                auto materialParamIndex = renderer._pimpl->_materialParameterBoxIndices[d->_drawCallIndex];
                auto geoParamIndex = renderer._pimpl->_geoParameterBoxIndices[d->_drawCallIndex];

                boundUniforms = sharedStateSet.BeginVariation(
                    context, parserContext.GetTechniqueContext(), techniqueIndex,
                    shaderNameIndex, techInterfIndex, geoParamIndex, materialParamIndex);
                currentVariationHash = d->_shaderVariationHash;
                currentResourceIndex = ~unsigned(0x0);
            }

            if (!boundUniforms) continue;

                // We have to do this transform update very frequently! isn't there a better way?
            {
                D3D11_MAPPED_SUBRESOURCE result;
                HRESULT hresult = context->GetUnderlying()->Map(
                    localTransformBuffer.GetUnderlying(), 0, D3D11_MAP_WRITE_DISCARD, 0, &result);
                assert(SUCCEEDED(hresult) && result.pData); (void)hresult;
                CopyTransform(((LocalTransformConstants*)result.pData)->_localToWorld, d->_modelToWorld);
                context->GetUnderlying()->Unmap(localTransformBuffer.GetUnderlying(), 0);
            }
            
            if (currentMesh != d->_cm) {
                context->Bind(renderer._pimpl->_indexBuffer, d->_cm->_indexFormat, d->_cm->_ibOffset);
                context->Bind(ResourceList<Metal::VertexBuffer, 1>(std::make_tuple(std::ref(renderer._pimpl->_vertexBuffer))), 
                    d->_cm->_vertexStride, d->_cm->_vbOffset);
                currentMesh = d->_cm;
                currentResourceIndex = ~unsigned(0x0);
            }

            auto resourceIndex          = renderer._pimpl->_resourcesIndices[d->_drawCallIndex];
            auto constantBufferIndex    = renderer._pimpl->_constantBufferIndices[d->_drawCallIndex];

                //  sometimes the same render call may be rendered in several different locations. In these cases,
                //  we can reduce the API thrashing to the minimum by avoiding re-setting resources and constants
            if (boundUniforms && (resourceIndex != currentResourceIndex || constantBufferIndex != currentConstantBufferIndex)) {
                const Metal::ShaderResourceView* srvs[16];
                assert(renderer._pimpl->_texturesPerMaterial <= dimof(srvs));
                for (unsigned c=0; c<renderer._pimpl->_texturesPerMaterial; c++) {
                    auto* t = renderer._pimpl->_boundTextures[resourceIndex * renderer._pimpl->_texturesPerMaterial + c];
                    srvs[c] = t?(&t->GetShaderResource()):nullptr;
                }
                pkts[1] = &renderer._pimpl->_constantBuffers[constantBufferIndex];
                boundUniforms->Apply(
                    *context, parserContext.GetGlobalUniformsStream(),
                    RenderCore::Metal::UniformsStream(nullptr, pkts, dimof(pkts), srvs, renderer._pimpl->_texturesPerMaterial));

                currentResourceIndex = resourceIndex;
                currentConstantBufferIndex = constantBufferIndex;
            }

            context->DrawIndexed(d->_indexCount, d->_firstIndex, 0);
        }
    }

    static std::string InvalidShaderName = "invalid";
    static const uint64 BindNormals = Hash64("NormalsTexture");
                                
    ModelRenderer::ModelRenderer(
        ModelScaffold& scaffold, MaterialScaffold& material, 
        SharedStateSet& sharedStateSet, unsigned levelOfDetail)
    {
        _scaffold = nullptr;
        _scaffoldLODIndex = (unsigned)-1;

        auto lod = std::find_if(scaffold._lods.cbegin(), scaffold._lods.cend(),
            [=](const ScaffoldLevelOfDetail& i) { return i._lodIndex == levelOfDetail; });
        if (lod == scaffold._lods.cend())
            return;

            //      The "scaffold" defines the raw structure of the input file
            //      For each valid mesh in the scaffold, we need to create a Mesh
            //      object, and fill in the vertex and index buffer values
            //      Loading the vertex buffer is a little difficult, because the data on
            //      disk is not interleaved. We want it interleaved in the vertex buffer, 
            //      so we have to push the data around a bit.

        BasicFile file(scaffold.Filename().c_str(), "rb");

        std::vector<uint8> nascentVB, nascentIB;
        std::vector<Pimpl::Mesh> meshes;
        std::vector<ParameterBox> materialParameterBoxes;

        std::vector<uint64> textureBindPoints;
        std::vector<int> subMaterialIndexToResourceIndex;
        std::vector<std::pair<int, unsigned>> materialIdToShaderName;
        std::vector<std::pair<int, unsigned>> materialIdToMaterialParameterBox;

        std::vector<std::unique_ptr<uint8[]>> prescientMaterialConstantBuffers;
        std::vector<std::pair<int, unsigned>> materialIdToConstantBuffer;

        struct BasicMaterialConstants
        {
                // fixed set of material parameters currently.
            Float3 _materialDiffuse;    float _opacity;
            Float3 _materialSpecular;   float _alphaThreshold;
        };

        unsigned mainGeoParamBox = ~unsigned(0x0);
        {
            ParameterBox geoParameters;
            geoParameters.SetParameter("GEO_HAS_TEXCOORD", 1);
            // geoParameters.SetParameter("GEO_HAS_NORMAL", 1);     (ignoring normal if we're using tangent frames for normal maps)
            // geoParameters.SetParameter("GEO_HAS_COLOUR", 1);     (ignoring colour always)
            geoParameters.SetParameter("GEO_HAS_TANGENT_FRAME", 1);
            mainGeoParamBox = sharedStateSet.InsertParameterBox(geoParameters);
        }

            //  First we need to bind each draw call to a material in our
            //  material scaffold. Then we need to find the superset of all bound textures
            //      This superset will be used to initialize all of the technique input interfaces
            //  The final resolved texture is defined by the "meshCall" object and by the "drawCall"
            //  objects. MeshCall gives us the material id, and the draw call gives us the sub material id.
        for (auto m=lod->_meshCalls.cbegin(); m!=lod->_meshCalls.cend(); ++m) {

                //  Lookup the mesh geometry and material information from their respective inputs.
            auto sourceMesh = std::find_if(
                scaffold._meshes.cbegin(), scaffold._meshes.cend(),
                [=](const ScaffoldMesh& mesh) { return mesh._meshId == m->_meshId; });
            if (sourceMesh == scaffold._meshes.cend())
                continue;

                //  We assign a "material" to a mesh, and a "sub material" to a draw call within the
                //  mesh.
            #if defined(_DEBUG)
                auto sourceMaterial = std::find_if(
                    scaffold._materials.cbegin(), scaffold._materials.cend(),
                    [=](const ScaffoldMaterial& mat) { return mat._materialId == m->_materialId; });
                if (sourceMaterial == scaffold._materials.cend())
                    continue;
            #endif

            for (auto d=sourceMesh->_drawCalls.cbegin(); d!=sourceMesh->_drawCalls.cend(); ++d) {

                    //  We need to find the material that is associated with this material and sub material pair
                const MaterialScaffold::MaterialDefinition* subMaterial = nullptr;
                if (d->_subMaterialIndex < int(material.GetSubMaterialCount())) {
                    subMaterial = &material.GetSubMaterial(d->_subMaterialIndex);
                }

                std::string shaderName;
                std::string normalMapTextureName;
                if (subMaterial) {
                    shaderName = subMaterial->_shaderName;

                        //  Special case for materials called "nodraw". Just skip these.
                    if (!_stricmp(shaderName.c_str(), "nodraw")) {
                        continue;
                    }
                    
                    for (auto t=subMaterial->_boundTextures.cbegin(); t!=subMaterial->_boundTextures.cend(); ++t) {
                        auto bindName = Hash64(t->first);
                        if (std::find(textureBindPoints.cbegin(), textureBindPoints.cend(), bindName) == textureBindPoints.cend()) {
                            textureBindPoints.push_back(bindName);
                        }
                        if (bindName == BindNormals) {
                            normalMapTextureName = t->second;
                        }
                    }
                } else {
                    shaderName = InvalidShaderName;
                }

                #if defined(_DEBUG)
                        //  Compare the sub-materials in the material scaffold to the sub material references 
                        //  from the model. The sub-material references in the model may be redundant, because
                        //  they can become out-of-sync with the material file. Currently we're actually 
                        //  ignoring the model sub-material references, except for this check.
                    if (d->_subMaterialIndex < int(sourceMaterial->_subMaterialCount)) {
                        auto subMaterialId = sourceMaterial->_subMaterialIds[d->_subMaterialIndex];
                        auto subMaterialName = std::find_if(
                            scaffold._materials.cbegin(), scaffold._materials.cend(), 
                            [=](const ScaffoldMaterial& mat) { return mat._materialId == subMaterialId; });
                        if (subMaterialName == scaffold._materials.cend()) {
                            if (subMaterial) {
                                LogWarning << "Warning -- sub-material is listed in the material file, but there is no material name chunk in the cgf file!\n";
                            }
                        }
                    }
                #endif

                    // create a mapping between sub-material and resource index
                auto mapping = std::find(subMaterialIndexToResourceIndex.cbegin(), subMaterialIndexToResourceIndex.cend(), d->_subMaterialIndex);
                if (mapping == subMaterialIndexToResourceIndex.cend()) {
                    subMaterialIndexToResourceIndex.push_back(d->_subMaterialIndex);
                }

                    // we need to create a list of all of the shader names that we encounter
                materialIdToShaderName.push_back(std::make_pair(d->_subMaterialIndex, 
                    sharedStateSet.InsertShaderName(shaderName)));

                    // build a material parameter box and look for it in our list
                ParameterBox materialParamBox;
                materialParamBox.SetParameter("RES_HAS_bfbf327ee9403009", !normalMapTextureName.empty());
                if (subMaterial && subMaterial->_alphaThreshold < 1.f) {
                    materialParamBox.SetParameter("MAT_ALPHA_TEST", 1);
                }

                    //  We need to decide whether the normal map is "DXT" 
                    //  format or not. This information isn't in the material
                    //  itself; we actually need to look at the texture file
                    //  to see what format it is. Unfortunately that means
                    //  opening the texture file to read it's header. However
                    //  we can accelerate it a bit by caching the result
                materialParamBox.SetParameter("RES_HAS_NormalsTexture_DXT", IsDXTNormalMap(normalMapTextureName));

                materialIdToMaterialParameterBox.push_back(
                    std::make_pair(d->_subMaterialIndex, 
                        sharedStateSet.InsertParameterBox(materialParamBox)));

                    //  Build a "basic material constants" buffer this material, and then try to combine
                    //  it with any buffers already created.
                BasicMaterialConstants constants = { Float3(1.f, 1.f, 1.f), 1.f, Float3(1.f, 1.f, 1.f), 1.f };
                if (subMaterial) {
                    constants._materialDiffuse = subMaterial->_diffuseColor;
                    constants._opacity = subMaterial->_opacity;
                    constants._materialSpecular = subMaterial->_specularColor;
                    constants._alphaThreshold = subMaterial->_alphaThreshold;
                }

                auto buffer = DuplicateMemory(constants);
                size_t bufferSize = sizeof(BasicMaterialConstants);
                auto* t = buffer.get();
                auto c = prescientMaterialConstantBuffers.cend();
                    // (can't get std::find_if to work correctly here)
                for (auto i = prescientMaterialConstantBuffers.cbegin(); i!=prescientMaterialConstantBuffers.cend(); ++i) {
                    if (XlCompareMemory(i->get(), t, bufferSize) == 0) {
                        c = i;
                        break;
                    }
                }
                if (c == prescientMaterialConstantBuffers.cend()) {
                    prescientMaterialConstantBuffers.push_back(std::move(buffer));
                    materialIdToConstantBuffer.push_back(std::make_pair(d->_subMaterialIndex, prescientMaterialConstantBuffers.size()-1));
                } else {
                    auto index = std::distance(prescientMaterialConstantBuffers.cbegin(), c);
                    materialIdToConstantBuffer.push_back(std::make_pair(d->_subMaterialIndex, index));
                }

            }
            
        }

        std::vector<unsigned>   resourcesIndices;
        std::vector<unsigned>   techniqueInterfaceIndices;
        std::vector<unsigned>   shaderNameIndices;
        std::vector<unsigned>   materialParameterBoxIndices;
        std::vector<unsigned>   geoParameterBoxIndices;
        std::vector<unsigned>   constantBufferIndices;

        for (auto m=lod->_meshCalls.cbegin(); m!=lod->_meshCalls.cend(); ++m) {
            if (!m->_materialId) continue;

            int meshId = m->_meshId;
            auto sourceMesh = std::find_if(
                scaffold._meshes.cbegin(), scaffold._meshes.cend(),
                [=](const ScaffoldMesh& mesh) { return mesh._meshId == meshId; });
            if (sourceMesh == scaffold._meshes.cend()) continue;

            bool atLeastOneValidDrawCall = false;
            for (auto d = sourceMesh->_drawCalls.cbegin(); d!=sourceMesh->_drawCalls.cend(); ++d) {
                auto snm = std::find_if(materialIdToShaderName.cbegin(), materialIdToShaderName.cend(),
                    [=](const std::pair<int, unsigned>& p) { return p.first == d->_subMaterialIndex; });
                if (snm != materialIdToShaderName.cend()) {
                    atLeastOneValidDrawCall = true;
                    break;
                }
            }
            if (!atLeastOneValidDrawCall) { continue; }

                //  Check to see if this mesh has at least one valid draw call. If there
                //  is none, we can skip it completely
    
                //   We only need a few things from this mesh:
                //      * vertex buffer
                //      * index buffer
                //      * technique input interface

            Pimpl::Mesh mesh;
            mesh._id = meshId;
            mesh._ibOffset = nascentIB.size();
            mesh._vbOffset = nascentVB.size();
            mesh._indexFormat = sourceMesh->_indexFormat;

            nascentIB.resize(mesh._ibOffset + sourceMesh->_indexBufferSize);
            file.Seek(sourceMesh->_indexBufferFileOffset, SEEK_SET);
            file.Read(&nascentIB[mesh._ibOffset], 1, sourceMesh->_indexBufferSize);
            
                //  Look at the input streams and figure out how to 
                //  interleave this data to make something work
                //
                //  For now, all input streams will be merged together into a single
                //  vertex buffer stream. Also we won't do any conversion on the types.
                //  We could do 32bit float to 16bit floats during this step... but actually,
                //  it would be better if we did that during an earlier stage (for example,
                //  during pre-processing)

            unsigned maxCount = 0;
            unsigned workingOffset = 0;
            std::vector<std::pair<unsigned, unsigned>> streamOffsets;
            for (unsigned c=0; c<sourceMesh->_dataStreams.size(); ++c) {
                auto& s = sourceMesh->_dataStreams[c];
                    // skip "COLOR" and "NORMAL" streams
                if (s._semantic[0] != "COLOR" && s._semantic[0] != "NORMAL") {
                    maxCount = std::max(maxCount, s._elementCount);
                    streamOffsets.push_back(std::make_pair(c, workingOffset));
                    workingOffset += s._elementSize;
                }
            }

            unsigned vertexSize = workingOffset;
            nascentVB.resize(mesh._vbOffset + vertexSize * maxCount);

            uint8* vbDest = &nascentVB[mesh._vbOffset];
            for (auto i=streamOffsets.cbegin(); i!=streamOffsets.cend(); ++i) {
                auto& s = sourceMesh->_dataStreams[i->first];
                auto sourceData = std::make_unique<uint8[]>(s._streamSize);
                file.Seek(s._fileOffset, SEEK_SET);
                file.Read(sourceData.get(), 1, s._streamSize);

                    //  we need to copy this data into vbDest, but so that it gets interleaved with
                    //  other vertex data
                uint8* dst = PtrAdd(vbDest, i->second);
                for (unsigned e=0; e<s._elementCount; ++e) {
                        //  we don't know how large the object is, so we have to use memcpy. But we
                        //  could speed this up by specializing for elements of certain sizes (ie: 4, 8, 12, 16)
                    XlCopyMemory(PtrAdd(dst, e*vertexSize), PtrAdd(sourceData.get(), e*s._elementSize), s._elementSize);
                }
            }

                //  We must define an input interface layout from the input streams.
            Metal::InputElementDesc inputDesc[12];
            unsigned vertexElementCount = 0;
            for (auto i=streamOffsets.cbegin(); i!=streamOffsets.cend(); ++i) {
                auto& sourceElement = sourceMesh->_dataStreams[i->first];
                if ((vertexElementCount+1) <= dimof(inputDesc)) {
                    inputDesc[vertexElementCount++] = Metal::InputElementDesc(
                        sourceElement._semantic[0], sourceElement._semanticIndex[0],
                        sourceElement._format[0], 0, i->second);
                }

                if (!sourceElement._semantic[1].empty() && (vertexElementCount+1) <= dimof(inputDesc)) {
                    inputDesc[vertexElementCount++] = Metal::InputElementDesc(
                        sourceElement._semantic[1], sourceElement._semanticIndex[1],
                        sourceElement._format[1], 0, i->second + Metal::BitsPerPixel(sourceElement._format[0])/8);
                }
            }

            auto techniqueInterfaceIndex = sharedStateSet.InsertTechniqueInterface(
                inputDesc, vertexElementCount, AsPointer(textureBindPoints.cbegin()), textureBindPoints.size());

            for (auto d = sourceMesh->_drawCalls.cbegin(); d!=sourceMesh->_drawCalls.cend(); ++d) {
                if (!d->_indexCount)
                    continue;

                auto snm = std::find_if(materialIdToShaderName.cbegin(), materialIdToShaderName.cend(),
                    [=](const std::pair<int, unsigned>& p) { return p.first == d->_subMaterialIndex; });
                if (snm == materialIdToShaderName.cend()) {
                    continue;   // missing shader name means a "no-draw" shader
                }
                shaderNameIndices.push_back(snm->second);
                
                techniqueInterfaceIndices.push_back(techniqueInterfaceIndex);
                auto i = std::find(subMaterialIndexToResourceIndex.cbegin(), subMaterialIndexToResourceIndex.cend(), d->_subMaterialIndex);
                assert(i != subMaterialIndexToResourceIndex.cend());
                resourcesIndices.push_back(std::distance(subMaterialIndexToResourceIndex.cbegin(), i));

                auto mpb = std::find_if(materialIdToMaterialParameterBox.cbegin(), materialIdToMaterialParameterBox.cend(),
                    [=](const std::pair<int, unsigned>& p) { return p.first == d->_subMaterialIndex; });
                assert(mpb != materialIdToMaterialParameterBox.cend());
                materialParameterBoxIndices.push_back(mpb->second);

                geoParameterBoxIndices.push_back(mainGeoParamBox);

                auto cb = std::find_if(materialIdToConstantBuffer.cbegin(), materialIdToConstantBuffer.cend(), 
                    [=](const std::pair<int, unsigned>& p) { return p.first == d->_subMaterialIndex; });
                assert(cb != materialIdToConstantBuffer.cend());
                constantBufferIndices.push_back(cb->second);

                mesh._drawCalls.push_back(*d);
            }

            mesh._vertexStride = vertexSize;
            meshes.push_back(mesh);

        }

            //  now that we have a list of all of the sub materials used, and we know how large the resource interface is, we
            //  build an array of deferred shader resources for shader inputs.
        std::vector<Metal::DeferredShaderResource*> boundTextures;
        auto texturesPerMaterial = textureBindPoints.size();
        boundTextures.resize(subMaterialIndexToResourceIndex.size() * texturesPerMaterial);
        unsigned workingIndex = 0;
        for (auto id=subMaterialIndexToResourceIndex.cbegin(); id!=subMaterialIndexToResourceIndex.cend(); ++id, ++workingIndex) {
            auto subMaterialIndex = *id;
            if (subMaterialIndex >= int(material.GetSubMaterialCount()))
                continue;

            auto subMaterial = &material.GetSubMaterial(subMaterialIndex);

                // find the bind textures in this input material that match our expected bind points
            for (auto t=subMaterial->_boundTextures.cbegin(); t!=subMaterial->_boundTextures.cend(); ++t) {
                auto bindName = Hash64(t->first);
                auto i = std::find(textureBindPoints.cbegin(), textureBindPoints.cend(), bindName);
                assert(i!=textureBindPoints.cend());
                auto index = std::distance(textureBindPoints.cbegin(), i);

                TRY {
                    boundTextures[workingIndex*texturesPerMaterial + index] = 
                        &::Assets::GetAsset<Metal::DeferredShaderResource>(t->second.c_str());
                } CATCH (const ::Assets::Exceptions::InvalidResource&) {
                    LogWarning << "Warning -- shader resource (" << t->second << ") couldn't be found";
                } CATCH_END
            }
        }

        std::vector<Metal::ConstantBuffer> finalConstantBuffers;
        for (auto cb=prescientMaterialConstantBuffers.cbegin(); cb!=prescientMaterialConstantBuffers.end(); ++cb) {
            Metal::ConstantBuffer newCB(cb->get(), sizeof(BasicMaterialConstants));
            finalConstantBuffers.push_back(std::move(newCB));
        }

        Metal::VertexBuffer vb(AsPointer(nascentVB.begin()), nascentVB.size());
        Metal::IndexBuffer  ib(AsPointer(nascentIB.begin()), nascentIB.size());

        auto pimpl = std::make_unique<Pimpl>();
        pimpl->_vertexBuffer = std::move(vb);
        pimpl->_indexBuffer = std::move(ib);
        pimpl->_meshes = std::move(meshes);
        pimpl->_techniqueInterfaceIndices = std::move(techniqueInterfaceIndices);
        pimpl->_resourcesIndices = std::move(resourcesIndices);
        pimpl->_boundTextures = std::move(boundTextures);
        pimpl->_shaderNameIndices = std::move(shaderNameIndices);
        pimpl->_materialParameterBoxIndices = std::move(materialParameterBoxIndices);
        pimpl->_geoParameterBoxIndices = std::move(geoParameterBoxIndices);
        pimpl->_constantBuffers = std::move(finalConstantBuffers);
        pimpl->_constantBufferIndices = std::move(constantBufferIndices);
        pimpl->_texturesPerMaterial = texturesPerMaterial;
        _pimpl = std::move(pimpl);
        _scaffoldLODIndex = std::distance(scaffold._lods.cbegin(), lod);
        _scaffold = &scaffold;
        _levelOfDetail = levelOfDetail;
    }

    ModelRenderer::~ModelRenderer()
    {
    }

}}}


