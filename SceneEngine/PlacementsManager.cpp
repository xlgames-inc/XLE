// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PlacementsManager.h"
#include "../RenderCore/Assets/ModelSimple.h"
#include "../RenderCore/Assets/SharedStateSet.h"
#include "../SceneEngine/LightingParserContext.h"
#include "../SceneEngine/ModelFormat.h"
#include "../Assets/Assets.h"
#include "../Assets/ChunkFile.h"

#include "../RenderCore/Assets/ModelRunTime.h"

#include "../ConsoleRig/Log.h"
#include "../Math/Matrix.h"
#include "../Math/Transformations.h"
#include "../Math/ProjectionMath.h"
#include "../Math/Geometry.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/HeapUtils.h"
#include "../Utility/IteratorUtils.h"
#include "../Utility/StringFormat.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/Streams/DataSerialize.h"
#include "../Core/Types.h"

#include <random>

namespace SceneEngine
{
    using Assets::ResChar;

///////////////////////////////////////////////////////////////////////////////////////////////////

        // Note that "placements" that interface methods in Placements are actually
        // very rarely called. So it should be fine to make those methods into virtual
        // methods, and use an abstract base class.
    class Placements
    {
    public:
        typedef std::pair<Float3, Float3> BoundingBox;

        class ObjectReference
        {
        public:
            Float3x4    _localToCell;
            BoundingBox _worldSpaceBoundary;
            unsigned    _modelFilenameOffset;       // note -- hash values should be stored with the filenames
            unsigned    _materialFilenameOffset;
            uint64      _guid;
        };
        
        const ObjectReference*  GetObjectReferences() const;
        unsigned                GetObjectReferenceCount() const;
        const void*             GetFilenamesBuffer() const;

        const ::Assets::DependencyValidation& GetDependancyValidation() const { return *_dependencyValidation; }

        Placements(const ResChar filename[]);
        Placements();
        ~Placements();
    protected:
        std::vector<ObjectReference>    _objects;
        std::vector<uint8>              _filenamesBuffer;

        std::shared_ptr<::Assets::DependencyValidation>   _dependencyValidation;
    };

    auto        Placements::GetObjectReferences() const -> const ObjectReference*   { return AsPointer(_objects.begin()); }
    unsigned    Placements::GetObjectReferenceCount() const                         { return _objects.size(); }
    const void* Placements::GetFilenamesBuffer() const                              { return AsPointer(_filenamesBuffer.begin()); }

    static const uint64 ChunkType_Placements = ConstHash64<'Plac','emen','ts'>::Value;

    class PlacementsHeader
    {
    public:
        unsigned _version;
        unsigned _objectRefCount;
        unsigned _filenamesBufferSize;
        unsigned _dummy;
    };

    Placements::Placements(const ResChar filename[])
    {
            //
            //  Extremely simple file format for placements
            //  We just need 2 blocks:
            //      * list of object references
            //      * list of filenames / strings
            //  The strings are kept separate from the object placements
            //  because many of the string will be referenced multiple
            //  times. It just helps reduce file size.
            //

        using namespace Serialization::ChunkFile;
        BasicFile file(filename, "rb");
        auto chunks = LoadChunkTable(file);
        auto i = std::find_if(
            chunks.begin(), chunks.end(), 
            [](const ChunkHeader& hdr) { return hdr._type == ChunkType_Placements; });
        if (i == chunks.end()) {
            ThrowException(::Assets::Exceptions::InvalidResource(filename, "Missing correct chunks"));
        }

        file.Seek(i->_fileOffset, SEEK_SET);
        PlacementsHeader hdr;
        file.Read(&hdr, sizeof(hdr), 1);
        if (hdr._version != 0) {
            ThrowException(::Assets::Exceptions::InvalidResource(filename, 
                StringMeld<128>() << "Unexpected version number (" << hdr._version << ")"));
        }

        std::vector<ObjectReference> objects;
        std::vector<uint8> filenamesBuffer;
        objects.resize(hdr._objectRefCount);
        objects.resize(hdr._filenamesBufferSize);
        file.Read(AsPointer(objects.begin()), sizeof(ObjectReference), hdr._objectRefCount);
        file.Read(AsPointer(filenamesBuffer.begin()), 1, hdr._filenamesBufferSize);

        auto depValidation = std::make_shared<Assets::DependencyValidation>();
        RegisterFileDependency(depValidation, filename);

        _objects = std::move(objects);
        _filenamesBuffer = std::move(filenamesBuffer);
        _dependencyValidation = std::move(depValidation);
    }

    Placements::Placements()
    {
        auto depValidation = std::make_shared<Assets::DependencyValidation>();
        _dependencyValidation = std::move(depValidation);
    }

    Placements::~Placements()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class PlacementCell
    {
    public:
        char        _filename[256];
        uint64      _filenameHash;
        Float4x4    _cellToWorld;
        Float3      _aabbMin, _aabbMax;
    };

    class PlacementsQuadTree;

    class PlacementsRenderer
    {
    public:
        void EnterState(RenderCore::Metal::DeviceContext* context);
        void Render(
            RenderCore::Metal::DeviceContext* context,
            LightingParserContext& parserContext, 
            unsigned techniqueIndex,
            const PlacementCell& cell);

        typedef RenderCore::Assets::Simple::ModelScaffold ModelScaffold;
        typedef RenderCore::Assets::Simple::MaterialScaffold MaterialScaffold;
        typedef RenderCore::Assets::Simple::ModelRenderer ModelRenderer;
        typedef ModelRenderer::SortedModelDrawCalls PreparedState;
        
        auto GetCachedModel(const ResChar filename[]) -> const ModelScaffold&;
        auto GetCachedPlacements(const char filename[]) -> const Placements&;
        void SetOverride(uint64 guid, const Placements* placements);

        PlacementsRenderer();
        ~PlacementsRenderer();
    protected:
        class CellRenderInfo
        {
        public:
            const Placements* _placements;
            // std::unique_ptr<PlacementsQuadTree> _quadTree;
        };

        std::vector<std::pair<uint64, CellRenderInfo>> _cellOverrides;
        std::vector<std::pair<uint64, CellRenderInfo>> _cells;
        
            //  We keep a single cache of model files for every cell
            //  This might mean that the SharedStateSet could grow
            //  very large. However, there is no way to remove states
            //  from that state, after they've been added.
        class Cache
        {
        public:
            LRUCache<ModelScaffold>             _modelScaffolds;
            LRUCache<MaterialScaffold>          _materialScaffolds;
            LRUCache<ModelRenderer>             _modelRenderers;
            RenderCore::Assets::SharedStateSet  _sharedStates;
            PreparedState                       _preparedRenders;

            Cache()
            : _modelScaffolds(2000)
            , _materialScaffolds(2000)
            , _modelRenderers(500) {}
        };
        std::unique_ptr<Cache> _cache;

        void Render(
            RenderCore::Metal::DeviceContext* context,
            LightingParserContext& parserContext, 
            unsigned techniqueIndex,
            const CellRenderInfo& renderInfo,
            const Float4x4& cellToWorld);
    };

    class PlacementsManager::Pimpl
    {
    public:
        std::vector<PlacementCell> _cells;
        std::shared_ptr<PlacementsRenderer> _renderer;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    void PlacementsRenderer::EnterState(RenderCore::Metal::DeviceContext*)
    {
        _cache->_preparedRenders.Reset();
        _cache->_sharedStates.Reset();
    }

    auto PlacementsRenderer::GetCachedModel(const ResChar filename[]) -> const ModelScaffold&
    {
        auto hash = Hash64(filename);
        auto model = _cache->_modelScaffolds.Get(hash);
        if (!model) {
            ModelFormat modelFormat;
            model = modelFormat.CreateModel(filename);
            _cache->_modelScaffolds.Insert(hash, model);
        }
        return *model;
    }

    auto PlacementsRenderer::GetCachedPlacements(const char filename[]) -> const Placements&
    {
        auto filenameHash = Hash64(filename);
        auto i = LowerBound(_cellOverrides, filenameHash);
        if (i != _cellOverrides.end() && i->first == filenameHash) {
            return *i->second._placements;
        } else {
            auto i2 = LowerBound(_cells, filenameHash);
            if (i2 == _cells.end() || i2->first == filenameHash) {
                CellRenderInfo newRenderInfo;   // note; we really want GetAssetDepImmediate here, to prevent Pending resources
                newRenderInfo._placements = &::Assets::GetAssetDep<Placements>(filename);
                i2 = _cells.insert(i2, std::make_pair(filenameHash, std::move(newRenderInfo)));
            }

            return *i2->second._placements;
        }
    }

    void PlacementsRenderer::SetOverride(uint64 guid, const Placements* placements)
    {
        CellRenderInfo newRenderInfo;
        newRenderInfo._placements = placements;

        auto i = LowerBound(_cellOverrides, guid);
        if (i ==_cellOverrides.end() || i->first != guid) {
            _cellOverrides.insert(i, std::make_pair(guid, newRenderInfo));
        } else {
            i->second = newRenderInfo; // override the previous one
        }
    }

    void PlacementsRenderer::Render(
        RenderCore::Metal::DeviceContext* context,
        LightingParserContext& parserContext, 
        unsigned techniqueIndex,
        const PlacementCell& cell)
    {
        // Look for a "RenderInfo" for this cell.. and create it if it doesn't exist
        // Note that there's a bit of extra overhead here:
        //  * in this design, we need to search for the cell by guid id
        //  * however, the cells are probably arranged in a 2d grid, and we probably
        //      know the 2d address -- which means we could go right to the correct
        //      cell.
        //
        // But this design allows for a little extra flexibility. We're no restricted
        // on how the placement cells are arranged, so we can have overlapping cells, or
        // separate cells for inside/outside/underwater/etc. Or we can have cells that
        // represent different states (like stages of building a castle, or if a zone
        // changes over time). 
        //
        // It seems useful to me. But if the overhead becomes too great, we can just change
        // to a basic 2d addressing model.

        if (CullAABB(parserContext.GetProjectionDesc()._worldToProjection, cell._aabbMin, cell._aabbMax)) {
            return;
        }

            //  We need to look in the "_cellOverride" list first.
            //  The overridden cells are actually designed for tools. When authoring 
            //  placements, we need a way to render them before they are flushed to disk.
        TRY 
        {
            auto i = LowerBound(_cellOverrides, cell._filenameHash);
            if (i != _cellOverrides.end() && i->first == cell._filenameHash) {
                Render(context, parserContext, techniqueIndex, i->second, cell._cellToWorld);
            } else {
                auto i2 = LowerBound(_cells, cell._filenameHash);
                if (i2 == _cells.end() || i2->first == cell._filenameHash) {
                    CellRenderInfo newRenderInfo;
                    newRenderInfo._placements = &::Assets::GetAssetDep<Placements>(cell._filename);
                    i2 = _cells.insert(i2, std::make_pair(cell._filenameHash, std::move(newRenderInfo)));
                }

                // if (!i2->second._quadTree) {
                //     i2->second._quadTree = std::make_unique<PlacementsQuadTree>(
                //         *i2->second._placements);
                // }

                Render(context, parserContext, techniqueIndex, i2->second, cell._cellToWorld);
            }
        } 
        CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
        CATCH (...) {
        } CATCH_END
    }

    void PlacementsRenderer::Render(
        RenderCore::Metal::DeviceContext* context,
        LightingParserContext& parserContext, 
        unsigned techniqueIndex,
        const CellRenderInfo& renderInfo,
        const Float4x4& cellToWorld)
    {
        assert(renderInfo._placements);

            //
            //  Here we render all of the placements defined by the placement
            //  file in renderInfo._placements.
            //
            //  Many engines would drop back to a scene-tree representation 
            //  for this kind of thing. The advantage of the scene-tree, is that
            //  nodes can become many different things.
            //
            //  But here, in this case, we want to deal with exactly one type
            //  of thing -- just an object placed in the world. We can always
            //  render other types of things afterwards. So long as we use
            //  the same shared state set and the same prepared state objects,
            //  they will be sorted efficiently for rendering.
            //
            //  If we know that all objects are just placements -- we can write
            //  a very straight-forward and efficient implementation of exactly
            //  the behaviour we want. That's the advantage of this model. 
            //
            //  Using a scene tree, or some other generic structure, often the
            //  true behaviour of the system can be obscured by layers of
            //  generality. But the behaviour of the system is the most critical
            //  thing in a system like this. We want to be able to design and chart
            //  out the behaviour, and get the exact results we want. Especially
            //  when the behaviour is actually fairly simple.
            //
            //  So, to that end... Let's find all of the objects to render (using
            //  whatever culling/occlusion methods we need) and prepare them all
            //  for rendering.
            //  

        auto worldToCullSpace = Combine(cellToWorld, parserContext.GetProjectionDesc()._worldToProjection);
        auto cameraPosition = ExtractTranslation(parserContext.GetProjectionDesc()._viewToWorld);
        cameraPosition = TransformPoint(InvertOrthonormalTransform(cellToWorld), cameraPosition);

        ModelFormat modelFormat;
        auto& placements = *renderInfo._placements;
        const auto* filenamesBuffer = placements.GetFilenamesBuffer();
        const auto* objRef = placements.GetObjectReferences();
        auto placementCount = placements.GetObjectReferenceCount();
        for (unsigned c=0; c<placementCount; ++c) {

            auto& obj = objRef[c];
            if (CullAABB(worldToCullSpace, obj._worldSpaceBoundary.first, obj._worldSpaceBoundary.second)) {
                continue;
            }

                // Basic draw distance calculation
                // many objects don't need to render out to the far clip

            const float maxDistanceSq = 1000.f * 1000.f;
            float distanceSq = MagnitudeSquared(
                .5f * (obj._worldSpaceBoundary.first + obj._worldSpaceBoundary.second) - cameraPosition);
            if (distanceSq > maxDistanceSq) {
                continue;
            }

            auto modelHash = *(uint64*)PtrAdd(filenamesBuffer, obj._modelFilenameOffset);
            auto modelFilename = (const ResChar*)PtrAdd(filenamesBuffer, obj._modelFilenameOffset + sizeof(uint64));
            auto materialHash = *(uint64*)PtrAdd(filenamesBuffer, obj._materialFilenameOffset);
            auto materialFilename = (const ResChar*)PtrAdd(filenamesBuffer, obj._materialFilenameOffset + sizeof(uint64));

            auto model = _cache->_modelScaffolds.Get(modelHash);
            if (!model) {
                model = modelFormat.CreateModel(modelFilename);
                _cache->_modelScaffolds.Insert(modelHash, model);
            }

            auto material = _cache->_materialScaffolds.Get(materialHash);
            if (!material) {
                TRY {
                    material = modelFormat.CreateMaterial(materialFilename);
                } CATCH (...) {    // sometimes get missing files
                    continue;
                } CATCH_END
                _cache->_materialScaffolds.Insert(materialHash, material);
            }

                // simple LOD calculation based on distanceSq from camera...
            unsigned LOD = std::min(model->GetMaxLOD(), unsigned(distanceSq / (150.f*150.f)));
            uint64 hashedModel = (uint64(model.get()) << 2) | (uint64(material.get()) << 48) | uint64(LOD);

                //  Here we have to choose a shared state set for this object.
                //  We could potentially have more than one shared state set for this renderer
                //  and separate the objects into their correct state set, as required...
            auto renderer = _cache->_modelRenderers.Get(hashedModel);
            if (!renderer) {
                renderer = modelFormat.CreateRenderer(
                    std::ref(*model), std::ref(*material), std::ref(_cache->_sharedStates), LOD);
                _cache->_modelRenderers.Insert(hashedModel, renderer);
            }

            auto localToWorld = Combine(AsFloat4x4(obj._localToCell), cellToWorld);
            renderer->Prepare(_cache->_preparedRenders, _cache->_sharedStates, localToWorld);

        }

        ModelRenderer::RenderPrepared(
            _cache->_preparedRenders, context, parserContext, techniqueIndex, _cache->_sharedStates);
    }

    PlacementsRenderer::PlacementsRenderer()
    {
        auto cache = std::make_unique<Cache>();
        _cache = std::move(cache);
    }

    PlacementsRenderer::~PlacementsRenderer() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    void PlacementsManager::Render(
        RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext,
        unsigned techniqueIndex)
    {
            // render every registered cell
        _pimpl->_renderer->EnterState(context);
        for (auto i=_pimpl->_cells.begin(); i!=_pimpl->_cells.end(); ++i) {
            _pimpl->_renderer->Render(context, parserContext, techniqueIndex, *i);
        }
    }
    
    std::shared_ptr<PlacementsRenderer> PlacementsManager::GetRenderer()
    {
        return _pimpl->_renderer;
    }

    std::shared_ptr<PlacementsEditor> PlacementsManager::CreateEditor()
    {
            // Create a new editor, and register all cells with in.
            // What happens if new cells are created? They must be registered with
            // all editors, also...!
        auto editor = std::make_shared<PlacementsEditor>(_pimpl->_renderer);
        for (auto i=_pimpl->_cells.begin(); i!=_pimpl->_cells.end(); ++i) {
            editor->RegisterCell(
                Truncate(i->_aabbMin), Truncate(i->_aabbMax), 
                i->_cellToWorld, i->_filename, i->_filenameHash);
        }
        return editor;
    }

    PlacementsManager::PlacementsManager(const WorldPlacementsConfig& cfg)
    {
            //  Using the given config file, let's construct the list of 
            //  placement cells
        auto pimpl = std::make_unique<Pimpl>();
        pimpl->_cells.reserve(cfg._cellCount[0] * cfg._cellCount[1]);
        for (unsigned y=0; y<cfg._cellCount[1]; ++y)
            for (unsigned x=0; x<cfg._cellCount[0]; ++x) {
                PlacementCell cell;
                _snprintf_s(cell._filename, dimof(cell._filename), "%s/placements/p%03i_%03i.plc", 
                    cfg._baseDir.c_str(), x, y);
                cell._filenameHash = Hash64(cell._filename);
                Float3 offset(cfg._cellSize * x, cfg._cellSize * y, 0.f);
                cell._cellToWorld = AsFloat4x4(offset);

                    // note -- we could shrink wrap this bounding box around th objects
                    //      inside. This might be necessary, actually, because some objects
                    //      may be straddling the edges of the area, so the cell bounding box
                    //      should be slightly larger.
                const float minHeight = 0.f, maxHeight = 3000.f;
                cell._aabbMin = offset + Float3(0.f, 0.f, minHeight);
                cell._aabbMax = offset + Float3(cfg._cellSize, cfg._cellSize, maxHeight);

                pimpl->_cells.push_back(cell);
            }
        pimpl->_renderer = std::make_shared<PlacementsRenderer>();
        _pimpl = std::move(pimpl);
    }

    PlacementsManager::~PlacementsManager() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class DynamicPlacements : public Placements
    {
    public:
        uint64 AddPlacement(
            const Float3x4& objectToWorld, 
            const std::pair<Float3, Float3>& worldSpaceBoundary,
            const ResChar modelFilename[], const ResChar materialFilename[]);

        std::vector<ObjectReference>& GetObjects() { return _objects; }

        DynamicPlacements(const Placements& copyFrom);
        DynamicPlacements();
    };

    uint64 BuildGuid()
    {
        std::random_device rd;
        static std::mt19937_64 generator(rd());
        return generator();
    }

    uint64 DynamicPlacements::AddPlacement(
        const Float3x4& objectToWorld,
        const std::pair<Float3, Float3>& worldSpaceBoundary,
        const ResChar modelFilename[], const ResChar materialFilename[])
    {
        unsigned modelOffset = ~unsigned(0x0), materialOffset = ~unsigned(0x0);
        auto modelHash = Hash64(modelFilename);
        auto materialHash = Hash64(materialFilename);

        for (auto i=_filenamesBuffer.begin(); i!=_filenamesBuffer.end() && (modelOffset == ~unsigned(0x0) || materialOffset == ~unsigned(0x0)); ++i) {
            auto h = *(uint64*)AsPointer(i);
            if (h == modelHash)     { modelOffset = unsigned(std::distance(_filenamesBuffer.begin(), i)); }
            if (h == materialHash)  { materialOffset = unsigned(std::distance(_filenamesBuffer.begin(), i)); }

            i += sizeof(uint64);
            i = std::find(i, _filenamesBuffer.end(), '\0');
        }

        if (modelOffset == ~unsigned(0x0)) {
            modelOffset = _filenamesBuffer.size();
            auto length = XlStringLen(modelFilename);
            _filenamesBuffer.resize(_filenamesBuffer.size() + sizeof(uint64) + length + sizeof(ResChar));
            auto* dest = &_filenamesBuffer[modelOffset];
            *(uint64*)dest = modelHash;
            XlCopyString((ResChar*)PtrAdd(dest, sizeof(uint64)), length+1, modelFilename);
        }

        if (materialOffset == ~unsigned(0x0)) {
            materialOffset = _filenamesBuffer.size();
            auto length = XlStringLen(materialFilename);
            _filenamesBuffer.resize(_filenamesBuffer.size() + sizeof(uint64) + length + sizeof(ResChar));
            auto* dest = &_filenamesBuffer[materialOffset];
            *(uint64*)dest = materialHash;
            XlCopyString((ResChar*)PtrAdd(dest, sizeof(uint64)), length+1, materialFilename);
        }

        ObjectReference newReference;
        newReference._localToCell = objectToWorld;
        newReference._worldSpaceBoundary = worldSpaceBoundary;
        newReference._modelFilenameOffset = modelOffset;
        newReference._materialFilenameOffset = materialOffset;
        newReference._guid = BuildGuid();

            // Insert the new object in sorted order
            //  We're sorting by GUID, which is an arbitrary random number. So the final
            //  order should end up very arbitrary. We could alternatively also sort by model name
            //  (or just encode the model name into to guid somehow)
        auto i = std::lower_bound(_objects.begin(), _objects.end(), newReference, 
            [](const ObjectReference& lhs, const ObjectReference& rhs) { return lhs._guid < rhs._guid; });
        assert(i == _objects.end() || i->_guid != newReference._guid);  // hitting this means a GUID collision. Should be extremely unlikely
        _objects.insert(i, newReference);

        return newReference._guid;
    }

    DynamicPlacements::DynamicPlacements(const Placements& copyFrom)
        : Placements(copyFrom)
    {}

    DynamicPlacements::DynamicPlacements() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class PlacementsEditor::Pimpl
    {
    public:
        class RegisteredCell
        {
        public:
            Float2 _mins;
            Float2 _maxs;
            Float4x4 _cellToWorld;
            std::string _name;
        };
        std::vector<std::pair<uint64, RegisteredCell>> _cells;
        std::vector<std::pair<uint64, std::shared_ptr<DynamicPlacements>>> _dynPlacements;

        std::shared_ptr<PlacementsRenderer> _renderer;
        std::shared_ptr<DynamicPlacements> GetDynPlacements(uint64 cellGuid);
        std::string GetCellName(uint64 cellGuid);
    };

    std::string PlacementsEditor::Pimpl::GetCellName(uint64 cellGuid)
    {
        auto p = LowerBound(_cells, cellGuid);
        if (p != _cells.end() && p->first == cellGuid) {
            return p->second._name;
        }
        return std::string();
    }

    std::shared_ptr<DynamicPlacements> PlacementsEditor::Pimpl::GetDynPlacements(uint64 cellGuid)
    {
        auto p = LowerBound(_dynPlacements, cellGuid);
        if (p == _dynPlacements.end() || p->first != cellGuid) {

            std::shared_ptr<DynamicPlacements> placements;
                //  We can get an invalid resource here. It probably means the file
                //  doesn't exist -- which can happen with an uninitialized data
                //  directory.
            auto cellName = GetCellName(cellGuid);
            assert(!cellName.empty());
            TRY {
                auto& sourcePlacements = Assets::GetAsset<Placements>(cellName.c_str());
                placements = std::make_shared<DynamicPlacements>(sourcePlacements);
            } CATCH (const Assets::Exceptions::PendingResource&) {
                throw;
            } CATCH (const std::exception& e) {
                LogWarning << "Got invalid resource while loading placements file (" << cellName << "). If this file exists, but is corrupted, the next save will overwrite it. Error: (" << e.what() << ").";
            } CATCH_END

            if (!placements) {
                placements = std::make_shared<DynamicPlacements>();
            }
            _renderer->SetOverride(cellGuid, placements.get());
            p = _dynPlacements.insert(p, std::make_pair(cellGuid, std::move(placements)));
        }

        return p->second;
    }

    PlacementGUID  PlacementsEditor::AddPlacement(
        const PlacementsTransform& objectToWorld, 
        const char modelFilename[], const char materialFilename[])
    {
        //  Add a new placement with the given transformation
        //  * first, we need to look for the cell that is registered at this location
        //  * if there is a dynamic placements object already created for that cell,
        //      then we can just add it to the dynamic placements object.
        //  * otherwise, we need to create a new dynamic placements object (which will
        //      be initialized with the static placements)
        //
        //  Note that we're going to need to know the bounding box for this model,
        //  whatever happens. So, if the first thing we can do is load the scaffold
        //  to get at the bounding box and use the center point of that box to search
        //  for the right cell.
        //
        //  Objects that straddle a cell boundary must be placed in only one of those
        //  cells -- so sometimes objects will stick out the side of a cell.

        auto& model = _pimpl->_renderer->GetCachedModel(modelFilename);
        auto boundingBoxCentre = LinearInterpolate(model.GetBoundingBox().first, model.GetBoundingBox().second, 0.5f);
        auto worldSpaceCenter = TransformPoint(objectToWorld, boundingBoxCentre);

        std::string defMatName = ModelFormat().DefaultMaterialName(model);
        if (!materialFilename || !materialFilename[0]) {
            materialFilename = defMatName.c_str();
        }

        for (auto i=_pimpl->_cells.cbegin(); i!=_pimpl->_cells.cend(); ++i) {
            auto& reg = i->second;
            if (    worldSpaceCenter[0] >= reg._mins[0] && worldSpaceCenter[0] < reg._maxs[0]
                &&  worldSpaceCenter[1] >= reg._mins[1] && worldSpaceCenter[1] < reg._maxs[1]) {
                
                    // This is the correct cell. Look for a dynamic placement associated
                auto dynPlacements = _pimpl->GetDynPlacements(i->first);

                auto objectToCell = AsFloat3x4(Combine(AsFloat4x4(objectToWorld), InvertOrthonormalTransform(reg._cellToWorld)));
                auto id = dynPlacements->AddPlacement(
                    objectToCell, TransformBoundingBox(objectToCell, model.GetBoundingBox()),
                    modelFilename, materialFilename);

                return PlacementGUID(i->first, id);

            }
        }

        return PlacementGUID(0, 0);   // could 0 be a valid hash value? Maybe, but very unlikely
    }

    std::vector<PlacementGUID> PlacementsEditor::FindPlacements(
        const Float3& worldSpaceMins, const Float3& worldSpaceMaxs,
        const std::function<bool(const ObjectDef&)>& predicate)
    {
            //  Look through all placements to find any that intersect with the given
            //  world space bounding box. 
            //
            //  Note that there's a potential issue here -- the world space bounding
            //  box of the cell isn't updated when the dynamic placements change. So
            //  it's possible that some dynamic placements might intersect with our
            //  test bounding box, but not the cell bounding box... We have to be 
            //  careful about this. It might mean that we have to test more cells than
            //  expected.

        std::vector<PlacementGUID> result;

        const float placementAssumedMaxRadius = 100.f;
        for (auto i=_pimpl->_cells.cbegin(); i!=_pimpl->_cells.cend(); ++i) {
            if (    worldSpaceMaxs[0] < (i->second._mins[0] - placementAssumedMaxRadius)
                ||  worldSpaceMaxs[1] < (i->second._mins[1] - placementAssumedMaxRadius)
                ||  worldSpaceMins[0] > (i->second._maxs[0] + placementAssumedMaxRadius)
                ||  worldSpaceMins[1] > (i->second._maxs[1] + placementAssumedMaxRadius)) {
                continue;
            }

                //  This cell intersects with the bounding box (or almost does).
                //  We have to test all internal objects. First, transform the bounding
                //  box into local cell space.
            auto cellSpaceBB = TransformBoundingBox(
                AsFloat3x4(InvertOrthonormalTransform(i->second._cellToWorld)),
                std::make_pair(worldSpaceMins, worldSpaceMaxs));

                //  We need to use the renderer to get either the asset or the 
                //  override placements associated with this cell. It's a little awkward
                //  Note that we could use the quad tree to acceleration these tests.
            TRY {
                auto& p = _pimpl->_renderer->GetCachedPlacements(i->second._name.c_str());
                for (unsigned c=0; c<p.GetObjectReferenceCount(); ++c) {
                    auto& obj = p.GetObjectReferences()[c];
                    if (   cellSpaceBB.second[0] < obj._worldSpaceBoundary.first[0]
                        || cellSpaceBB.second[1] < obj._worldSpaceBoundary.first[1]
                        || cellSpaceBB.second[2] < obj._worldSpaceBoundary.first[2]
                        || cellSpaceBB.first[0]  > obj._worldSpaceBoundary.second[0]
                        || cellSpaceBB.first[1]  > obj._worldSpaceBoundary.second[1]
                        || cellSpaceBB.first[2]  > obj._worldSpaceBoundary.second[2]) {
                        continue;
                    }

                    if (predicate) {
                        ObjectDef def;
                        def._localToWorld = Combine(obj._localToCell, i->second._cellToWorld);

                        auto& model = _pimpl->_renderer->GetCachedModel(
                            (const char*)PtrAdd(p.GetFilenamesBuffer(), obj._modelFilenameOffset + sizeof(uint64)));

                            // note -- we have access to the cell space bounding box. But the local
                            //          space box would be better.
                        def._localSpaceBoundingBox = model.GetBoundingBox();
                        def._model = *(uint64*)PtrAdd(p.GetFilenamesBuffer(), obj._modelFilenameOffset);
                        def._material = *(uint64*)PtrAdd(p.GetFilenamesBuffer(), obj._materialFilenameOffset);

                            // allow the predicate to exclude this item
                        if (!predicate(def)) { continue; }
                    }

                    result.push_back(std::make_pair(i->first, obj._guid));
                }

            } CATCH (...) {
            } CATCH_END
        }

        return std::move(result);
    }

    class CompareObjectId
    {
    public:
        bool operator()(const Placements::ObjectReference& lhs, uint64 rhs) { return lhs._guid < rhs; }
        bool operator()(uint64 lhs, const Placements::ObjectReference& rhs) { return lhs < rhs._guid; }
        bool operator()(const Placements::ObjectReference& lhs, const Placements::ObjectReference& rhs) { return lhs._guid < rhs._guid; }
    };

    void PlacementsEditor::DeletePlacements(const std::vector<PlacementGUID>& originalPlacements)
    {
            //  We need to sort; because this method is mostly assuming we're working
            //  with a sorted list. Most of the time originalPlacements will be close
            //  to sorted order (which, of course, means that quick sort isn't ideal, but, anyway...)
        auto placements = originalPlacements;
        std::sort(placements.begin(), placements.end(),
            [](const PlacementGUID& lhs, const PlacementGUID& rhs) -> bool {
                if (lhs.first == rhs.first) { return lhs.second < rhs.second; }
                return lhs.first < rhs.first;
            });

        for (auto i=placements.begin(); i!=placements.end();) {
            auto iend = std::find_if(i, placements.end(), [&](const PlacementGUID& guid) { return guid.first != i->first; });
            auto dynPlacements = _pimpl->GetDynPlacements(i->first);
            auto& placeObjects = dynPlacements->GetObjects();
            auto pIterator = placeObjects.begin();
            for (;i != iend; ++i) {
                    //  here, we're assuming everything is sorted, so we can just march forward
                    //  through the destination placements list
                pIterator = std::lower_bound(
                    placeObjects.begin(), placeObjects.end(), i->second, CompareObjectId());
                if (pIterator != placeObjects.end() && pIterator->_guid == i->second) {
                        // note -- not removing unreferenced filenames
                    pIterator = placeObjects.erase(pIterator);
                }
            }
        }
    }

    void PlacementsEditor::RegisterCell(
        const Float2& mins, const Float2& maxs, 
        const Float4x4& cellToWorld,
        const char name[], uint64 guid)
    {
        auto i = LowerBound(_pimpl->_cells, guid);
        assert(i == _pimpl->_cells.end() || i->first != guid);

        Pimpl::RegisteredCell cell;
        cell._mins = mins;
        cell._maxs = maxs;
        cell._name = name;
        cell._cellToWorld = cellToWorld;
        _pimpl->_cells.insert(i, std::make_pair(guid, cell));
    }

    PlacementsEditor::PlacementsEditor(std::shared_ptr<PlacementsRenderer> renderer)
    {
        auto pimpl = std::make_unique<Pimpl>();
        pimpl->_renderer = std::move(renderer);
        _pimpl = std::move(pimpl);
    }

    PlacementsEditor::~PlacementsEditor()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    WorldPlacementsConfig::WorldPlacementsConfig(const std::string& baseDir)
        : _baseDir(baseDir)
    {
        _cellCount = UInt2(0,0);
        _cellSize = 512.f;

        size_t fileSize = 0;
        auto sourceFile = LoadFileAsMemoryBlock(StringMeld<MaxPath>() << baseDir << "\\world.cfg", &fileSize);

        Data data;
        data.Load((const char*)sourceFile.get(), int(fileSize));
        auto* c = data.ChildWithValue("TerrainConfig");
        if (c) {
            _cellCount = Deserialize(c, "CellCount", _cellCount);
            _cellSize = Deserialize(c, "CellSize", _cellSize);
        }
    }

}

