// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PlacementsManager.h"
#include "../RenderCore/Assets/ModelSimple.h"
#include "../RenderCore/Assets/SharedStateSet.h"
#include "../RenderCore/Assets/IModelFormat.h"
#include "../SceneEngine/LightingParserContext.h"
#include "../Assets/Assets.h"
#include "../Assets/ChunkFile.h"

#include "../RenderCore/Assets/ModelRunTime.h"

#include "../ConsoleRig/Log.h"
#include "../ConsoleRig/Console.h"
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

namespace RenderCore { 
    extern char VersionString[];
    extern char BuildDateString[];
}

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
            BoundingBox _cellSpaceBoundary;
            unsigned    _modelFilenameOffset;       // note -- hash values should be stored with the filenames
            unsigned    _materialFilenameOffset;
            uint64      _guid;
        };
        
        const ObjectReference*  GetObjectReferences() const;
        unsigned                GetObjectReferenceCount() const;
        const void*             GetFilenamesBuffer() const;

        const ::Assets::DependencyValidation& GetDependancyValidation() const { return *_dependencyValidation; }

        void Save(const ResChar filename[]) const;

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

    void Placements::Save(const ResChar filename[]) const
    {
        using namespace Serialization::ChunkFile;
        SimpleChunkFileWriter fileWriter(1, filename, "wb", 0, 
            RenderCore::VersionString, RenderCore::BuildDateString);
        fileWriter.BeginChunk(ChunkType_Placements, 0, "Placements");

        PlacementsHeader hdr;
        hdr._version = 0;
        hdr._objectRefCount = _objects.size();
        hdr._filenamesBufferSize = _filenamesBuffer.size();
        hdr._dummy = 0;
        fileWriter.Write(&hdr, sizeof(hdr), 1);
        fileWriter.Write(AsPointer(_objects.begin()), sizeof(ObjectReference), hdr._objectRefCount);
        fileWriter.Write(AsPointer(_filenamesBuffer.begin()), 1, hdr._filenamesBufferSize);
    }

    Placements::Placements(const ResChar filename[])
    {
            //
            //      Extremely simple file format for placements
            //      We just need 2 blocks:
            //          * list of object references
            //          * list of filenames / strings
            //      The strings are kept separate from the object placements
            //      because many of the string will be referenced multiple
            //      times. It just helps reduce file size.
            //

        using namespace Serialization::ChunkFile;
        std::vector<ObjectReference> objects;
        std::vector<uint8> filenamesBuffer;

        TRY {
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

            objects.resize(hdr._objectRefCount);
            filenamesBuffer.resize(hdr._filenamesBufferSize);
            file.Read(AsPointer(objects.begin()), sizeof(ObjectReference), hdr._objectRefCount);
            file.Read(AsPointer(filenamesBuffer.begin()), 1, hdr._filenamesBufferSize);
        } CATCH (const Utility::Exceptions::IOException&) { // catch file errors
        } CATCH_END

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
            const PlacementCell& cell,
            const uint64* filterStart = nullptr, const uint64* filterEnd = nullptr);

        typedef RenderCore::Assets::Simple::ModelScaffold ModelScaffold;
        typedef RenderCore::Assets::Simple::MaterialScaffold MaterialScaffold;
        typedef RenderCore::Assets::Simple::ModelRenderer ModelRenderer;
        typedef ModelRenderer::SortedModelDrawCalls PreparedState;
        
        auto GetCachedModel(const ResChar filename[]) -> const ModelScaffold&;
        auto GetCachedPlacements(uint64 hash, const ResChar filename[]) -> const Placements&;
        void SetOverride(uint64 guid, const Placements* placements);
        auto GetModelFormat() -> std::shared_ptr<RenderCore::Assets::IModelFormat>& { return _modelFormat; }

        PlacementsRenderer(std::shared_ptr<RenderCore::Assets::IModelFormat> modelFormat);
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

        std::shared_ptr<RenderCore::Assets::IModelFormat> _modelFormat;

        void Render(
            RenderCore::Metal::DeviceContext* context,
            LightingParserContext& parserContext, 
            unsigned techniqueIndex,
            const CellRenderInfo& renderInfo,
            const Float4x4& cellToWorld,
            const uint64* filterStart, const uint64* filterEnd);
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
            model = _modelFormat->CreateModel(filename);
            _cache->_modelScaffolds.Insert(hash, model);
        }
        return *model;
    }

    auto PlacementsRenderer::GetCachedPlacements(uint64 filenameHash, const ResChar filename[]) -> const Placements&
    {
        auto i = LowerBound(_cellOverrides, filenameHash);
        if (i != _cellOverrides.end() && i->first == filenameHash) {
            return *i->second._placements;
        } else {
            auto i2 = LowerBound(_cells, filenameHash);
            if (i2 == _cells.end() || i2->first != filenameHash) {
                CellRenderInfo newRenderInfo;   // note; we really want GetAssetDepImmediate here, to prevent Pending resources
                newRenderInfo._placements = &::Assets::GetAssetDep<Placements>(filename);
                i2 = _cells.insert(i2, std::make_pair(filenameHash, std::move(newRenderInfo)));
            } else {
                    // check if we need to reload placements
                if (i2->second._placements->GetDependancyValidation().GetValidationIndex() != 0) {
                    i2->second._placements = &::Assets::GetAssetDep<Placements>(filename);
                }
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
            if (placements) {
                _cellOverrides.insert(i, std::make_pair(guid, newRenderInfo));
            }
        } else {
            if (placements) {
                i->second = newRenderInfo; // override the previous one
            } else {
                _cellOverrides.erase(i);
            }
        }
    }

    void PlacementsRenderer::Render(
        RenderCore::Metal::DeviceContext* context,
        LightingParserContext& parserContext, 
        unsigned techniqueIndex,
        const PlacementCell& cell,
        const uint64* filterStart, const uint64* filterEnd)
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
                Render(context, parserContext, techniqueIndex, i->second, cell._cellToWorld, filterStart, filterEnd);
            } else {
                auto i2 = LowerBound(_cells, cell._filenameHash);
                if (i2 == _cells.end() || i2->first != cell._filenameHash) {
                    CellRenderInfo newRenderInfo;
                    newRenderInfo._placements = &::Assets::GetAssetDep<Placements>(cell._filename);
                    i2 = _cells.insert(i2, std::make_pair(cell._filenameHash, std::move(newRenderInfo)));
                } else {
                        // check if we need to reload placements
                    if (i2->second._placements->GetDependancyValidation().GetValidationIndex() != 0) {
                        i2->second._placements = &::Assets::GetAssetDep<Placements>(cell._filename);
                    }
                }

                // if (!i2->second._quadTree) {
                //     i2->second._quadTree = std::make_unique<PlacementsQuadTree>(
                //         *i2->second._placements);
                // }

                Render(context, parserContext, techniqueIndex, i2->second, cell._cellToWorld, filterStart, filterEnd);
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
        const Float4x4& cellToWorld,
        const uint64* filterStart, const uint64* filterEnd)
    {
        assert(renderInfo._placements);
        if (!renderInfo._placements->GetObjectReferenceCount()) {
            return;
        }

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

        const uint64* filterIterator = filterStart;
        const bool doFilter = filterStart != filterEnd;

        auto& placements = *renderInfo._placements;
        const auto* filenamesBuffer = placements.GetFilenamesBuffer();
        const auto* objRef = placements.GetObjectReferences();
        auto placementCount = placements.GetObjectReferenceCount();
        for (unsigned c=0; c<placementCount; ++c) {

            auto& obj = objRef[c];
            if (CullAABB(worldToCullSpace, obj._cellSpaceBoundary.first, obj._cellSpaceBoundary.second)) {
                continue;
            }

            if (doFilter) {
                while (filterIterator != filterEnd && *filterIterator < obj._guid) { ++filterIterator; }
                if (filterIterator == filterEnd || *filterIterator != obj._guid) { continue; }
            }

                // Basic draw distance calculation
                // many objects don't need to render out to the far clip

            const float maxDistanceSq = 1000.f * 1000.f;
            float distanceSq = MagnitudeSquared(
                .5f * (obj._cellSpaceBoundary.first + obj._cellSpaceBoundary.second) - cameraPosition);
            if (distanceSq > maxDistanceSq) {
                continue;
            }

            auto modelHash = *(uint64*)PtrAdd(filenamesBuffer, obj._modelFilenameOffset);
            auto modelFilename = (const ResChar*)PtrAdd(filenamesBuffer, obj._modelFilenameOffset + sizeof(uint64));
            auto materialHash = *(uint64*)PtrAdd(filenamesBuffer, obj._materialFilenameOffset);
            auto materialFilename = (const ResChar*)PtrAdd(filenamesBuffer, obj._materialFilenameOffset + sizeof(uint64));

            auto model = _cache->_modelScaffolds.Get(modelHash);
            if (!model) {
                model = _modelFormat->CreateModel(modelFilename);
                _cache->_modelScaffolds.Insert(modelHash, model);
            }

            auto material = _cache->_materialScaffolds.Get(materialHash);
            if (!material) {
                TRY {
                    material = _modelFormat->CreateMaterial(materialFilename);
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
                renderer = _modelFormat->CreateRenderer(
                    std::ref(*model), std::ref(*material), std::ref(_cache->_sharedStates), LOD);
                _cache->_modelRenderers.Insert(hashedModel, renderer);
            }

            auto localToWorld = Combine(AsFloat4x4(obj._localToCell), cellToWorld);
            renderer->Prepare(_cache->_preparedRenders, _cache->_sharedStates, localToWorld);

        }

        ModelRenderer::RenderPrepared(
            _cache->_preparedRenders, context, parserContext, techniqueIndex, _cache->_sharedStates);
    }

    PlacementsRenderer::PlacementsRenderer(std::shared_ptr<RenderCore::Assets::IModelFormat> modelFormat)
    {
        assert(modelFormat);
        auto cache = std::make_unique<Cache>();
        _cache = std::move(cache);
        _modelFormat = std::move(modelFormat);
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
                *i,
                Truncate(i->_aabbMin), Truncate(i->_aabbMax));
        }
        return editor;
    }

    PlacementsManager::PlacementsManager(
        const WorldPlacementsConfig& cfg,
        std::shared_ptr<RenderCore::Assets::IModelFormat> modelFormat,
        const Float2& worldOffset)
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
                Float3 offset(cfg._cellSize * x + worldOffset[0], cfg._cellSize * y + worldOffset[1], 0.f);
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

        pimpl->_renderer = std::make_shared<PlacementsRenderer>(std::move(modelFormat));
        _pimpl = std::move(pimpl);
    }

    PlacementsManager::~PlacementsManager() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class DynamicPlacements : public Placements
    {
    public:
        uint64 AddPlacement(
            const Float3x4& objectToCell, 
            const std::pair<Float3, Float3>& cellSpaceBoundary,
            const ResChar modelFilename[], const ResChar materialFilename[],
            uint64 objectGuid);

        std::vector<ObjectReference>& GetObjects() { return _objects; }

        unsigned AddString(const ResChar str[]);

        DynamicPlacements(const Placements& copyFrom);
        DynamicPlacements();
    };

    uint64 BuildGuid()
    {
        static std::mt19937_64 generator(std::random_device().operator()());
        return generator();
    }

    unsigned DynamicPlacements::AddString(const ResChar str[])
    {
        unsigned result = ~unsigned(0x0);
        auto stringHash = Hash64(str);

        for (auto i=_filenamesBuffer.begin(); i!=_filenamesBuffer.end() && result == ~unsigned(0x0); ++i) {
            auto h = *(uint64*)AsPointer(i);
            if (h == stringHash) { result = unsigned(std::distance(_filenamesBuffer.begin(), i)); }

            i += sizeof(uint64);
            i = std::find(i, _filenamesBuffer.end(), '\0');
        }

        if (result == ~unsigned(0x0)) {
            result = _filenamesBuffer.size();
            auto length = XlStringLen(str);
            _filenamesBuffer.resize(_filenamesBuffer.size() + sizeof(uint64) + length + sizeof(ResChar));
            auto* dest = &_filenamesBuffer[result];
            *(uint64*)dest = stringHash;
            XlCopyString((ResChar*)PtrAdd(dest, sizeof(uint64)), length+1, str);
        }

        return result;
    }

    uint64 DynamicPlacements::AddPlacement(
        const Float3x4& objectToCell,
        const std::pair<Float3, Float3>& cellSpaceBoundary,
        const ResChar modelFilename[], const ResChar materialFilename[],
        uint64 objectGuid)
    {
        ObjectReference newReference;
        newReference._localToCell = objectToCell;
        newReference._cellSpaceBoundary = cellSpaceBoundary;
        newReference._modelFilenameOffset = AddString(modelFilename);
        newReference._materialFilenameOffset = AddString(materialFilename);
        newReference._guid = objectGuid;

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
        class RegisteredCell : public PlacementCell
        {
        public:
            Float2 _mins;
            Float2 _maxs;

            struct CompareHash
            {
            public:
                bool operator()(const RegisteredCell& lhs, const RegisteredCell& rhs) const { return lhs._filenameHash < rhs._filenameHash; }
                bool operator()(const RegisteredCell& lhs, uint64 rhs) const { return lhs._filenameHash < rhs; }
                bool operator()(uint64 lhs, const RegisteredCell& rhs) const { return lhs < rhs._filenameHash; }
            };
        };
        std::vector<RegisteredCell> _cells;
        std::vector<std::pair<uint64, std::shared_ptr<DynamicPlacements>>> _dynPlacements;

        std::shared_ptr<PlacementsRenderer> _renderer;
        std::shared_ptr<DynamicPlacements> GetDynPlacements(uint64 cellGuid);
        Float4x4 GetCellToWorld(uint64 cellGuid);
        const char* GetCellName(uint64 cellGuid);
    };

    const char* PlacementsEditor::Pimpl::GetCellName(uint64 cellGuid)
    {
        auto p = std::lower_bound(_cells.cbegin(), _cells.cend(), cellGuid, RegisteredCell::CompareHash());
        if (p != _cells.end() && p->_filenameHash == cellGuid) {
            return p->_filename;
        }
        return nullptr;
    }

    Float4x4 PlacementsEditor::Pimpl::GetCellToWorld(uint64 cellGuid)
    {
        auto p = std::lower_bound(_cells.cbegin(), _cells.cend(), cellGuid, RegisteredCell::CompareHash());
        if (p != _cells.end() && p->_filenameHash == cellGuid) {
            return p->_cellToWorld;
        }
        return Identity<Float4x4>();
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
            assert(cellName && cellName[0]);

            TRY {
                auto& sourcePlacements = Assets::GetAsset<Placements>(cellName);
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

    std::vector<PlacementGUID> PlacementsEditor::Find_RayIntersection(
        const Float3& rayStart, const Float3& rayEnd,
        const std::function<bool(const ObjIntersectionDef&)>& predicate)
    {
        std::vector<PlacementGUID> result;
        const float placementAssumedMaxRadius = 100.f;
        for (auto i=_pimpl->_cells.cbegin(); i!=_pimpl->_cells.cend(); ++i) {
            Float3 cellMin = i->_aabbMin - Float3(placementAssumedMaxRadius, placementAssumedMaxRadius, placementAssumedMaxRadius);
            Float3 cellMax = i->_aabbMax + Float3(placementAssumedMaxRadius, placementAssumedMaxRadius, placementAssumedMaxRadius);
            if (!RayVsAABB(std::make_pair(rayStart, rayEnd), cellMin, cellMax)) {
                continue;
            }

            auto worldToCell = InvertOrthonormalTransform(i->_cellToWorld);
            auto cellSpaceRay = std::make_pair(
                TransformPoint(worldToCell, rayStart),
                TransformPoint(worldToCell, rayEnd));

            TRY {
                auto& p = _pimpl->_renderer->GetCachedPlacements(i->_filenameHash, i->_filename);
                for (unsigned c=0; c<p.GetObjectReferenceCount(); ++c) {
                    auto& obj = p.GetObjectReferences()[c];
                        //  We're only doing a very rough world space bounding box vs ray test here...
                        //  Ideally, we should follow up with a more accurate test using the object loca
                        //  space bounding box
                    if (!RayVsAABB(cellSpaceRay, obj._cellSpaceBoundary.first, obj._cellSpaceBoundary.second)) {
                        continue;
                    }

                    auto& model = _pimpl->_renderer->GetCachedModel(
                        (const char*)PtrAdd(p.GetFilenamesBuffer(), obj._modelFilenameOffset + sizeof(uint64)));
                    const auto& localBoundingBox = model.GetBoundingBox();
                    if (!RayVsAABB( cellSpaceRay, AsFloat4x4(obj._localToCell), 
                                    localBoundingBox.first, localBoundingBox.second)) {
                        continue;
                    }

                    if (predicate) {
                        ObjIntersectionDef def;
                        def._localToWorld = Combine(obj._localToCell, i->_cellToWorld);

                            // note -- we have access to the cell space bounding box. But the local
                            //          space box would be better.
                        def._localSpaceBoundingBox = localBoundingBox;
                        def._model = *(uint64*)PtrAdd(p.GetFilenamesBuffer(), obj._modelFilenameOffset);
                        def._material = *(uint64*)PtrAdd(p.GetFilenamesBuffer(), obj._materialFilenameOffset);

                            // allow the predicate to exclude this item
                        if (!predicate(def)) { continue; }
                    }

                    result.push_back(std::make_pair(i->_filenameHash, obj._guid));
                }

            } CATCH (...) {
            } CATCH_END
        }

        return std::move(result);
    }

    std::vector<PlacementGUID> PlacementsEditor::Find_BoxIntersection(
        const Float3& worldSpaceMins, const Float3& worldSpaceMaxs,
        const std::function<bool(const ObjIntersectionDef&)>& predicate)
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
            if (    worldSpaceMaxs[0] < (i->_aabbMin[0] - placementAssumedMaxRadius)
                ||  worldSpaceMaxs[1] < (i->_aabbMin[1] - placementAssumedMaxRadius)
                ||  worldSpaceMins[0] > (i->_aabbMax[0] + placementAssumedMaxRadius)
                ||  worldSpaceMins[1] > (i->_aabbMax[1] + placementAssumedMaxRadius)) {
                continue;
            }

                //  This cell intersects with the bounding box (or almost does).
                //  We have to test all internal objects. First, transform the bounding
                //  box into local cell space.
            auto cellSpaceBB = TransformBoundingBox(
                AsFloat3x4(InvertOrthonormalTransform(i->_cellToWorld)),
                std::make_pair(worldSpaceMins, worldSpaceMaxs));

                //  We need to use the renderer to get either the asset or the 
                //  override placements associated with this cell. It's a little awkward
                //  Note that we could use the quad tree to acceleration these tests.
            TRY {
                auto& p = _pimpl->_renderer->GetCachedPlacements(i->_filenameHash, i->_filename);
                for (unsigned c=0; c<p.GetObjectReferenceCount(); ++c) {
                    auto& obj = p.GetObjectReferences()[c];
                    if (   cellSpaceBB.second[0] < obj._cellSpaceBoundary.first[0]
                        || cellSpaceBB.second[1] < obj._cellSpaceBoundary.first[1]
                        || cellSpaceBB.second[2] < obj._cellSpaceBoundary.first[2]
                        || cellSpaceBB.first[0]  > obj._cellSpaceBoundary.second[0]
                        || cellSpaceBB.first[1]  > obj._cellSpaceBoundary.second[1]
                        || cellSpaceBB.first[2]  > obj._cellSpaceBoundary.second[2]) {
                        continue;
                    }

                    if (predicate) {
                        ObjIntersectionDef def;
                        def._localToWorld = Combine(obj._localToCell, i->_cellToWorld);

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

                    result.push_back(std::make_pair(i->_filenameHash, obj._guid));
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

    class Transaction : public PlacementsEditor::ITransaction
    {
    public:
        typedef PlacementsEditor::ObjTransDef ObjTransDef;
        typedef PlacementsEditor::PlacementsTransform PlacementsTransform;

        const ObjTransDef&  GetObject(unsigned index) const;
        const ObjTransDef&  GetObjectOriginalState(unsigned index) const;
        PlacementGUID       GetGuid(unsigned index) const;
        PlacementGUID       GetOriginalGuid(unsigned index) const;
        unsigned            GetObjectCount() const;
        std::pair<Float3, Float3>   GetLocalBoundingBox(unsigned index) const;

        virtual void        SetObject(unsigned index, const ObjTransDef& newState);

        virtual bool        Create(const ObjTransDef& newState);
        virtual void        Delete(unsigned index);

        virtual void    Commit();
        virtual void    Cancel();
        virtual void    UndoAndRestart();

        Transaction(
            PlacementsEditor::Pimpl*    editorPimpl,
            const PlacementGUID*        placementsBegin,
            const PlacementGUID*        placementsEnd);
        ~Transaction();

    protected:
        PlacementsEditor::Pimpl*    _editorPimpl;

        std::vector<ObjTransDef>    _originalState;
        std::vector<ObjTransDef>    _objects;
        std::vector<PlacementGUID>  _guids;

        void PushObj(unsigned index, const ObjTransDef& newState);

        enum State { Active, Committed };
        State _state;
    };

    auto    Transaction::GetObject(unsigned index) const -> const ObjTransDef& { return _objects[index]; }
    auto    Transaction::GetObjectOriginalState(unsigned index) const -> const ObjTransDef& { return _originalState[index]; }
    PlacementGUID   Transaction::GetGuid(unsigned index) const { return _guids[index]; }
    PlacementGUID   Transaction::GetOriginalGuid(unsigned index) const { return _guids[index]; }

    unsigned    Transaction::GetObjectCount() const
    {
        assert(_guids.size() == _originalState.size());
        assert(_guids.size() == _objects.size());
        return _guids.size();
    }

    std::pair<Float3, Float3>   Transaction::GetLocalBoundingBox(unsigned index) const
    {
        auto& model = _editorPimpl->_renderer->GetCachedModel(_objects[index]._model.c_str());
        return model.GetBoundingBox();
    }

    void    Transaction::SetObject(unsigned index, const ObjTransDef& newState)
    {
        auto& currentState = _objects[index];
        auto currTrans = currentState._transaction;
        if (currTrans != ObjTransDef::Error && currTrans != ObjTransDef::Deleted) {
            currentState = newState;
            currentState._transaction = (currTrans == ObjTransDef::Created) ? ObjTransDef::Created : ObjTransDef::Modified;
            PushObj(index, currentState);
        }
    }

    static bool CompareGUID(const PlacementGUID& lhs, const PlacementGUID& rhs)
    {
        if (lhs.first == rhs.first) { return lhs.second < rhs.second; }
        return lhs.first < rhs.first;
    }

    bool    Transaction::Create(const ObjTransDef& newState)
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

        auto& model = _editorPimpl->_renderer->GetCachedModel(newState._model.c_str());
        auto boundingBoxCentre = LinearInterpolate(model.GetBoundingBox().first, model.GetBoundingBox().second, 0.5f);
        auto worldSpaceCenter = TransformPoint(newState._localToWorld, boundingBoxCentre);

        std::string materialFilename = newState._material;
        if (materialFilename.empty()) {
            materialFilename = _editorPimpl->_renderer->GetModelFormat()->DefaultMaterialName(model);
        }

        PlacementGUID guid(0, 0);
        PlacementsTransform localToCell = Identity<PlacementsTransform>();

        for (auto i=_editorPimpl->_cells.cbegin(); i!=_editorPimpl->_cells.cend(); ++i) {
            if (    worldSpaceCenter[0] >= i->_mins[0] && worldSpaceCenter[0] < i->_maxs[0]
                &&  worldSpaceCenter[1] >= i->_mins[1] && worldSpaceCenter[1] < i->_maxs[1]) {
                
                    // This is the correct cell. Look for a dynamic placement associated
                auto dynPlacements = _editorPimpl->GetDynPlacements(i->_filenameHash);

                localToCell = AsFloat3x4(Combine(newState._localToWorld, InvertOrthonormalTransform(i->_cellToWorld)));
                auto id = BuildGuid();
                dynPlacements->AddPlacement(
                    localToCell, TransformBoundingBox(localToCell, model.GetBoundingBox()),
                    newState._model.c_str(), materialFilename.c_str(), id);

                guid = PlacementGUID(i->_filenameHash, id);
                break;

            }
        }

        if (guid.first == 0 && guid.second == 0) return false;    // couldn't find a way to create this object
        
        ObjTransDef newObj = newState;
        newObj._transaction = ObjTransDef::Created;

        ObjTransDef originalState;
        originalState._localToWorld = Identity<Float4x4>();
        originalState._transaction = ObjTransDef::Error;

        auto insertLoc = std::lower_bound(_guids.begin(), _guids.end(), guid, CompareGUID);
        auto insertIndex = std::distance(_guids.begin(), insertLoc);

        _originalState.insert(_originalState.begin() + insertIndex, originalState);
        _objects.insert(_objects.begin() + insertIndex, newObj);
        _guids.insert(_guids.begin() + insertIndex, guid);

        return true;
    }

    void    Transaction::Delete(unsigned index)
    {
        _objects[index]._transaction = ObjTransDef::Deleted;
        PushObj(index, _objects[index]);
    }

    void Transaction::PushObj(unsigned index, const ObjTransDef& newState)
    {
            // update the DynPlacements object with the changes to the object at index "index"
        std::vector<ObjTransDef> originalState;
        
        auto guid = _guids[index];

        auto cellToWorld = _editorPimpl->GetCellToWorld(guid.first);
        auto& dynPlacements = *_editorPimpl->GetDynPlacements(guid.first);
        auto& objects = dynPlacements.GetObjects();

        auto dst = std::lower_bound(objects.begin(), objects.end(), guid.second, CompareObjectId());

        std::pair<Float3, Float3> cellSpaceBoundary;
        PlacementsTransform localToCell;
        std::string materialFilename = newState._material;
        if (newState._transaction != ObjTransDef::Deleted && newState._transaction != ObjTransDef::Error) {
            localToCell = AsFloat3x4(Combine(newState._localToWorld, InvertOrthonormalTransform(cellToWorld)));

            auto& model = _editorPimpl->_renderer->GetCachedModel(newState._model.c_str());
            cellSpaceBoundary = TransformBoundingBox(localToCell, model.GetBoundingBox());

            if (materialFilename.empty()) {
                materialFilename = _editorPimpl->_renderer->GetModelFormat()->DefaultMaterialName(model);
            }
        }

            // todo --  handle the case where an object should move to another cell!
            //          this should actually change the first part of the GUID

        if (dst != objects.end() && dst->_guid == guid.second) {
                // we found the referenced object already existing (we get "error" state when reverting a creation operation)
            if (newState._transaction == ObjTransDef::Deleted || newState._transaction == ObjTransDef::Error) {
                objects.erase(dst);
            } else {
                dst->_localToCell = localToCell;
                dst->_modelFilenameOffset = dynPlacements.AddString(newState._model.c_str());
                dst->_materialFilenameOffset = dynPlacements.AddString(materialFilename.c_str());
                dst->_cellSpaceBoundary = cellSpaceBoundary;
            }
        } else {
                // the referenced object wasn't there. We may have to create it
            if (newState._transaction == ObjTransDef::Created || newState._transaction == ObjTransDef::Unchanged) {
                dynPlacements.AddPlacement(
                    localToCell, cellSpaceBoundary, 
                    newState._model.c_str(), materialFilename.c_str(), 
                    guid.second);
            }
        }
    }

    void    Transaction::Commit()
    {
        _state = Committed;
    }

    void    Transaction::Cancel()
    {
        if (_state == Active) {
                // we need to revert all of the objects to their original state
            UndoAndRestart();
        }

        _state = Committed;
    }

    void    Transaction::UndoAndRestart()
    {
        if (_state != Active) return;

            // we just have to reset all objects to their previous state
        for (unsigned c=0; c<_objects.size(); ++c) {
            _objects[c] = _originalState[c];
            PushObj(c, _originalState[c]);
        }
    }
    
    Transaction::Transaction(
        PlacementsEditor::Pimpl*    editorPimpl,
        const PlacementGUID*        guidsBegin,
        const PlacementGUID*        guidsEnd)
    {
        //  We need to sort; because this method is mostly assuming we're working
            //  with a sorted list. Most of the time originalPlacements will be close
            //  to sorted order (which, of course, means that quick sort isn't ideal, but, anyway...)
        auto guids = std::vector<PlacementGUID>(guidsBegin, guidsEnd);
        std::sort(guids.begin(), guids.end(), CompareGUID);

        std::vector<ObjTransDef> originalState;
        auto cellIterator = editorPimpl->_cells.begin();
        for (auto i=guids.begin(); i!=guids.end();) {
            auto iend = std::find_if(i, guids.end(), 
                [&](const PlacementGUID& guid) { return guid.first != i->first; });

            cellIterator = std::lower_bound(cellIterator, editorPimpl->_cells.end(), i->first, PlacementsEditor::Pimpl::RegisteredCell::CompareHash());
            if (cellIterator == editorPimpl->_cells.end() || cellIterator->_filenameHash != i->first) {
                continue;
            }

            auto cellToWorld = cellIterator->_cellToWorld;

            auto& placements = editorPimpl->_renderer->GetCachedPlacements(cellIterator->_filenameHash, cellIterator->_filename);
            auto pIterator = placements.GetObjectReferences();
            auto pEnd = &placements.GetObjectReferences()[placements.GetObjectReferenceCount()];
            for (;i != iend; ++i) {
                    //  Here, we're assuming everything is sorted, so we can just march forward
                    //  through the destination placements list
                pIterator = std::lower_bound(pIterator, pEnd, i->second, CompareObjectId());
                if (pIterator != pEnd && pIterator->_guid == i->second) {
                        // Build a ObjTransDef object from this object, and record it
                    ObjTransDef def;
                    def._localToWorld = Combine(pIterator->_localToCell, cellToWorld);
                    def._model = (const char*)PtrAdd(placements.GetFilenamesBuffer(), sizeof(uint64) + pIterator->_modelFilenameOffset);
                    def._material = (const char*)PtrAdd(placements.GetFilenamesBuffer(), sizeof(uint64) + pIterator->_materialFilenameOffset);
                    def._transaction = ObjTransDef::Unchanged;
                    originalState.push_back(def);
                } else {
                        // we couldn't find an original for this object. It's invalid
                    ObjTransDef def;
                    def._localToWorld = Identity<Float4x4>();
                    def._transaction = ObjTransDef::Error;
                    originalState.push_back(def);
                }
            }
        }

        _objects = originalState;
        _originalState = std::move(originalState);
        _guids = std::move(guids);
        _editorPimpl = editorPimpl;
        _state = Active;
    }

    Transaction::~Transaction()
    {
        if (_state == Active) {
            Cancel();
        }
    }

    void PlacementsEditor::RegisterCell(
        const PlacementCell& cell,
        const Float2& mins, const Float2& maxs)
    {
        auto i = std::lower_bound(_pimpl->_cells.begin(), _pimpl->_cells.end(), cell._filenameHash, Pimpl::RegisteredCell::CompareHash());
        assert(i == _pimpl->_cells.end() || i->_filenameHash != cell._filenameHash);

        Pimpl::RegisteredCell newCell;
        *(PlacementCell*)&newCell = cell;
        newCell._mins = mins;
        newCell._maxs = maxs;
        _pimpl->_cells.insert(i, newCell);
    }

    void PlacementsEditor::RenderFiltered(
        RenderCore::Metal::DeviceContext* context,
        LightingParserContext& parserContext,
        unsigned techniqueIndex,
        const PlacementGUID* begin, const PlacementGUID* end)
    {
        _pimpl->_renderer->EnterState(context);

            //  We need to take a copy, so we don't overwrite
            //  and reorder the caller's version.
        std::vector<PlacementGUID> copy(begin, end);
        std::sort(copy.begin(), copy.end());

        auto ci = _pimpl->_cells.begin();
        for (auto i=copy.begin(); i!=copy.end();) {
            auto i2 = i+1;
            for (; i2!=copy.end() && i2->first == i->first; ++i2) {}

            while (ci->_filenameHash < i->first && ci != _pimpl->_cells.end()) { ++ci; }

            if (ci != _pimpl->_cells.end() && ci->_filenameHash == i->first) {

                    // re-write the object guids for the renderer's convenience
                uint64* tStart = &i->first;
                uint64* t = tStart;
                while (i < i2) { *t++ = i->second; i++; }

                _pimpl->_renderer->Render(
                    context, parserContext, techniqueIndex,
                    *ci, tStart, t);

            } else {
                i = i2;
            }
        }
    }

    static void SavePlacements(const char outputFilename[], Placements& placements)
    {
        placements.Save(outputFilename);
        ConsoleRig::Console::GetInstance().Print(StringMeld<256>() << "Writing placements to: " << outputFilename << "\n");
    }

    void PlacementsEditor::Save()
    {
            //  Save all of the placement files that have changed. 
            //
            //  Changed placement cells will have a "dynamic" placements object associated.
            //  These should get flushed to disk. Then we can delete the dynamic placements,
            //  because the changed static placements should get automatically reloaded from
            //  disk (making the dynamic placements cells now redundant)
            //
            //  We may need to commit or cancel any active transaction. How do we know
            //  if we need to commit or cancel them?

        for (auto i = _pimpl->_dynPlacements.begin(); i!=_pimpl->_dynPlacements.end(); ++i) {
            auto cellGuid = i->first;
            auto& placements = *i->second;

            auto* cellName = _pimpl->GetCellName(cellGuid);
            SavePlacements(cellName, placements);

                // clear the renderer links
            _pimpl->_renderer->SetOverride(cellGuid, nullptr);
        }

        _pimpl->_dynPlacements.clear();
    }

    std::shared_ptr<RenderCore::Assets::IModelFormat> PlacementsEditor::GetModelFormat()
    {
        return _pimpl->_renderer->GetModelFormat();
    }

    std::pair<Float3, Float3> PlacementsEditor::GetModelBoundingBox(const ResChar modelName[]) const
    {
        auto& model = _pimpl->_renderer->GetCachedModel(modelName);
        return model.GetBoundingBox();
    }

    auto PlacementsEditor::Transaction_Begin(
        const PlacementGUID* placementsBegin, 
        const PlacementGUID* placementsEnd) -> std::shared_ptr<ITransaction>
    {
        return std::make_shared<Transaction>(_pimpl.get(), placementsBegin, placementsEnd);
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
        auto* c = data.ChildWithValue("PlacementsConfig");
        if (c) {
            _cellCount = Deserialize(c, "CellCount", _cellCount);
            _cellSize = Deserialize(c, "CellSize", _cellSize);
        }
    }

}

