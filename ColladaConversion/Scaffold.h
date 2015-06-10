// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ScaffoldDataFlow.h"
#include "../Math/Vector.h"
#include "../Math/Matrix.h"
#include "../Utility/Streams/XmlStreamFormatter.h"
#include "../Utility/Streams/StreamDom.h"
#include <vector>
#include <memory>

namespace ColladaConversion
{
    using Formatter = XmlInputStreamFormatter<utf8>;
    using Section = Formatter::InteriorSection;
    using SubDoc = Utility::Document<Formatter>;
    using String = std::basic_string<Formatter::value_type>;

    class Effect;
    class MeshGeometry;
    class VisualScene;
    class SkinController;
    class Material;
    class VertexInputs;

    class AssetDesc
    {
    public:
        float _metersPerUnit;
        enum class UpAxis { X, Y, Z };
        UpAxis _upAxis;

        AssetDesc();
        AssetDesc(Formatter& formatter);
    };

    class PublishedElements
    {
    public:
        const DataFlow::Source* FindSource(uint64 id) const;
        const VertexInputs* FindVertexInputs(uint64 id) const;

        void Add(DataFlow::Source&& element);
        void Add(VertexInputs&& vertexInputs);

        PublishedElements();
        ~PublishedElements();
    protected:
        std::vector<std::pair<uint64, DataFlow::Source>> _sources;
        std::vector<std::pair<uint64, VertexInputs>> _vertexInputs;
    };

    class ElementGuid
    {
    public:
        uint64 _fileHash;
        uint64 _id;

        ElementGuid(Section uri);
    };

    class URIResolveContext
    {
    public:
        const PublishedElements* FindFile(uint64) const;

        URIResolveContext(std::shared_ptr<PublishedElements> localDoc);
    protected:
        std::vector<std::pair<uint64, std::shared_ptr<PublishedElements>>> _files;
    };

    class DocumentScaffold
    {
    public:
        void Parse(Formatter& formatter);

        void Parse_LibraryEffects(Formatter& formatter);
        void Parse_LibraryGeometries(Formatter& formatter);
        void Parse_LibraryVisualScenes(Formatter& formatter);
        void Parse_LibraryControllers(Formatter& formatter);
        void Parse_LibraryMaterials(Formatter& formatter);
        void Parse_Scene(Formatter& formatter);

        DocumentScaffold();
        ~DocumentScaffold();

    // protected:
        AssetDesc _rootAsset;

        std::vector<Effect> _effects;
        std::vector<MeshGeometry> _geometries;
        std::vector<VisualScene> _visualScenes;
        std::vector<SkinController> _skinControllers;
        std::vector<Material> _materials;

        std::shared_ptr<PublishedElements> _published;

        Section _visualScene;
        Section _physicsScene;
        Section _kinematicsScene;
    };


    enum class SamplerAddress
    {
        Wrap, Mirror, Clamp, Border, MirrorOnce
    };

    enum class SamplerFilter { Point, Linear, Anisotropic };
    enum class SamplerDimensionality { T2D, T3D, Cube };
    
    class ParameterSet
    {
    public:
        class BasicParameter
        {
        public:
            Section _sid;
            Section _type;
            Section _value;
        };

        class SamplerParameter
        {
        public:
            Section _sid;
            Section _type;
            Section _image;
            SamplerDimensionality _dimensionality;
            SamplerAddress _addressS;
            SamplerAddress _addressT;
            SamplerAddress _addressQ;
            SamplerFilter _minFilter;
            SamplerFilter _maxFilter;
            SamplerFilter _mipFilter;
            Float4 _borderColor;
            unsigned _minMipLevel;
            unsigned _maxMipLevel;
            float _mipMapBias;
            unsigned _maxAnisotrophy;
            SubDoc _extra;

            SamplerParameter();
            SamplerParameter(Formatter& formatter, Section sid, Section eleName);
            ~SamplerParameter();
        };

        class SurfaceParameter
        {
        public:
            Section _sid;
            Section _type;
            Section _initFrom;

            SurfaceParameter() {}
            SurfaceParameter(Formatter& formatter, Section sid, Section eleName);
        };

        void ParseParam(Formatter& formatter);
        
        ParameterSet();
        ~ParameterSet();
        ParameterSet(ParameterSet&& moveFrom) never_throws;
        ParameterSet& operator=(ParameterSet&&) never_throws;

    protected:
        std::vector<BasicParameter> _parameters;
        std::vector<SamplerParameter> _samplerParameters;
        std::vector<SurfaceParameter> _surfaceParameters;
    };

    class GeometryPrimitives
    {
    public:
        size_t GetInputCount() const { return _inputCount; }
        const DataFlow::Input& GetInput(size_t index) const
        {
            assert(index < _inputCount);
            if (index < dimof(_inputs)) return _inputs[index];
            return _inputsOverflow[index - dimof(_inputs)];
        }

        size_t GetPrimitiveDataCount() const { return _primitiveDataCount; }
        Section GetPrimitiveData(size_t index) const
        {
            assert(index < _primitiveDataCount);
            if (index < dimof(_primitiveData)) return _primitiveData[index];
            return _primitiveDataOverflow[index - dimof(_primitiveData)];
        }

        Section GetType() const { return _type; }
        Section GetVCountArray() const { return _vcount; }
        Section GetMaterialBinding() const { return _materialBinding; }
        unsigned GetPrimitiveCount() const { return _primitiveCount; }

        const StreamLocation& GetLocation() const { return _location; }

        GeometryPrimitives(Formatter& formatter, Section type);
        GeometryPrimitives();
        GeometryPrimitives(GeometryPrimitives&& moveFrom) never_throws;
        GeometryPrimitives& operator=(GeometryPrimitives&& moveFrom) never_throws;

    protected:
        Section _type;

        DataFlow::Input _inputs[6];
        std::vector<DataFlow::Input> _inputsOverflow;
        unsigned _inputCount;

            // in most cases, there is only one "_primitiveData" element
            // but for trianglestrip, there may be multiple
        Section _primitiveData[1];
        std::vector<Section> _primitiveDataOverflow;
        unsigned _primitiveDataCount;

        Section _vcount;
        Section _materialBinding;
        unsigned _primitiveCount;
        StreamLocation _location;
    };


    class MeshGeometry
    {
    public:
        size_t GetPrimitivesCount() const { return _geoPrimitives.size(); }
        auto GetPrimitives(size_t index) const -> const GeometryPrimitives& { return _geoPrimitives[index]; }

        Section GetBaseVertexInputs() const { return _baseVertexInputs; }

        MeshGeometry(Formatter& formatter, PublishedElements& pub);

        MeshGeometry();
        MeshGeometry(MeshGeometry&& moveFrom) never_throws;
        MeshGeometry& operator=(MeshGeometry&& moveFrom) never_throws;

    protected:
        void ParseMesh(Formatter& formatter, PublishedElements& pub);

        std::vector<DataFlow::Source> _sources;
        std::vector<GeometryPrimitives> _geoPrimitives;
        SubDoc _extra;

        Section _baseVertexInputs;
    };

    class VertexInputs
    {
    public:
        size_t GetCount() const { return _vertexInputs.size(); }
        auto GetInput(size_t index) const -> const DataFlow::InputUnshared& { return _vertexInputs[index]; }

        Section GetId() const { return _id; }

        VertexInputs(Formatter& formatter);
        VertexInputs();
        VertexInputs(VertexInputs&& moveFrom) never_throws;
        VertexInputs& operator=(VertexInputs&& moveFrom) never_throws;
    protected:
        std::vector<DataFlow::InputUnshared> _vertexInputs;
        Section _id;
    };

    class SkinController
    {
    public:
        
        SkinController(Formatter& formatter, Section id, Section name);
        SkinController(SkinController&& moveFrom) never_throws;
        SkinController& operator=(SkinController&& moveFrom) never_throws;
        SkinController();
        ~SkinController();

    protected:
        void ParseJoints(Formatter& formatter);
        void ParseVertexWeights(Formatter& formatter);

        Section _baseMesh;
        Section _id;
        Section _name;
        SubDoc _extra;

        Float4x4 _bindShapeMatrix;
        unsigned _weightCount;
        Section _influenceCountPerVertex;   // (this the <vcount> element)
        Section _influences;                // (this is the <v> element)
        std::vector<DataFlow::Input> _influenceInputs;

        std::vector<DataFlow::Source> _sources;
        std::vector<DataFlow::InputUnshared> _jointInputs;
    };


    class Material
    {
    public:
        Section _id;
        Section _name;
        Section _effectReference;   // uri
        SubDoc _extra;

        Material() {}
        Material(Formatter& formatter);

    protected:
        void ParseInstanceEffect(Formatter& formatter);
    };
}

