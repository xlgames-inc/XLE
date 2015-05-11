// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PlacementsManager.h"
#include "PlacementsQuadTree.h"
#include "../RenderCore/Assets/SharedStateSet.h"

#include "../RenderCore/Assets/ModelRunTime.h"
#include "../RenderCore/Assets/ModelRunTimeInternal.h"
#include "../Assets/CompileAndAsyncManager.h"
#include "../Assets/IntermediateResources.h"
#include "../RenderCore/Assets/ColladaCompilerInterface.h"
#include "../RenderCore/Assets/IModelFormat.h"
#include "../RenderCore/Assets/DelayedDrawCall.h"

#include "../RenderCore/Techniques/ParsingContext.h"

#include "../Assets/Assets.h"
#include "../Assets/ChunkFile.h"
#include "../Assets/AssetUtils.h"

#include "../RenderCore/Assets/ModelRunTime.h"
#include "../RenderCore/Assets/MaterialScaffold.h"
#include "../RenderCore/RenderUtils.h"

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
#include "../Utility/Streams/PathUtils.h"
#include "../Core/Types.h"

#include <random>

namespace RenderCore { 
    extern char VersionString[];
    extern char BuildDateString[];
}

namespace SceneEngine
{
    using Assets::ResChar;

    using RenderCore::Assets::ModelRenderer;
    using RenderCore::Assets::ModelScaffold;
    using RenderCore::Assets::MaterialScaffold;

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

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _dependencyValidation; }

        void Save(const ResChar filename[]) const;
        void LogDetails(const char title[]) const;

        Placements(const ResChar filename[]);
        Placements();
        ~Placements();
    protected:
        std::vector<ObjectReference>    _objects;
        std::vector<uint8>              _filenamesBuffer;

        std::shared_ptr<::Assets::DependencyValidation>   _dependencyValidation;
        void ReplaceString(const char oldString[], const char newString[]);
    };

    auto        Placements::GetObjectReferences() const -> const ObjectReference*   { return AsPointer(_objects.begin()); }
    unsigned    Placements::GetObjectReferenceCount() const                         { return unsigned(_objects.size()); }
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
        hdr._objectRefCount = (unsigned)_objects.size();
        hdr._filenamesBufferSize = unsigned(_filenamesBuffer.size());
        hdr._dummy = 0;
        fileWriter.Write(&hdr, sizeof(hdr), 1);
        fileWriter.Write(AsPointer(_objects.begin()), sizeof(ObjectReference), hdr._objectRefCount);
        fileWriter.Write(AsPointer(_filenamesBuffer.begin()), 1, hdr._filenamesBufferSize);
    }

    void Placements::LogDetails(const char title[]) const
    {
        // write some details about this placements file to the log
        LogInfo << "---<< Placements file: " << title << " >>---";
        LogInfo << "    (" << _objects.size() << ") object references -- " << sizeof(ObjectReference) * _objects.size() / 1024.f << "k in objects, " << _filenamesBuffer.size() / 1024.f << "k in string table";

        unsigned configCount = 0;
        auto i = _objects.cbegin();
        while (i != _objects.cend()) {
            auto starti = i;
            while (i != _objects.cend() && i->_materialFilenameOffset == starti->_materialFilenameOffset && i->_modelFilenameOffset == starti->_modelFilenameOffset) { ++i; }
            ++configCount;
        }
        LogInfo << "    (" << configCount << ") configurations";

        i = _objects.cbegin();
        while (i != _objects.cend()) {
            auto starti = i;
            while (i != _objects.cend() && i->_materialFilenameOffset == starti->_materialFilenameOffset && i->_modelFilenameOffset == starti->_modelFilenameOffset) { ++i; }

            auto modelName = (const char*)PtrAdd(AsPointer(_filenamesBuffer.begin()), starti->_modelFilenameOffset + sizeof(uint64));
            auto materialName = (const char*)PtrAdd(AsPointer(_filenamesBuffer.begin()), starti->_materialFilenameOffset + sizeof(uint64));
            LogInfo << "    [" << (i-starti) << "] objects (" << modelName << "), (" << materialName << ")";
        }
    }

    void Placements::ReplaceString(const char oldString[], const char newString[])
    {
        unsigned replacementStart = 0, preReplacementEnd = 0;
        unsigned postReplacementEnd = 0;

        uint64 oldHash = Hash64(oldString);
        uint64 newHash = Hash64(newString);

            //  first, look through and find the old string.
            //  then, 
        auto i = _filenamesBuffer.begin();
        for(;i !=_filenamesBuffer.end();) {
            auto starti = i;
            if (std::distance(i, _filenamesBuffer.end()) < sizeof(uint64)) {
                assert(0);
                break;  // not enough room for a full hash code. Seems like the string table is corrupted
            }
            i += sizeof(uint64);
            while (i != _filenamesBuffer.end() && *i) { ++i; }
            if (i != _filenamesBuffer.end()) { ++i; }   // one more increment to include the null character

            if (*(const uint64*)AsPointer(starti) == oldHash) {

                    // if this is our string, then we need to erase the old content and insert
                    // the new

                auto length = XlStringLen(newString);
                std::vector<uint8> replacementContent(sizeof(uint64) + length + 1, 0);
                *(uint64*)AsPointer(replacementContent.begin()) = newHash;

                XlCopyMemory(
                    AsPointer(replacementContent.begin() + sizeof(uint64)),
                    newString, length);

                replacementStart = (unsigned)std::distance(_filenamesBuffer.begin(), starti);
                preReplacementEnd = (unsigned)std::distance(_filenamesBuffer.begin(), i);
                postReplacementEnd = unsigned(replacementStart + replacementContent.size());
                i = _filenamesBuffer.erase(starti, i);
                auto dst = std::distance(_filenamesBuffer.begin(), i);
                _filenamesBuffer.insert(i, replacementContent.begin(), replacementContent.end());
                i = _filenamesBuffer.begin();
                std::advance(i, dst + replacementContent.size());

                    // Now we have to adjust all of the offsets in the ObjectReferences
                for (auto o=_objects.begin(); o!=_objects.end(); ++o) {
                    if (o->_modelFilenameOffset > replacementStart) {
                        o->_modelFilenameOffset -= preReplacementEnd - postReplacementEnd;
                        assert(o->_modelFilenameOffset > replacementStart);
                    }
                    if (o->_materialFilenameOffset > replacementStart) {
                        o->_materialFilenameOffset -= preReplacementEnd - postReplacementEnd;
                        assert(o->_materialFilenameOffset > replacementStart);
                    }
                }
                return;
            }
        }
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

        #if defined(_DEBUG)
            if (!_objects.empty()) {
                LogDetails(filename);
            }
        #endif
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
        Float3x4    _cellToWorld;
        Float3      _aabbMin, _aabbMax;
    };

    class PlacementsQuadTree;

    class PlacementsRenderer
    {
    public:
        void BeginRender(RenderCore::Metal::DeviceContext* context);
        void EndRender(
            RenderCore::Metal::DeviceContext* context, 
            RenderCore::Techniques::ParsingContext& parserContext, unsigned techniqueIndex);
        void CommitTranslucent(
            RenderCore::Metal::DeviceContext* context,
            RenderCore::Techniques::ParsingContext& parserContext,
            unsigned techniqueIndex, RenderCore::Assets::DelayStep delayStep);
        void FilterRenders(const std::function<bool(const RenderCore::Assets::DelayedDrawCall&)>& predicate);

        void Render(
            RenderCore::Metal::DeviceContext* context,
            RenderCore::Techniques::ParsingContext& parserContext,
            const PlacementCell& cell,
            const uint64* filterStart = nullptr, const uint64* filterEnd = nullptr);

        typedef RenderCore::Assets::DelayedDrawCallSet PreparedState;
        
        auto GetCachedModel(const ResChar filename[]) -> const ModelScaffold*;
        auto GetCachedMaterial(const ResChar model[], const ResChar material[]) -> const MaterialScaffold*;
        auto GetCachedRenderer(const ResChar modelFilename[], const ResChar materialFilename[], unsigned LOD) -> const ModelRenderer*;
        auto GetCachedPlacements(uint64 hash, const ResChar filename[]) -> const Placements&;
        void SetOverride(uint64 guid, const Placements* placements);
        auto GetModelFormat() -> std::shared_ptr<RenderCore::Assets::IModelFormat>& { return _modelFormat; }
        auto GetCachedQuadTree(uint64 cellFilenameHash) const -> const PlacementsQuadTree*;

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
            , _modelRenderers(500)
            , _preparedRenders(__uuidof(ModelRenderer)) {}
        };

        PlacementsRenderer(std::shared_ptr<RenderCore::Assets::IModelFormat> modelFormat);
        ~PlacementsRenderer();
    protected:
        class CellRenderInfo
        {
        public:
            const Placements* _placements;
            std::unique_ptr<PlacementsQuadTree> _quadTree;

            CellRenderInfo() {}
            CellRenderInfo(CellRenderInfo&& moveFrom) never_throws
            : _placements(moveFrom._placements)
            , _quadTree(std::move(moveFrom._quadTree))
            {
                moveFrom._placements = nullptr;
            }

            CellRenderInfo& operator=(CellRenderInfo&& moveFrom) never_throws
            {
                _placements = moveFrom._placements;
                moveFrom._placements = nullptr;
                _quadTree = std::move(moveFrom._quadTree);
                return *this;
            }

        private:
            CellRenderInfo(const CellRenderInfo&);
            CellRenderInfo& operator=(const CellRenderInfo&);
        };

        std::vector<std::pair<uint64, CellRenderInfo>> _cellOverrides;
        std::vector<std::pair<uint64, CellRenderInfo>> _cells;
        std::unique_ptr<Cache> _cache;

        std::shared_ptr<RenderCore::Assets::IModelFormat> _modelFormat;

        void Render(
            RenderCore::Metal::DeviceContext* context,
            RenderCore::Techniques::ParsingContext& parserContext,
            const CellRenderInfo& renderInfo,
            const Float3x4& cellToWorld,
            const uint64* filterStart, const uint64* filterEnd);
    };

    class PlacementsManager::Pimpl
    {
    public:
        std::vector<PlacementCell> _cells;
        std::shared_ptr<PlacementsRenderer> _renderer;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    void PlacementsRenderer::BeginRender(RenderCore::Metal::DeviceContext* devContext)
    {
        _cache->_preparedRenders.Reset();
        _cache->_sharedStates.CaptureState(devContext);
    }

    void PlacementsRenderer::EndRender(
        RenderCore::Metal::DeviceContext* context,
        RenderCore::Techniques::ParsingContext& parserContext,
        unsigned techniqueIndex)
    {
            // we can commit the opaque-render part now...
        TRY
        {
            ModelRenderer::RenderPrepared(
                RenderCore::Assets::ModelRendererContext(context, parserContext, techniqueIndex),
                _cache->_sharedStates, _cache->_preparedRenders, RenderCore::Assets::DelayStep::OpaqueRender);
        }
        CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
        CATCH_END
        _cache->_sharedStates.ReleaseState(context);
    }

    void PlacementsRenderer::CommitTranslucent(
        RenderCore::Metal::DeviceContext* context,
        RenderCore::Techniques::ParsingContext& parserContext,
        unsigned techniqueIndex, RenderCore::Assets::DelayStep delayStep)
    {
            // draw the translucent parts of models that were previously prepared
        _cache->_sharedStates.CaptureState(context);
        TRY
        {
            ModelRenderer::RenderPrepared(
                RenderCore::Assets::ModelRendererContext(context, parserContext, techniqueIndex),
                _cache->_sharedStates, _cache->_preparedRenders, delayStep);
        }
        CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
        CATCH_END
        _cache->_sharedStates.ReleaseState(context);
    }

    void PlacementsRenderer::FilterRenders(
        const std::function<bool(const RenderCore::Assets::DelayedDrawCall&)>& predicate)
    {
        _cache->_preparedRenders.Filter(predicate);
    }

    namespace Internal
    {
        std::shared_ptr<ModelScaffold> CreateModelScaffold(const ResChar filename[], RenderCore::Assets::IModelFormat& modelFormat)
        {
            auto& compilers = ::Assets::Services::GetAsyncMan().GetIntermediateCompilers();
            auto& store = ::Assets::Services::GetAsyncMan().GetIntermediateStore();
            auto marker = compilers.PrepareResource(
                RenderCore::Assets::ColladaCompiler::Type_Model, 
                (const char**)&filename, 1, store);
            return std::make_shared<ModelScaffold>(std::move(marker));
        }

        std::shared_ptr<MaterialScaffold> CreateMaterialScaffold(const ResChar model[], const ResChar material[], RenderCore::Assets::IModelFormat& modelFormat)
        {
            auto& compilers = ::Assets::Services::GetAsyncMan().GetIntermediateCompilers();
            auto& store = ::Assets::Services::GetAsyncMan().GetIntermediateStore();
            const ResChar* inits[] = { material, model };
            auto marker = compilers.PrepareResource(
                MaterialScaffold::CompileProcessType, 
                inits, dimof(inits), store);
            return std::make_shared<MaterialScaffold>(std::move(marker));
        }
    }

    auto PlacementsRenderer::GetCachedModel(const ResChar filename[]) -> const ModelScaffold*
    {
        auto hash = Hash64(filename);
        auto model = _cache->_modelScaffolds.Get(hash);
        if (!model) {
            model = Internal::CreateModelScaffold(filename, *_modelFormat);
            if (model) {
                _cache->_modelScaffolds.Insert(hash, model);
            }
        }
        return model.get();
    }

    auto PlacementsRenderer::GetCachedMaterial(const ResChar model[], const ResChar material[]) -> const MaterialScaffold*
    {
        auto hash = HashCombine(Hash64(material), Hash64(model));
        auto materialScaffold = _cache->_materialScaffolds.Get(hash);
        if (!materialScaffold) {
            materialScaffold = Internal::CreateMaterialScaffold(model, material, *_modelFormat);
            if (materialScaffold) {
                _cache->_materialScaffolds.Insert(hash, materialScaffold);
            }
        }
        return materialScaffold.get();
    }

    auto PlacementsRenderer::GetCachedRenderer(const ResChar modelFilename[], const ResChar materialFilename[], unsigned LOD) -> const ModelRenderer*
    {
        auto model = GetCachedModel(modelFilename);
        auto material = GetCachedMaterial(modelFilename, materialFilename);
        if (!model || !material) return nullptr;

        uint64 hashedRenderer = (uint64(model) << 2ull) | (uint64(material) << 48ull) | uint64(LOD);
        return _cache->_modelRenderers.Get(hashedRenderer).get();
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
                if (i2->second._placements->GetDependencyValidation()->GetValidationIndex() != 0) {
                    i2->second._placements = &::Assets::GetAssetDep<Placements>(filename);
                }
            }

            return *i2->second._placements;
        }
    }

    auto PlacementsRenderer::GetCachedQuadTree(uint64 cellFilenameHash) const -> const PlacementsQuadTree*
    {
        auto i2 = LowerBound(_cells, cellFilenameHash);
        if (i2!=_cells.end() && i2->first == cellFilenameHash) {
            return i2->second._quadTree.get();
        }
        return nullptr;
    }

    void PlacementsRenderer::SetOverride(uint64 guid, const Placements* placements)
    {
        CellRenderInfo newRenderInfo;
        newRenderInfo._placements = placements;

        auto i = LowerBound(_cellOverrides, guid);
        if (i ==_cellOverrides.end() || i->first != guid) {
            if (placements) {
                _cellOverrides.insert(i, std::make_pair(guid, std::move(newRenderInfo)));
            }
        } else {
            if (placements) {
                i->second = std::move(newRenderInfo); // override the previous one
            } else {
                _cellOverrides.erase(i);
            }
        }
    }

    void PlacementsRenderer::Render(
        RenderCore::Metal::DeviceContext* context,
        RenderCore::Techniques::ParsingContext& parserContext,
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

        if (CullAABB_Aligned(
                AsFloatArray(parserContext.GetProjectionDesc()._worldToProjection), 
                cell._aabbMin, cell._aabbMax)) {
            return;
        }

            //  We need to look in the "_cellOverride" list first.
            //  The overridden cells are actually designed for tools. When authoring 
            //  placements, we need a way to render them before they are flushed to disk.
        TRY 
        {
            auto i = LowerBound(_cellOverrides, cell._filenameHash);
            if (i != _cellOverrides.end() && i->first == cell._filenameHash) {
                Render(context, parserContext, i->second, cell._cellToWorld, filterStart, filterEnd);
            } else {
                auto i2 = LowerBound(_cells, cell._filenameHash);
                if (i2 == _cells.end() || i2->first != cell._filenameHash) {
                    CellRenderInfo newRenderInfo;
                    newRenderInfo._placements = &::Assets::GetAssetDep<Placements>(cell._filename);
                    i2 = _cells.insert(i2, std::make_pair(cell._filenameHash, std::move(newRenderInfo)));
                } else {
                        // check if we need to reload placements
                    if (i2->second._placements->GetDependencyValidation()->GetValidationIndex() != 0) {
                        i2->second._placements = &::Assets::GetAssetDep<Placements>(cell._filename);
                        i2->second._quadTree.reset();
                    }
                }

                if (!i2->second._quadTree) {
                    i2->second._quadTree = std::make_unique<PlacementsQuadTree>(
                        &i2->second._placements->GetObjectReferences()->_cellSpaceBoundary,
                        sizeof(Placements::ObjectReference), 
                        i2->second._placements->GetObjectReferenceCount());
                }

                Render(context, parserContext, i2->second, cell._cellToWorld, filterStart, filterEnd);
            }
        } 
        CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
        CATCH (...) {} 
        CATCH_END
    }

    namespace Internal
    {
        class RendererHelper
        {
        public:
            void Render(
                PlacementsRenderer::Cache& cache,
                RenderCore::Assets::IModelFormat& modelFormat,
                const void* filenamesBuffer,
                const Placements::ObjectReference& obj,
                const Float3x4& cellToWorld,
                const Float3& cameraPosition);

            RendererHelper()
            {
                _currentModel = _currentMaterial = _currentRenderer = 0ull;
                _model = nullptr;
                _material = nullptr;
                _renderer = nullptr;
            }
        protected:
            uint64 _currentModel, _currentMaterial, _currentRenderer;
            ModelScaffold* _model;
            MaterialScaffold* _material;
            ModelRenderer* _renderer;
        };

        void RendererHelper::Render(
            PlacementsRenderer::Cache& cache,
            RenderCore::Assets::IModelFormat& modelFormat,
            const void* filenamesBuffer,
            const Placements::ObjectReference& obj,
            const Float3x4& cellToWorld,
            const Float3& cameraPosition)
        {
                // Basic draw distance calculation
                // many objects don't need to render out to the far clip

            float distanceSq = MagnitudeSquared(
                .5f * (obj._cellSpaceBoundary.first + obj._cellSpaceBoundary.second) - cameraPosition);
            const float maxDistanceSq = 1000.f * 1000.f;
            if (distanceSq > maxDistanceSq) { return; }

                //  Objects should be sorted by model & material. This is important for
                //  reducing the work load in "_cache". Typically cells will only refer
                //  to a limited number of different types of objects, but the same object
                //  may be repeated many times. In these cases, we want to minimize the
                //  workload for every repeat.
            auto modelHash = *(uint64*)PtrAdd(filenamesBuffer, obj._modelFilenameOffset);
            if (modelHash != _currentModel) {
                _model = cache._modelScaffolds.Get(modelHash).get();
                if (!_model || _model->GetDependencyValidation()->GetValidationIndex() > 0) {
                    auto modelFilename = (const ResChar*)PtrAdd(filenamesBuffer, obj._modelFilenameOffset + sizeof(uint64));
                    auto newModel = CreateModelScaffold(modelFilename, modelFormat);
                    _model = newModel.get();
                    cache._modelScaffolds.Insert(modelHash, std::move(newModel));
                }
                _currentModel = modelHash;
            }

            auto materialHash = *(uint64*)PtrAdd(filenamesBuffer, obj._materialFilenameOffset);
            materialHash = HashCombine(materialHash, modelHash);
            if (materialHash != _currentMaterial) {
                _material = cache._materialScaffolds.Get(materialHash).get();
                if (!_material || _material->GetDependencyValidation()->GetValidationIndex() > 0) {
                    std::shared_ptr<MaterialScaffold> newMaterial;
                    auto modelFilename = (const ResChar*)PtrAdd(filenamesBuffer, obj._modelFilenameOffset + sizeof(uint64));
                    auto materialFilename = (const ResChar*)PtrAdd(filenamesBuffer, obj._materialFilenameOffset + sizeof(uint64));
                    newMaterial = CreateMaterialScaffold(modelFilename, materialFilename, modelFormat);
                    _material = newMaterial.get();
                    assert(_material);
                    cache._materialScaffolds.Insert(materialHash, std::move(newMaterial));
                }
                _currentMaterial = materialHash;
            }

                // Simple LOD calculation based on distanceSq from camera...
                //      Currently all models have only the single LOD. But this
                //      may cause problems with models with multiple LOD, because
                //      it may mean rapidly switching back and forth between 
                //      renderers (which can be expensive)
            unsigned LOD = std::min(_model->GetMaxLOD(), unsigned(distanceSq / (150.f*150.f)));
            uint64 hashedRenderer = (uint64(_model) << 2ull) | (uint64(_material) << 48ull) | uint64(LOD);

            if (hashedRenderer != _currentRenderer) {
                    //  Here we have to choose a shared state set for this object.
                    //  We could potentially have more than one shared state set for this renderer
                    //  and separate the objects into their correct state set, as required...
                _renderer = cache._modelRenderers.Get(hashedRenderer).get();
                if (!_renderer || _renderer->GetDependencyValidation()->GetValidationIndex() > 0) {
                    auto modelFilename = (const ResChar*)PtrAdd(filenamesBuffer, obj._modelFilenameOffset + sizeof(uint64));
                    auto searchRules = ::Assets::DefaultDirectorySearchRules(modelFilename);

                    auto newRenderer = std::make_shared<ModelRenderer>(
                        std::ref(*_model), std::ref(*_material), std::ref(cache._sharedStates), &searchRules, LOD);
                    _renderer = newRenderer.get();
                    cache._modelRenderers.Insert(hashedRenderer, std::move(newRenderer));
                }
                _currentRenderer = hashedRenderer;
            }

            auto localToWorld = Combine(obj._localToCell, cellToWorld);

                //  if we have internal transforms, we must use them.
                //  But some models don't have any internal transforms -- in these
                //  cases, the _defaultTransformCount will be zero
            if (_model->ImmutableData()._defaultTransformCount) {
                ModelRenderer::MeshToModel mtm(_model->ImmutableData()._defaultTransforms, (unsigned)_model->ImmutableData()._defaultTransformCount);
                _renderer->Prepare(cache._preparedRenders, cache._sharedStates, AsFloat4x4(localToWorld), &mtm);
            } else {
                _renderer->Prepare(cache._preparedRenders, cache._sharedStates, AsFloat4x4(localToWorld));
            }
        }
    }

    void PlacementsRenderer::Render(
        RenderCore::Metal::DeviceContext* context,
        RenderCore::Techniques::ParsingContext& parserContext,
        const CellRenderInfo& renderInfo,
        const Float3x4& cellToWorld,
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

        auto& placements = *renderInfo._placements;
        auto placementCount = placements.GetObjectReferenceCount();
        if (!placementCount) {
            return;
        }
        
        __declspec(align(16)) auto cellToCullSpace = Combine(cellToWorld, parserContext.GetProjectionDesc()._worldToProjection);
        auto cameraPosition = ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld);
        cameraPosition = TransformPoint(InvertOrthonormalTransform(cellToWorld), cameraPosition);

        const uint64* filterIterator = filterStart;
        const bool doFilter = filterStart != filterEnd;
        Internal::RendererHelper helper;
        
        const auto* filenamesBuffer = placements.GetFilenamesBuffer();
        const auto* objRef = placements.GetObjectReferences();
        
        if (renderInfo._quadTree) {

            unsigned visibleObjs[10*1024];
            unsigned visibleObjCount = 0;
            renderInfo._quadTree->CalculateVisibleObjects(
                AsFloatArray(cellToCullSpace), &objRef->_cellSpaceBoundary,
                sizeof(Placements::ObjectReference),
                visibleObjs, visibleObjCount, dimof(visibleObjs));

                // we have to sort to return to our expected order
            std::sort(visibleObjs, &visibleObjs[visibleObjCount]);

            for (unsigned c=0; c<visibleObjCount; ++c) {
                auto& obj = objRef[visibleObjs[c]];

                if (doFilter) {
                    while (filterIterator != filterEnd && *filterIterator < obj._guid) { ++filterIterator; }
                    if (filterIterator == filterEnd || *filterIterator != obj._guid) { continue; }
                }

                helper.Render(
                    *_cache, *_modelFormat, 
                    filenamesBuffer, obj, cellToWorld, cameraPosition);
            }

        } else {
            for (unsigned c=0; c<placementCount; ++c) {
                auto& obj = objRef[c];
                if (CullAABB_Aligned(AsFloatArray(cellToCullSpace), obj._cellSpaceBoundary.first, obj._cellSpaceBoundary.second)) {
                    continue;
                }

                    // Filtering is required in some cases (for example, if we want to render only
                    // a single object in highlighted state). Rendering only part of a cell isn't
                    // ideal for this architecture. Mostly the cell is intended to work as a 
                    // immutable atomic object. However, we really need filtering for some things.

                if (doFilter) {
                    while (filterIterator != filterEnd && *filterIterator < obj._guid) { ++filterIterator; }
                    if (filterIterator == filterEnd || *filterIterator != obj._guid) { continue; }
                }

                helper.Render(
                    *_cache, *_modelFormat, 
                    filenamesBuffer, obj, cellToWorld, cameraPosition);
            }
        }
    }

    PlacementsRenderer::PlacementsRenderer(std::shared_ptr<RenderCore::Assets::IModelFormat> modelFormat)
    {
        // assert(modelFormat);
        auto cache = std::make_unique<Cache>();
        _cache = std::move(cache);
        _modelFormat = std::move(modelFormat);
    }

    PlacementsRenderer::~PlacementsRenderer() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    void PlacementsManager::Render(
        RenderCore::Metal::DeviceContext* context, 
        RenderCore::Techniques::ParsingContext& parserContext,
        unsigned techniqueIndex)
    {
        TRY 
        {
                // render every registered cell
            _pimpl->_renderer->BeginRender(context);
            for (auto i=_pimpl->_cells.begin(); i!=_pimpl->_cells.end(); ++i) {
                _pimpl->_renderer->Render(context, parserContext, *i);
            }
            _pimpl->_renderer->EndRender(context, parserContext, techniqueIndex);
        }
        CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
        CATCH_END
    }

    void PlacementsManager::RenderTransparent(
        RenderCore::Metal::DeviceContext* context, 
        RenderCore::Techniques::ParsingContext& parserContext,
        unsigned techniqueIndex)
    {
            // assuming that we previously called "Render" to render
            // the main opaque part of the placements, let's now go
            // over each cell and render the transluent parts.
            // we don't need to cull the cells again, because the previous
            // render should have prepared a list of all the draw calls we
            // need for this step
        _pimpl->_renderer->CommitTranslucent(
            context, parserContext, techniqueIndex, RenderCore::Assets::DelayStep::PostDeferred);
    }

    auto PlacementsManager::GetVisibleQuadTrees(const Float4x4& worldToClip) const
            -> std::vector<std::pair<Float3x4, const PlacementsQuadTree*>>
    {
        std::vector<std::pair<Float3x4, const PlacementsQuadTree*>> result;
        for (auto i=_pimpl->_cells.begin(); i!=_pimpl->_cells.end(); ++i) {
            if (!CullAABB(worldToClip, i->_aabbMin, i->_aabbMax)) {
                auto* tree = _pimpl->_renderer->GetCachedQuadTree(i->_filenameHash);
                result.push_back(std::make_pair(i->_cellToWorld, tree));
            }
        }
        return std::move(result);
    }

    auto PlacementsManager::GetObjectBoundingBoxes(const Float4x4& worldToClip) const
            -> std::vector<std::pair<Float3x4, ObjectBoundingBoxes>>
    {
        std::vector<std::pair<Float3x4, ObjectBoundingBoxes>> result;
        for (auto i=_pimpl->_cells.begin(); i!=_pimpl->_cells.end(); ++i) {
            if (!CullAABB(worldToClip, i->_aabbMin, i->_aabbMax)) {
                auto& placements = Assets::GetAsset<Placements>(i->_filename);
                ObjectBoundingBoxes obb;
                obb._boundingBox = &placements.GetObjectReferences()->_cellSpaceBoundary;
                obb._stride = sizeof(Placements::ObjectReference);
                obb._count = placements.GetObjectReferenceCount();
                result.push_back(std::make_pair(i->_cellToWorld, obb));
            }
        }
        return std::move(result);
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
                cell._cellToWorld = AsFloat3x4(offset);

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
        bool HasObject(uint64 guid);

        unsigned AddString(const ResChar str[]);

        DynamicPlacements(const Placements& copyFrom);
        DynamicPlacements();
    };

    uint64 BuildGuid64()
    {
        static std::mt19937_64 generator(std::random_device().operator()());
        return generator();
    }

    static uint32 BuildGuid32()
    {
        static std::mt19937 generator(std::random_device().operator()());
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
            result = unsigned(_filenamesBuffer.size());
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

    bool DynamicPlacements::HasObject(uint64 guid)
    {
        ObjectReference dummy;
        XlZeroMemory(dummy);
        dummy._guid = guid;
        auto i = std::lower_bound(_objects.begin(), _objects.end(), dummy, 
            [](const ObjectReference& lhs, const ObjectReference& rhs) { return lhs._guid < rhs._guid; });
        return (i != _objects.end() && i->_guid == guid);
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
        Float3x4 GetCellToWorld(uint64 cellGuid);
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

    Float3x4 PlacementsEditor::Pimpl::GetCellToWorld(uint64 cellGuid)
    {
        auto p = std::lower_bound(_cells.cbegin(), _cells.cend(), cellGuid, RegisteredCell::CompareHash());
        if (p != _cells.end() && p->_filenameHash == cellGuid) {
            return p->_cellToWorld;
        }
        return Identity<Float3x4>();
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

                    auto& model = *_pimpl->_renderer->GetCachedModel(
                        (const char*)PtrAdd(p.GetFilenamesBuffer(), obj._modelFilenameOffset + sizeof(uint64)));
                    const auto& localBoundingBox = model.GetStaticBoundingBox();
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

    std::vector<PlacementGUID> PlacementsEditor::Find_FrustumIntersection(
        const Float4x4& worldToProjection,
        const std::function<bool(const ObjIntersectionDef&)>& predicate)
    {
        std::vector<PlacementGUID> result;
        const float placementAssumedMaxRadius = 100.f;
        for (auto i=_pimpl->_cells.cbegin(); i!=_pimpl->_cells.cend(); ++i) {
            Float3 cellMin = i->_aabbMin - Float3(placementAssumedMaxRadius, placementAssumedMaxRadius, placementAssumedMaxRadius);
            Float3 cellMax = i->_aabbMax + Float3(placementAssumedMaxRadius, placementAssumedMaxRadius, placementAssumedMaxRadius);
            if (CullAABB(worldToProjection, cellMin, cellMax)) {
                continue;
            }

            auto cellToProjection = Combine(i->_cellToWorld, worldToProjection);

            TRY {
                auto& p = _pimpl->_renderer->GetCachedPlacements(i->_filenameHash, i->_filename);
                for (unsigned c=0; c<p.GetObjectReferenceCount(); ++c) {
                    auto& obj = p.GetObjectReferences()[c];
                        //  We're only doing a very rough world space bounding box vs ray test here...
                        //  Ideally, we should follow up with a more accurate test using the object loca
                        //  space bounding box
                    if (CullAABB(cellToProjection, obj._cellSpaceBoundary.first, obj._cellSpaceBoundary.second)) {
                        continue;
                    }

                    auto& model = *_pimpl->_renderer->GetCachedModel(
                        (const char*)PtrAdd(p.GetFilenamesBuffer(), obj._modelFilenameOffset + sizeof(uint64)));
                    const auto& localBoundingBox = model.GetStaticBoundingBox();
                    if (CullAABB(Combine(AsFloat4x4(obj._localToCell), cellToProjection), localBoundingBox.first, localBoundingBox.second)) {
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
                InvertOrthonormalTransform(i->_cellToWorld),
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

                        auto* model = _pimpl->_renderer->GetCachedModel(
                            (const char*)PtrAdd(p.GetFilenamesBuffer(), obj._modelFilenameOffset + sizeof(uint64)));

                            // note -- we have access to the cell space bounding box. But the local
                            //          space box would be better.
                        def._localSpaceBoundingBox = model ? model->GetStaticBoundingBox()
                            : std::make_pair(Float3(FLT_MAX, FLT_MAX, FLT_MAX), Float3(-FLT_MAX, -FLT_MAX, -FLT_MAX));
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
        std::pair<Float3, Float3>   GetWorldBoundingBox(unsigned index) const;
        std::string         GetMaterialName(unsigned objectIndex, uint64 materialGuid) const;

        virtual void        SetObject(unsigned index, const ObjTransDef& newState);

        virtual bool        Create(const ObjTransDef& newState);
        virtual bool        Create(PlacementGUID guid, const ObjTransDef& newState);
        virtual void        Delete(unsigned index);

        virtual void    Commit();
        virtual void    Cancel();
        virtual void    UndoAndRestart();

        Transaction(
            PlacementsEditor::Pimpl*    editorPimpl,
            const PlacementGUID*        placementsBegin,
            const PlacementGUID*        placementsEnd,
            PlacementsEditor::TransactionFlags::BitField transactionFlags = 0);
        ~Transaction();

    protected:
        PlacementsEditor::Pimpl*    _editorPimpl;

        std::vector<ObjTransDef>    _originalState;
        std::vector<ObjTransDef>    _objects;

        std::vector<PlacementGUID>  _originalGuids;
        std::vector<PlacementGUID>  _pushedGuids;

        void PushObj(unsigned index, const ObjTransDef& newState);

        enum State { Active, Committed };
        State _state;
    };

    auto    Transaction::GetObject(unsigned index) const -> const ObjTransDef& { return _objects[index]; }
    auto    Transaction::GetObjectOriginalState(unsigned index) const -> const ObjTransDef& { return _originalState[index]; }
    PlacementGUID   Transaction::GetGuid(unsigned index) const { return _pushedGuids[index]; }
    PlacementGUID   Transaction::GetOriginalGuid(unsigned index) const { return _originalGuids[index]; }

    unsigned    Transaction::GetObjectCount() const
    {
        assert(_originalGuids.size() == _originalState.size());
        assert(_originalGuids.size() == _objects.size());
        assert(_originalGuids.size() == _pushedGuids.size());
        return (unsigned)_originalGuids.size();
    }

    std::pair<Float3, Float3>   Transaction::GetLocalBoundingBox(unsigned index) const
    {
        auto* model = _editorPimpl->_renderer->GetCachedModel(_objects[index]._model.c_str());
        return model ? model->GetStaticBoundingBox() : std::make_pair(Float3(FLT_MAX, FLT_MAX, FLT_MAX), Float3(-FLT_MAX, -FLT_MAX, -FLT_MAX));
    }

    std::pair<Float3, Float3>   Transaction::GetWorldBoundingBox(unsigned index) const
    {
        auto guid = _pushedGuids[index];
        auto cellToWorld = _editorPimpl->GetCellToWorld(guid.first);
        auto& dynPlacements = *_editorPimpl->GetDynPlacements(guid.first);
        auto& objects = dynPlacements.GetObjects();

        auto dst = std::lower_bound(objects.begin(), objects.end(), guid.second, CompareObjectId());
        return TransformBoundingBox(cellToWorld, dst->_cellSpaceBoundary);
    }

    std::string Transaction::GetMaterialName(unsigned objectIndex, uint64 materialGuid) const
    {
        if (objectIndex >= _objects.size()) return std::string();

        // const unsigned LOD = 0;
        // const auto* renderer = _editorPimpl->_renderer->GetCachedRenderer(
        //     _objects[objectIndex]._model.c_str(), 
        //     _objects[objectIndex]._material.c_str(), LOD);
        // 
        // auto binding = renderer->DrawCallToMaterialBinding();
        // if (drawCallIndex >= binding.size()) return std::string();
        // 
        // auto matGuid = binding[drawCallIndex];

        const auto* material = _editorPimpl->_renderer->GetCachedMaterial(_objects[objectIndex]._model.c_str(), _objects[objectIndex]._material.c_str());
        if (!material) return std::string();
            
        auto res = material->GetMaterialName(materialGuid);
        return res ? std::string(res) : std::string();
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

    static uint32 EverySecondBit(uint64 input)
    {
        uint32 result = 0;
        for (unsigned c=0; c<32; ++c) {
            result |= uint32((input >> (uint64(c)*2ull)) & 0x1ull)<<c;
        }
        return result;
    }

    static uint64 ObjectIdTopPart(const std::string& model, const std::string& material)
    {
        auto modelAndMaterialHash = Hash64(model, Hash64(material));
        return uint64(EverySecondBit(modelAndMaterialHash)) << 32ull;
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

        auto& model = *_editorPimpl->_renderer->GetCachedModel(newState._model.c_str());
        auto boundingBoxCentre = LinearInterpolate(model.GetStaticBoundingBox().first, model.GetStaticBoundingBox().second, 0.5f);
        auto worldSpaceCenter = TransformPoint(newState._localToWorld, boundingBoxCentre);

        std::string materialFilename = newState._material;

        PlacementGUID guid(0, 0);
        PlacementsTransform localToCell = Identity<PlacementsTransform>();

        for (auto i=_editorPimpl->_cells.cbegin(); i!=_editorPimpl->_cells.cend(); ++i) {
            if (    worldSpaceCenter[0] >= i->_mins[0] && worldSpaceCenter[0] < i->_maxs[0]
                &&  worldSpaceCenter[1] >= i->_mins[1] && worldSpaceCenter[1] < i->_maxs[1]) {
                
                    // This is the correct cell. Look for a dynamic placement associated
                auto dynPlacements = _editorPimpl->GetDynPlacements(i->_filenameHash);

                localToCell = Combine(newState._localToWorld, InvertOrthonormalTransform(i->_cellToWorld));
                
                    //  Build a GUID for this object. We're going to sort by GUID, and we want
                    //  objects with the name model and material to appear together. So let's
                    //  build the top 32 bits from the model and material hash. The bottom 
                    //  32 bits can be a random number.
                    //  Note that it's possible that the bottom 32 bits could collide with an
                    //  existing object. It's unlikely, but possible. So let's make sure we
                    //  have a unique GUID before we add it.
                uint64 id, idTopPart = ObjectIdTopPart(newState._model, materialFilename);
                for (;;) {
                    auto id32 = BuildGuid32();
                    id = idTopPart | uint64(id32);
                    if (!dynPlacements->HasObject(id)) { break; }
                }

                dynPlacements->AddPlacement(
                    localToCell, TransformBoundingBox(localToCell, model.GetStaticBoundingBox()),
                    newState._model.c_str(), materialFilename.c_str(), id);

                guid = PlacementGUID(i->_filenameHash, id);
                break;

            }
        }

        if (guid.first == 0 && guid.second == 0) return false;    // couldn't find a way to create this object
        
        ObjTransDef newObj = newState;
        newObj._transaction = ObjTransDef::Created;

        ObjTransDef originalState;
        originalState._localToWorld = Identity<Float3x4>();
        originalState._transaction = ObjTransDef::Error;

        auto insertLoc = std::lower_bound(_originalGuids.begin(), _originalGuids.end(), guid, CompareGUID);
        auto insertIndex = std::distance(_originalGuids.begin(), insertLoc);

        _originalState.insert(_originalState.begin() + insertIndex, originalState);
        _objects.insert(_objects.begin() + insertIndex, newObj);
        _originalGuids.insert(_originalGuids.begin() + insertIndex, guid);
        _pushedGuids.insert(_pushedGuids.begin() + insertIndex, guid);

        return true;
    }

    bool    Transaction::Create(PlacementGUID guid, const ObjTransDef& newState)
    {
        auto& model = *_editorPimpl->_renderer->GetCachedModel(newState._model.c_str());
        auto boundingBoxCentre = LinearInterpolate(model.GetStaticBoundingBox().first, model.GetStaticBoundingBox().second, 0.5f);
        auto worldSpaceCenter = TransformPoint(newState._localToWorld, boundingBoxCentre);

        std::string materialFilename = newState._material;

        PlacementsTransform localToCell = Identity<PlacementsTransform>();
        bool foundCell = false;

        for (auto i=_editorPimpl->_cells.cbegin(); i!=_editorPimpl->_cells.cend(); ++i) {
            if (i->_filenameHash == guid.first) {
                auto dynPlacements = _editorPimpl->GetDynPlacements(i->_filenameHash);
                localToCell = Combine(newState._localToWorld, InvertOrthonormalTransform(i->_cellToWorld));

                auto idTopPart = ObjectIdTopPart(newState._model, materialFilename);
                uint64 id = idTopPart | uint64(guid.second & 0xffffffffull);
                if (dynPlacements->HasObject(id)) {
                    assert(0);      // got a hash collision or duplicated id
                    return false;
                }

                dynPlacements->AddPlacement(
                    localToCell, TransformBoundingBox(localToCell, model.GetStaticBoundingBox()),
                    newState._model.c_str(), materialFilename.c_str(), id);

                guid.second = id;
                foundCell = true;
                break;
            }
        }
        if (!foundCell) return false;    // couldn't find a way to create this object
        
        ObjTransDef newObj = newState;
        newObj._transaction = ObjTransDef::Created;

        ObjTransDef originalState;
        originalState._localToWorld = Identity<Float3x4>();
        originalState._transaction = ObjTransDef::Error;

        auto insertLoc = std::lower_bound(_originalGuids.begin(), _originalGuids.end(), guid, CompareGUID);
        auto insertIndex = std::distance(_originalGuids.begin(), insertLoc);

        _originalState.insert(_originalState.begin() + insertIndex, originalState);
        _objects.insert(_objects.begin() + insertIndex, newObj);
        _originalGuids.insert(_originalGuids.begin() + insertIndex, guid);
        _pushedGuids.insert(_pushedGuids.begin() + insertIndex, guid);

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
        
        auto& guid = _pushedGuids[index];

        auto cellToWorld = _editorPimpl->GetCellToWorld(guid.first);
        auto& dynPlacements = *_editorPimpl->GetDynPlacements(guid.first);
        auto& objects = dynPlacements.GetObjects();

        auto dst = std::lower_bound(objects.begin(), objects.end(), guid.second, CompareObjectId());

        std::pair<Float3, Float3> cellSpaceBoundary;
        PlacementsTransform localToCell;
        std::string materialFilename = newState._material;
        if (newState._transaction != ObjTransDef::Deleted && newState._transaction != ObjTransDef::Error) {
            localToCell = Combine(newState._localToWorld, InvertOrthonormalTransform(cellToWorld));

            auto* model = _editorPimpl->_renderer->GetCachedModel(newState._model.c_str());
            cellSpaceBoundary = model
                ? TransformBoundingBox(localToCell, model->GetStaticBoundingBox())
                : std::make_pair(Float3(FLT_MAX, FLT_MAX, FLT_MAX), Float3(-FLT_MAX, -FLT_MAX, -FLT_MAX));
        }

            // todo --  handle the case where an object should move to another cell!
            //          this should actually change the first part of the GUID
            //          Also, if the type of the object changes, it should change the guid... Which
            //          means that it should change location in the list of objects. In this case
            //          we should erase the old object and create a new one

        bool isDeleteOp = newState._transaction == ObjTransDef::Deleted || newState._transaction == ObjTransDef::Error;
        bool destroyExisting = isDeleteOp;
        bool hasExisting = dst != objects.end() && dst->_guid == guid.second;

            // awkward case where the object id has changed... This can happen
            // if the object model or material was changed
        auto newIdTopPart = ObjectIdTopPart(newState._model, materialFilename);
        bool objectIdChanged = newIdTopPart != (guid.second & 0xffffffff00000000ull);
        if (objectIdChanged) {
            auto id32 = uint32(guid.second);
            for (;;) {
                guid.second = newIdTopPart | uint64(id32);
                if (!dynPlacements.HasObject(guid.second)) { break; }
                id32 = BuildGuid32();
            }

                // destroy & re-create
            destroyExisting = true;
        }

        if (destroyExisting && hasExisting) {
            objects.erase(dst);
            hasExisting = false;
        } 
        
        if (!isDeleteOp) {
            if (hasExisting) {
                dst->_localToCell = localToCell;
                dst->_modelFilenameOffset = dynPlacements.AddString(newState._model.c_str());
                dst->_materialFilenameOffset = dynPlacements.AddString(materialFilename.c_str());
                dst->_cellSpaceBoundary = cellSpaceBoundary;
            } else {
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
        const PlacementGUID*        guidsEnd,
        PlacementsEditor::TransactionFlags::BitField transactionFlags)
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
            if (transactionFlags & PlacementsEditor::TransactionFlags::IgnoreIdTop32Bits) {
                    //  Sometimes we want to ignore the top 32 bits of the id. It works, but it's
                    //  much less efficient, because we can't take advantage of the sorting.
                    //  Ideally we should avoid this path
                for (;i != iend; ++i) {
                    uint32 comparison = uint32(i->second);
                    auto pend = &placements.GetObjectReferences()[placements.GetObjectReferenceCount()];
                    auto pIterator = std::find_if(
                        placements.GetObjectReferences(), pend,
                        [=](const Placements::ObjectReference& obj) { return uint32(obj._guid) == comparison; });
                    if (pIterator!=pend) {
                        i->second = pIterator->_guid;       // set the recorded guid to the full guid

                        ObjTransDef def;
                        def._localToWorld = Combine(pIterator->_localToCell, cellToWorld);
                        def._model = (const char*)PtrAdd(placements.GetFilenamesBuffer(), sizeof(uint64) + pIterator->_modelFilenameOffset);
                        def._material = (const char*)PtrAdd(placements.GetFilenamesBuffer(), sizeof(uint64) + pIterator->_materialFilenameOffset);
                        def._transaction = ObjTransDef::Unchanged;
                        originalState.push_back(def);
                    } else {
                            // we couldn't find an original for this object. It's invalid
                        ObjTransDef def;
                        def._localToWorld = Identity<Float3x4>();
                        def._transaction = ObjTransDef::Error;
                        originalState.push_back(def);
                    }
                }
            } else {
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
                        def._localToWorld = Identity<Float3x4>();
                        def._transaction = ObjTransDef::Error;
                        originalState.push_back(def);
                    }
                }
            }
        }

        _objects = originalState;
        _originalState = std::move(originalState);
        _originalGuids = guids;
        _pushedGuids = std::move(guids);
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

    uint64 PlacementsEditor::CreateCell(
        PlacementsManager& manager,
        const ::Assets::ResChar name[],
        const Float2& mins, const Float2& maxs)
    {
            //  The implementation here is not great. Originally, PlacementsManager
            //  was supposed to be constructed with all of it's cells already created.
            //  But we need create/delete for the interface with the editor
        PlacementCell newCell;
        XlCopyString(newCell._filename, name);
        newCell._filenameHash = Hash64(newCell._filename);
        newCell._cellToWorld = AsFloat3x4(Expand(mins, 0.f));
        newCell._aabbMin = Expand(mins, -10000.f);
        newCell._aabbMax = Expand(maxs,  10000.f);
        manager._pimpl->_cells.push_back(newCell);
        RegisterCell(newCell, mins, maxs);
        return newCell._filenameHash;
    }

    void PlacementsEditor::RemoveCell(PlacementsManager& manager, uint64 id)
    {
        assert(0);  // not implemented yet
    }

    uint64 PlacementsEditor::GenerateObjectGUID()
    {
        return uint64(BuildGuid32());
    }

    void PlacementsEditor::RenderFiltered(
        RenderCore::Metal::DeviceContext* context,
        RenderCore::Techniques::ParsingContext& parserContext,
        unsigned techniqueIndex,
        const PlacementGUID* begin, const PlacementGUID* end,
        const std::function<bool(const RenderCore::Assets::DelayedDrawCall&)>& predicate)
    {
        _pimpl->_renderer->BeginRender(context);

            //  We need to take a copy, so we don't overwrite
            //  and reorder the caller's version.
        if (begin || end) {
            std::vector<PlacementGUID> copy(begin, end);
            std::sort(copy.begin(), copy.end());

            auto ci = _pimpl->_cells.begin();
            for (auto i=copy.begin(); i!=copy.end();) {
                auto i2 = i+1;
                for (; i2!=copy.end() && i2->first == i->first; ++i2) {}

			    while (ci != _pimpl->_cells.end() && ci->_filenameHash < i->first) { ++ci; }

                if (ci != _pimpl->_cells.end() && ci->_filenameHash == i->first) {

                        // re-write the object guids for the renderer's convenience
                    uint64* tStart = &i->first;
                    uint64* t = tStart;
                    while (i < i2) { *t++ = i->second; i++; }

                    _pimpl->_renderer->Render(
                        context, parserContext,
                        *ci, tStart, t);

                } else {
                    i = i2;
                }
            }
        } else {
                // in this case we're not filtering by object GUID (though we may apply a predicate on the prepared draw calls)
            for (auto i=_pimpl->_cells.begin(); i!=_pimpl->_cells.end(); ++i) {
                _pimpl->_renderer->Render(context, parserContext, *i);
            }
        }

        if (predicate) {
            _pimpl->_renderer->FilterRenders(predicate);
        }
        _pimpl->_renderer->EndRender(context, parserContext, techniqueIndex);

            // we also have to commit translucent steps. We must use the geometry from all translucent steps
        for (unsigned c=unsigned(RenderCore::Assets::DelayStep::OpaqueRender)+1; c<unsigned(RenderCore::Assets::DelayStep::Max); ++c)
            _pimpl->_renderer->CommitTranslucent(context, parserContext, techniqueIndex, RenderCore::Assets::DelayStep(c));
    }

	void PlacementsEditor::PerformGUIDFixup(PlacementGUID* begin, PlacementGUID* end) const
	{
		std::sort(begin, end);

		auto ci = _pimpl->_cells.begin();
		for (auto i = begin; i != end;) {
			auto i2 = i + 1;
			for (; i2 != end && i2->first == i->first; ++i2) {}

			while (ci != _pimpl->_cells.end() && ci->_filenameHash < i->first) { ++ci; }

			if (ci != _pimpl->_cells.end() && ci->_filenameHash == i->first) {

				// The ids will usually have their
				// top 32 bit zeroed out. We must fix them by finding the match placements
				// in our cached placements, and fill in the top 32 bits...
				auto& cachedPlacements = _pimpl->_renderer->GetCachedPlacements(ci->_filenameHash, ci->_filename);
				auto count = cachedPlacements.GetObjectReferenceCount();
				auto* placements = cachedPlacements.GetObjectReferences();

				for (auto i3 = i; i3 < i2; ++i3) {
					auto p = std::find_if(placements, &placements[count],
						[=](const Placements::ObjectReference& obj) { return uint32(obj._guid) == uint32(i3->second); });
                    if (p != &placements[count])
                        i3->second = p->_guid;
				}

			}
            
            i = i2;
		}

			// re-sort again
		std::sort(begin, end);
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
        auto* model = _pimpl->_renderer->GetCachedModel(modelName);
        return model ? model->GetStaticBoundingBox() : std::make_pair(Float3(FLT_MAX, FLT_MAX, FLT_MAX), Float3(-FLT_MAX, -FLT_MAX, -FLT_MAX));
    }

    auto PlacementsEditor::Transaction_Begin(
        const PlacementGUID* placementsBegin, 
        const PlacementGUID* placementsEnd,
        TransactionFlags::BitField transactionFlags) -> std::shared_ptr<ITransaction>
    {
        return std::make_shared<Transaction>(_pimpl.get(), placementsBegin, placementsEnd, transactionFlags);
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

    WorldPlacementsConfig::WorldPlacementsConfig()
    {
        _cellCount = UInt2(0,0);
        _cellSize = 512.f;
    }

}

