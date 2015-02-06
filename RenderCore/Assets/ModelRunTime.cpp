// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ModelRunTime.h"
#include "ModelRunTimeInternal.h"
#include "TransformationCommands.h"
#include "AssetUtils.h"     // maybe only needed for chunk ids
#include "RawAnimationCurve.h"
#include "SharedStateSet.h"

#include "../Metal/Buffer.h"
#include "../Metal/State.h"
#include "../Metal/DeviceContext.h"
#include "../Metal/DeviceContextImpl.h"
#include "../Resource.h"
#include "../RenderUtils.h"
#include "../../Assets/AssetUtils.h"

#include "../../Assets/BlockSerializer.h"
#include "../../Assets/ChunkFile.h"
#include "../../Assets/IntermediateResources.h"

#include "../../Core/Exceptions.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/IteratorUtils.h"
#include "../../Math/Transformations.h"
#include "../../ConsoleRig/Console.h"

#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/Techniques.h"
#include "../../SceneEngine/ResourceBox.h"

#include "../../Utility/StringFormat.h"

#include <string>

#include "../DX11/Metal/IncludeDX11.h"

#pragma warning(disable:4189)
#pragma warning(disable:4127)       //  warning C4127: conditional expression is constant

namespace RenderCore { namespace Assets
{
    /// <summary>Internal namespace with utilities for constructing models</summary>
    /// These functions are normally used within the constructor of ModelRenderer
    namespace ModelConstruction
    {
        class BasicMaterialConstants
        {
        public:
                // fixed set of material parameters currently.
            Float3 _materialDiffuse;    float _opacity;
            Float3 _materialSpecular;   float _alphaThreshold;
        };

        class MaterialBindingInfo
        {
        public:
            std::string _shaderName;
            std::vector<uint8> _constants;
            SceneEngine::ParameterBox _materialParamBox;
        };

        static const std::string DefaultShader = "illum";

        static MaterialBindingInfo MakeMaterialBindingInfo(const std::string& normalMapTextureName)
        {
            MaterialBindingInfo result;
            result._shaderName = DefaultShader;
            BasicMaterialConstants constants = { Float3(1.f, 1.f, 1.f), 1.f, Float3(1.f, 1.f, 1.f), 1.f };
            result._constants = std::vector<uint8>((uint8*)&constants, (uint8*)PtrAdd(&constants, sizeof(BasicMaterialConstants)));

            result._materialParamBox.SetParameter("RES_HAS_NORMAL_MAP", !normalMapTextureName.empty());
            // result._materialParamBox.SetParameter("MAT_ALPHA_TEST", ??);

                //  We need to decide whether the normal map is "DXT" 
                //  format or not. This information isn't in the material
                //  itself; we actually need to look at the texture file
                //  to see what format it is. Unfortunately that means
                //  opening the texture file to read it's header. However
                //  we can accelerate it a bit by caching the result
            result._materialParamBox.SetParameter("RES_HAS_NORMAL_MAP_DXT", IsDXTNormalMap(normalMapTextureName));

            return result;
        }

        static size_t InsertOrCombine(std::vector<std::vector<uint8>>& dest, std::vector<uint8>&& compare)
        {
            for (auto i = dest.cbegin(); i!=dest.cend(); ++i) {
                if (i->size() == compare.size() && (XlCompareMemory(AsPointer(i->begin()), AsPointer(compare.begin()), i->size()) == 0)) {
                    return std::distance(dest.cbegin(), i);
                }
            }

            dest.push_back(std::forward<std::vector<uint8>>(compare));
            return dest.size()-1;
        }

        struct SubMatResources 
        { 
            unsigned _shaderName; 
            unsigned _matParams; 
            unsigned _constantBuffer; 
            unsigned _texturesIndex; 
        };

        static unsigned ScaffoldMaterialIndex(ModelScaffold& scaffold, unsigned geoCallIndex, unsigned drawCallIndex)
        {
            auto& cmdStream = scaffold.CommandStream();
            auto& meshData = scaffold.ImmutableData();
            auto geoCallCount = cmdStream.GetGeoCallCount();

            auto& geoCall = (geoCallIndex < geoCallCount) ? cmdStream.GetGeoCall(geoCallIndex) : cmdStream.GetSkinCall(geoCallIndex - geoCallCount);
            auto& geo = (geoCallIndex < geoCallCount) ? meshData._geos[geoCall._geoId] : (RawGeometry&)meshData._boundSkinnedControllers[geoCall._geoId];
            unsigned subMatI = geo._drawCalls[drawCallIndex]._subMaterialIndex;

                //  the "sub material index" in the draw call is an index
                //  into the array in the geo call
            if (subMatI < geoCall._materialCount) {
                return geoCall._materialIds[subMatI];
            }

            return ~unsigned(0x0);
        }

        template <typename T> static bool AtLeastOneValidDrawCall(const RawGeometry& geo, ModelScaffold& scaffold, unsigned geoCallIndex, std::vector<std::pair<unsigned, T>>& subMatResources)
        {
                //  look for at least one valid draw call in this geo instance
                //  a valid draw call should have got shader and material information bound
            bool atLeastOneValidDrawCall = false;
            for (unsigned di=0; di<geo._drawCallsCount; ++di) {
                auto& d = geo._drawCalls[di];
                if (d._indexCount) {
                    auto subMatIndex = ScaffoldMaterialIndex(scaffold, geoCallIndex, di);
                    auto snm = LowerBound(subMatResources, subMatIndex);
                    if (snm != subMatResources.cend() && snm->first == subMatIndex) {
                        return true;
                    }
                }
            }

            return false;       // didn't find any draw calls with good material information. This whole geo object can be ignored.
        }

        static void LoadBlock(BasicFile& file, uint8 destination[], size_t fileOffset, size_t readSize)
        {
            file.Seek(fileOffset, SEEK_SET);
            file.Read(destination, 1, readSize);
        }

        static const std::string    DefaultNormalsTextureBinding = "NormalsTexture";
        static const uint64         DefaultNormalsTextureBindingHash = Hash64(
            AsPointer(DefaultNormalsTextureBinding.cbegin()), AsPointer(DefaultNormalsTextureBinding.cend()));
    }

    ModelRenderer::ModelRenderer(ModelScaffold& scaffold, SharedStateSet& sharedStateSet, const ::Assets::DirectorySearchRules* searchRules, unsigned levelOfDetail)
    {
        using namespace ModelConstruction;

            // build the underlying objects required to render the given scaffold 
            //  (at the given level of detail)
        BasicFile file(scaffold.Filename().c_str(), "rb");
        auto largeBlocksOffset = scaffold.LargeBlocksOffset();

        std::vector<uint64> textureBindPoints;
        std::vector<std::vector<uint8>> prescientMaterialConstantBuffers;

        std::vector<std::pair<unsigned, SubMatResources>> subMatResources;
        unsigned textureSetCount = 0;

        auto& cmdStream = scaffold.CommandStream();
        auto& meshData = scaffold.ImmutableData();

        unsigned mainGeoParamBox = ~unsigned(0x0);
        {
                // todo -- this should come from the geometry type!
            SceneEngine::ParameterBox geoParameters;
            geoParameters.SetParameter("GEO_HAS_TEXCOORD", 1);
            geoParameters.SetParameter("GEO_HAS_NORMAL", 1);
            mainGeoParamBox = sharedStateSet.InsertParameterBox(geoParameters);
        }

            //  First we need to bind each draw call to a material in our
            //  material scaffold. Then we need to find the superset of all bound textures
            //      This superset will be used to initialize all of the technique 
            //      input interfaces
            //  The final resolved texture is defined by the "meshCall" object and by the 
            //  "drawCall" objects. MeshCall gives us the material id, and the draw call 
            //  gives us the sub material id.

        auto geoCallCount = cmdStream.GetGeoCallCount();
        auto skinCallCount = cmdStream.GetSkinCallCount();
        for (unsigned gi=0; gi<geoCallCount + skinCallCount; ++gi) {
            auto& geoInst = (gi < geoCallCount) ? cmdStream.GetGeoCall(gi) : cmdStream.GetSkinCall(gi - geoCallCount);
            if (geoInst._levelOfDetail != levelOfDetail) { continue; }

                //  Lookup the mesh geometry and material information from their respective inputs.
            auto& geo = (gi < geoCallCount) ? meshData._geos[geoInst._geoId] : (RawGeometry&)meshData._boundSkinnedControllers[geoInst._geoId];
            for (unsigned di=0; di<geo._drawCallsCount; ++di) {
                auto scaffoldMatIndex = ScaffoldMaterialIndex(scaffold, gi, di);
                auto existing = LowerBound(subMatResources, scaffoldMatIndex);
                if (existing == subMatResources.cend() || existing->first != scaffoldMatIndex) {
                    subMatResources.insert(existing, std::make_pair(scaffoldMatIndex, SubMatResources()));
                }
            }
        }

            // fill in the details for all of the material references we found
        for (auto i=subMatResources.begin(); i!=subMatResources.end(); ++i) {

            std::string boundNormalMapName;

                //  we need to create a list of all of the texture bind points that are referenced
                //  by all of the materials used here. They will end up in sorted order
            if (i->first < scaffold.ImmutableData()._materialBindingsCount) {
                const auto& materialScaffoldData = scaffold.ImmutableData()._materialBindings[i->first];
                for (auto i=materialScaffoldData._bindings.cbegin(); i!=materialScaffoldData._bindings.cend(); ++i) {
                    auto bindName = i->_bindHash;

                    if (bindName == DefaultNormalsTextureBindingHash) {
                        boundNormalMapName = i->_resourceName;
                    }

                    auto q = std::lower_bound(textureBindPoints.begin(), textureBindPoints.end(), bindName);
                    if (q != textureBindPoints.end() && *q == bindName) { continue; }
                    textureBindPoints.insert(q, bindName);
                }
            }

                //  create a "material binding" object, and instantiate the low level objects
                //  associated directly with this material.
            auto matBinding = MakeMaterialBindingInfo(boundNormalMapName);
            i->second._shaderName = sharedStateSet.InsertShaderName(matBinding._shaderName);
            i->second._matParams = sharedStateSet.InsertParameterBox(matBinding._materialParamBox);
            i->second._constantBuffer = InsertOrCombine(prescientMaterialConstantBuffers, std::move(matBinding._constants));
            i->second._texturesIndex = textureSetCount++;
        }

            //  Note that we can have cases where the same mesh is referenced multiple times by 
            //  a single "geo call". In these cases, we want the mesh data to be stored once
            //  in the vertex buffer / index buffer but for there to be multiple sets of "draw calls"
            //  So, we have to separate the mesh processing from the draw call processing here
        
        size_t nascentVBSize = 0, nascentIBSize = 0;

            //  these are parallel arrays in the order of the draw calls as we will encounter them 
            //  during normal rendering
        std::vector<unsigned>   resourcesIndices;
        std::vector<unsigned>   techniqueInterfaceIndices;
        std::vector<unsigned>   shaderNameIndices;
        std::vector<unsigned>   materialParameterBoxIndices;
        std::vector<unsigned>   geoParameterBoxIndices;
        std::vector<unsigned>   constantBufferIndices;

            //  We should calculate the size we'll need for nascentIB & nascentVB first, 
            //  so we don't have to do any reallocations

            ////////////////////////////////////////////////////////////////////////
                //          u n s k i n n e d   g e o                           //
        std::vector<Pimpl::Mesh> meshes;
        std::vector<Pimpl::MeshAndDrawCall> drawCalls;

        for (unsigned gi=0; gi<geoCallCount; ++gi) {
            auto& geoInst = cmdStream.GetGeoCall(gi);
            if (geoInst._levelOfDetail != levelOfDetail) { continue; }

                //  Check to see if this mesh has at least one valid draw call. If there
                //  is none, we can skip it completely
            assert(geoInst._geoId < meshData._geoCount);
            auto& geo = meshData._geos[geoInst._geoId];
            if (!AtLeastOneValidDrawCall(geo, scaffold, gi, subMatResources)) { continue; }

                // if we encounter the same mesh multiple times, we don't need to store it every time
            auto existing = FindIf(meshes, [=](const Pimpl::Mesh& mesh) { return mesh._id == geoInst._geoId; });
            if (existing != meshes.end()) { continue; }

            Pimpl::Mesh mesh;
            mesh._id = geoInst._geoId;
            mesh._ibOffset = nascentIBSize;
            mesh._vbOffset = nascentVBSize;
            mesh._indexFormat = geo._ib._format;
            mesh._vertexStride = geo._vb._ia._vertexStride;
            mesh._sourceFileIBOffset = largeBlocksOffset + geo._ib._offset;
            mesh._sourceFileIBSize = geo._ib._size;
            mesh._sourceFileVBOffset = largeBlocksOffset + geo._vb._offset;
            mesh._sourceFileVBSize = geo._vb._size;

            nascentVBSize += mesh._sourceFileVBSize;
            nascentIBSize += mesh._sourceFileIBSize;

                // we have to align the index buffer offset correctly
            unsigned indexStride = (mesh._indexFormat == RenderCore::Metal::NativeFormat::R32_UINT)?4:2;
            unsigned rem = mesh._ibOffset % indexStride;
            if (rem != 0) {
                mesh._ibOffset += indexStride - rem;
                nascentIBSize += indexStride - rem;
            }

            meshes.push_back(mesh);
        }

            // construct vb, ib & draw calls (for GeoCalls)
        for (unsigned gi=0; gi<geoCallCount; ++gi) {
            auto& geoInst = cmdStream.GetGeoCall(gi);
            if (geoInst._levelOfDetail != levelOfDetail) { continue; }
            
            auto& geo = meshData._geos[geoInst._geoId];
            auto meshI = FindIf(meshes, [=](const Pimpl::Mesh& mesh) { return mesh._id == geoInst._geoId; });
            if (meshI == meshes.end()) { continue; }
            auto meshIndex = std::distance(meshes.begin(), meshI);

                //  We must define an input interface layout from the input streams.
            Metal::InputElementDesc inputDesc[12];
            unsigned vertexElementCount = BuildLowLevelInputAssembly(
                inputDesc, geo._vb._ia._elements, geo._vb._ia._elementCount);

                // setup the "Draw call" objects next
                //  each draw calls can have separate technique indices, and different material assignments.
                //  the material assignments will be burnt down to constant buffers, technique params 
                //  and bound textures here
            auto techniqueInterfaceIndex = sharedStateSet.InsertTechniqueInterface(
                inputDesc, vertexElementCount, AsPointer(textureBindPoints.cbegin()), textureBindPoints.size());

            for (unsigned di=0; di<geo._drawCallsCount; ++di) {
                auto& d = geo._drawCalls[di];
                if (!d._indexCount) { continue; }

                auto scaffoldMatIndex = ScaffoldMaterialIndex(scaffold, gi, di);
                auto subMatResI = LowerBound(subMatResources, scaffoldMatIndex);
                if (subMatResI == subMatResources.cend() || subMatResI->first != scaffoldMatIndex) {
                    continue;   // missing shader name means a "no-draw" shader
                }

                auto& subMatRes = subMatResI->second;
                shaderNameIndices.push_back(subMatRes._shaderName);
                materialParameterBoxIndices.push_back(subMatRes._matParams);
                geoParameterBoxIndices.push_back(mainGeoParamBox);
                constantBufferIndices.push_back(subMatRes._constantBuffer);
                
                techniqueInterfaceIndices.push_back(techniqueInterfaceIndex);
                resourcesIndices.push_back(subMatRes._texturesIndex);

                drawCalls.push_back(std::make_pair(gi, d));
            }
        }

            ////////////////////////////////////////////////////////////////////////
                //          s k i n n e d   g e o                           //
        std::vector<Pimpl::SkinnedMesh> skinnedMeshes;
        std::vector<Pimpl::MeshAndDrawCall> skinnedDrawCalls;

        for (unsigned gi=0; gi<skinCallCount; ++gi) {
            auto& geoInst = cmdStream.GetSkinCall(gi);
            if (geoInst._levelOfDetail != levelOfDetail) { continue; }

                //  Check to see if this mesh has at least one valid draw call. If there
                //  is none, we can skip it completely
            assert(geoInst._geoId < meshData._boundSkinnedControllerCount);
            auto& geo = meshData._boundSkinnedControllers[geoInst._geoId];
            if (!AtLeastOneValidDrawCall(geo, scaffold, geoCallCount + gi, subMatResources)) { continue; }

                // if we encounter the same mesh multiple times, we don't need to store it every time
            auto existing = FindIf(skinnedMeshes, [=](const Pimpl::SkinnedMesh& mesh) { return mesh._id == geoInst._geoId; });
            if (existing != skinnedMeshes.end()) { continue; }

            Pimpl::SkinnedMesh mesh;
            mesh._id = geoInst._geoId;
            mesh._ibOffset = nascentIBSize;
            mesh._vbOffset = nascentVBSize;
            mesh._indexFormat = geo._ib._format;
            mesh._vertexStride = geo._vb._ia._vertexStride;
            mesh._sourceFileIBOffset = largeBlocksOffset + geo._ib._offset;
            mesh._sourceFileIBSize = geo._ib._size;
            mesh._sourceFileVBOffset = largeBlocksOffset + geo._vb._offset;
            mesh._sourceFileVBSize = geo._vb._size;

            nascentVBSize += mesh._sourceFileVBSize;
            nascentIBSize += mesh._sourceFileIBSize;

            auto animGeo = Pimpl::SkinnedMesh::VertexStreams::AnimatedGeo;
            auto skelBind = Pimpl::SkinnedMesh::VertexStreams::SkeletonBinding;

            mesh._extraVbOffset[animGeo] = nascentVBSize;
            mesh._extraVbStride[animGeo] = geo._animatedVertexElements._ia._vertexStride;
            mesh._sourceFileExtraVBOffset[animGeo] = largeBlocksOffset + geo._animatedVertexElements._offset;
            mesh._sourceFileExtraVBSize[animGeo] = geo._animatedVertexElements._size;
            nascentVBSize += mesh._sourceFileExtraVBSize[animGeo];

            mesh._extraVbOffset[skelBind] = nascentVBSize;
            mesh._extraVbStride[skelBind] = geo._skeletonBinding._ia._vertexStride;
            mesh._sourceFileExtraVBOffset[skelBind] = largeBlocksOffset + geo._skeletonBinding._offset;
            mesh._sourceFileExtraVBSize[skelBind] = geo._skeletonBinding._size;
                // precalculate a hash value that is useful for binding animation data...
            mesh._animatedAIHash = geo._animatedVertexElements._ia.BuildHash() ^ geo._skeletonBinding._ia.BuildHash();
            mesh._scaffold = &geo;
            nascentVBSize += mesh._sourceFileExtraVBSize[skelBind];

                // we have to align the index buffer offset correctly
            unsigned indexStride = (mesh._indexFormat == RenderCore::Metal::NativeFormat::R32_UINT)?4:2;
            unsigned rem = mesh._ibOffset % indexStride;
            if (rem != 0) {
                mesh._ibOffset += indexStride - rem;
                nascentIBSize += indexStride - rem;
            }

            skinnedMeshes.push_back(mesh);
        }

            // construct vb, ib & draw calls (for SkinCalls)
        for (unsigned gi=0; gi<skinCallCount; ++gi) {
            auto& geoInst = cmdStream.GetSkinCall(gi);
            if (geoInst._levelOfDetail != levelOfDetail) { continue; }

            auto& geo = meshData._boundSkinnedControllers[geoInst._geoId];
            auto meshI = FindIf(skinnedMeshes, [=](const Pimpl::SkinnedMesh& mesh) { return mesh._id == geoInst._geoId; });
            if (meshI == skinnedMeshes.end()) { continue; }
            auto meshIndex = std::distance(skinnedMeshes.begin(), meshI);
            
            ////////////////////////////////////////////////////////////////////////////////
                //  We have two input assemblies -- one for the skinning prepare, and
                //  one for the actual render
            Metal::InputElementDesc inputDescForRender[12];
            unsigned vertexElementForRenderCount = BuildLowLevelInputAssembly(
                inputDescForRender, geo._animatedVertexElements._ia._elements, geo._animatedVertexElements._ia._elementCount);
            vertexElementForRenderCount = BuildLowLevelInputAssembly(
                inputDescForRender, geo._vb._ia._elements, geo._vb._ia._elementCount, vertexElementForRenderCount, 1);

            auto techniqueInterfaceIndex = sharedStateSet.InsertTechniqueInterface(
                inputDescForRender, vertexElementForRenderCount, AsPointer(textureBindPoints.cbegin()), textureBindPoints.size());

            for (unsigned di=0; di<geo._drawCallsCount; ++di) {
                auto& d = geo._drawCalls[di];
                if (!d._indexCount) { continue; }
                
                auto scaffoldMatIndex = ScaffoldMaterialIndex(scaffold, geoCallCount + gi, di);
                auto subMatResI = LowerBound(subMatResources, scaffoldMatIndex);
                if (subMatResI == subMatResources.cend() || subMatResI->first != scaffoldMatIndex) {
                    continue;   // missing shader name means a "no-draw" shader
                }

                auto& subMatRes = subMatResI->second;
                shaderNameIndices.push_back(subMatRes._shaderName);
                materialParameterBoxIndices.push_back(subMatRes._matParams);
                geoParameterBoxIndices.push_back(mainGeoParamBox);
                constantBufferIndices.push_back(subMatRes._constantBuffer);
                
                techniqueInterfaceIndices.push_back(techniqueInterfaceIndex);
                resourcesIndices.push_back(subMatRes._texturesIndex);

                skinnedDrawCalls.push_back(std::make_pair(gi, d));
            }

            ////////////////////////////////////////////////////////////////////////////////
            Metal::InputElementDesc inputDescForSkin[12];
            unsigned vertexElementForSkinCount = BuildLowLevelInputAssembly(
                inputDescForSkin, geo._animatedVertexElements._ia._elements, geo._animatedVertexElements._ia._elementCount);
            vertexElementForSkinCount = BuildLowLevelInputAssembly(
                inputDescForSkin, geo._skeletonBinding._ia._elements, geo._skeletonBinding._ia._elementCount, vertexElementForSkinCount, 1);

        }

        std::vector<uint8> nascentVB, nascentIB;
        nascentVB.resize(nascentVBSize);
        nascentIB.resize(nascentIBSize);

        for (auto m=meshes.begin(); m!=meshes.end(); ++m) {
            LoadBlock(file, &nascentIB[m->_ibOffset], m->_sourceFileIBOffset, m->_sourceFileIBSize);
            LoadBlock(file, &nascentVB[m->_vbOffset], m->_sourceFileVBOffset, m->_sourceFileVBSize);
        }

        for (auto m=skinnedMeshes.begin(); m!=skinnedMeshes.end(); ++m) {
            LoadBlock(file, &nascentIB[m->_ibOffset], m->_sourceFileIBOffset, m->_sourceFileIBSize);
            LoadBlock(file, &nascentVB[m->_vbOffset], m->_sourceFileVBOffset, m->_sourceFileVBSize);
            for (unsigned s=0; s<Pimpl::SkinnedMesh::VertexStreams::Max; ++s) {
                LoadBlock(file, &nascentVB[m->_extraVbOffset[s]], m->_sourceFileExtraVBOffset[s], m->_sourceFileExtraVBSize[s]);
            }
        }


            //  now that we have a list of all of the sub materials used, and we know how large the resource 
            //  interface is, we build an array of deferred shader resources for shader inputs.
        std::vector<Metal::DeferredShaderResource*> boundTextures;
        std::vector<SceneEngine::ParameterBox> materialParameterBoxes;

        auto texturesPerMaterial = textureBindPoints.size();
        boundTextures.resize(textureSetCount * texturesPerMaterial);

        for (auto i=subMatResources.begin(); i!=subMatResources.end(); ++i) {
            unsigned subMatIndex = i->first;
            unsigned textureSetIndex = i->second._texturesIndex;
        
            if (subMatIndex >= scaffold.ImmutableData()._materialBindingsCount)
                continue;

            const auto& materialScaffoldData = scaffold.ImmutableData()._materialBindings[subMatIndex];
            for (auto t=materialScaffoldData._bindings.cbegin(); t!=materialScaffoldData._bindings.cend(); ++t) {
                auto bindName = t->_bindHash;
                auto i = std::find(textureBindPoints.cbegin(), textureBindPoints.cend(), bindName);
                assert(i!=textureBindPoints.cend());
                auto index = std::distance(textureBindPoints.cbegin(), i);
        
                TRY {
                        // note --  Ideally we want to do this filename resolve in a background thread
                        //          however, it doesn't work well with our resources system. Because we're
                        //          expecting to create the DeferredShaderResource from a definitive file
                        //          name, something that can be matched against other (already loaded) resources.
                        //          So we need something different here... Something that can resolve a filename
                        //          in the background, and then return a shareable resource afterwards
                    if (searchRules) {
                        char resolvedPath[MaxPath];
                        searchRules->ResolveFile(resolvedPath, dimof(resolvedPath), t->_resourceName.c_str());
                        boundTextures[textureSetIndex*texturesPerMaterial + index] = 
                            &::Assets::GetAsset<Metal::DeferredShaderResource>(resolvedPath);
                    } else {
                        boundTextures[textureSetIndex*texturesPerMaterial + index] = 
                            &::Assets::GetAsset<Metal::DeferredShaderResource>(t->_resourceName.c_str());
                    }
                } CATCH (const ::Assets::Exceptions::InvalidResource&) {
                    LogWarning << "Warning -- shader resource (" << t->_resourceName << ") couldn't be found";
                } CATCH_END
            }
        }

        std::vector<Metal::ConstantBuffer> finalConstantBuffers;
        for (auto cb=prescientMaterialConstantBuffers.cbegin(); cb!=prescientMaterialConstantBuffers.end(); ++cb) {
            Metal::ConstantBuffer newCB(AsPointer(cb->begin()), cb->size());
            finalConstantBuffers.push_back(std::move(newCB));
        }

        Metal::VertexBuffer vb(AsPointer(nascentVB.begin()), nascentVB.size());
        Metal::IndexBuffer  ib(AsPointer(nascentIB.begin()), nascentIB.size());

        auto pimpl = std::make_unique<Pimpl>();
        pimpl->_vertexBuffer = std::move(vb);
        pimpl->_indexBuffer = std::move(ib);
        pimpl->_meshes = std::move(meshes);
        pimpl->_drawCalls = std::move(drawCalls);
        pimpl->_techniqueInterfaceIndices = std::move(techniqueInterfaceIndices);
        pimpl->_resourcesIndices = std::move(resourcesIndices);
        pimpl->_boundTextures = std::move(boundTextures);
        pimpl->_shaderNameIndices = std::move(shaderNameIndices);
        pimpl->_materialParameterBoxIndices = std::move(materialParameterBoxIndices);
        pimpl->_geoParameterBoxIndices = std::move(geoParameterBoxIndices);
        pimpl->_constantBuffers = std::move(finalConstantBuffers);
        pimpl->_constantBufferIndices = std::move(constantBufferIndices);
        pimpl->_skinnedDrawCalls = std::move(skinnedDrawCalls);
        pimpl->_skinnedMeshes = std::move(skinnedMeshes);
        pimpl->_texturesPerMaterial = texturesPerMaterial;
        pimpl->_scaffold = &scaffold;
        pimpl->_levelOfDetail = levelOfDetail;
        _pimpl = std::move(pimpl);
    }

    ModelRenderer::~ModelRenderer()
    {}

    class ModelRenderingBox
    {
    public:
        class Desc {};

        Metal::ConstantBuffer _localTransformBuffer;
        ModelRenderingBox(const Desc&)
        {
            Metal::ConstantBuffer localTransformBuffer(nullptr, sizeof(LocalTransformConstants));
            _localTransformBuffer = std::move(localTransformBuffer);
        }
        ~ModelRenderingBox() {}
    };


    Metal::BoundUniforms*    ModelRenderer::Pimpl::BeginVariation(
            const Context&          context,
            const SharedStateSet&   sharedStateSet,
            unsigned                drawCallIndex) const
    {
        auto techInterfIndex        = _techniqueInterfaceIndices[drawCallIndex];
        auto shaderNameIndex        = _shaderNameIndices[drawCallIndex];
        auto materialParamIndex     = _materialParameterBoxIndices[drawCallIndex];
        auto geoParamIndex          = _geoParameterBoxIndices[drawCallIndex];

        return sharedStateSet.BeginVariation(
            context._context, *context._parserContext, context._techniqueIndex,
            shaderNameIndex, techInterfIndex, geoParamIndex, materialParamIndex);

    }

    void ModelRenderer::Pimpl::BeginGeoCall(
            const Context&          context,
            Metal::ConstantBuffer&  localTransformBuffer,
            const MeshToModel*      transforms,
            Float4x4                modelToWorld,
            unsigned                geoCallIndex) const
    {
        auto& cmdStream = _scaffold->CommandStream();
        auto& geoCall = cmdStream.GetGeoCall(geoCallIndex);

        LocalTransformConstants trans;
        if (transforms) {
            auto localToModel = transforms->GetMeshToModel(geoCall._transformMarker);
            trans = MakeLocalTransform(Combine(localToModel, modelToWorld), ExtractTranslation(context._parserContext->GetProjectionDesc()._cameraToWorld));
        } else {
            trans = MakeLocalTransform(modelToWorld, ExtractTranslation(context._parserContext->GetProjectionDesc()._cameraToWorld));
        }
        localTransformBuffer.Update(*context._context, &trans, sizeof(trans));

            // todo -- should be possible to avoid this search
        auto cm = FindIf(_meshes, [=](const Pimpl::Mesh& mesh) { return mesh._id == geoCall._geoId; });
        assert(cm != _meshes.end());

        auto& devContext = *context._context;
        devContext.Bind(_indexBuffer, cm->_indexFormat, cm->_ibOffset);
        devContext.Bind(ResourceList<Metal::VertexBuffer, 1>(std::make_tuple(std::ref(_vertexBuffer))), cm->_vertexStride, cm->_vbOffset);
    }

    void ModelRenderer::Pimpl::BeginSkinCall(
            const Context&          context,
            Metal::ConstantBuffer&  localTransformBuffer,
            const MeshToModel*      transforms,
            Float4x4                modelToWorld,
            unsigned                geoCallIndex,
            PreparedAnimation*      preparedAnimation) const
    {
        auto& cmdStream = _scaffold->CommandStream();
        auto& geoCall = cmdStream.GetSkinCall(geoCallIndex);

            // we only need to use the "transforms" array when we don't have prepared animation
            //  (otherwise that information gets burned into the prepared vertex positions)
        if (!preparedAnimation && transforms) {
            modelToWorld = Combine(transforms->GetMeshToModel(geoCall._transformMarker), modelToWorld);
        }

        auto trans = MakeLocalTransform(modelToWorld, ExtractTranslation(context._parserContext->GetProjectionDesc()._cameraToWorld));
        localTransformBuffer.Update(*context._context, &trans, sizeof(trans));

        auto cm = FindIf(_skinnedMeshes, [=](const Pimpl::SkinnedMesh& mesh) { return mesh._id == geoCall._geoId; });
        assert(cm != _skinnedMeshes.end());

        auto& devContext = *context._context;
        devContext.Bind(_indexBuffer, cm->_indexFormat, cm->_ibOffset);

            // todo -- we need to replace the animated vertex stream with the post anim data
        auto animGeo = SkinnedMesh::VertexStreams::AnimatedGeo;
        UINT strides[2], offsets[2];
        ID3D::Buffer* underlyingVBs[2];
        strides[0] = cm->_extraVbStride[animGeo];
        strides[1] = cm->_vertexStride;
        offsets[0] = cm->_extraVbOffset[animGeo];
        offsets[1] = cm->_vbOffset;
        underlyingVBs[0] = underlyingVBs[1] = _vertexBuffer.GetUnderlying();

        if (preparedAnimation) {
            underlyingVBs[0] = preparedAnimation->_skinningBuffer.GetUnderlying();

                // we have to recalculate the offset currently
            unsigned vbOffset = 0;
            for (auto m=_skinnedMeshes.cbegin(); m!=_skinnedMeshes.cend(); ++m) {
                if (m == cm) { break; }
                vbOffset += m->_sourceFileExtraVBSize[Pimpl::SkinnedMesh::VertexStreams::AnimatedGeo];
            }
            offsets[0] = vbOffset;
        }

        context._context->GetUnderlying()->IASetVertexBuffers(0, 2, underlyingVBs, strides, offsets);
    }

    void ModelRenderer::Pimpl::ApplyBoundUnforms(
        const Context&                  context,
        Metal::BoundUniforms&           boundUniforms,
        unsigned                        resourcesIndex,
        unsigned                        constantsIndex,
        const Metal::ConstantBuffer*    cbs[2])
    {
        const Metal::ShaderResourceView* srvs[16];
        assert(_texturesPerMaterial <= dimof(srvs));
        for (unsigned c=0; c<_texturesPerMaterial; c++) {
            auto* t = _boundTextures[resourcesIndex * _texturesPerMaterial + c];
            srvs[c] = t?(&t->GetShaderResource()):nullptr;
        }
        cbs[1] = &_constantBuffers[constantsIndex];
        boundUniforms.Apply(
            *context._context, context._parserContext->GetGlobalUniformsStream(),
            RenderCore::Metal::UniformsStream(nullptr, cbs, dimof(cbs), srvs, _texturesPerMaterial));
    }

    void    ModelRenderer::Render(
            const Context&      context,
            const Float4x4&     modelToWorld,
            const MeshToModel*  transforms,
            PreparedAnimation*  preparedAnimation) const
    {
        auto& box = SceneEngine::FindCachedBox<ModelRenderingBox>(ModelRenderingBox::Desc());
        const Metal::ConstantBuffer* pkts[] = { &box._localTransformBuffer, nullptr };

        unsigned currRes = ~unsigned(0x0), currCB = ~unsigned(0x0), currGeoCall = ~unsigned(0x0);
        Metal::BoundUniforms* currUniforms = nullptr;
        auto& devContext = *context._context;
        auto& scaffold = *_pimpl->_scaffold;
        auto& cmdStream = scaffold.CommandStream();

        TRY
        {

                // skinned and unskinned geometry are almost the same, except for
                // "BeginGeoCall" / "BeginSkinCall". Never the less, we need to split
                // them into separate loops

                //////////// Render un-skinned geometry ////////////

            unsigned drawCallIndex = 0;
            for (auto md = _pimpl->_drawCalls.cbegin(); md != _pimpl->_drawCalls.cend(); ++md, ++drawCallIndex) {
                if (md->first != currGeoCall) {
                    _pimpl->BeginGeoCall(context, box._localTransformBuffer, transforms, modelToWorld, md->first);
                    currGeoCall = md->first;
                }

                auto* boundUniforms = _pimpl->BeginVariation(context, *context._sharedStateSet, drawCallIndex);
                auto res = _pimpl->_resourcesIndices[drawCallIndex];
                auto cb = _pimpl->_constantBufferIndices[drawCallIndex];

                if (boundUniforms != currUniforms || res != currRes || cb != currCB) {
                    if (boundUniforms) {
                        _pimpl->ApplyBoundUnforms(context, *boundUniforms, res, cb, pkts);
                    }

                    currRes = res; currCB = cb;
                    currUniforms = boundUniforms;
                }
            
                const auto& d = md->second;
                devContext.Bind(d._topology);  // do we really need to set the topology every time?
                devContext.DrawIndexed(d._indexCount, d._firstIndex, d._firstVertex);
            }


                //////////// Render skinned geometry ////////////

            currGeoCall = ~unsigned(0x0);
            for (auto md = _pimpl->_skinnedDrawCalls.cbegin(); md != _pimpl->_skinnedDrawCalls.cend(); ++md, ++drawCallIndex) {
                if (md->first != currGeoCall) {
                    _pimpl->BeginSkinCall(context, box._localTransformBuffer, transforms, modelToWorld, md->first, preparedAnimation);
                    currGeoCall = md->first;
                }

                auto* boundUniforms = _pimpl->BeginVariation(context, *context._sharedStateSet, drawCallIndex);
                auto res = _pimpl->_resourcesIndices[drawCallIndex];
                auto cb = _pimpl->_constantBufferIndices[drawCallIndex];

                if (boundUniforms != currUniforms || res != currRes || cb != currCB) {
                    if (boundUniforms) {
                        _pimpl->ApplyBoundUnforms(context, *boundUniforms, res, cb, pkts);
                    }

                    currRes = res; currCB = cb;
                    currUniforms = boundUniforms;
                }
            
                const auto& d = md->second;
                devContext.Bind(d._topology);  // do we really need to set the topology every time?
                devContext.DrawIndexed(d._indexCount, d._firstIndex, d._firstVertex);
            }

        } 
        CATCH(::Assets::Exceptions::InvalidResource& e) { context._parserContext->Process(e); }
        CATCH(::Assets::Exceptions::PendingResource& e) { context._parserContext->Process(e); }
        CATCH_END
    }

    static intrusive_ptr<ID3D::Device> ExtractDevice(RenderCore::Metal::DeviceContext* context)
    {
        ID3D::Device* tempPtr;
        context->GetUnderlying()->GetDevice(&tempPtr);
        return moveptr(tempPtr);
    }

    bool ModelRenderer::CanDoPrepareAnimation(Metal::DeviceContext* context)
    {
        auto featureLevel = ExtractDevice(context)->GetFeatureLevel();
        return (featureLevel >= D3D_FEATURE_LEVEL_10_0);
    }

////////////////////////////////////////////////////////////////////////////////

        /// \todo --    this DestroyArray stuff is too awkward. Instead, let's create
        ///             some "SerializeableArray" or "SerializeableBlock" or something --
        ///             and have it deal with the internals (even if it means increasing memory size slightly)
    ModelCommandStream::~ModelCommandStream()
    {
        DestroyArray(_geometryInstances,        &_geometryInstances[_geometryInstanceCount]);
        DestroyArray(_skinControllerInstances,  &_skinControllerInstances[_skinControllerInstanceCount]);
    }

    GeoInputAssembly::~GeoInputAssembly()
    {
        DestroyArray(_elements, &_elements[_elementCount]);
    }

    RawGeometry::~RawGeometry() {}
    BoundSkinnedGeometry::~BoundSkinnedGeometry() {}

    ModelImmutableData::~ModelImmutableData()
    {
        DestroyArray(_geos, &_geos[_geoCount]);
        DestroyArray(_boundSkinnedControllers, &_boundSkinnedControllers[_boundSkinnedControllerCount]);
        DestroyArray(_materialBindings, &_materialBindings[_materialBindingsCount]);
    }

        ////////////////////////////////////////////////////////////

    uint64 GeoInputAssembly::BuildHash() const
    {
            //  Build a hash for this object.
            //  Note that we should be careful that we don't get an
            //  noise from characters in the left-over space in the
            //  semantic names. Do to this right, we should make sure
            //  that left over space has no effect.
        auto elementsHash = Hash64(_elements, PtrAdd(_elements, _elementCount * sizeof(VertexElement)));
        elementsHash ^= uint64(_vertexStride);
        return elementsHash;
    }

        ////////////////////////////////////////////////////////////

    Float4x4 ModelRenderer::MeshToModel::GetMeshToModel(unsigned transformMarker) const
    {
            //  The "skeleton binding" tells us how to map from the matrices that
            //  are output from the transformation machine to the input matrices
            //  expected by the "transformMarker" index scheme
        if (_skeletonBinding) {
            assert(transformMarker < _skeletonBinding->_modelJointIndexToMachineOutput.size());
            auto machineOutputIndex = _skeletonBinding->_modelJointIndexToMachineOutput[transformMarker];
            if (machineOutputIndex == ~unsigned(0x0)) {
                    // no mapping... we just have to assume identity mesh-to-model
                return Identity<Float4x4>();
            } else if (machineOutputIndex >= _skeletonOutputCount) {
                assert(0);
                return Identity<Float4x4>();
            }
            return _skeletonOutput[machineOutputIndex];
        } else {            
            if (transformMarker >= _skeletonOutputCount) {
                assert(0);
                return Identity<Float4x4>();
            }
            return _skeletonOutput[transformMarker];
        }
    }

    ModelRenderer::MeshToModel::MeshToModel()
    {
        _skeletonOutput = nullptr;
        _skeletonOutputCount = 0;
        _skeletonBinding = nullptr;
    }

    ModelRenderer::MeshToModel::MeshToModel(
        const Float4x4 skeletonOutput[], unsigned skeletonOutputCount,
        const SkeletonBinding* binding)
    {
        _skeletonOutput = skeletonOutput;
        _skeletonOutputCount = skeletonOutputCount;
        _skeletonBinding = binding;
    }

        ////////////////////////////////////////////////////////////

    static std::pair<std::unique_ptr<uint8[]>, unsigned> LoadRawData(const char filename[])
    {
        BasicFile file(filename, "rb");
        auto chunks = Serialization::ChunkFile::LoadChunkTable(file);

            // look for the first model scaffold chunk
        Serialization::ChunkFile::ChunkHeader largeBlocksChunk;
        Serialization::ChunkFile::ChunkHeader scaffoldChunk;

        for (auto i=chunks.begin(); i!=chunks.end(); ++i) {
            if (i->_type == ChunkType_ModelScaffold && !scaffoldChunk._fileOffset) {
                scaffoldChunk = *i;
            }
            if (i->_type == ChunkType_ModelScaffoldLargeBlocks && !largeBlocksChunk._fileOffset) {
                largeBlocksChunk = *i;
            }
        }

        if (    !scaffoldChunk._fileOffset
            ||  !largeBlocksChunk._fileOffset) {
            throw ::Assets::Exceptions::FormatError("Missing model scaffold chunks: %s", filename);
        }

        if (scaffoldChunk._chunkVersion != 0) {
            throw ::Assets::Exceptions::FormatError("Incorrect file version: %s", filename);
        }

        auto rawMemoryBlock = std::make_unique<uint8[]>(scaffoldChunk._size);
        file.Seek(scaffoldChunk._fileOffset, SEEK_SET);
        file.Read(rawMemoryBlock.get(), 1, scaffoldChunk._size);

        return std::make_pair(std::move(rawMemoryBlock), largeBlocksChunk._fileOffset);
    }

    ModelScaffold::ModelScaffold(const ResChar filename[])
    {
        std::unique_ptr<uint8[]> rawMemoryBlock;
        unsigned largeBlocksOffset = 0;
        std::tie(rawMemoryBlock, largeBlocksOffset) = LoadRawData(filename);
        
        Serialization::Block_Initialize(rawMemoryBlock.get());        
        _data = (const ModelImmutableData*)Serialization::Block_GetFirstObject(rawMemoryBlock.get());

        auto validationCallback = std::make_shared<::Assets::DependencyValidation>();
        RegisterFileDependency(validationCallback, filename);
        
        _filename = filename;
        _rawMemoryBlock = std::move(rawMemoryBlock);
        _largeBlocksOffset = largeBlocksOffset;
        _validationCallback = std::move(validationCallback);
    }
    
    ModelScaffold::ModelScaffold(std::shared_ptr<::Assets::PendingCompileMarker>&& marker)
    {
        _data = nullptr;
        _largeBlocksOffset = 0;
        auto validationCallback = std::make_shared<::Assets::DependencyValidation>();
        std::unique_ptr<uint8[]> rawMemoryBlock;
        unsigned largeBlocksOffset = 0;

        if (marker) {
            if (marker->GetState() == ::Assets::AssetState::Invalid) {
                ThrowException(::Assets::Exceptions::InvalidResource(marker->Initializer(), ""));
            } else if (marker->GetState() == ::Assets::AssetState::Pending) {
                    // we need to throw immediately on pending resource
                    // this object is useless while it's pending.
                ThrowException(::Assets::Exceptions::PendingResource(marker->Initializer(), ""));
            }

                // note -- here, we can alternatively go into a background load and enter into a pending state
            std::tie(rawMemoryBlock, largeBlocksOffset) = LoadRawData(marker->_sourceID0);

            Serialization::Block_Initialize(rawMemoryBlock.get());        
            _data = (const ModelImmutableData*)Serialization::Block_GetFirstObject(rawMemoryBlock.get());

            _filename = marker->_sourceID0;
        }

        _rawMemoryBlock = std::move(rawMemoryBlock);
        _largeBlocksOffset = largeBlocksOffset;
        _validationCallback = std::move(validationCallback);
    }

    ModelScaffold::ModelScaffold(ModelScaffold&& moveFrom)
    : _rawMemoryBlock(std::move(moveFrom._rawMemoryBlock))
    , _filename(std::move(moveFrom._filename))
    {
        _data = moveFrom._data;
        moveFrom._data = nullptr;
    }

    ModelScaffold& ModelScaffold::operator=(ModelScaffold&& moveFrom)
    {
        _rawMemoryBlock = std::move(moveFrom._rawMemoryBlock);
        _data = moveFrom._data;
        moveFrom._data = nullptr;
        _filename = std::move(moveFrom._filename);
        return *this;
    }

    ModelScaffold::~ModelScaffold()
    {
        _data->~ModelImmutableData();
    }

    const ModelCommandStream&   ModelScaffold::CommandStream() const       { return _data->_visualScene; }
    std::pair<Float3, Float3>   ModelScaffold::GetStaticBoundingBox() const { return _data->_boundingBox; }

}}

