// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Metal/Format.h"
#include "../Metal/Forward.h"
#include "../../Math/Matrix.h"
#include "../../Core/Types.h"
#include <vector>
#include <memory>

namespace SceneEngine 
{
    class LightingParserContext; class TechniqueInterface; 
    class ResolvedShader;
}

namespace RenderCore { namespace Assets { class SharedStateSet; } }

namespace RenderCore { namespace Assets { namespace Simple
{
    ///////////////////////////////////////////////////////////////////////////////
        //   s c a f f o l d   //
    ///////////////////////////////////////////////////////////////////////////////

    class ScaffoldMaterial
    {
    public:
        char        _name[128];
        int         _materialId;
        unsigned    _subMaterialCount;
        int         _subMaterialIds[32];
    };

    class ScaffoldDrawCall
    {
    public:
        unsigned    _firstIndex, _indexCount;
        unsigned    _firstVertex, _vertexCount;
        int         _subMaterialIndex;
    };

    class ScaffoldMesh
    {
    public:
        class DataStream
        {
        public:
                //
                //  Some streams have multiple vertex input elements. 
                //  In these cases, the first 3 members apply to vertex 
                //  input elements.
                //  But, the final members apply to the entire data stream.
                //
            std::string _semantic[2];
            unsigned    _semanticIndex[2];
            Metal::NativeFormat::Enum _format[2];

            unsigned    _elementSize;
            unsigned    _elementCount;
            unsigned    _streamSize;
            unsigned    _fileOffset;
        };

        int _meshId;
        int _vertexCount, _indexCount;

        std::vector<DataStream>         _dataStreams;
        unsigned                        _indexBufferFileOffset;
        unsigned                        _indexBufferSize;
        Metal::NativeFormat::Enum       _indexFormat;
        std::vector<ScaffoldDrawCall>   _drawCalls;
    };

    class ScaffoldLevelOfDetail
    {
    public:
        std::unique_ptr<uint8>  _transformMachine;
        unsigned                _lodIndex;

        class MeshCall
        {
        public:
            unsigned    _transformMarker;
            int         _meshId;
            int         _materialId;
        };
        std::vector<MeshCall>   _meshCalls;

        ScaffoldLevelOfDetail();
        ~ScaffoldLevelOfDetail();
        ScaffoldLevelOfDetail(ScaffoldLevelOfDetail&& moveFrom);
        ScaffoldLevelOfDetail& operator=(ScaffoldLevelOfDetail&& moveFrom);

    private:
        ScaffoldLevelOfDetail(const ScaffoldLevelOfDetail&);
        ScaffoldLevelOfDetail& operator=(const ScaffoldLevelOfDetail&);
    };

    /// <summary>Contains information related to the layout and contents of a model file<summary>
    /// When loading a model file, there are 2 steps:
    ///     * load the scaffold
    ///     * load the large resources (like vertex and index buffers) from locations defined by the scaffold
    /// The scaffold itself is just a light-weight list of everything contained in the model file.
    /// It's intended to require little memory, but provide information about where large resources are
    /// stored in the source file.
    ///
    /// There are a bunch of advantages to this model:
    ///     * we can load a model when the low-level graphics API is not available
    ///         (to query a file for metrics -- vertex count, etc)
    ///     * we can load only a part of the large resources in a file
    ///         (for example, loading only a single LOD out of many)
    ///     * we can retain the scaffold information in memory longer than the large resources
    ///         (since the scaffold is small, we can store many in memory, without concerns
    ///         for overflowing the heap or causing fragmentation)
    ///
    /// Normally the ModelScaffold is used with a MaterialScaffold. These can come from different files
    /// (or perhaps the same file). Generally the model should contain geometry and shape information. The
    /// material should define how to render that geometry (including texture and shader bindings)
    ///
    /// Separating the two allows for a little bit of flexibility. Sometimes we want to render the same
    /// model with multiple different materials (for example, multiple re-textures). It might also be 
    /// handy for work-flow to separate the information.
    ///
    /// Before rendering a model, we must construct a ModelRenderer. This object loads the graphics api
    /// resources and prepares the model for rendering.
    ///
    /// <seealso cref="MaterialScaffold" />
    /// <seealso cref="ModelRenderer" />
    class ModelScaffold
    {
    public:
        const std::string&          Filename() const { return _filename; }
        std::pair<Float3, Float3>   GetStaticBoundingBox(unsigned lodIndex = 0) const;
        unsigned                    GetMaxLOD() const { return _maxLOD; }

        unsigned                    GetMaterialRefCount() const            { return _materials.size(); }
        const ScaffoldMaterial&     GetMaterialRef(unsigned index) const   { return _materials[index]; }

        ModelScaffold();
        ModelScaffold(ModelScaffold&& moveFrom);
        ModelScaffold& operator=(ModelScaffold&& moveFrom);
        ~ModelScaffold();

    protected:
        std::vector<ScaffoldLevelOfDetail>  _lods;
        std::vector<ScaffoldMesh>           _meshes;
        std::vector<ScaffoldMaterial>       _materials;
        std::string _filename;
        unsigned _maxLOD;

        ModelScaffold(const ModelScaffold&);
        ModelScaffold& operator=(const ModelScaffold&);

        friend class ModelRenderer;
    };

    /// <summary>Contains information related to the layout and contents of a material file<summary>
    /// See ModelScaffold for some related information.
    ///
    /// \todo --    We need some support for animated material parameters. Generally the values 
    ///             within the scaffold itself should be constant (and reflect the contents on
    ///             disk). But some of these parameters might be animated; so there should be
    ///             a way to provide animations for them.
    /// <seealso cref="ModelScaffold" />
    class MaterialScaffold
    {
    public:
        class MaterialDefinition
        {
        public:
            struct Flags { enum Enum { DoubleSided }; typedef unsigned BitField; };
            std::string _materialName, _shaderName;
            Float3 _diffuseColor, _specularColor;
            float _opacity, _alphaThreshold;
            std::vector<std::pair<std::string, std::string>> _boundTextures;
            Flags::BitField _flags;
            
            MaterialDefinition();
        };

        size_t GetSubMaterialCount() const                          { return _subMaterials.size(); }
        const MaterialDefinition& GetSubMaterial(size_t c) const    { return _subMaterials[c]; }
        const MaterialDefinition* GetSubMaterial(const char name[]) const;

        MaterialScaffold();
        MaterialScaffold(MaterialScaffold&& moveFrom);
        MaterialScaffold& operator=(MaterialScaffold&& moveFrom);
        ~MaterialScaffold();
    
    protected:
        std::vector<MaterialDefinition> _subMaterials;
    };

    ///////////////////////////////////////////////////////////////////////////////
        //   r e n d e r e r   //
    ///////////////////////////////////////////////////////////////////////////////
    
    /// <summary>Functionality for rendering a given model file<summary>
    /// Given a model and a material, prepare for rendering.
    ///
    /// Normally the ModelScaffold and MaterialScaffold don't contain all of the resources
    /// required for rendering. These only contain some layout and metrics information. To
    /// actually create the graphics api resources and prepare for rendering, we need to
    /// instantiate a ModelRenderer.
    ///
    /// The renderer will bind together the model and material and prepare an optimized
    /// internal representation for rendering.
    ///
    /// Often models are loaded as part of a larger set of similar objects. For example, 
    /// there may be many different tree models loaded. In these cases, it's useful to
    /// share some state information between models. Many models might use the same shader.
    /// In these cases we can use a SharedStateSet to share some resources between multiple
    /// models. 
    ///
    /// Use of SharedStateSets is also critical for sorting render operations by state.
    /// This is important for reducing the api load when rendering large scenes.
    ///
    /// <seealso cref="ModelScaffold"/>
    /// <seealso cref="MaterialScaffold"/>
    /// <seealso cref="SharedStateSet"/>
    class ModelRenderer
    {
    public:
        void    Render(
            Metal::DeviceContext* context, 
            SceneEngine::LightingParserContext& parserContext,
            unsigned techniqueIndex,
            const SharedStateSet& sharedStateSet,
            const Float4x4& modelToWorld, unsigned vegetationSpawnObjectIndex = 0);

        class SortedModelDrawCalls
        {
        public:
            class Entry;
            std::vector<Entry> _entries;
            SortedModelDrawCalls();
            ~SortedModelDrawCalls();
            void Reset();
        };
        void    Prepare(SortedModelDrawCalls& dest, const SharedStateSet& sharedStateSet, const Float4x4& modelToWorld);
        static void RenderPrepared(
            SortedModelDrawCalls& drawCalls,
            Metal::DeviceContext* context, 
            SceneEngine::LightingParserContext& parserContext,
            unsigned techniqueIndex,
            const SharedStateSet& sharedStateSet);
                                
        ModelRenderer(ModelScaffold& scaffold, MaterialScaffold& material, SharedStateSet& sharedStateSet, unsigned levelOfDetail);
        ~ModelRenderer();

    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        ModelScaffold * _scaffold;      // unprotected pointer!
        unsigned        _levelOfDetail;
        unsigned        _scaffoldLODIndex;
    };


}}}



