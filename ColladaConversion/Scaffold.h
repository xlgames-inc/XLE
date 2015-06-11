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
    class DocumentScaffold;

    class AssetDesc
    {
    public:
        float _metersPerUnit;
        enum class UpAxis { X, Y, Z };
        UpAxis _upAxis;

        AssetDesc();
        AssetDesc(Formatter& formatter);
    };

///////////////////////////////////////////////////////////////////////////////////////////////////
        //      E F F E C T S   A N D   M A T E R I A L S
///////////////////////////////////////////////////////////////////////////////////////////////////

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

    class Material
    {
    public:
        DocScopeId _id;
        Section _name;
        Section _effectReference;   // uri
        SubDoc _extra;

        const DocScopeId& GetId() const { return _id; }

        Material() {}
        Material(Formatter& formatter);

    protected:
        void ParseInstanceEffect(Formatter& formatter);
    };

///////////////////////////////////////////////////////////////////////////////////////////////////
        //      G E O M E T R Y
///////////////////////////////////////////////////////////////////////////////////////////////////

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
        const DocScopeId& GetId() const { return _id; }
        Section GetName() const { return _name; }

        MeshGeometry(Formatter& formatter, DocumentScaffold& pub);

        MeshGeometry();
        MeshGeometry(MeshGeometry&& moveFrom) never_throws;
        MeshGeometry& operator=(MeshGeometry&& moveFrom) never_throws;

    protected:
        void ParseMesh(Formatter& formatter, DocumentScaffold& pub);

        std::vector<DataFlow::Source> _sources;
        std::vector<GeometryPrimitives> _geoPrimitives;
        SubDoc _extra;

        Section _baseVertexInputs;
        DocScopeId _id;
        Section _name;
    };

    class VertexInputs
    {
    public:
        size_t GetCount() const { return _vertexInputs.size(); }
        auto GetInput(size_t index) const -> const DataFlow::InputUnshared& { return _vertexInputs[index]; }

        const DocScopeId& GetId() const { return _id; }

        VertexInputs(Formatter& formatter);
        VertexInputs();
        VertexInputs(VertexInputs&& moveFrom) never_throws;
        VertexInputs& operator=(VertexInputs&& moveFrom) never_throws;
    protected:
        std::vector<DataFlow::InputUnshared> _vertexInputs;
        DocScopeId _id;
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
        DocScopeId _id;
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

///////////////////////////////////////////////////////////////////////////////////////////////////
        //      T R A N S F O R M A T I O N S
///////////////////////////////////////////////////////////////////////////////////////////////////

    class Transformation;

    class TransformationSet
    {
    public:
        enum class Type 
        {
            None,
            LookAt, Matrix4x4, Rotate, 
            Scale, Skew, Translate 
        };

        static bool IsTransform(Section section);
        unsigned ParseTransform(
            Formatter& formatter, Section elementName, 
            unsigned previousSibling = ~unsigned(0));

        Transformation Get(unsigned index) const;

        TransformationSet();
        TransformationSet(TransformationSet&& moveFrom) never_throws;
        TransformationSet& operator=(TransformationSet&& moveFrom) never_throws;
        ~TransformationSet();

    protected:
        class RawOperation;
        std::vector<RawOperation> _operations;

        friend class Transformation;
    };

    class Transformation
    {
    public:
        TransformationSet::Type GetType() const;
        Transformation GetNext() const;
        const void* GetUnionData() const;

        operator bool() const;
        bool operator!() const;

    protected:
        unsigned _index;
        const TransformationSet* _set;

        Transformation(const TransformationSet& set, unsigned index);
        friend class TransformationSet;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////
        //      I N S T A N T I A T E
///////////////////////////////////////////////////////////////////////////////////////////////////

    class InstanceGeometry
    {
    public:
        class MaterialBinding
        {
        public:
            Section _technique;
            Section _reference;
            Section _bindingSymbol;

            MaterialBinding(Formatter& formatter, Section technique);
        };

        Section _reference;
        std::vector<MaterialBinding> _matBindings;

        InstanceGeometry(Formatter& formatter);
        InstanceGeometry();
        InstanceGeometry(InstanceGeometry&& moveFrom) never_throws;
        InstanceGeometry& operator=(InstanceGeometry&& moveFrom) never_throws;

    protected:
        void ParseBindMaterial(Formatter& formatter);
        void ParseTechnique(Formatter& formatter, Section techniqueProfile);
    };

    class InstanceController : public InstanceGeometry
    {
    public:
        Section _skeleton;

        InstanceController(Formatter& formatter);
        InstanceController(InstanceController&& moveFrom) never_throws;
        InstanceController& operator=(InstanceController&& moveFrom) never_throws;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////
        //      V I S U A L   S C E N E   S E C T I O N
///////////////////////////////////////////////////////////////////////////////////////////////////

    class Node;
    class InstanceGeometry;
    class InstanceController;

    class VisualScene
    {
    public:
        SubDoc _extra;

        Node GetRootNode() const;

        const InstanceGeometry& GetInstanceGeometry(unsigned index) const;
        Node GetInstanceGeometry_Attach(unsigned index) const;
        unsigned GetInstanceGeometryCount() const;

        const InstanceGeometry& GetInstanceController(unsigned index) const;
        Node GetInstanceController_Attach(unsigned index) const;
        unsigned GetInstanceControllerCount() const;

        VisualScene(Formatter& formatter);
        VisualScene();
        VisualScene(VisualScene&& moveFrom) never_throws;
        VisualScene& operator=(VisualScene&& moveFrom) never_throws;

    protected:
        using IndexIntoNodes = unsigned;
        using TransformationSetIndex = unsigned;
        static const IndexIntoNodes IndexIntoNodes_Invalid = ~IndexIntoNodes(0);
        static const TransformationSetIndex TransformationSetIndex_Invalid = ~TransformationSetIndex(0);

        class RawNode;
        std::vector<RawNode> _nodes;
        std::vector<std::pair<IndexIntoNodes, InstanceGeometry>> _geoInstances;
        std::vector<std::pair<IndexIntoNodes, InstanceController>> _controllerInstances;
        TransformationSet _transformSet;

        friend class Node;
    };

    class Node
    {
    public:
        Node GetNextSibling() const;
        Node GetFirstChild() const;
        Node GetParent() const;
        Transformation GetFirstTransform() const;
        Section GetName() const;
        const DocScopeId& GetId() const;
        
        operator bool() const;
        bool operator!() const;

    protected:
        VisualScene::IndexIntoNodes _index;
        const VisualScene* _scene;

        Node(const VisualScene& scene, VisualScene::IndexIntoNodes index);
        friend class VisualScene;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////
        //      D O C U M E N T   R E L A T E D
///////////////////////////////////////////////////////////////////////////////////////////////////

    class IDocScopeIdResolver
    {
    public:
        virtual const DataFlow::Source* FindSource(uint64 guid) const = 0;
        virtual const VertexInputs*     FindVertexInputs(uint64 guid) const = 0;
        virtual const MeshGeometry*     FindMeshGeometry(uint64 guid) const = 0;
        virtual const Material*         FindMaterial(uint64 guid) const = 0;
        virtual ~IDocScopeIdResolver();
    };

    class GuidReference
    {
    public:
        uint64 _fileHash;
        uint64 _id;

        GuidReference(Section uri);
    };

    class URIResolveContext
    {
    public:
        const IDocScopeIdResolver* FindFile(uint64) const;

        URIResolveContext(std::shared_ptr<IDocScopeIdResolver> localDoc);
        ~URIResolveContext();
    protected:
        std::vector<std::pair<uint64, std::shared_ptr<IDocScopeIdResolver>>> _files;
    };

    class DocumentScaffold : public IDocScopeIdResolver
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

        void Add(DataFlow::Source&& element);
        void Add(VertexInputs&& vertexInputs);

        const DataFlow::Source* FindSource(uint64 guid) const;
        const VertexInputs*     FindVertexInputs(uint64 guid) const;
        const MeshGeometry*     FindMeshGeometry(uint64 guid) const;
        const Material*         FindMaterial(uint64 guid) const;

    // protected:
        AssetDesc _rootAsset;

        std::vector<Effect> _effects;
        std::vector<MeshGeometry> _geometries;
        std::vector<VisualScene> _visualScenes;
        std::vector<SkinController> _skinControllers;
        std::vector<Material> _materials;

        Section _visualScene;
        Section _physicsScene;
        Section _kinematicsScene;

        std::vector<std::pair<uint64, DataFlow::Source>> _sources;
        std::vector<std::pair<uint64, VertexInputs>> _vertexInputs;
    };
    
}

