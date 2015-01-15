// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ModelRunTime.h"
#include "AnimationRunTime.h"

namespace RenderCore { namespace Assets 
{
    class SkinningBindingBox;
    class TableOfObjects;
    class TransformationParameterSet;
    class RawAnimationCurve;
    class AnimationImmutableData;
    class AnimationSet;
    class MaterialParameters;

    #pragma pack(push)
    #pragma pack(1)

////////////////////////////////////////////////////////////////////////////////////////////
    //      g e o m e t r y         //

    class ModelCommandStream : noncopyable
    {
    public:
            //  "Geo calls" & "draw calls". Geo calls have 
            //  a vertex buffer and index buffer, and contain
            //  draw calls within them.
        class GeoCall
        {
        public:
            unsigned    _geoId;
            unsigned    _transformMarker;
            unsigned*   _materialIds;
            size_t      _materialCount;
            unsigned    _levelOfDetail;
        };

        class InputInterface
        {
        public:
            uint64*     _jointNames;
            size_t      _jointCount;
        };

            /////   -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-   /////
        const GeoCall&      GetGeoCall(size_t index) const;
        size_t              GetGeoCallCount() const;

        const GeoCall&      GetSkinCall(size_t index) const;
        size_t              GetSkinCallCount() const;

        const InputInterface&   GetInputInterface() const { return _inputInterface; }

        ~ModelCommandStream();
    private:
        GeoCall*        _geometryInstances;
        size_t          _geometryInstanceCount;
        GeoCall*        _skinControllerInstances;
        size_t          _skinControllerInstanceCount;
        InputInterface  _inputInterface;
    };

    inline auto         ModelCommandStream::GetGeoCall(size_t index) const -> const GeoCall&    { return _geometryInstances[index]; }
    inline size_t       ModelCommandStream::GetGeoCallCount() const                             { return _geometryInstanceCount; }
    inline auto         ModelCommandStream::GetSkinCall(size_t index) const -> const GeoCall&   { return _skinControllerInstances[index]; }
    inline size_t       ModelCommandStream::GetSkinCallCount() const                            { return _skinControllerInstanceCount; }

    class DrawCallDesc
    {
    public:
        unsigned    _firstIndex, _indexCount;
        unsigned    _firstVertex;
        unsigned    _subMaterialIndex;
        Topology    _topology;

        DrawCallDesc(unsigned firstIndex, unsigned indexCount, unsigned firstVertex, unsigned subMaterialIndex, Metal::Topology::Enum topology) 
        : _firstIndex(firstIndex), _indexCount(indexCount), _firstVertex(firstVertex), _subMaterialIndex(subMaterialIndex), _topology(topology) {}
    };

    class VertexElement : noncopyable
    {
    public:
        char            _semantic[16];  // limited max size for semantic name (only alternative is to use a hash value)
        unsigned        _semanticIndex;
        NativeFormat    _format;
        unsigned        _startOffset;
    };

    class GeoInputAssembly
    {
    public:
        VertexElement*  _elements;
        unsigned        _elementCount;
        unsigned        _vertexStride;

        uint64 BuildHash() const;

        ~GeoInputAssembly();
    };

    class VertexData
    {
    public:
        GeoInputAssembly    _ia;
        unsigned            _offset, _size;
    };

    class IndexData
    {
    public:
        NativeFormat        _format;
        unsigned            _offset, _size;
    };

    class RawGeometry : noncopyable
    {
    public:
        VertexData      _vb;
        IndexData       _ib;

            // Draw calls
        DrawCallDesc*   _drawCalls;
        size_t          _drawCallsCount;

        ~RawGeometry();
    private:
        RawGeometry();
    };

    class BoundSkinnedGeometry : public RawGeometry
    {
    public:

            //  The "RawGeometry" base class contains the 
            //  unanimated vertex elements (and draw calls for
            //  rendering the object as a whole)
        VertexData      _animatedVertexElements;
        VertexData      _skeletonBinding;

        Float4x4*       _inverseBindMatrices;
        size_t          _inverseBindMatrixCount;
        Float4x4*       _inverseBindByBindShapeMatrices;
        size_t          _inverseBindByBindShapeMatrixCount;
        uint16*         _jointMatrices;         // (uint16 or uint8 for this array)
        size_t          _jointMatrixCount;
        Float4x4        _bindShapeMatrix;

        DrawCallDesc*   _preskinningDrawCalls;
        size_t          _preskinningDrawCallCount;

        std::pair<Float3, Float3>   _localBoundingBox;

        ~BoundSkinnedGeometry();
    private:
        BoundSkinnedGeometry();
    };

    class ModelImmutableData
    {
    public:
        ModelCommandStream          _visualScene;
        
        RawGeometry*                _geos;
        size_t                      _geoCount;
        BoundSkinnedGeometry*       _boundSkinnedControllers;
        size_t                      _boundSkinnedControllerCount;
        MaterialParameters*         _materialBindings;
        size_t                      _materialBindingsCount;

        std::pair<Float3, Float3>   _boundingBox;

        ~ModelImmutableData();
    };

////////////////////////////////////////////////////////////////////////////////////////////
    //      s k e l e t o n         //

    class TransformationMachine : noncopyable
    {
    public:
        unsigned                            GetOutputMatrixCount() const        { return _outputMatrixCount; }
        const TransformationParameterSet&   GetDefaultParameters() const        { return _defaultParameters; }

        void GenerateOutputTransforms   (   Float4x4 output[], unsigned outputCount,
                                            const TransformationParameterSet*   parameterSet) const;

        typedef void DebugIterator(const Float4x4& parent, const Float4x4& child, const void* userData);
        void GenerateOutputTransforms   (   Float4x4 output[], unsigned outputCount,
                                            const TransformationParameterSet*   parameterSet,
                                            DebugIterator*  debugIterator,
                                            const void*     iteratorUserData) const;

        class InputInterface
        {
        public:
            struct Parameter
            {
                uint64  _name;
                uint32  _index;
                TransformationParameterSet::Type::Enum  _type;
            };

            Parameter*  _parameters;
            size_t      _parameterCount;
        };

        class OutputInterface
        {
        public:
            uint64*     _outputMatrixNames;
            Float4x4*   _skeletonInverseBindMatrices;
            size_t      _outputMatrixNameCount;
        };

        const InputInterface&           GetInputInterface() const   { return _inputInterface; }
        const OutputInterface&          GetOutputInterface() const  { return _outputInterface; }

        TransformationMachine();
        ~TransformationMachine();
    protected:
        uint32*                         _commandStream;
        size_t                          _commandStreamSize;
        unsigned                        _outputMatrixCount;
        TransformationParameterSet      _defaultParameters;

        InputInterface      _inputInterface;
        OutputInterface     _outputInterface;

        const uint32*   GetCommandStream()      { return _commandStream; }
        const size_t    GetCommandStreamSize()  { return _commandStreamSize; }

        template<typename IteratorType>
            void GenerateOutputTransformsInternal(
                Float4x4 output[], unsigned outputCount,
                const TransformationParameterSet*   parameterSet,
                IteratorType                        debugIterator,
                const void*                         iteratorUserData) const;
    };

    #pragma pack(pop)

////////////////////////////////////////////////////////////////////////////////////////////
    //      b i n d i n g s         //

    class AnimationSetBinding
    {
    public:
        AnimationSetBinding(const AnimationSet::OutputInterface&            output,
                            const TransformationMachine::InputInterface&    input);

        std::vector<unsigned>   _animDriverToMachineParameter;
    };

    class SkeletonBinding
    {
    public:
        SkeletonBinding(    const TransformationMachine::OutputInterface&   output,
                            const ModelCommandStream::InputInterface&       input);

        std::vector<unsigned>   _modelJointIndexToMachineOutput;
        std::vector<Float4x4>   _modelJointIndexToInverseBindMatrix;
    };

////////////////////////////////////////////////////////////////////////////////////////////
    //      r e n d e r e r         //

    class ModelRenderer::Pimpl
    {
    public:
        class Mesh
        {
        public:
                // each mesh within the model can have a different vertex input interface
                //  (and so needs a different technique interface)
            unsigned _id;
            unsigned _vbOffset, _ibOffset;
            unsigned _vertexStride;
            NativeFormat _indexFormat;

            unsigned _sourceFileVBOffset, _sourceFileVBSize;
            unsigned _sourceFileIBOffset, _sourceFileIBSize;
        };

        class SkinnedMesh : public Mesh
        {
        public:
                //  vertex data is separated into several "streams". These match the low level api streams.
                //  "mesh" members contain the static geometry elements.
            struct VertexStreams { enum Enum { AnimatedGeo, SkeletonBinding, Max }; };

            unsigned _extraVbOffset[VertexStreams::Max];
            unsigned _extraVbStride[VertexStreams::Max];

            unsigned _sourceFileExtraVBOffset[VertexStreams::Max], _sourceFileExtraVBSize[VertexStreams::Max];

            uint64 _animatedAIHash;

            BoundSkinnedGeometry* _scaffold;
        };

        std::vector<Metal::DeferredShaderResource*> _boundTextures;
        size_t  _texturesPerMaterial;
        
            //  Parallel arrays, ordered by draw calls, as we encounter them 
            //  while rendering each mesh.
        std::vector<unsigned>       _resourcesIndices;
        std::vector<unsigned>       _techniqueInterfaceIndices;
        std::vector<unsigned>       _shaderNameIndices;
        std::vector<unsigned>       _materialParameterBoxIndices;
        std::vector<unsigned>       _geoParameterBoxIndices;
        std::vector<unsigned>       _constantBufferIndices;

        Metal::VertexBuffer         _vertexBuffer;
        Metal::IndexBuffer          _indexBuffer;
        std::vector<Mesh>           _meshes;
        std::vector<SkinnedMesh>    _skinnedMeshes;
        std::vector<Metal::ConstantBuffer>  _constantBuffers;

        typedef std::pair<unsigned, DrawCallDesc> MeshAndDrawCall;
        std::vector<MeshAndDrawCall>    _drawCalls;
        std::vector<MeshAndDrawCall>    _skinnedDrawCalls;

        ModelScaffold*      _scaffold;
        unsigned            _levelOfDetail;

        Pimpl() : _scaffold(nullptr), _levelOfDetail(~unsigned(0x0)) {}
        ~Pimpl() {}

        Metal::BoundUniforms* BeginVariation(
            const Context&, const SharedStateSet&, unsigned) const;

        void BeginGeoCall(
            const Context&          context,
            Metal::ConstantBuffer&  localTransformBuffer,
            const MeshToModel*      transforms,
            Float4x4                modelToWorld,
            unsigned                geoCallIndex) const;

        void BeginSkinCall(
            const Context&          context,
            Metal::ConstantBuffer&  localTransformBuffer,
            const MeshToModel*      transforms,
            Float4x4                modelToWorld,
            unsigned                geoCallIndex,
            PreparedAnimation*      preparedAnimation) const;

        void ApplyBoundUnforms(
            const Context&                  context,
            Metal::BoundUniforms&           boundUniforms,
            unsigned                        resourcesIndex,
            unsigned                        constantsIndex,
            const Metal::ConstantBuffer*    cbs[2]);

        void BuildSkinnedBuffer(
                Metal::DeviceContext*       context,
                const SkinnedMesh&          mesh,
                const Float4x4              transformationMachineResult[],
                const SkeletonBinding&      skeletonBinding,
                Metal::VertexBuffer&        outputResult,
                unsigned                    outputOffset) const;

        void StartBuildingSkinning(
            Metal::DeviceContext& context, SkinningBindingBox& bindingBox) const;
        void EndBuildingSkinning(Metal::DeviceContext& context) const;
    };


    template <int Count>
        static unsigned BuildLowLevelInputAssembly(Metal::InputElementDesc (&result)[Count], 
            const VertexElement* source, unsigned sourceCount,
            unsigned startPoint = 0, unsigned lowLevelSlot = 0)
        {
            unsigned vertexElementCount = startPoint;
            for (unsigned i=0; i<sourceCount; ++i) {
                auto& sourceElement = source[i];
                assert((vertexElementCount+1) <= Count);
                if ((vertexElementCount+1) <= Count) {
                        // in some cases we need multiple "slots". When we have multiple slots, the vertex data 
                        //  should be one after another in the vb (that is, not interleaved)
                    result[vertexElementCount++] = Metal::InputElementDesc(
                        sourceElement._semantic, sourceElement._semanticIndex,
                        sourceElement._format, lowLevelSlot, sourceElement._startOffset);
                }
            }
            return vertexElementCount;
        }

    template <typename Type>
        void DestroyArray(const Type* begin, const Type* end)
        {
            for (auto i=begin; i!=end; ++i) { i->~Type(); }
        }

}}

