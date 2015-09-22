// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ModelRunTime.h"
#include "ModelScaffoldInternal.h"
#include "../Metal/Forward.h"
#include "../../Utility/Streams/Serialization.h"

namespace RenderCore { namespace Assets 
{
    class SkinningBindingBox;

////////////////////////////////////////////////////////////////////////////////////////////
    //      r e n d e r e r         //

    namespace ModelConstruction { class BuffersUnderConstruction; class ParamBoxDescriptions; }

    class DeferredShaderResource;

    class ModelRenderer::Pimpl
    {
    public:
        typedef unsigned TechniqueInterface;
        class Mesh
        {
        public:
                // each mesh within the model can have a different vertex input interface
                //  (and so needs a different technique interface)
            unsigned _id;
            unsigned _vbOffset, _ibOffset;
            unsigned _vertexStride;
            NativeFormatPlaceholder _indexFormat;
            unsigned _geoParamBox;
            TechniqueInterface _techniqueInterface;

            unsigned _sourceFileVBOffset, _sourceFileVBSize;
            unsigned _sourceFileIBOffset, _sourceFileIBSize;
        };

        class SkinnedMesh : public Mesh
        {
        public:
                //  Vertex data is separated into several "streams". 
                //  These match the low level api streams.
                //  The base class "Mesh" members contain the static geometry elements.

            struct VertexStreams { enum Enum { AnimatedGeo, SkeletonBinding, Max }; };
            unsigned    _extraVbOffset[VertexStreams::Max];
            unsigned    _extraVbStride[VertexStreams::Max];
            unsigned    _sourceFileExtraVBOffset[VertexStreams::Max];
            unsigned    _sourceFileExtraVBSize[VertexStreams::Max];
            TechniqueInterface    _skinnedTechniqueInterface;
        };

        class SkinnedMeshAnimBinding
        {
        public:
            uint64      _iaAnimationHash;
            const BoundSkinnedGeometry* _scaffold;

            TechniqueInterface    _techniqueInterface;
            unsigned    _vertexStride;
        };

        std::vector<const DeferredShaderResource*> _boundTextures;
        size_t  _texturesPerMaterial;

        ///////////////////////////////////////////////////////////////////////////////
            // "resources" or state information attached on a per-draw call basis
        class DrawCallResources
        {
        public:
            unsigned _shaderName;
            unsigned _geoParamBox;
            unsigned _materialParamBox;

            unsigned _textureSet;
            unsigned _constantBuffer;
            unsigned _renderStateSet;

            DelayStep _delayStep;
            MaterialGuid _materialBindingGuid;

            DrawCallResources();
            DrawCallResources(
                unsigned shaderName,
                unsigned geoParamBox, unsigned matParamBox,
                unsigned textureSet, unsigned constantBuffer,
                unsigned renderStateSet, DelayStep delayStep, MaterialGuid materialBindingIndex);
        };
        std::vector<DrawCallResources>   _drawCallRes;

        ///////////////////////////////////////////////////////////////////////////////
        Metal::VertexBuffer         _vertexBuffer;
        Metal::IndexBuffer          _indexBuffer;
        std::vector<Mesh>           _meshes;
        std::vector<SkinnedMesh>            _skinnedMeshes;
        std::vector<SkinnedMeshAnimBinding> _skinnedBindings;
        std::vector<Metal::ConstantBuffer>  _constantBuffers;

        ///////////////////////////////////////////////////////////////////////////////
        typedef std::pair<unsigned, DrawCallDesc> MeshAndDrawCall;
        std::vector<MeshAndDrawCall>    _drawCalls;
        std::vector<MeshAndDrawCall>    _skinnedDrawCalls;

        const ModelScaffold*  _scaffold;
        unsigned        _levelOfDetail;

        ///////////////////////////////////////////////////////////////////////////////
        #if defined(_DEBUG)
            unsigned _vbSize, _ibSize;
            std::vector<::Assets::rstring> _boundTextureNames;
            std::vector<std::pair<unsigned,std::string>> _paramBoxDesc;
        #endif

        ///////////////////////////////////////////////////////////////////////////////
        Pimpl() : _scaffold(nullptr), _levelOfDetail(~unsigned(0x0)) {}
        ~Pimpl() {}

        Metal::BoundUniforms* BeginVariation(
            const ModelRendererContext&, const SharedStateSet&, unsigned, TechniqueInterface) const;

        TechniqueInterface BeginGeoCall(
            const ModelRendererContext& context,
            Metal::ConstantBuffer&  localTransformBuffer,
            const MeshToModel&      transforms,
            const Float4x4&         modelToWorld,
            unsigned                geoCallIndex) const;

        TechniqueInterface BeginSkinCall(
            const ModelRendererContext&     context,
            Metal::ConstantBuffer&  localTransformBuffer,
            const MeshToModel&      transforms,
            const Float4x4&         modelToWorld,
            unsigned                geoCallIndex,
            PreparedAnimation*      preparedAnimation) const;

        void ApplyBoundUnforms(
            const ModelRendererContext&     context,
            Metal::BoundUniforms&           boundUniforms,
            unsigned                        resourcesIndex,
            unsigned                        constantsIndex,
            const Metal::ConstantBuffer*    cbs[2]);

        void BuildSkinnedBuffer(
            Metal::DeviceContext*   context,
            const SkinnedMesh&      mesh,
            const SkinnedMeshAnimBinding& preparedAnimBinding, 
            const Float4x4          transformationMachineResult[],
            const SkeletonBinding&  skeletonBinding,
            Metal::VertexBuffer&    outputResult,
            unsigned                outputOffset) const;

        static auto BuildMesh(
            const ModelCommandStream::GeoCall& geoInst,
            const RawGeometry& geo,
            ModelConstruction::BuffersUnderConstruction& workingBuffers,
            SharedStateSet& sharedStateSet,
            const uint64 textureBindPoints[], unsigned textureBindPointsCnt,
            ModelConstruction::ParamBoxDescriptions& paramBoxDesc,
            bool normalFromSkinning = false) -> Mesh;

        static auto BuildMesh(
            const ModelCommandStream::GeoCall& geoInst,
            const BoundSkinnedGeometry& geo,
            ModelConstruction::BuffersUnderConstruction& workingBuffers,
            SharedStateSet& sharedStateSet,
            const uint64 textureBindPoints[], unsigned textureBindPointsCnt,
            ModelConstruction::ParamBoxDescriptions& paramBoxDesc) -> SkinnedMesh;

        static auto BuildAnimBinding(
            const ModelCommandStream::GeoCall& geoInst,
            const BoundSkinnedGeometry& geo,
            SharedStateSet& sharedStateSet,
            const uint64 textureBindPoints[], unsigned textureBindPointsCnt) -> SkinnedMeshAnimBinding;

        static void InitialiseSkinningVertexAssembly(
            uint64 inputAssemblyHash,
            const BoundSkinnedGeometry& scaffoldGeo);

        static unsigned BuildPostSkinInputAssembly(
            Metal::InputElementDesc dst[],
            unsigned dstCount,
            const BoundSkinnedGeometry& scaffoldGeo);

        void StartBuildingSkinning(Metal::DeviceContext& context, SkinningBindingBox& bindingBox) const;
        void EndBuildingSkinning(Metal::DeviceContext& context) const;
    };

    unsigned BuildLowLevelInputAssembly(
        Metal::InputElementDesc dst[], unsigned dstMaxCount,
        const VertexElement* source, unsigned sourceCount,
        unsigned lowLevelSlot = 0);

    template <typename Type>
        void DestroyArray(const Type* begin, const Type* end)
        {
            for (auto i=begin; i!=end; ++i) { i->~Type(); }
        }

}}

