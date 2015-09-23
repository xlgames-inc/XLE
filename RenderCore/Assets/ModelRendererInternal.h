// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ModelRunTime.h"
#include "ModelScaffoldInternal.h"
#include "SharedStateSet.h" // for SharedShaderName, SharedParameterBox, etc
#include "../Metal/Forward.h"
#include "../../Utility/Streams/Serialization.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/IteratorUtils.h"

namespace RenderCore { namespace Assets 
{
    class SkinningBindingBox;

////////////////////////////////////////////////////////////////////////////////////////////
    //      r e n d e r e r         //

    namespace ModelConstruction { class BuffersUnderConstruction; class ParamBoxDescriptions; }

    class DeferredShaderResource;

    class PendingGeoUpload
    {
    public:
        unsigned _sourceFileOffset, _size;
        unsigned _bufferDestination;
    };

    class ModelRenderer::Pimpl
    {
    public:
        typedef unsigned UnifiedBufferOffset;
        static const auto MaxVertexStreams = 3u;

        class Mesh
        {
        public:
            unsigned    _id;

                // indices
            UnifiedBufferOffset     _ibOffset;
            NativeFormatPlaceholder _indexFormat;

                // vertices
            UnifiedBufferOffset _vbOffsets[MaxVertexStreams];
            unsigned    _vertexStrides[MaxVertexStreams];
            unsigned    _vertexStreamCount;

                // geo params & technique interface
                // each mesh within the model can have a different vertex input interface
                //  (and so needs a different technique interface)
            SharedParameterBox          _geoParamBox;
            SharedTechniqueInterface    _techniqueInterface;

            #if defined(_DEBUG)
                unsigned _vbSize, _ibSize;  // used for metrics
            #endif
        };

        std::vector<const DeferredShaderResource*> _boundTextures;
        size_t  _texturesPerMaterial;

        ///////////////////////////////////////////////////////////////////////////////
            // "resources" or state information attached on a per-draw call basis
        class DrawCallResources
        {
        public:
            SharedShaderName    _shaderName;
            SharedParameterBox  _geoParamBox;
            SharedParameterBox  _materialParamBox;

            unsigned        _textureSet;
            unsigned        _constantBuffer;
            SharedRenderStateSet _renderStateSet;

            DelayStep       _delayStep;
            MaterialGuid    _materialBindingGuid;

            DrawCallResources();
            DrawCallResources(
                SharedShaderName shaderName,
                SharedParameterBox geoParamBox, SharedParameterBox matParamBox,
                unsigned textureSet, unsigned constantBuffer,
                SharedRenderStateSet renderStateSet, DelayStep delayStep, MaterialGuid materialBindingIndex);
        };
        std::vector<DrawCallResources>   _drawCallRes;

        ///////////////////////////////////////////////////////////////////////////////
        Metal::VertexBuffer     _vertexBuffer;
        Metal::IndexBuffer      _indexBuffer;
        std::vector<Mesh>       _meshes;
        std::vector<Metal::ConstantBuffer>  _constantBuffers;

        ///////////////////////////////////////////////////////////////////////////////
        typedef std::pair<unsigned, DrawCallDesc> MeshAndDrawCall;
        std::vector<MeshAndDrawCall>    _drawCalls;

        const ModelScaffold*    _scaffold;
        unsigned                _levelOfDetail;

        ///////////////////////////////////////////////////////////////////////////////
        std::vector<PendingGeoUpload>  _vbUploads;
        std::vector<PendingGeoUpload>  _ibUploads;

        ///////////////////////////////////////////////////////////////////////////////
        #if defined(_DEBUG)
            unsigned _vbSize, _ibSize;
            std::vector<::Assets::rstring> _boundTextureNames;
            std::vector<std::pair<SharedParameterBox,std::string>> _paramBoxDesc;
        #endif

        ///////////////////////////////////////////////////////////////////////////////
        Pimpl() : _scaffold(nullptr), _levelOfDetail(~unsigned(0x0)) {}
        ~Pimpl() {}

        Metal::BoundUniforms* BeginVariation(
            const ModelRendererContext&, const SharedStateSet&, unsigned, SharedTechniqueInterface) const;

        SharedTechniqueInterface BeginGeoCall(
            const ModelRendererContext& context,
            Metal::ConstantBuffer&  localTransformBuffer,
            const MeshToModel&      transforms,
            const Float4x4&         modelToWorld,
            unsigned                geoCallIndex) const;

        void ApplyBoundUnforms(
            const ModelRendererContext&     context,
            Metal::BoundUniforms&           boundUniforms,
            unsigned                        resourcesIndex,
            unsigned                        constantsIndex,
            const Metal::ConstantBuffer*    cbs[2]);

    ///////////////////////////////////////////////////////////////////////////////
        //   B U I L D I N G   A N D   I N I T I A L I Z A T I O N   //
    ///////////////////////////////////////////////////////////////////////////////

        static auto BuildMesh(
            const ModelCommandStream::GeoCall& geoInst,
            const RawGeometry& geo,
            IteratorRange<VertexData**> supplements,
            ModelConstruction::BuffersUnderConstruction& workingBuffers,
            SharedStateSet& sharedStateSet,
            const uint64 textureBindPoints[], unsigned textureBindPointsCnt,
            ModelConstruction::ParamBoxDescriptions& paramBoxDesc,
            bool normalFromSkinning = false) -> Mesh;
    };

    class ModelRenderer::PimplWithSkinning : public Pimpl
    {
    public:
        class SkinnedMesh : public Mesh
        {
        public:
                //  Vertex data is separated into several "streams". 
                //  These match the low level api streams.
                //  The base class "Mesh" members contain the static geometry elements.

            struct VertexStreams { enum Enum { AnimatedGeo, SkeletonBinding, Max }; };
            UnifiedBufferOffset         _extraVbOffset[VertexStreams::Max];
            unsigned                    _extraVbStride[VertexStreams::Max];
            UnifiedBufferOffset         _sourceFileExtraVBOffset[VertexStreams::Max];
            unsigned                    _sourceFileExtraVBSize[VertexStreams::Max];
            SharedTechniqueInterface    _skinnedTechniqueInterface;
        };

        class SkinnedMeshAnimBinding
        {
        public:
            uint64      _iaAnimationHash;
            const BoundSkinnedGeometry* _scaffold;

            SharedTechniqueInterface    _techniqueInterface;
            unsigned                    _vertexStride;
        };

        std::vector<SkinnedMesh>            _skinnedMeshes;
        std::vector<SkinnedMeshAnimBinding> _skinnedBindings;
        std::vector<MeshAndDrawCall>        _skinnedDrawCalls;

        SharedTechniqueInterface BeginSkinCall(
            const ModelRendererContext&     context,
            Metal::ConstantBuffer&  localTransformBuffer,
            const MeshToModel&      transforms,
            const Float4x4&         modelToWorld,
            unsigned                geoCallIndex,
            PreparedAnimation*      preparedAnimation) const;

    ///////////////////////////////////////////////////////////////////////////////
        //   B U I L D I N G   A N D   I N I T I A L I Z A T I O N   //
    ///////////////////////////////////////////////////////////////////////////////

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
            const BoundSkinnedGeometry& geo,
            IteratorRange<VertexData**> supplements,
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

        ///////////////////////////////////////////////////////////////////////////////
            //   S K I N N I N G   S E T U P   //
        ///////////////////////////////////////////////////////////////////////////////

        void StartBuildingSkinning(Metal::DeviceContext& context, SkinningBindingBox& bindingBox) const;
        void EndBuildingSkinning(Metal::DeviceContext& context) const;
    };

    unsigned BuildLowLevelInputAssembly(
        Metal::InputElementDesc dst[], unsigned dstMaxCount,
        const VertexElement* source, unsigned sourceCount,
        unsigned lowLevelSlot = 0);

    inline unsigned BuildLowLevelInputAssembly(
        Metal::InputElementDesc dst[], unsigned dstMaxCount,
        const SerializableVector<VertexElement>& source,
        unsigned lowLevelSlot = 0)
    {
        return BuildLowLevelInputAssembly(
            dst, dstMaxCount,
            AsPointer(source.cbegin()), (unsigned)source.size(), lowLevelSlot);
    }

    template <typename Type>
        void DestroyArray(const Type* begin, const Type* end)
        {
            for (auto i=begin; i!=end; ++i) { i->~Type(); }
        }

}}

