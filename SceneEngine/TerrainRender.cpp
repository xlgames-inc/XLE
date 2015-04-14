// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Terrain.h"
#include "TerrainInternal.h"
#include "TerrainUberSurface.h"
#include "LightingParserContext.h"
#include "Noise.h"
#include "SimplePatchBox.h"
#include "SceneEngineUtils.h"
#include "SurfaceHeightsProvider.h"
#include "TerrainMaterial.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../Utility/Streams/DataSerialize.h"
#include "../Utility/StringFormat.h"

#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/DataPacket.h"
#include "../RenderCore/Metal/Format.h"
#include "../RenderCore/Resource.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/DeviceContextImpl.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/RenderUtils.h"
#include "../Assets/CompileAndAsyncManager.h"
#include "../Assets/AssetUtils.h"

#include "../Math/Transformations.h"
#include "../Math/ProjectionMath.h"
#include "../Math/Geometry.h"
#include "../Utility/BitHeap.h"
#include "../Utility/BitUtils.h"
#include "../Utility/HeapUtils.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/Conversion.h"
#include "../ConsoleRig/Console.h"

#include "../../RenderCore/DX11/Metal/DX11.h"
#include "../../RenderCore/DX11/Metal/DX11Utils.h"
#include <stack>

#include "../../RenderCore/DX11/Metal/IncludeDX11.h"
#include <D3DX11.h>
#include "../../../Foreign/DirectXTex/DirectXTex/DirectXTex.h"

namespace SceneEngine
{
    float SunDirectionAngle = 1.0821046f;

    using namespace RenderCore;
    using namespace RenderCore::Metal;

    static UInt2 AsUInt2(Float2 input) { return UInt2(unsigned(input[0]), unsigned(input[1])); }

    //////////////////////////////////////////////////////////////////////////////////////////
    class TextureTile
    {
    public:
        BufferUploads::TransactionID _transaction;
        unsigned _x, _y, _arrayIndex;
        unsigned _width, _height;
        unsigned _uploadId;

        TextureTile();
        ~TextureTile();
        TextureTile(TextureTile&& moveFrom);
        TextureTile& operator=(TextureTile&& moveFrom);

        void swap(TextureTile& other);
    };

    TextureTile::TextureTile()
    {
        _transaction = ~BufferUploads::TransactionID(0x0);
        _x = _y = _arrayIndex = ~unsigned(0x0);
        _width = _height = ~unsigned(0x0);
        _uploadId = ~unsigned(0x0);
    }

    TextureTile::~TextureTile()
    {
            // make sure our transactions have been cancelled,
            //  if you get this assert, it means we haven't called BufferUploads::IManager::Transaction_End
            //  on this transaction before this destructor.
        assert(_transaction == ~BufferUploads::TransactionID(0x0));
    }

    TextureTile::TextureTile(TextureTile&& moveFrom)
    {
        _transaction = std::move(moveFrom._transaction);
        moveFrom._transaction = ~BufferUploads::TransactionID(0x0);
        _x = std::move(moveFrom._x);
        _y = std::move(moveFrom._y);
        _arrayIndex = std::move(moveFrom._arrayIndex);
        _width = std::move(moveFrom._width);
        _height = std::move(moveFrom._height);
        _uploadId = std::move(moveFrom._uploadId);
        moveFrom._transaction = ~BufferUploads::TransactionID(0x0);
        moveFrom._x = moveFrom._y = moveFrom._arrayIndex = ~unsigned(0x0);
        moveFrom._width = moveFrom._height = ~unsigned(0x0);
        moveFrom._uploadId = ~unsigned(0x0);
    }

    TextureTile& TextureTile::operator=(TextureTile&& moveFrom)
    {
            //  can't reassign while a transaction is going, because
            //  we don't have a pointer to buffer uploads to end the
            //  previous transaction
        assert(_transaction == ~BufferUploads::TransactionID(0x0));
        TextureTile(moveFrom).swap(*this);
        return *this;
    }

    void TextureTile::swap(TextureTile& other)
    {
        std::swap(_transaction, other._transaction);
        std::swap(_x, other._x);
        std::swap(_y, other._y);
        std::swap(_arrayIndex, other._arrayIndex);
        std::swap(_width, other._width);
        std::swap(_height, other._height);
        std::swap(_uploadId, other._uploadId);
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    /** <summary>A set of "texture tiles", all of which are the same size</summary> */
    class TextureTileSet
    {
    public:
        void    Transaction_Begin(
                    TextureTile& tile,
                    const void* fileHandle, size_t offset, size_t dataSize);

        bool    IsValid(TextureTile& tile);

        BufferUploads::IManager&    GetBufferUploads() { return *_bufferUploads; }
        ShaderResourceView&         GetShaderResource() { return _shaderResource; }
        UnorderedAccessView&        GetUnorderedAccessView() { return _uav; }
        
        TextureTileSet( BufferUploads::IManager& bufferUploads,
                        Int2 elementSize, unsigned elementCount,
                        RenderCore::Metal::NativeFormat::Enum format,
                        bool allowModification);

    private:
        class ArraySlice
        {
        public:
            BitHeap     _allocationFlags;
            unsigned    _unallocatedCount;

            ArraySlice(int count);
            ArraySlice(ArraySlice&& moveFrom);
            ArraySlice& operator=(ArraySlice&& moveFrom);
        };

        Int2                        _elementsPerArraySlice;
        Int2                        _elementSize;
        std::vector<ArraySlice>     _slices;
        BufferUploads::IManager *   _bufferUploads;
        bool                        _allowModification;

        std::vector<unsigned>       _uploadIds;
        LRUQueue                    _lruQueue;

        intrusive_ptr<BufferUploads::ResourceLocator>      _resource;
        ShaderResourceView              _shaderResource;
        UnorderedAccessView             _uav;
        BufferUploads::TransactionID    _creationTransaction;

        void    CompleteCreation();
    };

    static Int3 LinearToCoords(unsigned linearAddress, Int2 elesPerSlice)
    {
        unsigned slice = linearAddress / (elesPerSlice[0]*elesPerSlice[1]);
        unsigned t = (linearAddress - slice * (elesPerSlice[0]*elesPerSlice[1]));
        return Int3(t % elesPerSlice[0], t / elesPerSlice[1], slice);
    }

    static unsigned CoordsToLinear(Int3 coords, Int2 elesPerSlice)
    {
        return coords[0] + coords[1] * elesPerSlice[0] + coords[2] * (elesPerSlice[0] * elesPerSlice[1]);
    }

    void    TextureTileSet::Transaction_Begin(
                TextureTile& tile,
                const void* fileHandle, size_t offset, size_t dataSize)
    {
        CompleteCreation();
        if (!_resource || _resource->IsEmpty()) {
            return; // cannot begin transactions until the resource is allocated
        }

            //  Begin a streaming operation, loading from the provided file ptr
            //  This is useful if we have a single file with many texture within.
            //  Often we want to keep that file open, and stream in textures from
            //  it as required.
            //
            //  First, find an avoid element in our tile array. 
            //      Note that if we don't find something available, we need to
            //      evict a texture... That adds complexity, because we need to
            //      implement a LRU scheme, or some other scheme to pick the texture
            //      to evict.
            //
            //  Start a buffer uploads transaction to first load from the file
            //  into a staging texture, and then copy from there into the main
            //  tile set.
        Int3 address;
        bool foundFreeSpace = false;
        for (auto i=_slices.begin(); i!=_slices.end(); ++i) {
            if (i->_unallocatedCount) {
                auto heapIndex = i->_allocationFlags.Allocate();
                address[0] = heapIndex % _elementsPerArraySlice[0];
                address[1] = heapIndex / _elementsPerArraySlice[0];
                address[2] = (int)std::distance(_slices.begin(), i);
                --i->_unallocatedCount;
                foundFreeSpace = true;
                break;
            }
        }
        
        if (!foundFreeSpace) {
                //  look for the oldest block... We keep a queue of old blocks
                //  ... todo; we need to lock tiles that are pending, or have 
                //  been previously used this frame.
            auto oldestBlock = _lruQueue.GetOldestValue();
            if (oldestBlock == ~unsigned(0x0)) {
                assert(0);
                return; // cannot complete the upload
            }

            address = LinearToCoords(oldestBlock, _elementsPerArraySlice);
        }

        unsigned linearId = CoordsToLinear(address, _elementsPerArraySlice);
        _lruQueue.BringToFront(linearId);
        unsigned uploadId = ++_uploadIds[linearId];

            //  begin a transaction, and pass the id back to the client
            //  buffer uploads should read from the file at the given
            //  address, and upload the data into the given location
        if (tile._transaction == ~BufferUploads::TransactionID(0x0)) {
                //  if there's already a transaction on this tile, we might
                //  be chasing the same upload again
            tile._transaction = _bufferUploads->Transaction_Begin(_resource);
        }
        assert(tile._transaction != ~BufferUploads::TransactionID(0x0));

        BufferUploads::Box2D destinationBox;
        destinationBox._left    = address[0] * _elementSize[0];
        destinationBox._top     = address[1] * _elementSize[1];
        destinationBox._right   = (address[0] + 1) * _elementSize[0];
        destinationBox._bottom  = (address[1] + 1) * _elementSize[1];

        tile._x = address[0] * _elementSize[0];
        tile._y = address[1] * _elementSize[1];
        tile._arrayIndex = address[2];
        tile._width = _elementSize[0];
        tile._height = _elementSize[1];
        tile._uploadId = uploadId;
        assert(tile._width != ~unsigned(0x0) && tile._height != ~unsigned(0x0));

        auto dataPacket = BufferUploads::CreateFileDataSource(fileHandle, offset, dataSize);
        _bufferUploads->UpdateData(
            tile._transaction, dataPacket.get(),
            BufferUploads::PartialResource(destinationBox, 0, 0, address[2]));
    }

    bool    TextureTileSet::IsValid(TextureTile& tile)
    {
        if (tile._width == ~unsigned(0x0) || tile._height == ~unsigned(0x0))
            return false;
        
            //  check to see if this tile has been invalidated in an overwrite 
            //  operation.
        unsigned linearAddress = CoordsToLinear(Int3(   tile._x / _elementSize[0], 
                                                        tile._y / _elementSize[1], 
                                                        tile._arrayIndex), _elementsPerArraySlice);
        if (_uploadIds[linearAddress] != tile._uploadId)
            return false;   // another tile has been uploaded into this same spot

        _lruQueue.BringToFront(linearAddress);
        return true;
    }

    void    TextureTileSet::CompleteCreation()
    {
        if (_creationTransaction != ~BufferUploads::TransactionID(0x0)) {
            if (_bufferUploads->IsCompleted(_creationTransaction)) {
                auto uploadsResource = _bufferUploads->GetResource(_creationTransaction);
                if (uploadsResource && !uploadsResource->IsEmpty()) {
                    ShaderResourceView shaderResource(uploadsResource->GetUnderlying());
                    UnorderedAccessView uav;
                    if (_allowModification) {
                        uav = UnorderedAccessView(uploadsResource->GetUnderlying());
                    }
                    _bufferUploads->Transaction_End(_creationTransaction);
                    _creationTransaction = ~BufferUploads::TransactionID(0x0);

                    _resource = std::move(uploadsResource);
                    _shaderResource = std::move(shaderResource);
                    _uav = std::move(uav);
                }
            }
        }
    }

    TextureTileSet::TextureTileSet( BufferUploads::IManager& bufferUploads,
                                    Int2 elementSize, unsigned elementCount,
                                    RenderCore::Metal::NativeFormat::Enum format,
                                    bool allowModification)
    {
        _creationTransaction = ~BufferUploads::TransactionID(0x0);
        _allowModification = allowModification;

            //  keep pages of around 2048x2048... just add enough array elements
            //  to have as many elements as requested. Normally powers-of-two
            //  are best for element size and element count.
        const unsigned defaultPageSize = 2048;
        const int elementsH = defaultPageSize / elementSize[0];
        const int elementsV = defaultPageSize / elementSize[1];
        const int elementsPerPage = elementsH * elementsV;
        const int pageCount = (elementCount+elementsPerPage-1) / elementsPerPage;
        
            // fill in the slices (everything unallocated initially //
        std::vector<ArraySlice> slices;
        slices.reserve(pageCount);
        for (int c=0; c<pageCount; ++c) {
            slices.push_back(ArraySlice(elementsPerPage));
        }

        using namespace BufferUploads;
        BufferDesc desc;
        desc._type = BufferDesc::Type::Texture;
        desc._bindFlags = BindFlag::ShaderResource;
        desc._cpuAccess = 0;
        desc._gpuAccess = GPUAccess::Read;
        desc._allocationRules = 0;
        desc._textureDesc = BufferUploads::TextureDesc::Plain2D(
            defaultPageSize, defaultPageSize, format, 1, uint8(pageCount));
        XlCopyString(desc._name, "TextureTileSet");

        if (allowModification) {
            desc._bindFlags |= BindFlag::UnorderedAccess;
            desc._gpuAccess |= GPUAccess::Write;
        }

        intrusive_ptr<BufferUploads::ResourceLocator> resource;
        ShaderResourceView shaderResource;
        UnorderedAccessView uav;
        
        #pragma warning(disable:4127)       // warning C4127: conditional expression is constant
        const bool immediateCreate = false;
        if (immediateCreate) {
            resource            = bufferUploads.Transaction_Immediate(desc, nullptr);
            shaderResource      = ShaderResourceView(resource->GetUnderlying());
            uav                 = UnorderedAccessView(resource->GetUnderlying());
        } else {
            _creationTransaction = bufferUploads.Transaction_Begin(desc, (RawDataPacket*)nullptr, TransactionOptions::ForceCreate);
        }

        LRUQueue lruQueue(elementsPerPage * pageCount);
        std::vector<unsigned> uploadIds;
        uploadIds.resize(elementsPerPage * pageCount, ~unsigned(0x0));

        _bufferUploads = &bufferUploads;
        _resource = std::move(resource);
        _slices = std::move(slices);
        _elementsPerArraySlice = Int2(elementsH, elementsV);
        _elementSize = elementSize;
        _shaderResource = std::move(shaderResource);
        _uav = std::move(uav);
        _uploadIds = std::move(uploadIds);
        _lruQueue = std::move(lruQueue);
    }

    TextureTileSet::ArraySlice::ArraySlice(int count)
    {
        _allocationFlags.Reserve(count);
        _unallocatedCount = count;
    }

    TextureTileSet::ArraySlice::ArraySlice(ArraySlice&& moveFrom)
    {
        _allocationFlags = std::move(moveFrom._allocationFlags);
        _unallocatedCount = std::move(moveFrom._unallocatedCount);
        moveFrom._unallocatedCount = 0;
    }

    auto TextureTileSet::ArraySlice:: operator=(ArraySlice&& moveFrom) -> ArraySlice&
    {
        _allocationFlags = std::move(moveFrom._allocationFlags);
        _unallocatedCount = std::move(moveFrom._unallocatedCount);
        moveFrom._unallocatedCount = 0;
        return *this;
    }

    //////////////////////////////////////////////////////////////////////////////////////////

    class TerrainCellId
    {
    public:
        static const unsigned CoverageCount = 1;
        char        _heightMapFilename[256];
        char        _coverageFilename[CoverageCount][256];
        Float4x4    _cellToWorld;
        Float3      _aabbMin, _aabbMax;

        uint64      BuildHash() const;
        
        TerrainCellId()
        {
            _heightMapFilename[0] = '\0';
            for (unsigned c=0; c<CoverageCount; ++c)
                _coverageFilename[c][0] = '\0';
            _cellToWorld = Identity<Float4x4>();
            _aabbMin = _aabbMax = Float3(0.f, 0.f, 0.f);
        }
    };

    uint64      TerrainCellId::BuildHash() const
    {
        uint64 result = Hash64(_heightMapFilename);
        for (unsigned c=0; c<CoverageCount; ++c) {
            if (_coverageFilename[c] && _coverageFilename[c][0]) {
                result = Hash64(
                    _coverageFilename[c], &_coverageFilename[c][XlStringLen(_coverageFilename[c])], 
                    result);
            }
        }
        return result;
    }

    static const RenderCore::Metal::NativeFormat::Enum CoverageFileFormat[TerrainCellId::CoverageCount] = {
        // RenderCore::Metal::NativeFormat::BC1_UNORM
        RenderCore::Metal::NativeFormat::R16G16_UNORM
    };

    class TerrainRenderingContext;
    class TerrainCollapseContext;
    class TerrainCellRenderer
    {
    public:
        void CullNodes( DeviceContext* context, LightingParserContext& parserContext, 
                        TerrainRenderingContext& terrainContext, TerrainCollapseContext& collapseContext,
                        const TerrainCellId& cell);
        void WriteQueuedNodes(TerrainRenderingContext& renderingContext, TerrainCollapseContext& collapseContext);
        void CompletePendingUploads();
        void QueueUploads(TerrainRenderingContext& terrainContext);
        void Render(DeviceContext* context, LightingParserContext& parserContext, TerrainRenderingContext& terrainContext);

        void HeightMapShortCircuit(std::string cellName, UInt2 cellOrigin, UInt2 cellMax, const ShortCircuitUpdate& upd);

        Int2 GetElementSize() const { return _elementSize; }

        TerrainCellRenderer(std::shared_ptr<ITerrainFormat> ioFormat, BufferUploads::IManager* bufferUploads, Int2 heightMapNodeElementWidth);
        ~TerrainCellRenderer();

    private:
        class NodeRenderInfo
        {
        public:
            TextureTile _heightMapTile;
            TextureTile _heightMapPendingTile;

            TextureTile _coverageTile[TerrainCellId::CoverageCount];
            TextureTile _coveragePendingTile[TerrainCellId::CoverageCount];

            void QueueHeightMapUpload(      TextureTileSet& heightMapTileSet, 
                                            const void* filePtr, const void* cacheFilePtr,
                                            const TerrainCell::Node& sourceNode);
            void QueueCoverageUpload(       unsigned index, TextureTileSet& coverageTileSet, 
                                            const void* filePtr, unsigned fileOffset, unsigned fileSize);
            bool CompleteHeightMapUpload(   BufferUploads::IManager& bufferUploads);
            bool CompleteCoverageUpload(    BufferUploads::IManager& bufferUploads);

            NodeRenderInfo();
            NodeRenderInfo(const TerrainCell::Node& node);
            NodeRenderInfo(NodeRenderInfo&& moveFrom);
            NodeRenderInfo& operator=(NodeRenderInfo&& moveFrom);
        };

        class CellRenderInfo
        {
        public:
            CellRenderInfo(const TerrainCell& cell, const TerrainCellTexture* cellCoverage[TerrainCellId::CoverageCount]);
            CellRenderInfo(CellRenderInfo&& moveFrom);
            CellRenderInfo& operator=(CellRenderInfo&& moveFrom) throw();
            ~CellRenderInfo();

            std::vector<NodeRenderInfo> _nodes;
            const TerrainCell*          _sourceCell;       // unguarded ptr... Perhaps keep a reference count?
            const TerrainCellTexture*   _sourceCoverage[TerrainCellId::CoverageCount];
            const void*                 _heightMapStreamingFilePtr;
            const void*                 _heightMapCacheStreamingFilePtr;
            const void*                 _coverageStreamingFilePtr[TerrainCellId::CoverageCount];

        private:
            CellRenderInfo(const CellRenderInfo& );
            CellRenderInfo& operator=(const CellRenderInfo& );
        };

        typedef std::pair<uint64, std::unique_ptr<CellRenderInfo>> CRIPair;

        std::unique_ptr<TextureTileSet> _heightMapTileSet;
        std::unique_ptr<TextureTileSet> _coverageTileSet[TerrainCellId::CoverageCount];
        std::vector<CRIPair>            _renderInfos;
        Int2                            _elementSize;
        Int2                            _coverageElementSize;

        typedef std::pair<CellRenderInfo*, unsigned> UploadPair;
        std::vector<UploadPair>         _pendingHeightMapUploads;
        std::vector<UploadPair>         _pendingCoverageUploads;

        std::shared_ptr<ITerrainFormat> _ioFormat;

        friend class TerrainRenderingContext;
        friend class TerrainCollapseContext;
        friend class TerrainSurfaceHeightsProvider;

        void    CullNodes(DeviceContext* context, LightingParserContext& parserContext, TerrainRenderingContext& terrainContext, CellRenderInfo& cellRenderInfo, const Float4x4& localToWorld);
        void    RenderNode(DeviceContext* context, LightingParserContext& parserContext, TerrainRenderingContext& terrainContext, CellRenderInfo& cellRenderInfo, unsigned absNodeIndex, int8 neighbourLodDiffs[4]);

        void    CullNodes(
            TerrainRenderingContext& terrainContext, TerrainCollapseContext& collapseContext,
            const Float4x4& worldToProjection, const Float3& viewPositionWorld,
            CellRenderInfo& cellRenderInfo, const Float4x4& cellToWorld);

        void    ShortCircuitTileUpdate(const TextureTile& tile, UInt2 nodeMin, UInt2 nodeMax, unsigned downsample, Float4x4& localToCell, const ShortCircuitUpdate& upd);

        TerrainCellRenderer(const TerrainCellRenderer&);
        TerrainCellRenderer& operator=(const TerrainCellRenderer&);
    };

    //////////////////////////////////////////////////////////////////////////////////////////
    static float CalculateScreenSpaceEdgeLength(
        const Float4x4& localToProjection,
        float viewportWidth, float viewportHeight)
    {
        const Float3 corners[] = {
            Float3(0.f, 0.f, 0.f), Float3(1.f, 0.f, 0.f),
            Float3(1.f, 1.f, 0.f), Float3(0.f, 1.f, 0.f)
        };

        Float3 transformedCorners[4];
        for (unsigned c=0; c<dimof(transformedCorners); ++c) {
            auto t = localToProjection * Expand(corners[c], 1.f);
            transformedCorners[c] = Float3(t[0]/t[3] * .5f * viewportWidth, t[1]/t[3] * .5f * viewportHeight, t[2]/t[3]);
        }

        float screenSpaceLengths[4] = {
            Magnitude(transformedCorners[0] - transformedCorners[1]),
            Magnitude(transformedCorners[1] - transformedCorners[2]),
            Magnitude(transformedCorners[2] - transformedCorners[3]),
            Magnitude(transformedCorners[3] - transformedCorners[0])
        };

        return std::max(    std::max(screenSpaceLengths[0], screenSpaceLengths[1]),
                            std::max(screenSpaceLengths[2], screenSpaceLengths[3]));
    }

    struct TileConstants
    {
    public:
        Float4x4 _localToCell;
        Int3 _heightMapOrigin; float _dummy;
        Float4 _coverageTexCoordMins, _coverageTexCoordMaxs;
        Int3 _coverageOrigin;
        int _tileDimensionsInVertices;
        Int4 _neighbourLodDiffs;
    };

    class TerrainMaterialTextures
    {
    public:
        enum Resources { Diffuse, Normal, Specularity, ResourceCount };
        intrusive_ptr<ID3D::Resource> _textureArray[ResourceCount];
        RenderCore::Metal::ShaderResourceView _srv[ResourceCount];
        RenderCore::Metal::ConstantBuffer _texturingConstants;
        unsigned _strataCount;
        
        TerrainMaterialTextures();
        TerrainMaterialTextures(const TerrainMaterialScaffold& scaffold);
        ~TerrainMaterialTextures();

        const Assets::DependencyValidation& GetDependencyValidation() const   { return *_validationCallback; }
    private:
        std::shared_ptr<Assets::DependencyValidation>  _validationCallback;
    };

    class TerrainRenderingContext
    {
    public:
        ConstantBuffer  _tileConstantsBuffer;
        ConstantBuffer  _localTransformConstantsBuffer;
        ViewportDesc    _currentViewport;
        unsigned        _indexDrawCount;

        bool            _isTextured;
        Int2            _elementSize;
        bool            _dynamicTessellation;
        
        struct QueuedNode
        {
            TerrainCellRenderer::CellRenderInfo* _cell;
            unsigned    _fieldIndex;
            unsigned    _absNodeIndex;
            float       _priority;
            Float4x4    _cellToWorld;
            int8        _neighbourLODDiff[4];   // top, left, bottom, right

            struct Flags { enum Enum { HasValidData = 1<<0, NeedsHeightMapUpload = 1<<1, NeedsCoverageUpload0 = 1<<2, NeedsCoverageUpload1 = 1<<3, NeedsCoverageUpload2 = 1<<4, NeedsCoverageUpload3 = 1<<5 }; typedef unsigned BitField; };
            Flags::BitField _flags;
        };
        static std::vector<QueuedNode> _queuedNodes;        // HACK -- static to avoid allocation!

        TerrainRenderingContext(bool isTextured, Int2 elementSize);

        enum Mode { Mode_Normal, Mode_RayTest, Mode_VegetationPrepare };

        void    EnterState(DeviceContext* context, LightingParserContext& parserContext, const TerrainMaterialTextures& materials, Mode mode = Mode_Normal);
        void    ExitState(DeviceContext* context, LightingParserContext& parserContext);
    };

    std::vector<TerrainRenderingContext::QueuedNode> TerrainRenderingContext::_queuedNodes;        // HACK -- static to avoid allocation!

    TerrainRenderingContext::TerrainRenderingContext(bool isTextured, Int2 elementSize)
    : _currentViewport(0.f, 0.f, 0.f, 0.f, 0.f, 0.f)
    {
        _indexDrawCount = 0;
        _isTextured = isTextured;
        _elementSize = elementSize;
        _dynamicTessellation = false;

        ConstantBuffer tileConstantsBuffer(nullptr, sizeof(TileConstants));
        ConstantBuffer localTransformConstantsBuffer(nullptr, sizeof(Techniques::LocalTransformConstants));

        _tileConstantsBuffer = std::move(tileConstantsBuffer);
        _localTransformConstantsBuffer = std::move(localTransformConstantsBuffer);
    }

    class TerrainRenderingResources
    {
    public:
        class Desc
        {
        public:
            TerrainRenderingContext::Mode _mode;
            bool _doExtraSmoothing, _noisyTerrain, _isTextured;
            bool _drawWireframe;
            unsigned _strataCount;

            Desc(   TerrainRenderingContext::Mode mode,
                    bool doExtraSmoothing, bool noisyTerrain, bool isTextured,
                    bool drawWireframe, unsigned strataCount)
            {
                std::fill((uint8*)this, (uint8*)PtrAdd(this, sizeof(*this)), 0);
                _mode = mode;
                _doExtraSmoothing = doExtraSmoothing;
                _noisyTerrain = noisyTerrain;
                _isTextured = isTextured;
                _drawWireframe = drawWireframe;
                _strataCount = strataCount;
            }
        };

        const DeepShaderProgram* _shaderProgram;
        RenderCore::Metal::BoundUniforms _boundUniforms;

        TerrainRenderingResources(const Desc& desc);

        const Assets::DependencyValidation& GetDependencyValidation() const   { return *_validationCallback; }
    private:
        std::shared_ptr<Assets::DependencyValidation>  _validationCallback;
    };

    TerrainRenderingResources::TerrainRenderingResources(const Desc& desc)
    {
        char definesBuffer[256];
        _snprintf_s(definesBuffer, _TRUNCATE, 
            "DO_EXTRA_SMOOTHING=%i;SOLIDWIREFRAME_TEXCOORD=%i;DO_ADD_NOISE=%i;OUTPUT_WORLD_POSITION=1;SOLIDWIREFRAME_WORLDPOSITION=1;DRAW_WIREFRAME=%i;STRATA_COUNT=%i", 
            int(desc._doExtraSmoothing), int(desc._isTextured), int(desc._noisyTerrain), int(desc._drawWireframe), desc._strataCount);
        const char* ps = desc._isTextured ? "game/xleres/forward/terrain_generator.sh:ps_main:ps_*" : "game/xleres/solidwireframe.psh:main:ps_*";

        if (Tweakable("LightingModel", 0) == 1 && desc._isTextured) {
                // manually switch to the forward shading pixel shader depending on the lighting model
            ps = "game/xleres/forward/terrain_generator.sh:ps_main_forward:ps_*";
        }

        InputElementDesc eles[] = {
            InputElementDesc("INTERSECTION", 0, NativeFormat::R32G32B32A32_FLOAT)
        };

        const char* gs;
        if (desc._mode == TerrainRenderingContext::Mode_RayTest) {
            ps = "";
            unsigned strides = sizeof(float)*4;
            GeometryShader::SetDefaultStreamOutputInitializers(
                GeometryShader::StreamOutputInitializers(eles, dimof(eles), &strides, 1));
            gs = "game/xleres/forward/terrain_generator.sh:gs_intersectiontest:gs_*";
        } else if (desc._mode == TerrainRenderingContext::Mode_VegetationPrepare) {
            ps = "";
            gs = "game/xleres/Vegetation/InstanceSpawn.gsh:main:gs_*";
        } else {
            gs = "game/xleres/solidwireframe.gsh:main:gs_*";
        }

        const DeepShaderProgram* shaderProgram;
        TRY {
            shaderProgram = &Assets::GetAssetDep<DeepShaderProgram>(
                "game/xleres/forward/terrain_generator.sh:vs_dyntess_main:vs_*", 
                gs, ps, 
                "game/xleres/forward/terrain_generator.sh:hs_main:hs_*",
                "game/xleres/forward/terrain_generator.sh:ds_main:ds_*",
                definesBuffer);
        } CATCH (...) {
            GeometryShader::SetDefaultStreamOutputInitializers(GeometryShader::StreamOutputInitializers());
            throw;
        } CATCH_END

        if (desc._mode == TerrainRenderingContext::Mode_RayTest) {
            GeometryShader::SetDefaultStreamOutputInitializers(GeometryShader::StreamOutputInitializers());
        }

        BoundUniforms boundUniforms(*shaderProgram);
        Techniques::TechniqueContext::BindGlobalUniforms(boundUniforms);

        auto validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(validationCallback, &shaderProgram->GetDependencyValidation());

        _shaderProgram = shaderProgram;
        _boundUniforms = std::move(boundUniforms);
        _validationCallback = std::move(validationCallback);
    }

    void        TerrainRenderingContext::EnterState(DeviceContext* context, LightingParserContext& parserContext, const TerrainMaterialTextures& texturing, Mode mode)
    {
        _dynamicTessellation = Tweakable("TerrainDynamicTessellation", true);
        if (_dynamicTessellation) {
            const bool doExtraSmoothing = Tweakable("TerrainExtraSmoothing", false);
            const bool noisyTerrain = Tweakable("TerrainNoise", false);
            const bool drawWireframe = Tweakable("TerrainWireframe", false);

            auto& box = Techniques::FindCachedBoxDep<TerrainRenderingResources>(
                TerrainRenderingResources::Desc(mode, doExtraSmoothing, noisyTerrain, _isTextured, drawWireframe, texturing._strataCount));

            context->Bind(*box._shaderProgram);
            context->Bind(Topology::PatchList4);
            box._boundUniforms.Apply(*context, parserContext.GetGlobalUniformsStream(), UniformsStream());

                //  when using dynamic tessellation, the basic geometry should just be
                //  a quad. We'll use a vertex generator shader.
        } else {
            const ShaderProgram* shaderProgram;
            if (mode == Mode_Normal) {
                shaderProgram = &Assets::GetAssetDep<ShaderProgram>(
                    "game/xleres/forward/terrain_generator.sh:vs_main:vs_*", 
                    "game/xleres/solidwireframe.gsh:main:gs_*", 
                    "game/xleres/solidwireframe.psh:main:ps_*", "");
            } else if (mode == Mode_VegetationPrepare) {
                shaderProgram = &Assets::GetAssetDep<ShaderProgram>(
                    "game/xleres/forward/terrain_generator.sh:vs_main:vs_*", 
                    "game/xleres/Vegetation/InstanceSpawn.gsh:main:gs_*", 
                    "", "OUTPUT_WORLD_POSITION=1");
            } else {
                shaderProgram = &Assets::GetAssetDep<ShaderProgram>(
                    "game/xleres/forward/terrain_generator.sh:vs_main:vs_*", 
                    "game/xleres/forward/terrain_generator.sh:gs_intersectiontest:gs_*", 
                    "", "OUTPUT_WORLD_POSITION=1");
            }

            context->Bind(*shaderProgram);

            BoundUniforms uniforms(*shaderProgram);
            Techniques::TechniqueContext::BindGlobalUniforms(uniforms);
            uniforms.Apply(*context, parserContext.GetGlobalUniformsStream(), UniformsStream());

            auto& simplePatchBox = Techniques::FindCachedBox<SimplePatchBox>(
                SimplePatchBox::Desc(_elementSize[0], _elementSize[1], true));
            context->Bind(simplePatchBox._simplePatchIndexBuffer, NativeFormat::R32_UINT);
            context->Bind(RenderCore::Metal::Topology::TriangleList);
            _indexDrawCount = simplePatchBox._simplePatchIndexCount;
        }

            ////-////-/====/-////-////
        context->Unbind<VertexBuffer>();
        context->Unbind<BoundInputLayout>();
            ////-////-/====/-////-////

        auto& perlinNoiseRes = Techniques::FindCachedBox<PerlinNoiseResources>(PerlinNoiseResources::Desc());
        context->BindVS(MakeResourceList(12, perlinNoiseRes._gradShaderResource, perlinNoiseRes._permShaderResource));
        context->BindDS(MakeResourceList(12, perlinNoiseRes._gradShaderResource, perlinNoiseRes._permShaderResource));
        context->BindVS(MakeResourceList(7, _tileConstantsBuffer));
        context->BindDS(MakeResourceList(7, _tileConstantsBuffer));
        context->BindHS(MakeResourceList(7, _tileConstantsBuffer));
        context->BindPS(MakeResourceList(7, _tileConstantsBuffer));
        context->BindVS(MakeResourceList(1, _localTransformConstantsBuffer));
        context->BindDS(MakeResourceList(1, _localTransformConstantsBuffer));

        context->Bind(Techniques::CommonResources()._dssReadWrite);

        if (mode == Mode_VegetationPrepare) {
            context->BindGS(MakeResourceList(7, _tileConstantsBuffer));
        }
    }

    void    TerrainRenderingContext::ExitState(DeviceContext* context, LightingParserContext& )
    {
        context->Unbind<HullShader>();
        context->Unbind<DomainShader>();
        context->Bind(Topology::TriangleList);
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    void    TerrainCellRenderer::CullNodes( 
        DeviceContext* context, LightingParserContext& parserContext, 
        TerrainRenderingContext& terrainContext, TerrainCollapseContext& collapseContext,
        const TerrainCellId& cell)
    {
            // Cull on a cell level (prevent loading of distance cell resources)
            //      todo -- if we knew the cell min/max height, we could do this more accurately
        if (CullAABB_Aligned(AsFloatArray(parserContext.GetProjectionDesc()._worldToProjection), cell._aabbMin, cell._aabbMax))
            return;

            // look for a valid "CellRenderInfo" already in our cache
            //  Note that we have a very flexible method for how cells are addressed
            //  see the comments in PlacementsRenderer::Render for more information on 
            //  this. It means we can overlapping cells, or switch cells in and our as
            //  time or situation changes.
        auto hash = cell.BuildHash();
        auto i = std::lower_bound(
            _renderInfos.begin(), _renderInfos.end(), 
            hash, CompareFirst<uint64, std::unique_ptr<CellRenderInfo>>());

        CellRenderInfo* renderInfo = nullptr;
        if (i != _renderInfos.end() && i->first == hash) {

                // if it's been invalidated on disk, reload
            bool invalidation = false;
            if (i->second->_sourceCell) {
                invalidation |= (i->second->_sourceCell->GetDependencyValidation().GetValidationIndex()!=0);
                for (unsigned c=0; c<TerrainCellId::CoverageCount; ++c)
                    invalidation |= (i->second->_sourceCoverage[c]->GetDependencyValidation().GetValidationIndex()!=0);
            }
            if (invalidation) {
                    // before we delete it, we need to erase it from the pending uploads
                for (;;) {
                    auto pi = std::find_if(_pendingHeightMapUploads.begin(), _pendingHeightMapUploads.end(), [=](const UploadPair& p) { return p.first == i->second.get(); });
                    if (pi == _pendingHeightMapUploads.end()) break;
                    _pendingHeightMapUploads.erase(pi);
                }
                for (;;) {
                    auto pi = std::find_if(_pendingCoverageUploads.begin(), _pendingCoverageUploads.end(), [=](const UploadPair& p) { return p.first == i->second.get(); });
                    if (pi == _pendingCoverageUploads.end()) break;
                    _pendingCoverageUploads.erase(pi);
                }
                i->second.reset();

                TerrainCellTexture const* tex[TerrainCellId::CoverageCount];
                for (unsigned c=0; c<TerrainCellId::CoverageCount; ++c) {
                    tex[c] = &_ioFormat->LoadCoverage(cell._coverageFilename[c]);
                }

                i->second = std::make_unique<CellRenderInfo>(std::ref(_ioFormat->LoadHeights(cell._heightMapFilename)), tex);
            }

            renderInfo = i->second.get();

        } else {

            TerrainCellTexture const* tex[TerrainCellId::CoverageCount];
            for (unsigned c=0; c<TerrainCellId::CoverageCount; ++c) {
                tex[c] = &_ioFormat->LoadCoverage(cell._coverageFilename[c]);
            }
            auto newRenderInfo = std::make_unique<CellRenderInfo>(std::ref(_ioFormat->LoadHeights(cell._heightMapFilename)), tex);
            auto newIterator = _renderInfos.insert(i, CRIPair(hash, std::move(newRenderInfo)));
            assert(newIterator->first == hash);
            renderInfo = newIterator->second.get();

        }

            // find all of the nodes that we're go to render this frame
        static bool useNewCulling = true;
        if (!useNewCulling) {
            CullNodes(
                context, parserContext, terrainContext, 
                *renderInfo, cell._cellToWorld);
        } else {
            CullNodes(
                terrainContext, collapseContext,
                parserContext.GetProjectionDesc()._worldToProjection,
                ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld),
                *renderInfo, cell._cellToWorld);
        }
    }

    void        TerrainCellRenderer::CompletePendingUploads()
    {
            // note; ideally we want an erase/remove that will reorder the vector here... That is on erase,
            // just do a std::swap between the erased item and the last item.
            // that should be most efficient when we complete multiple operations per frame (which will be normal)
        {
            auto i = std::remove_if(_pendingHeightMapUploads.begin(), _pendingHeightMapUploads.end(),
                    [=](const UploadPair& p) { return p.first->_nodes[p.second].CompleteHeightMapUpload(_heightMapTileSet->GetBufferUploads()); });
            if (i != _pendingHeightMapUploads.end())
                _pendingHeightMapUploads.erase(i);
        }

        {
            auto i = std::remove_if(_pendingCoverageUploads.begin(), _pendingCoverageUploads.end(),
                    [=](const UploadPair& p) { return p.first->_nodes[p.second].CompleteCoverageUpload(_coverageTileSet[0]->GetBufferUploads()); });
            if (i != _pendingCoverageUploads.end())
                _pendingCoverageUploads.erase(i);
        }
    }

    void        TerrainCellRenderer::QueueUploads(TerrainRenderingContext& terrainContext)
    {
            //  After we've culled the list of nodes we need for this frame, let's queue all of the uploads that
            //  need to occur. Sort the list by priority so that if we run out of upload slots, the most important
            //  nodes will be processed first.
        std::sort(
            terrainContext._queuedNodes.begin(), terrainContext._queuedNodes.end(),
            [](const TerrainRenderingContext::QueuedNode& lhs, const TerrainRenderingContext::QueuedNode& rhs)
                { return lhs._priority < rhs._priority; });

        // queue new uploads
        static unsigned frameUploadLimit = 500;
        static unsigned totalActiveUploadLimit = 1000;

        unsigned uploadsThisFrame = 0;
        typedef TerrainRenderingContext::QueuedNode::Flags Flags;
        for (auto i = terrainContext._queuedNodes.begin(); i!=terrainContext._queuedNodes.end(); ++i) {
            if (uploadsThisFrame >= frameUploadLimit)
                break;
            if ((_pendingHeightMapUploads.size() + _pendingCoverageUploads.size()) >= totalActiveUploadLimit)
                break;

            if (i->_flags & Flags::NeedsHeightMapUpload) {
                auto& cellRenderInfo = *i->_cell;
                auto& sourceCell = *cellRenderInfo._sourceCell;
                unsigned n = i->_absNodeIndex;
                auto& renderNode = cellRenderInfo._nodes[n];
                auto& sourceNode = sourceCell._nodes[n];
                renderNode.QueueHeightMapUpload(
                    *_heightMapTileSet, 
                    cellRenderInfo._heightMapStreamingFilePtr, cellRenderInfo._heightMapCacheStreamingFilePtr, *sourceNode);
                ++uploadsThisFrame;
                _pendingHeightMapUploads.push_back(UploadPair(&cellRenderInfo, n));
            }
            
            for (unsigned c=0; c<TerrainCellId::CoverageCount; ++c) {
                auto& cellRenderInfo = *i->_cell;
                auto n = i->_absNodeIndex;
                bool anyCoverageUploads = false;
                if (i->_flags & (Flags::NeedsCoverageUpload0<<c)) {
                    auto& sourceCoverage = *cellRenderInfo._sourceCoverage[c];
                    auto& renderNode = cellRenderInfo._nodes[n];
                    renderNode.QueueCoverageUpload(
                        c, *_coverageTileSet[c], 
                        cellRenderInfo._coverageStreamingFilePtr[c], 
                        sourceCoverage._nodeFileOffsets[n],
                        sourceCoverage._nodeTextureByteCount);
                    ++uploadsThisFrame;
                    anyCoverageUploads = true;
                }

                if (anyCoverageUploads) {
                    _pendingCoverageUploads.push_back(UploadPair(&cellRenderInfo, n));
                }
            }
        }
    }

    class TerrainCollapseContext
    {
    public:
        static const unsigned MaxLODLevels = 6;
        class NodeID
        {
        public:
            unsigned _lodField;
            unsigned _nodeId;
            unsigned _cellId;

            explicit NodeID(unsigned lodField=~unsigned(0x0), unsigned nodeId=~unsigned(0x0), 
                            unsigned cellId=~unsigned(0x0))
                : _lodField(lodField), _nodeId(nodeId), _cellId(cellId) {}

            bool Equivalent(const NodeID& other) const 
            {
                    // we're equivalent for the same cell and node id (ie, referencing exactly the same node)
                return _cellId == other._cellId && _nodeId == other._nodeId; 
            }
        };

        struct Neighbours
        {
            enum Enum 
            {
                    //  For each edge, we record two neighbours.
                    //  But, of course, both slots for the same
                    //  edge might reference the same neighbour.
                TopEdgeLeft,        TopEdgeRight,
                RightEdgeTop,       RightEdgeBottom,
                BottomEdgeRight,    BottomEdgeLeft,
                LeftEdgeBottom,     LeftEdgeTop,
                Count
            };
        };

        class Node
        {
        public:
            NodeID _id;
            bool _entirelyWithinFrustum;
            bool _lodPromoted;
            float _screenSpaceEdgeLength;
            NodeID _neighbours[Neighbours::Count];
            explicit Node(NodeID id) : _id(id), _entirelyWithinFrustum(false), _lodPromoted(false), _screenSpaceEdgeLength(FLT_MAX) {}
        };

        std::vector<Node> _activeNodes[MaxLODLevels];
        std::vector<Float4x4> _cellToWorlds;
        std::vector<TerrainCellRenderer::CellRenderInfo*> _cells;

        std::vector<Float4x4> _cellToProjection;
        std::vector<Float3> _cellPositionMinusViewPosition;

        void AddNode(const Node& node) { _activeNodes[node._id._lodField].push_back(node); }
        unsigned _startLod;
        float _screenSpaceEdgeThreshold;

        void AttemptLODPromote(unsigned startLod, TerrainRenderingContext& renderingContext);

    private:
        auto FindNeighbour(const NodeID& id, std::vector<Node>& workingField, unsigned workingFieldLOD) -> Node*;
    };

    static Int2 ToFieldXY(const TerrainCell::NodeField& field, unsigned nodeIndex)
    {
        signed fieldx = signed(nodeIndex) % field._widthInNodes;
        signed fieldy = signed(nodeIndex) / field._widthInNodes;
        return Int2(fieldx, fieldy);
    }

    static unsigned ToNodeIndex(const TerrainCell::NodeField& field, Int2 xy)
    {
        if (xy[0] < 0 || xy[1] < 0 || xy[0] >= signed(field._widthInNodes) || xy[1] >= signed(field._heightInNodes)) {
            return ~unsigned(0x0);
        }
        return xy[1] * field._widthInNodes + xy[0];
    }

    auto TerrainCollapseContext::FindNeighbour(const NodeID& id, std::vector<Node>& workingField, unsigned workingFieldLOD) -> Node*
    {
        if (id._nodeId == ~unsigned(0x0) || id._lodField == ~unsigned(0x0)) { return nullptr; }

        if (id._lodField == workingFieldLOD) {
            auto i = std::find_if(workingField.begin(), workingField.end(),
                [&](const Node& n) { return n._id.Equivalent(id); });
            if (i != workingField.end()) {
                return AsPointer(i);
            } else {
                return nullptr;
            }
        } else {
            auto& field = _activeNodes[id._lodField];
            auto i = std::find_if(field.begin(), field.end(),
                [&](const Node& n) { return n._id.Equivalent(id); });
            if (i != field.end()) {
                return AsPointer(i);
            } else {
                return nullptr;
            }
        }
    }

    void TerrainCollapseContext::AttemptLODPromote(unsigned startLod, TerrainRenderingContext& renderingContext)
    {
            //  Attempt to collapse the nodes in the given start LOD
            //  Some of the nodes will be shifted to the next highest LOD
            //  (but only if they meet the restrictions & requirements)
            //
            //  Every time we shift nodes, we have to update the neighbour
            //  references.

        std::vector<Node> collapsedField;
        for (auto n=_activeNodes[startLod].begin(); n!=_activeNodes[startLod].end(); ++n) {

            auto& sourceCell = *_cells[n->_id._cellId]->_sourceCell;
            if (sourceCell._nodeFields.size() <= (n->_id._lodField+1)) { continue; }        // not collapsible, because no high levels of detail
            auto& field = sourceCell._nodeFields[n->_id._lodField];

            bool doCollapse = false;
            if (n->_screenSpaceEdgeLength > _screenSpaceEdgeThreshold) {
                    //  Try to drop to a lower lod.
                    //  Currently, we can only do this if all of our neighbours are at
                    //  a equal or lower LOD (otherwise that would mean having more than 2
                    //  neighbours on an edge)
                    //  When there is no neighbour, we want this test to succeeded...
                    //      -- note; there is a problem if the neighbour has just been culled
                    //              this frame. The neighbour may have been prevent a LOD
                    //              promote; but as soon as it is no visible, the promote will
                    //              occur. It may appear too sudden and weird.
                doCollapse = 
                        n->_neighbours[ Neighbours::TopEdgeLeft     ]._lodField >= startLod
                    &&  n->_neighbours[ Neighbours::RightEdgeTop    ]._lodField >= startLod
                    &&  n->_neighbours[ Neighbours::BottomEdgeRight ]._lodField >= startLod
                    &&  n->_neighbours[ Neighbours::LeftEdgeBottom  ]._lodField >= startLod;
            }

            if (doCollapse) {
                    //  mark this node as collapsed, and then 
                    //  create 4 new nodes, at high levels of detail
                n->_lodPromoted = true;

                auto baseXY = ToFieldXY(field, n->_id._nodeId-field._nodeBegin);
                auto& newField = sourceCell._nodeFields[n->_id._lodField+1];
                auto ni0 = ToNodeIndex(newField, baseXY*2 + Int2(0,0)) + newField._nodeBegin;
                auto ni1 = ToNodeIndex(newField, baseXY*2 + Int2(1,0)) + newField._nodeBegin;
                auto ni2 = ToNodeIndex(newField, baseXY*2 + Int2(0,1)) + newField._nodeBegin;
                auto ni3 = ToNodeIndex(newField, baseXY*2 + Int2(1,1)) + newField._nodeBegin;

                auto f = n->_id._lodField+1;
                Node newNodes[4] = {
                    Node(NodeID(f, ni0, n->_id._cellId)),
                    Node(NodeID(f, ni1, n->_id._cellId)),
                    Node(NodeID(f, ni2, n->_id._cellId)),
                    Node(NodeID(f, ni3, n->_id._cellId))
                };

                {
                    auto& cellToProjection = _cellToProjection[n->_id._cellId];
                        //  We have to do another culling test to see if any of these smaller
                        //  nodes are off-screen
                    for (unsigned c=0; c<dimof(newNodes); ++c) {
                        assert(newNodes[c]._id._nodeId != ~unsigned(0x0));
                        auto& sourceNode = sourceCell._nodes[newNodes[c]._id._nodeId];
                        __declspec(align(16)) auto localToProjection = Combine(sourceNode->_localToCell, cellToProjection);

                            // once a parent node is entirely within the frustum, so to must be all children
                        if (n->_entirelyWithinFrustum) {
                            auto aabbTest = TestAABB_Aligned(AsFloatArray(localToProjection), Float3(0.f, 0.f, 0.f), Float3(1.f, 1.f, float(0xffff)));
                            if (aabbTest == AABBIntersection::Culled) { 
                                newNodes[c]._id._nodeId = ~unsigned(0x0);
                                continue; 
                            }

                            newNodes[c]._entirelyWithinFrustum = aabbTest == AABBIntersection::Within;
                        } else {
                            newNodes[c]._entirelyWithinFrustum = true;
                        }

                        if ((f+1) < sourceCell._nodeFields.size()) {
                            newNodes[c]._screenSpaceEdgeLength = CalculateScreenSpaceEdgeLength(
                                localToProjection, renderingContext._currentViewport.Width, renderingContext._currentViewport.Height);
                        } else {
                            newNodes[c]._screenSpaceEdgeLength = FLT_MAX;
                        }
                    }

                        //  Setup the neighbours for the new nodes
                        //  Do this after culling, so we don't have neighbour references to
                        //  nodes that were culled out.
                        // (internal)
                    newNodes[0]._neighbours[Neighbours::RightEdgeTop] = newNodes[0]._neighbours[Neighbours::RightEdgeBottom] = newNodes[1]._id;
                    newNodes[0]._neighbours[Neighbours::BottomEdgeLeft] = newNodes[0]._neighbours[Neighbours::BottomEdgeRight] = newNodes[2]._id;

                    newNodes[1]._neighbours[Neighbours::LeftEdgeTop] = newNodes[1]._neighbours[Neighbours::LeftEdgeBottom] = newNodes[0]._id;
                    newNodes[1]._neighbours[Neighbours::BottomEdgeLeft] = newNodes[1]._neighbours[Neighbours::BottomEdgeRight] = newNodes[3]._id;

                    newNodes[2]._neighbours[Neighbours::RightEdgeTop] = newNodes[2]._neighbours[Neighbours::RightEdgeBottom] = newNodes[3]._id;
                    newNodes[2]._neighbours[Neighbours::TopEdgeLeft] = newNodes[2]._neighbours[Neighbours::TopEdgeRight] = newNodes[0]._id;

                    newNodes[3]._neighbours[Neighbours::LeftEdgeTop] = newNodes[3]._neighbours[Neighbours::LeftEdgeBottom] = newNodes[2]._id;
                    newNodes[3]._neighbours[Neighbours::TopEdgeLeft] = newNodes[3]._neighbours[Neighbours::TopEdgeRight] = newNodes[1]._id;

                        // (external)
                    newNodes[0]._neighbours[Neighbours::TopEdgeLeft] = newNodes[0]._neighbours[Neighbours::TopEdgeRight] = n->_neighbours[Neighbours::TopEdgeLeft];
                    newNodes[0]._neighbours[Neighbours::LeftEdgeTop] = newNodes[0]._neighbours[Neighbours::LeftEdgeBottom] = n->_neighbours[Neighbours::LeftEdgeTop];

                    newNodes[1]._neighbours[Neighbours::TopEdgeLeft] = newNodes[1]._neighbours[Neighbours::TopEdgeRight] = n->_neighbours[Neighbours::TopEdgeRight];
                    newNodes[1]._neighbours[Neighbours::RightEdgeTop] = newNodes[1]._neighbours[Neighbours::RightEdgeBottom] = n->_neighbours[Neighbours::RightEdgeTop];

                    newNodes[2]._neighbours[Neighbours::BottomEdgeLeft] = newNodes[2]._neighbours[Neighbours::BottomEdgeRight] = n->_neighbours[Neighbours::BottomEdgeLeft];
                    newNodes[2]._neighbours[Neighbours::LeftEdgeTop] = newNodes[2]._neighbours[Neighbours::LeftEdgeBottom] = n->_neighbours[Neighbours::LeftEdgeBottom];

                    newNodes[3]._neighbours[Neighbours::RightEdgeTop] = newNodes[3]._neighbours[Neighbours::RightEdgeBottom] = n->_neighbours[Neighbours::RightEdgeBottom];
                    newNodes[3]._neighbours[Neighbours::BottomEdgeLeft] = newNodes[3]._neighbours[Neighbours::BottomEdgeRight] = n->_neighbours[Neighbours::BottomEdgeRight];

                        // commit...
                    for (unsigned c=0; c<dimof(newNodes); ++c) {
                        if (newNodes[c]._id._nodeId != ~unsigned(0x0)) {
                            collapsedField.push_back(newNodes[c]);
                        }
                    }
                }

                    //  Update the neighbour details of attached nodes.
                    //  There's only two possibilities currently 
                    //      -- our neighbour is at the same LOD level we started at
                    //      -- our neighbour is at the same LOD we ended at
                    //  it should be make it simplier.
                Neighbours::Enum mirrorNeighbours[] = 
                {
                    Neighbours::BottomEdgeLeft,         // <--> TopEdgeLeft
                    Neighbours::BottomEdgeRight,        // <--> TopEdgeRight
                    Neighbours::LeftEdgeTop,            // <--> RightEdgeTop
                    Neighbours::LeftEdgeBottom,         // <--> RightEdgeBottom
                    Neighbours::TopEdgeRight,           // <--> BottomEdgeRight
                    Neighbours::TopEdgeLeft,            // <--> BottomEdgeLeft
                    Neighbours::RightEdgeBottom,        // <--> LeftEdgeBottom
                    Neighbours::RightEdgeTop            // <--> LeftEdgeTop
                };
                unsigned attachSubNode[] =  { 0, 1, 1, 3, 3, 2, 2, 0 };
                for (unsigned c=0; c<Neighbours::Count; ++c) {
                    auto* node = FindNeighbour(n->_neighbours[c], collapsedField, startLod+1);
                    if (!node) continue;

                    assert(node->_neighbours[mirrorNeighbours[c]].Equivalent(n->_id));
                    if (node->_id._lodField == startLod) {
                        // neighbour is so far uncollapsed. So we just update as needed
                        node->_neighbours[mirrorNeighbours[c]] = newNodes[attachSubNode[c]]._id;
                    } else {
                        // neighbour is already collapsed. So we need to update 2 neighbour references
                        // there.
                        unsigned n0 = mirrorNeighbours[c] & ~0x1;
                        unsigned n1 = n0 + 1;
                        node->_neighbours[n0] = newNodes[attachSubNode[c]]._id;
                        node->_neighbours[n1] = newNodes[attachSubNode[c]]._id;
                    }
                }
            }

        }

        assert(_activeNodes[startLod+1].empty());
        _activeNodes[startLod+1] = std::move(collapsedField);
    }

    void TerrainCellRenderer::WriteQueuedNodes(TerrainRenderingContext& renderingContext, TerrainCollapseContext& collapseContext)
    {
        // After calculating the correct LOD level and neighbours for each cell, we need to do 2 final things
        //      * queue texture updates
        //      * queue the node for actual rendering

        for (unsigned c=0; c<TerrainCollapseContext::MaxLODLevels; ++c) {
            for (auto n=collapseContext._activeNodes[c].cbegin(); n!=collapseContext._activeNodes[c].cend(); ++n) {

                if (n->_lodPromoted) { continue; }        // collapsed into larger LOD

                auto& cellRenderInfo = *collapseContext._cells[n->_id._cellId];
                auto& sourceCell = *cellRenderInfo._sourceCell;
                auto& sourceCoverage = *cellRenderInfo._sourceCoverage;
                auto& sourceNode = sourceCell._nodes[n->_id._nodeId];
                auto& renderNode = cellRenderInfo._nodes[n->_id._nodeId];

                typedef TerrainRenderingContext::QueuedNode::Flags Flags;
                Flags::BitField flags = 0;
                bool validHeightMap = _heightMapTileSet->IsValid(renderNode._heightMapTile);
                bool validCoverage = true;
                for (unsigned c=0; c<TerrainCellId::CoverageCount; ++c)
                    validCoverage &= _coverageTileSet[c]->IsValid(renderNode._coverageTile[c]);
                flags |= Flags::HasValidData * unsigned(validHeightMap);    // (we can render without valid coverage)

                    //  if there's no valid data, and no currently pending data, we need to queue a 
                    //  new upload
                if (!validHeightMap && !_heightMapTileSet->IsValid(renderNode._heightMapPendingTile))
                    flags |= Flags::NeedsHeightMapUpload;

                if (!validCoverage) {
                        // some tiles don't have any coverage information. 
                    for (unsigned c=0; c<TerrainCellId::CoverageCount; ++c) {
                        if (n->_id._lodField < sourceCoverage[c]._fieldCount && sourceCoverage[c]._nodeFileOffsets[n->_id._nodeId] != ~unsigned(0x0))
                            if (!_coverageTileSet[c]->IsValid(renderNode._coveragePendingTile[c]))
                                flags |= Flags::NeedsCoverageUpload0<<c;
                    }
                }

                auto& minusViewPosition = collapseContext._cellPositionMinusViewPosition[n->_id._cellId];
                auto& cellToWorld = collapseContext._cellToWorlds[n->_id._cellId];

                TerrainRenderingContext::QueuedNode queuedNode;
                queuedNode._cell = &cellRenderInfo;
                queuedNode._fieldIndex = n->_id._lodField;
                queuedNode._absNodeIndex = n->_id._nodeId;
                auto worldSpaceMean = Truncate(cellToWorld * (sourceNode->_localToCell * Float4(0.5f, 0.5f, 0.f, 1.f)));
                queuedNode._priority = MagnitudeSquared(worldSpaceMean + minusViewPosition);
                queuedNode._flags = flags;
                queuedNode._cellToWorld = cellToWorld;    // note -- it's a pity we have to store this for every node (it's a per-cell property)

                    // calculate LOD differentials for neighbouring nodes
                for (unsigned c=0; c<4; ++c) {
                    auto& n0 = n->_neighbours[c*2 + 0];
                    auto& n1 = n->_neighbours[c*2 + 1];
                    signed diff = 0;
                    if (n0._nodeId != ~unsigned(0x0)) {
                        diff = signed(n0._lodField) - signed(n->_id._lodField);
                        if (n1._nodeId != ~unsigned(0x0) && XlAbs(signed(n1._lodField) - signed(n->_id._lodField)) > XlAbs(diff)) {
                            diff = signed(n1._lodField) - signed(n->_id._lodField);
                        }
                    } else if (n1._nodeId != ~unsigned(0x0)) {
                        diff = signed(n1._lodField) - signed(n->_id._lodField);
                    }
                    queuedNode._neighbourLODDiff[c] = (int8)diff;
                }

                renderingContext._queuedNodes.push_back(queuedNode);

            }
        }
    }

    void TerrainCellRenderer::CullNodes(
        TerrainRenderingContext& terrainContext, TerrainCollapseContext& collapseContext,
        const Float4x4& worldToProjection, const Float3& viewPositionWorld, CellRenderInfo& cellRenderInfo, const Float4x4& cellToWorld)
    {
        if (cellRenderInfo._nodes.empty()) { return; }
        if (cellRenderInfo._heightMapStreamingFilePtr == INVALID_HANDLE_VALUE || cellRenderInfo._coverageStreamingFilePtr == INVALID_HANDLE_VALUE)
            return;

        auto& sourceCell = *cellRenderInfo._sourceCell;
        if (collapseContext._startLod >= sourceCell._nodeFields.size())
            return;

        auto cellToProjection = Combine(cellToWorld, worldToProjection);
        auto& field = sourceCell._nodeFields[collapseContext._startLod];

        unsigned cellId = unsigned(collapseContext._cells.size());
        collapseContext._cellToWorlds.push_back(cellToWorld);
        collapseContext._cellToProjection.push_back(cellToProjection);
        collapseContext._cellPositionMinusViewPosition.push_back(-viewPositionWorld);
        collapseContext._cells.push_back(&cellRenderInfo);
        auto f = collapseContext._startLod;

            //              We need to initialize the collapse context with all of the nodes in this LOD            //
            //      Note that we're just going to add in all of the non-culled nodes, first. We'll calculate the
            //      appropriate LOD levels later.

        std::vector<AABBIntersection::Enum> cullResults;
        std::vector<float> screenSpaceEdgeLengths;
        cullResults.resize(field._nodeEnd - field._nodeBegin);
        screenSpaceEdgeLengths.resize(field._nodeEnd - field._nodeBegin, FLT_MAX);

        for (unsigned n=field._nodeBegin; n<field._nodeEnd; ++n) {
            auto& sourceNode = sourceCell._nodes[n];

            const unsigned expectedDataSize = sourceNode->_widthInElements*sourceNode->_widthInElements*2;
            if (std::max(sourceNode->_heightMapFileSize, sourceNode->_secondaryCacheSize) < expectedDataSize) {
                    // some nodes have "holes". We have to ignore them.
                cullResults[n - field._nodeBegin] = AABBIntersection::Culled;
            } else {
                __declspec(align(16)) auto localToProjection = Combine(sourceNode->_localToCell, cellToProjection);
                cullResults[n - field._nodeBegin] = TestAABB_Aligned(AsFloatArray(localToProjection), Float3(0.f, 0.f, 0.f), Float3(1.f, 1.f, float(0xffff)));
                if (cullResults[n - field._nodeBegin] != AABBIntersection::Culled) {
                    screenSpaceEdgeLengths[n - field._nodeBegin] = CalculateScreenSpaceEdgeLength(
                        localToProjection, terrainContext._currentViewport.Width, terrainContext._currentViewport.Height);
                }
            }
        }

        for (unsigned n=field._nodeBegin; n<field._nodeEnd; ++n) {
            auto cullTest = cullResults[n - field._nodeBegin];
            if (cullTest == AABBIntersection::Culled) { continue; }

            typedef TerrainCollapseContext::NodeID NodeID;
            typedef TerrainCollapseContext::Node Node;
            typedef TerrainCollapseContext::Neighbours Neighbours;

            NodeID nid(f, n, cellId);
            Node node(nid);
            node._entirelyWithinFrustum = cullTest == AABBIntersection::Within;
            node._screenSpaceEdgeLength = screenSpaceEdgeLengths[n - field._nodeBegin];

                //  We can calculate the neighbours within this cell. But neighbours
                //  in other cells become difficult. At this point, all nodes in the
                //  collapse context are at the same LOD (and so should be the same size).
            
            auto baseXY     = ToFieldXY(field, n-field._nodeBegin);
            auto topEdge    = ToNodeIndex(field, baseXY + Int2( 0, -1));
            auto rightEdge  = ToNodeIndex(field, baseXY + Int2( 1,  0));
            auto bottomEdge = ToNodeIndex(field, baseXY + Int2( 0,  1));
            auto leftEdge   = ToNodeIndex(field, baseXY + Int2(-1,  0));
            if ((topEdge != ~unsigned(0x0)) && (cullResults[topEdge] != AABBIntersection::Culled)) {
                node._neighbours[Neighbours::TopEdgeLeft] = NodeID(f, topEdge+field._nodeBegin, cellId);
                node._neighbours[Neighbours::TopEdgeRight] = NodeID(f, topEdge+field._nodeBegin, cellId);
            }
            if ((rightEdge != ~unsigned(0x0)) && (cullResults[rightEdge] != AABBIntersection::Culled)) {
                node._neighbours[Neighbours::RightEdgeTop] = NodeID(f, rightEdge+field._nodeBegin, cellId);
                node._neighbours[Neighbours::RightEdgeBottom] = NodeID(f, rightEdge+field._nodeBegin, cellId);
            }
            if ((bottomEdge != ~unsigned(0x0)) && (cullResults[bottomEdge] != AABBIntersection::Culled)) {
                node._neighbours[Neighbours::BottomEdgeRight] = NodeID(f, bottomEdge+field._nodeBegin, cellId);
                node._neighbours[Neighbours::BottomEdgeLeft] = NodeID(f, bottomEdge+field._nodeBegin, cellId);
            }
            if ((leftEdge != ~unsigned(0x0)) && (cullResults[leftEdge] != AABBIntersection::Culled)) {
                node._neighbours[Neighbours::LeftEdgeBottom] = NodeID(f, leftEdge+field._nodeBegin, cellId);
                node._neighbours[Neighbours::LeftEdgeTop] = NodeID(f, leftEdge+field._nodeBegin, cellId);
            }

            collapseContext.AddNode(node);
        }
    }

    void TerrainCellRenderer::CullNodes( 
        DeviceContext* context, LightingParserContext& parserContext, TerrainRenderingContext& terrainContext,
        CellRenderInfo& cellRenderInfo, const Float4x4& localToWorld)
    {
        if (cellRenderInfo._nodes.empty())
            return;

            // any cells that are missing either the height map or coverage map should just be excluded
        if (cellRenderInfo._heightMapStreamingFilePtr == INVALID_HANDLE_VALUE || cellRenderInfo._coverageStreamingFilePtr == INVALID_HANDLE_VALUE)
            return;

        auto cellToProjection = Combine(localToWorld, parserContext.GetProjectionDesc()._worldToProjection);
        Float3 cellPositionMinusViewPosition = ExtractTranslation(localToWorld) - ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld);

        auto& sourceCell = *cellRenderInfo._sourceCell;
        auto& sourceCoverage = *cellRenderInfo._sourceCoverage;
        const unsigned startLod = Tweakable("TerrainLOD", 1);
        const unsigned maxLod = unsigned(sourceCell._nodeFields.size()-1);      // sometimes the coverage doesn't have all of the LODs. In these cases, we have to clamp the LOD number (for both heights and coverage...!)
        const float screenSpaceEdgeThreshold = Tweakable("TerrainEdgeThreshold", 384.f);
        auto& field = sourceCell._nodeFields[startLod];

            // DavidJ -- HACK -- making this "static" to try to avoid extra memory allocations
        static std::stack<std::pair<unsigned, unsigned>> pendingNodes;
        for (unsigned n=0; n<field._nodeEnd - field._nodeBegin; ++n)
            pendingNodes.push(std::make_pair(startLod, n));

        while (!pendingNodes.empty()) {
            auto nodeRef = pendingNodes.top(); pendingNodes.pop();
            auto& field = sourceCell._nodeFields[nodeRef.first];
            unsigned n = field._nodeBegin + nodeRef.second;

            auto& sourceNode = sourceCell._nodes[n];
            auto& renderNode = cellRenderInfo._nodes[n];

                //  do a culling step first... If the node is completely outside
                //  of the frustum, let's cull it quickly
            const __declspec(align(16)) auto localToProjection = Combine(sourceNode->_localToCell, cellToProjection);
            if (CullAABB_Aligned(AsFloatArray(localToProjection), Float3(0.f, 0.f, 0.f), Float3(1.f, 1.f, float(0xffff)))) {
                continue;
            }

                //   We can choose to render this node at this lod level, or drop down to
                //   a higher quality lod.
                //   Here is the easiest method for deciding lod:
                //        *   If we assume that every node is 33x33, we can try to balance the
                //            size of the quads in each node in screen-space. Let's project
                //            the corners into screen space and find the screen space edge lengths.
                //   Ideally we should take into account curvature of the surface, as well
                //    -- eg: by looking at average screen space error.

            if (nodeRef.first < maxLod) {
                float screenSpaceEdgeLength = CalculateScreenSpaceEdgeLength(
                    localToProjection, terrainContext._currentViewport.Width, terrainContext._currentViewport.Height);
                if (screenSpaceEdgeLength >= screenSpaceEdgeThreshold) {
                        // drop to lower lod... push in the nodes from the lower lod
                    unsigned fieldx = nodeRef.second % field._widthInNodes;
                    unsigned fieldy = nodeRef.second / field._widthInNodes;
                    pendingNodes.push(std::make_pair(nodeRef.first+1, (fieldy*2+0) * (field._widthInNodes*2) + (fieldx*2+0)));
                    pendingNodes.push(std::make_pair(nodeRef.first+1, (fieldy*2+0) * (field._widthInNodes*2) + (fieldx*2+1)));
                    pendingNodes.push(std::make_pair(nodeRef.first+1, (fieldy*2+1) * (field._widthInNodes*2) + (fieldx*2+0)));
                    pendingNodes.push(std::make_pair(nodeRef.first+1, (fieldy*2+1) * (field._widthInNodes*2) + (fieldx*2+1)));
                    continue;
                }
            }
            
            const unsigned expectedDataSize = sourceNode->_widthInElements*sourceNode->_widthInElements*2;
            if (std::max(sourceNode->_heightMapFileSize, sourceNode->_secondaryCacheSize) < expectedDataSize) {
                continue;   // some nodes have "holes". We have to ignore them.
            }

                //  we should check for valid data & required uploads. Mark the flags now, and we'll 
                //  do the processing later.
            typedef TerrainRenderingContext::QueuedNode::Flags Flags;
            Flags::BitField flags = 0;
            bool validHeightMap = _heightMapTileSet->IsValid(renderNode._heightMapTile);
            bool validCoverage = true;
            for (unsigned c=0; c<TerrainCellId::CoverageCount; ++c)
                validCoverage &= _coverageTileSet[c]->IsValid(renderNode._coverageTile[c]);
            flags |= Flags::HasValidData * unsigned(validHeightMap);    // (we can render without valid coverage)

                //  if there's no valid data, and no currently pending data, we need to queue a 
                //  new upload
            if (!validHeightMap && !_heightMapTileSet->IsValid(renderNode._heightMapPendingTile))
                flags |= Flags::NeedsHeightMapUpload;

            if (!validCoverage) {
                    // some tiles don't have any coverage information. 
                for (unsigned c=0; c<TerrainCellId::CoverageCount; ++c) {
                    if (nodeRef.first < sourceCoverage[c]._fieldCount && sourceCoverage[c]._nodeFileOffsets[n] != ~unsigned(0x0))
                        if (!_coverageTileSet[c]->IsValid(renderNode._coveragePendingTile[c]))
                            flags |= Flags::NeedsCoverageUpload0<<c;
                }
            }

            TerrainRenderingContext::QueuedNode queuedNode;
            queuedNode._cell = &cellRenderInfo;
            queuedNode._fieldIndex = nodeRef.first;
            queuedNode._absNodeIndex = n;
            queuedNode._priority = MagnitudeSquared(ExtractTranslation(sourceNode->_localToCell) + cellPositionMinusViewPosition);
            queuedNode._flags = flags;
            queuedNode._cellToWorld = localToWorld;    // note -- it's a pity we have to store this for every node (it's a per-cell property)
            queuedNode._neighbourLODDiff[0] = queuedNode._neighbourLODDiff[1] = queuedNode._neighbourLODDiff[2] = queuedNode._neighbourLODDiff[3] = 0;
            terrainContext._queuedNodes.push_back(queuedNode);
        }
    }

    void        TerrainCellRenderer::Render(    DeviceContext* context, LightingParserContext& parserContext, 
                                                TerrainRenderingContext& terrainContext)
    {
        context->BindVS(MakeResourceList(_heightMapTileSet->GetShaderResource()));
        context->BindDS(MakeResourceList(_heightMapTileSet->GetShaderResource()));
        for (unsigned c=0; c<TerrainCellId::CoverageCount; ++c) {
            context->BindPS(MakeResourceList(c, _coverageTileSet[c]->GetShaderResource()));
                //  for instance spawn mode, we also need the coverage resources in the geometry shader. Perhaps
                //  we could use techniques to make this a little more reliable...?
            context->BindGS(MakeResourceList(c, _coverageTileSet[c]->GetShaderResource()));
        }

            // go through all nodes that were previously queued (with CullNodes) and render them
        TRY {
            CellRenderInfo* currentCell = nullptr;
            for (auto i=terrainContext._queuedNodes.begin(); i!=terrainContext._queuedNodes.end(); ++i)
                if (i->_flags & TerrainRenderingContext::QueuedNode::Flags::HasValidData) {

                        // we only have to upload the local transform when the current cell changes
                        // note that the current culling process is reordering queued nodes by
                        // lod level. So, when a cell is split across multiple LOD levels, we may
                        // switch to a given cell more than once (resulting in multiple uploads of
                        // it's local transform)
                    if (i->_cell != currentCell) {
                        auto localTransform = Techniques::MakeLocalTransform(i->_cellToWorld, Float3(0,0,0));
                        terrainContext._localTransformConstantsBuffer.Update(*context, &localTransform, sizeof(localTransform));
                        currentCell = i->_cell;
                    }

                    RenderNode(context, parserContext, terrainContext, *i->_cell, i->_absNodeIndex, i->_neighbourLODDiff);
                }
        } CATCH (...) { // suppress pending / invalid resources
        } CATCH_END
    }

    void        TerrainCellRenderer::RenderNode(    DeviceContext* context,
                                                    LightingParserContext& parserContext,
                                                    TerrainRenderingContext& terrainContext,
                                                    CellRenderInfo& cellRenderInfo, unsigned absNodeIndex,
                                                    int8 neighbourLodDiffs[4])
    {
        const bool isTextured = cellRenderInfo._coverageStreamingFilePtr != INVALID_HANDLE_VALUE;
        assert(  isTextured == terrainContext._isTextured); (void)isTextured;
        assert(_elementSize == terrainContext._elementSize);

        auto& sourceCell = *cellRenderInfo._sourceCell;
        auto& sourceNode = sourceCell._nodes[absNodeIndex];
        auto& renderNode = cellRenderInfo._nodes[absNodeIndex];

        /////////////////////////////////////////////////////////////////////////////
            //  if we've got some texture data, we can go ahead and
            //  render this object
        assert(renderNode._heightMapTile._width && renderNode._heightMapTile._height);

        TileConstants tileConstants;
        tileConstants._localToCell = sourceNode->_localToCell;
        tileConstants._heightMapOrigin = Int3(renderNode._heightMapTile._x, renderNode._heightMapTile._y, renderNode._heightMapTile._arrayIndex);

        if (renderNode._coverageTile[0]._width != ~unsigned(0x0) && renderNode._coverageTile[0]._height != ~unsigned(0x0)) {
            const unsigned overlap = 1;
            tileConstants._coverageTexCoordMins[0]  = (.5f / 2048.f) + renderNode._coverageTile[0]._x / 2048.f;
            tileConstants._coverageTexCoordMins[1]  = (.5f / 2048.f) + renderNode._coverageTile[0]._y / 2048.f;
            tileConstants._coverageTexCoordMaxs[0]  = (.5f / 2048.f) + (renderNode._coverageTile[0]._x + (renderNode._coverageTile[0]._width-overlap)) / 2048.f;
            tileConstants._coverageTexCoordMaxs[1]  = (.5f / 2048.f) + (renderNode._coverageTile[0]._y + (renderNode._coverageTile[0]._height-overlap)) / 2048.f;
            tileConstants._coverageOrigin           = Int3(renderNode._coverageTile[0]._x, renderNode._coverageTile[0]._y, renderNode._coverageTile[0]._arrayIndex);
        } else {
            tileConstants._coverageTexCoordMins = Float4(0.f, 0.f, 0.f, 0.f);
            tileConstants._coverageTexCoordMaxs = Float4(0.f, 0.f, 0.f, 0.f);
            tileConstants._coverageOrigin = Int3(0, 0, 0);
        }

        tileConstants._tileDimensionsInVertices = _elementSize[1];
        tileConstants._neighbourLodDiffs = Int4(neighbourLodDiffs[0], neighbourLodDiffs[1], neighbourLodDiffs[2], neighbourLodDiffs[3]);
        terrainContext._tileConstantsBuffer.Update(*context, &tileConstants, sizeof(tileConstants));

        /////////////////////////////////////////////////////////////////////////////
            //  If we're using dynamic tessellation, then we have a vertex generator shader with no index buffer,
            //  otherwise we're still using a vertex generator shader, but we are using an index buffer to 
            //  order the triangles.
        if (terrainContext._dynamicTessellation) {
            context->Draw(4);
        } else {
            context->DrawIndexed(terrainContext._indexDrawCount);
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////

    static BufferUploads::BufferDesc RWBufferDesc(unsigned size, unsigned structureSize)
    {
        using namespace BufferUploads;
        BufferDesc result;
        result._type = BufferDesc::Type::LinearBuffer;
        result._bindFlags = BindFlag::UnorderedAccess|BindFlag::StructuredBuffer;
        result._cpuAccess = 0;
        result._gpuAccess = GPUAccess::Read|GPUAccess::Write;
        result._linearBufferDesc._sizeInBytes = size;
        result._linearBufferDesc._structureByteSize = structureSize;
        result._name[0] = '\0';
        return result;
    }

    static BufferUploads::BufferDesc RWTexture2DDesc(unsigned width, unsigned height, NativeFormat::Enum format)
    {
        using namespace BufferUploads;
        BufferDesc result;
        result._type = BufferDesc::Type::Texture;
        result._bindFlags = BindFlag::UnorderedAccess;
        result._cpuAccess = 0;
        result._gpuAccess = GPUAccess::Read|GPUAccess::Write;
        result._textureDesc = BufferUploads::TextureDesc::Plain2D(width, height, format);
        result._name[0] = '\0';
        return result;
    }

    void    TerrainCellRenderer::ShortCircuitTileUpdate(const TextureTile& tile, UInt2 nodeMin, UInt2 nodeMax, unsigned downsample, Float4x4& localToCell, const ShortCircuitUpdate& upd)
    {
        auto& context = *upd._context;

        TRY 
        {
            const char firstPassShader[] = "game/xleres/ui/copyterraintile.sh:WriteToMidway:cs_*";
            const char secondPassShader[] = "game/xleres/ui/copyterraintile.sh:CommitToFinal:cs_*";
            auto& byteCode = Assets::GetAssetDep<CompiledShaderByteCode>(firstPassShader);
            auto& cs0 = Assets::GetAssetDep<ComputeShader>(firstPassShader);
            auto& cs1 = Assets::GetAssetDep<ComputeShader>(secondPassShader);

            struct TileCoords
            {
                float minHeight, heightScale;
                unsigned workingMinHeight, workingMaxHeight;
            } tileCoords = { localToCell(2, 3), localToCell(2, 2), 0xffffffffu, 0x0u };

            auto& uploads = _heightMapTileSet->GetBufferUploads();
            auto tileCoordsBuffer = uploads.Transaction_Immediate(RWBufferDesc(sizeof(TileCoords), sizeof(TileCoords)), 
                BufferUploads::CreateBasicPacket(sizeof(tileCoords), &tileCoords).get())->AdoptUnderlying();
            UnorderedAccessView tileCoordsUAV(tileCoordsBuffer.get());

            auto midwayBuffer = uploads.Transaction_Immediate(RWTexture2DDesc(tile._width, tile._height, NativeFormat::R32_FLOAT), nullptr)->AdoptUnderlying();
            UnorderedAccessView midwayBufferUAV(midwayBuffer.get());

            struct Parameters
            {
                Int2 _sourceMin;
                Int2 _sourceMax;
                Int2 _updateMin;
                Int2 _updateMax;
                Int3 _dstTileAddress;
                int _sampleArea;
            }
            parameters = 
            { 
                upd._resourceMins - Int2(nodeMin),
                upd._resourceMaxs - Int2(nodeMax),
                upd._updateAreaMins - Int2(nodeMin),
                upd._updateAreaMaxs - Int2(nodeMax),
                Int3(tile._x, tile._y, tile._arrayIndex),
                1<<downsample
            };
            ConstantBufferPacket pkts[] = { RenderCore::MakeSharedPkt(parameters) };
            const ShaderResourceView* srv[] = { upd._srv.get(), &_heightMapTileSet->GetShaderResource() };

            BoundUniforms boundLayout(byteCode);
            boundLayout.BindConstantBuffer(Hash64("Parameters"), 0, 1);
            boundLayout.BindShaderResource(Hash64("Input"), 0, 1);
            boundLayout.BindShaderResource(Hash64("OldHeights"), 1, 1);

            boundLayout.Apply(context, UniformsStream(), UniformsStream(pkts, nullptr, dimof(pkts), srv, dimof(srv)));

            context.BindCS(MakeResourceList(1, tileCoordsUAV, midwayBufferUAV));

            const unsigned threadGroupWidth = 6;
            context.Bind(cs0);
            context.Dispatch(   unsigned(XlCeil(tile._width /float(threadGroupWidth))), 
                                unsigned(XlCeil(tile._height/float(threadGroupWidth))));

                //  if everything is ok up to this point, we can commit to the final
                //  output --
            context.BindCS(MakeResourceList(_heightMapTileSet->GetUnorderedAccessView()));
            context.Bind(cs1);
            context.Dispatch(   unsigned(XlCeil(tile._width /float(threadGroupWidth))), 
                                unsigned(XlCeil(tile._height/float(threadGroupWidth))));

                //  We need to read back the new min/max heights
                //  we could write these back to the original terrain cell -- but it
                //  would be better to keep them cached only in the NodeRenderInfo
            auto readback = uploads.Resource_ReadBack(BufferUploads::ResourceLocator(tileCoordsBuffer.get()));
            float* readbackData = (float*)readback->GetData(0,0);
            if (readbackData) {
                float newHeightOffset   = readbackData[2];
                float newHeightScale    = (readbackData[3] - readbackData[2]) / float(0xffff);
                localToCell(2,2) = newHeightScale;
                localToCell(2,3) = newHeightOffset;
            }

            context.UnbindCS<UnorderedAccessView>(0, 1);
        }
        CATCH (...) {
            // note, it's a real problem when we get a invalid resource get... We should ideally stall until all the required
            // resources are loaded
        } CATCH_END
    }

    void    TerrainCellRenderer::HeightMapShortCircuit(std::string cellName, UInt2 cellOrigin, UInt2 cellMax, const ShortCircuitUpdate& upd)
    {
            //      We need to find the CellRenderInfo objects associated with the terrain cell with this name.
            //      Then, for any completed height map tiles within that object, we must copy in the data
            //      from our update information (sometimes doing the downsample along the way).
            //      This will update the tiles with new data, without hitting the disk or requiring a re-upload

            //  Don't use GetResourceDep here -- we don't want to destroy the TerrainCell, because there
            //  maybe be CellRenderInfo's pointing to it. We need some better way to handle this case
        auto& sourceCell = _ioFormat->LoadHeights(cellName.c_str(), true);
        for (auto i=_renderInfos.begin(); i!=_renderInfos.end(); ++i) {
            if (i->second->_sourceCell == &sourceCell) {

                auto& tileSet = *_heightMapTileSet;
                    //  Got a match. Find all with completed tiles (ignoring the pending tiles) and 
                    //  write over that data.
                auto& cri = *i->second;
                for (auto ni=cri._nodes.begin(); ni!=cri._nodes.end(); ++ni) {

                        // todo -- cancel any pending tiles, because they can cause problems

                    if (tileSet.IsValid(ni->_heightMapTile)) {

                        auto nodeIndex = std::distance(cri._nodes.begin(), ni);
                        auto& sourceNode = sourceCell._nodes[nodeIndex];

                            //  We need to transform the coordinates for this node into
                            //  the uber-surface coordinate system. If there's an overlap
                            //  between the node coords and the update box, we need to do
                            //  a copy.

                        Float3 nodeMinInCell = TransformPoint(sourceNode->_localToCell, Float3(0.f, 0.f, 0.f));
                        Float3 nodeMaxInCell = TransformPoint(sourceNode->_localToCell, Float3(1.f, 1.f, float(0xffff)));

                        UInt2 nodeMin(
                            (unsigned)LinearInterpolate(float(cellOrigin[0]), float(cellMax[0]), nodeMinInCell[0]),
                            (unsigned)LinearInterpolate(float(cellOrigin[1]), float(cellMax[1]), nodeMinInCell[1]));
                        UInt2 nodeMax(
                            (unsigned)LinearInterpolate(float(cellOrigin[0]), float(cellMax[0]), nodeMaxInCell[0]),
                            (unsigned)LinearInterpolate(float(cellOrigin[1]), float(cellMax[1]), nodeMaxInCell[1]));

                        if (    nodeMin[0] <= upd._updateAreaMaxs[0] && nodeMax[0] >= upd._updateAreaMins[0]
                            &&  nodeMin[1] <= upd._updateAreaMaxs[1] && nodeMax[1] >= upd._updateAreaMins[1]) {

                                // downsampling required depends on which field we're in.
                            auto fi = std::find_if(sourceCell._nodeFields.cbegin(), sourceCell._nodeFields.cend(),
                                [=](const TerrainCell::NodeField& field) { return unsigned(nodeIndex) >= field._nodeBegin && unsigned(nodeIndex) < field._nodeEnd; });
                            size_t fieldIndex = std::distance(sourceCell._nodeFields.cbegin(), fi);
                            unsigned downsample = unsigned(4-fieldIndex);

                            ShortCircuitTileUpdate(ni->_heightMapTile, nodeMin, nodeMax, downsample, sourceNode->_localToCell, upd);

                        }
                    }
                }
            }
        }
    }

    TerrainCellRenderer::TerrainCellRenderer(std::shared_ptr<ITerrainFormat> ioFormat, BufferUploads::IManager* bufferUploads, Int2 heightMapNodeElementWidth)
    {
        auto heightMapTextureTileSet = std::unique_ptr<TextureTileSet>(
            new TextureTileSet(*bufferUploads, heightMapNodeElementWidth, 16*1024, RenderCore::Metal::NativeFormat::R16_UINT, true));
        Int2 coverageElementSize(33, 33);
        std::unique_ptr<TextureTileSet> coverageTileSets[TerrainCellId::CoverageCount];
        for (unsigned c=0; c<TerrainCellId::CoverageCount; ++c) {
            coverageTileSets[c] = std::unique_ptr<TextureTileSet>(
                new TextureTileSet(*bufferUploads, coverageElementSize, 16*1024, CoverageFileFormat[c], false));
        }

        _renderInfos.reserve(64);
        _heightMapTileSet = std::move(heightMapTextureTileSet);
        for (unsigned c=0; c<TerrainCellId::CoverageCount; ++c) {
            _coverageTileSet[c] = std::move(coverageTileSets[c]);
        }
        _elementSize = heightMapNodeElementWidth;
        _coverageElementSize = coverageElementSize;
        _ioFormat = std::move(ioFormat);
    }

    TerrainCellRenderer::~TerrainCellRenderer()
    {
        CompletePendingUploads();
            // note;    there's no protection to make sure these get completed. If we want to release
            //          the upload transaction, we should complete them all
        assert(_pendingHeightMapUploads.empty());
        assert(_pendingCoverageUploads.empty());
    }


    //////////////////////////////////////////////////////////////////////////////////////////
    TerrainCellRenderer::CellRenderInfo::CellRenderInfo(const TerrainCell& cell, const TerrainCellTexture* cellCoverage[TerrainCellId::CoverageCount])
    {
            //  we need to create a "NodeRenderInfo" for each node.
            //  this will keep track of texture uploads, etc
        size_t nodeCount = cell._nodes.size();
        _sourceCell = nullptr;
        std::fill(_sourceCoverage, &_sourceCoverage[dimof(_sourceCoverage)], nullptr);
        _heightMapStreamingFilePtr = _heightMapCacheStreamingFilePtr = INVALID_HANDLE_VALUE;
        std::fill(_coverageStreamingFilePtr, &_coverageStreamingFilePtr[dimof(_coverageStreamingFilePtr)], INVALID_HANDLE_VALUE);

        if (nodeCount && !cell.SourceFile().empty()) {
            std::vector<NodeRenderInfo> nodes;
            nodes.reserve(nodeCount);
            for (size_t c=0; c<nodeCount; ++c) {
                nodes.push_back(NodeRenderInfo(*cell._nodes[c]));
            }

                //  we also need to open a file for streaming access. 
                //  we should open this file here, and keep it open permanently.
                //  we don't want to have to open and close it for each streaming
                //  operation.
                //  No wrapped asynchronous file io yet. We have to use the win32 api
                //  directly. Using overlapped io is best for this task. Mapped files
                //  might be better in 64 bit.. But it depends on the size of the terrain
                //  files.
                //
                //      Maybe set "FILE_FLAG_NO_BUFFERING" ?
            auto heightMapFileHandle = ::CreateFile(
                cell.SourceFile().c_str(), GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED|FILE_FLAG_RANDOM_ACCESS, nullptr);
            if (heightMapFileHandle == INVALID_HANDLE_VALUE) {
                ThrowException(::Exceptions::BasicLabel("Failed opening terrain height-map file for streaming"));
            }

            auto heightMapCacheFileHandle = ::CreateFile(
                cell.SecondaryCacheFile().c_str(), GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED|FILE_FLAG_RANDOM_ACCESS, nullptr);
            // height map cache file can be invalid -- it's not always required

            void* coverageFileHandle[TerrainCellId::CoverageCount];
            std::fill(coverageFileHandle, &coverageFileHandle[dimof(coverageFileHandle)], INVALID_HANDLE_VALUE);
            if (cellCoverage) {
                for (unsigned c=0; c<TerrainCellId::CoverageCount; ++c) {
                    coverageFileHandle[c] = ::CreateFile(
                        cellCoverage[c]->SourceFile().c_str(), GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE,
                        nullptr, OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED|FILE_FLAG_RANDOM_ACCESS, nullptr);
                    assert(coverageFileHandle[c] != INVALID_HANDLE_VALUE);
                }
            }

            _nodes = std::move(nodes);
            _sourceCell = &cell;
            _heightMapStreamingFilePtr = heightMapFileHandle;
            _heightMapCacheStreamingFilePtr = heightMapCacheFileHandle;
            
			if (cellCoverage) {
				for (unsigned c = 0; c < TerrainCellId::CoverageCount; ++c) {
					_sourceCoverage[c] = cellCoverage[c];
					_coverageStreamingFilePtr[c] = coverageFileHandle[c];
				}
			} else {
				std::fill(_sourceCoverage, &_sourceCoverage[TerrainCellId::CoverageCount], nullptr);
				std::fill(_coverageStreamingFilePtr, &_coverageStreamingFilePtr[TerrainCellId::CoverageCount], INVALID_HANDLE_VALUE);
			}
        }
    }

    TerrainCellRenderer::CellRenderInfo::CellRenderInfo(CellRenderInfo&& moveFrom)
    {
        _nodes = std::move(moveFrom._nodes);
        _sourceCell = std::move(moveFrom._sourceCell);
        _heightMapStreamingFilePtr = std::move(moveFrom._heightMapStreamingFilePtr);
        _heightMapCacheStreamingFilePtr = std::move(moveFrom._heightMapCacheStreamingFilePtr);

        for (unsigned c=0; c<TerrainCellId::CoverageCount; ++c) {
            _sourceCoverage[c] = std::move(moveFrom._sourceCoverage[c]);
            _coverageStreamingFilePtr[c] = std::move(moveFrom._coverageStreamingFilePtr[c]);
        }

        moveFrom._heightMapStreamingFilePtr = INVALID_HANDLE_VALUE;
        moveFrom._heightMapCacheStreamingFilePtr = INVALID_HANDLE_VALUE;
        std::fill(moveFrom._coverageStreamingFilePtr, &moveFrom._coverageStreamingFilePtr[dimof(moveFrom._coverageStreamingFilePtr)], INVALID_HANDLE_VALUE);
    }

    auto TerrainCellRenderer::CellRenderInfo::operator=(CellRenderInfo&& moveFrom) throw() -> CellRenderInfo&
    {
        _nodes = std::move(moveFrom._nodes);
        _sourceCell = std::move(moveFrom._sourceCell);
        _heightMapStreamingFilePtr = std::move(moveFrom._heightMapStreamingFilePtr);
        _heightMapCacheStreamingFilePtr = std::move(moveFrom._heightMapCacheStreamingFilePtr);

        for (unsigned c=0; c<TerrainCellId::CoverageCount; ++c) {
            _sourceCoverage[c] = std::move(moveFrom._sourceCoverage[c]);
            _coverageStreamingFilePtr[c] = std::move(moveFrom._coverageStreamingFilePtr[c]);
        }

        moveFrom._heightMapStreamingFilePtr = INVALID_HANDLE_VALUE;
        moveFrom._heightMapCacheStreamingFilePtr = INVALID_HANDLE_VALUE;
        std::fill(moveFrom._coverageStreamingFilePtr, &moveFrom._coverageStreamingFilePtr[dimof(moveFrom._coverageStreamingFilePtr)], INVALID_HANDLE_VALUE);
        return *this;
    }

    TerrainCellRenderer::CellRenderInfo::~CellRenderInfo()
    {
            //  we need to cancel any buffer uploads transactions that are still active
            //      -- note they may still complete in a background thread
        auto& bufferUploads = *GetBufferUploads();
        for (auto i = _nodes.begin(); i!=_nodes.end(); ++i) {
                //  note that when we complete the transaction like this, we might
                //  leave the tile in an invalid state, because it may still point
                //  to some allocated space in the tile set. In other words, the 
                //  destination area in the tile set is not deallocated during when
                //  the transaction ends.
            for (unsigned c=0; c<TerrainCellId::CoverageCount; ++c) {
                if (i->_coveragePendingTile[c]._transaction != ~BufferUploads::TransactionID(0x0)) {
                    bufferUploads.Transaction_End(i->_coveragePendingTile[c]._transaction);
                    i->_coveragePendingTile[c]._transaction = ~BufferUploads::TransactionID(0x0);
                }
                if (i->_coverageTile[c]._transaction != ~BufferUploads::TransactionID(0x0)) {
                    bufferUploads.Transaction_End(i->_coverageTile[c]._transaction);
                    i->_coverageTile[c]._transaction = ~BufferUploads::TransactionID(0x0);
                }
            }
            if (i->_heightMapTile._transaction != ~BufferUploads::TransactionID(0x0)) {
                bufferUploads.Transaction_End(i->_heightMapTile._transaction);
                i->_heightMapTile._transaction = ~BufferUploads::TransactionID(0x0);
            }
            if (i->_heightMapPendingTile._transaction != ~BufferUploads::TransactionID(0x0)) {
                bufferUploads.Transaction_End(i->_heightMapPendingTile._transaction);
                i->_heightMapPendingTile._transaction = ~BufferUploads::TransactionID(0x0);
            }
        }

        if (_heightMapStreamingFilePtr && _heightMapStreamingFilePtr!=INVALID_HANDLE_VALUE) {
            CloseHandle((HANDLE)_heightMapStreamingFilePtr);
        }
        if (_heightMapCacheStreamingFilePtr && _heightMapCacheStreamingFilePtr!=INVALID_HANDLE_VALUE) {
            CloseHandle((HANDLE)_heightMapCacheStreamingFilePtr);
        }
        for (unsigned c=0; c<TerrainCellId::CoverageCount; ++c) {
            if (_coverageStreamingFilePtr[c] && _coverageStreamingFilePtr[c]!=INVALID_HANDLE_VALUE) {
                CloseHandle((HANDLE)_coverageStreamingFilePtr[c]);
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    bool        TerrainCellRenderer::NodeRenderInfo::CompleteHeightMapUpload(BufferUploads::IManager& bufferUploads)
    {
        if (_heightMapPendingTile._transaction != ~BufferUploads::TransactionID(0x0)) {
            if (!bufferUploads.IsCompleted(_heightMapPendingTile._transaction))
                return false;

            bufferUploads.Transaction_End(_heightMapPendingTile._transaction);
            _heightMapPendingTile._transaction = ~BufferUploads::TransactionID(0x0);
            _heightMapTile = std::move(_heightMapPendingTile);
        }
        return true;
    }

    bool        TerrainCellRenderer::NodeRenderInfo::CompleteCoverageUpload(BufferUploads::IManager& bufferUploads)
    {
        bool result = true;
        for (unsigned c=0; c<TerrainCellId::CoverageCount; ++c) {
            if (_coveragePendingTile[c]._transaction != ~BufferUploads::TransactionID(0x0)) {
                if (!bufferUploads.IsCompleted(_coveragePendingTile[c]._transaction)) {
                    result = false;
                    continue;
                }

                bufferUploads.Transaction_End(_coveragePendingTile[c]._transaction);
                _coveragePendingTile[c]._transaction = ~BufferUploads::TransactionID(0x0);
                _coverageTile[c] = std::move(_coveragePendingTile[c]);
            }
        }
        return result;
    }

    void        TerrainCellRenderer::NodeRenderInfo::QueueHeightMapUpload(
                        TextureTileSet& heightMapTileSet,
                        const void* filePtr, const void* cacheFilePtr, const TerrainCell::Node& sourceNode)
    {
            // the caller should check to see if we need an upload before calling this
        assert(!heightMapTileSet.IsValid(_heightMapTile));
        assert(!heightMapTileSet.IsValid(_heightMapPendingTile));

        if (sourceNode._heightMapFileSize) {
            heightMapTileSet.Transaction_Begin(_heightMapPendingTile, filePtr, 
                sourceNode._heightMapFileOffset, sourceNode._heightMapFileSize);
        } else {
            assert(sourceNode._secondaryCacheSize);
            heightMapTileSet.Transaction_Begin(_heightMapPendingTile, cacheFilePtr, 
                sourceNode._secondaryCacheOffset, sourceNode._secondaryCacheSize);
        }
    }

    void        TerrainCellRenderer::NodeRenderInfo::QueueCoverageUpload(
                        unsigned index, TextureTileSet& coverageTileSet,
                        const void* filePtr, unsigned fileOffset, unsigned fileSize)
    {
            // the caller should check to see if we need an upload before calling this
        assert(!coverageTileSet.IsValid(_coverageTile[index]));
        assert(!coverageTileSet.IsValid(_coveragePendingTile[index]));
        coverageTileSet.Transaction_Begin(
            _coveragePendingTile[index], filePtr, fileOffset, fileSize);
    }

    TerrainCellRenderer::NodeRenderInfo::NodeRenderInfo()
    {}

    TerrainCellRenderer::NodeRenderInfo::NodeRenderInfo(const TerrainCell::Node& node)
    {}

    TerrainCellRenderer::NodeRenderInfo::NodeRenderInfo(NodeRenderInfo&& moveFrom)
    {
        _heightMapTile = std::move(moveFrom._heightMapTile);
        _heightMapPendingTile = std::move(moveFrom._heightMapPendingTile);
        for (unsigned c=0; c<TerrainCellId::CoverageCount; ++c) {
            _coverageTile[c] = std::move(moveFrom._coverageTile[c]);
            _coveragePendingTile[c] = std::move(moveFrom._coveragePendingTile[c]);
        }
    }

    auto TerrainCellRenderer::NodeRenderInfo::operator=(NodeRenderInfo&& moveFrom) -> NodeRenderInfo& 
    {
        _heightMapTile = std::move(moveFrom._heightMapTile);
        _heightMapPendingTile = std::move(moveFrom._heightMapPendingTile);
        for (unsigned c=0; c<TerrainCellId::CoverageCount; ++c) {
            _coverageTile[c] = std::move(moveFrom._coverageTile[c]);
            _coveragePendingTile[c] = std::move(moveFrom._coveragePendingTile[c]);
        }
        return *this;
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    static void DoHeightMapShortCircuitUpdate(
        std::string cellName, 
        std::shared_ptr<TerrainCellRenderer> renderer,
        UInt2 cellOrigin, UInt2 cellMax, const ShortCircuitUpdate& upd)
    {
        renderer->HeightMapShortCircuit(cellName, cellOrigin, cellMax, upd);
    }

    //////////////////////////////////////////////////////////////////////////////////////////

    class TerrainSurfaceHeightsProvider : public ISurfaceHeightsProvider
    {
    public:
        virtual SRV         GetSRV();
        virtual Addressing  GetAddress(Float2 minCoord, Float2 maxCoord);
        virtual bool        IsFloatFormat() const;

        TerrainSurfaceHeightsProvider(  std::shared_ptr<TerrainCellRenderer> terrainRenderer, 
                                        const TerrainConfig& terrainConfig,
                                        const TerrainCoordinateSystem& coordSystem);

    private:
        std::shared_ptr<TerrainCellRenderer> _terrainRenderer;
        TerrainCoordinateSystem _coordSystem;
        TerrainConfig _terrainConfig;
    };

    auto TerrainSurfaceHeightsProvider::GetSRV() -> SRV
    {
        return _terrainRenderer->_heightMapTileSet->GetShaderResource();
    }

    bool TerrainSurfaceHeightsProvider::IsFloatFormat() const { return false; }

    static bool PointInside(const Float2& pt, const Float2& mins, const Float2& maxs)
    {
        return  pt[0] >= mins[0] && pt[1] >= mins[1]
            &&  pt[0] <= maxs[0] && pt[1] <= maxs[1];
    }

    auto  TerrainSurfaceHeightsProvider::GetAddress(Float2 minCoord, Float2 maxCoord) -> Addressing
    {
        Addressing result;
        result._valid = false;
        result._heightScale = result._heightOffset = 0;

            //  We need to find the node that contains this position
            //  and return the coordinates of the height map texture (in the tile set)
            
        auto terrainCoordsMin = _coordSystem.WorldSpaceToTerrainCoords(minCoord);
        auto terrainCoordsMax = _coordSystem.WorldSpaceToTerrainCoords(maxCoord);

            //  we're going to assume that both points are in the same cell.
            //  we can only return a contiguous texture if they both belong to the same cell
        Float2 cellCoordMin = _terrainConfig.TerrainCoordsToCellBasedCoords(terrainCoordsMin);
        Float2 cellCoordMax = _terrainConfig.TerrainCoordsToCellBasedCoords(terrainCoordsMax);

        UInt2 cellIndex = UInt2(unsigned(XlFloor(cellCoordMin[0])), unsigned(XlFloor(cellCoordMin[1])));
        assert(unsigned(XlFloor(cellCoordMax[0])) == cellIndex[0]);
        assert(unsigned(XlFloor(cellCoordMax[1])) == cellIndex[1]);

            //  Currently we don't have a strong mapping between world space and rendered terrain
            //  we have to calculate the names of the height map and coverage files, and then look
            //  for those names in the TerrainCellRenderer's cache
            //      -- maybe there's a better way to go directly to a hash value?
        char heightMapFile[MaxPath], coverageFile[MaxPath];
        _terrainConfig.GetCellFilename(heightMapFile, dimof(heightMapFile), UInt2(cellIndex), TerrainConfig::FileType::Heightmap);
        _terrainConfig.GetCellFilename(coverageFile, dimof(coverageFile), UInt2(cellIndex), TerrainConfig::FileType::ShadowCoverage);

        auto hash = Hash64(heightMapFile);
        hash = Hash64(coverageFile, &coverageFile[XlStringLen(coverageFile)], hash);
        auto i = std::lower_bound(
            _terrainRenderer->_renderInfos.begin(), _terrainRenderer->_renderInfos.end(), 
            hash, CompareFirst<uint64, std::unique_ptr<TerrainCellRenderer::CellRenderInfo>>());
        if (i!=_terrainRenderer->_renderInfos.end() && i->first == hash) {

                //  We found the correct cell...
                //
                //  Now we need to find the node within it at the maximum
                //  detail level (or maybe whatever is the highest detail level
                //  that we currently have loaded).
                //  We can't go directly to the node... we need to find it within
                //  in the list.
                //  
                //  We want to find the highest detail node that contains both 
                //  points (and is actually loaded). Often it should mean going
                //  down to the highest detail level

            auto& sourceCell = *i->second->_sourceCell;
            const unsigned startLod = 0;
            auto& field = sourceCell._nodeFields[startLod];
            Float2 cellSpaceSearchMin = (cellCoordMin - Float2(cellIndex));
            Float2 cellSpaceSearchMax = (cellCoordMax - Float2(cellIndex));

            std::stack<std::pair<unsigned, unsigned>> pendingNodes;
            for (unsigned n=0; n<field._nodeEnd - field._nodeBegin; ++n)
                pendingNodes.push(std::make_pair(startLod, n));

            while (!pendingNodes.empty()) {
                auto nodeRef = pendingNodes.top(); pendingNodes.pop();
                auto& field = sourceCell._nodeFields[nodeRef.first];
                unsigned n = field._nodeBegin + nodeRef.second;

                auto& sourceNode = sourceCell._nodes[n];

                    // (we can simplify this by making some assumptions about localToCell...)
                Float3 mins = TransformPoint(sourceNode->_localToCell, Float3(0.f, 0.f, 0.f));
                Float3 maxs = TransformPoint(sourceNode->_localToCell, Float3(1.f, 1.f, float(0xffff)));
                if (    PointInside(cellSpaceSearchMin, Truncate(mins), Truncate(maxs)) 
                    &&  PointInside(cellSpaceSearchMax, Truncate(mins), Truncate(maxs))) {

                    auto& node = i->second->_nodes[n];
                    if (node._heightMapTile._width > 0) {

                            //  This node is a valid result.
                            //  But it may not be the best result.
                            //      we can attempt to search deeper in the tree
                            //      to find a better result

                        result._baseCoordinate[0] = node._heightMapTile._x;
                        result._baseCoordinate[1] = node._heightMapTile._y;
                        result._baseCoordinate[2] = node._heightMapTile._arrayIndex;
                        result._heightScale = sourceNode->_localToCell(2,2);
                        result._heightOffset = sourceNode->_localToCell(2,3) + _coordSystem.TerrainOffset()[2];

                            //  what is the coordinate in our texture for the "minCoord" ? 
                            //  we want an actual pixel location

                            //  We can't use "InvertOrthonormalTransform" here, because there are
                            //  scales on the matrix. But we could simplify this by making some assumptions
                            //  (and only taking into account the X & Y transformations)
                        auto cellToLocal = Inverse(sourceNode->_localToCell);
                        Float2 nodeLocalMin = Truncate(TransformPoint(cellToLocal, Expand(cellSpaceSearchMin, 0.f)));
                        Float2 nodeLocalMax = Truncate(TransformPoint(cellToLocal, Expand(cellSpaceSearchMax, 0.f)));
                            
                        const unsigned textureDims = sourceNode->_widthInElements;
                        const unsigned overlapWidth = sourceNode->GetOverlapWidth();
                            // -1 is for the 1 pixel border (overlapping neighbouring grids)
                            //      .. if we increase the border size, we need to change this!
                        result._minCoordOffset = UInt2(unsigned(nodeLocalMin[0] * float(textureDims-overlapWidth)), unsigned(nodeLocalMin[1] * float(textureDims-overlapWidth)));
                        result._maxCoordOffset = UInt2(unsigned(nodeLocalMax[0] * float(textureDims-overlapWidth)), unsigned(nodeLocalMax[1] * float(textureDims-overlapWidth)));

                        result._valid = true;

                    }

                        //  if we get a bounding box success on this node, they we know
                        //  that all siblings must not be valid. The only future node's
                        //  we're interested in are this node's children.
                        //      (why is there no clear for a stack?)
                    while (!pendingNodes.empty()) pendingNodes.pop();

                    if ((nodeRef.first+1) < sourceCell._nodeFields.size()) {

                            // push in the higher quality nodes
                            //      note that if we were stricter with how we store nodes
                            //      we could just select the particular node based on the quadrant
                            //      the points appear in
                        unsigned fieldx = nodeRef.second % field._widthInNodes;
                        unsigned fieldy = nodeRef.second / field._widthInNodes;
                        pendingNodes.push(std::make_pair(nodeRef.first+1, (fieldy*2+0) * (field._widthInNodes*2) + (fieldx*2+0)));
                        pendingNodes.push(std::make_pair(nodeRef.first+1, (fieldy*2+0) * (field._widthInNodes*2) + (fieldx*2+1)));
                        pendingNodes.push(std::make_pair(nodeRef.first+1, (fieldy*2+1) * (field._widthInNodes*2) + (fieldx*2+0)));
                        pendingNodes.push(std::make_pair(nodeRef.first+1, (fieldy*2+1) * (field._widthInNodes*2) + (fieldx*2+1)));

                    }

                }
            }

        }

        return result;
    }

    TerrainSurfaceHeightsProvider::TerrainSurfaceHeightsProvider(
        std::shared_ptr<TerrainCellRenderer> terrainRenderer,
        const TerrainConfig& terrainConfig,
        const TerrainCoordinateSystem& coordSystem)
    : _terrainRenderer(std::move(terrainRenderer))
    , _coordSystem(coordSystem)
    , _terrainConfig(terrainConfig)
    {}

    extern ISurfaceHeightsProvider* MainSurfaceHeightsProvider;

    //////////////////////////////////////////////////////////////////////////////////////////
    static RenderCore::Metal::DeviceContext GetImmediateContext()
    {
        ID3D::DeviceContext* immContextTemp = nullptr;
        RenderCore::Metal::ObjectFactory().GetUnderlying()->GetImmediateContext(&immContextTemp);
        intrusive_ptr<ID3D::DeviceContext> immContext = moveptr(immContextTemp);
        return RenderCore::Metal::DeviceContext(std::move(immContext));
    }

    template <typename Type>
        static const Type& GetAssetImmediate(const char initializer[])
    {
        for (;;) {
            TRY {
                return Assets::GetAsset<Type>(initializer);
            } CATCH (::Assets::Exceptions::PendingResource&) {
                ::Assets::CompileAndAsyncManager::GetInstance().Update();
            } CATCH_END
        }
    }

    static void LoadTextureIntoArray(ID3D::Resource* destinationArray, const char sourceFile[], unsigned arrayIndex, bool sourceIsLinearFormat=false)
    {
            //      We want to load the given texture, and merge it into
            //      the texture array. We have to do this synchronously, otherwise the scheduling
            //      is too awkward
            //      We're also using the "immediate context" -- so this should be run in 
            //      the main rendering thread (or whatever thread is associated with the 
            //      immediate context)

        auto inputTexture = LoadTextureImmediately(sourceFile, sourceIsLinearFormat);

        TextureDesc2D destinationDesc(destinationArray);
        const auto mipCount = destinationDesc.MipLevels;

        TextureDesc2D sourceDesc(inputTexture.get());
        int mipDifference = sourceDesc.MipLevels - mipCount;

        auto context = GetImmediateContext();
        for (unsigned m=0; m<mipCount; ++m) {
            int sourceMip = m + mipDifference;
            if (sourceMip < 0) {

                PrintFormat(&ConsoleRig::GetWarningStream(), 
                    "LoadTextureIntoArray -- performing resample on texture (%s). All textures in the array must be the same size!\n", sourceFile);

                    //  We have to up-sample to get the same number of mips
                    //  Using the highest LOD from the source texture, resample into
                    //  a default texture
                const unsigned expectedWidth = destinationDesc.Width >> m;
                const unsigned expectedHeight = destinationDesc.Height >> m;

                auto destFormat = destinationDesc.Format;
                auto resamplingFormat = destFormat;
                auto compressionType = RenderCore::Metal::GetCompressionType((RenderCore::Metal::NativeFormat::Enum)destFormat);
                if (compressionType == RenderCore::Metal::FormatCompressionType::BlockCompression) {
                        // resampling via a higher precision buffer -- just for kicks.
                    resamplingFormat = (DXGI_FORMAT)RenderCore::Metal::NativeFormat::R16G16B16A16_FLOAT;
                }

                auto& bufferUploads = *GetBufferUploads();
                using namespace BufferUploads;
                BufferDesc desc;
                desc._type = BufferDesc::Type::Texture;
                desc._bindFlags = BindFlag::UnorderedAccess;
                desc._cpuAccess = 0;
                desc._gpuAccess = GPUAccess::Read|GPUAccess::Write;
                desc._allocationRules = 0;
                desc._textureDesc = BufferUploads::TextureDesc::Plain2D(expectedWidth, expectedHeight, resamplingFormat);
                XlCopyString(desc._name, "ResamplingTexture");
                auto resamplingBuffer = bufferUploads.Transaction_Immediate(desc, nullptr);
                RenderCore::Metal::UnorderedAccessView uav(resamplingBuffer->GetUnderlying());
                RenderCore::Metal::ShaderResourceView srv(inputTexture.get());

                auto& resamplingShader = GetAssetImmediate<RenderCore::Metal::ComputeShader>("game/xleres/basic.csh:Resample:cs_*");
                context.Bind(resamplingShader);
                context.BindCS(MakeResourceList(uav));
                context.BindCS(MakeResourceList(srv));
                context.Dispatch(expectedWidth/8, expectedHeight/8);
                context.UnbindCS<UnorderedAccessView>(0, 1);

                if (resamplingFormat!=destFormat) {
                        // We have to re-compress the texture. It's annoying, but we can use a library to do it
                    auto rawData = bufferUploads.Resource_ReadBack(*resamplingBuffer);
                    DirectX::Image image;
                    image.width = expectedWidth;
                    image.height = expectedHeight;
                    image.format = resamplingFormat;
                    image.rowPitch = rawData->GetRowAndSlicePitch(0,0).first;
                    image.slicePitch = rawData->GetRowAndSlicePitch(0,0).second;
                    image.pixels = (uint8_t*)rawData->GetData(0,0);

                    intrusive_ptr<ID3D::Device> device;
                    {
                        ID3D::Device* deviceTemp = nullptr;
                        context.GetUnderlying()->GetDevice(&deviceTemp);
                        device = intrusive_ptr<ID3D::Device>(deviceTemp, false);
                    }

                    DirectX::ScratchImage compressedImage;
                    auto hresult = DirectX::Compress(
                        image, destFormat, DirectX::TEX_COMPRESS_DITHER | DirectX::TEX_COMPRESS_SRGB, 0.f, compressedImage);
                    assert(SUCCEEDED(hresult)); (void)hresult;
                    assert(compressedImage.GetImageCount()==1);
                    
                    auto& final = *compressedImage.GetImage(0,0,0);
                    desc._bindFlags = BindFlag::ShaderResource;
                    desc._textureDesc._nativePixelFormat = destFormat;
                    auto compressedBuffer = bufferUploads.Transaction_Immediate(
                            desc, BufferUploads::CreateBasicPacket(final.slicePitch, final.pixels, std::make_pair(unsigned(final.rowPitch), unsigned(final.slicePitch))).get());

                    resamplingBuffer = compressedBuffer;   
                }

                context.GetUnderlying()->CopySubresourceRegion(
                    destinationArray, 
                    D3D11CalcSubresource(m, arrayIndex, mipCount),
                    0, 0, 0, resamplingBuffer->GetUnderlying(), 0, nullptr);

            } else {

                context.GetUnderlying()->CopySubresourceRegion(
                    destinationArray, 
                    D3D11CalcSubresource(m, arrayIndex, mipCount),
                    0, 0, 0, 
                    inputTexture.get(), D3D11CalcSubresource(sourceMip, 0, mipCount),
                    nullptr);

            }
        }
    }

    TerrainMaterialScaffold::TerrainMaterialScaffold()
    {
        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        _cachedHashValue = ~0x0ull;
    }

    TerrainMaterialScaffold::TerrainMaterialScaffold(const char definitionFile[])
    {
        const char * textureNames[] = { "Texture0", "Texture1", "Slopes" };

        size_t fileSize = 0;
        auto file = LoadFileAsMemoryBlock(definitionFile, &fileSize);
        if (!fileSize)
            ThrowException(::Exceptions::BasicLabel("Parse error while loading terrain texture list"));

        Data data;
        bool loadResult = data.Load((const char*)file.get(), int(fileSize));
        if (!loadResult)
            ThrowException(::Exceptions::BasicLabel("Parse error while loading terrain texture list"));

        auto* cfg = data.ChildWithValue("Config");
        _diffuseDims = Deserialize(cfg, "DiffuseDims", UInt2(512, 512));
        _normalDims = Deserialize(cfg, "NormalDims", UInt2(512, 512));
        _paramDims = Deserialize(cfg, "ParamDims", UInt2(512, 512));

        auto* strata = data.ChildWithValue("Strata");
        unsigned strataCount = 0;
        for (auto* c = strata->child; c; c = c->next) { ++strataCount; }

        unsigned strataIndex = 0;
        for (auto* d = strata->child; d; d = d->next, ++strataIndex) {
            Strata newStrata;
            for (unsigned t=0; t<dimof(textureNames); ++t) {
                auto*tex = d->ChildWithValue(textureNames[t]);
                if (tex && tex->ChildAt(0)) {
                    auto* n = tex->ChildAt(0);
                    if (n->value && _stricmp(n->value, "null")!=0) {
                        newStrata._texture[t] = n->value;
                    }
                }
            }

            newStrata._endHeight = Deserialize(d, "EndHeight", 0.f);
            auto mappingConst = Deserialize(d, "Mapping", Float4(1.f, 1.f, 1.f, 1.f));
            newStrata._mappingConstant[0] = mappingConst[0];
            newStrata._mappingConstant[1] = mappingConst[1];
            newStrata._mappingConstant[2] = mappingConst[2];

            _strata.push_back(newStrata);
        }

        _searchRules = ::Assets::DefaultDirectorySearchRules(definitionFile);
        _cachedHashValue = 0ull;

        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterFileDependency(_validationCallback, definitionFile);
    }

    TerrainMaterialScaffold::~TerrainMaterialScaffold() {}

    TerrainMaterialTextures::TerrainMaterialTextures()
    {
        _strataCount = 0;
    }

    TerrainMaterialTextures::TerrainMaterialTextures(const TerrainMaterialScaffold& scaffold)
    {
        auto strataCount = (unsigned)scaffold._strata.size();

        auto texturingConstants = std::make_unique<Float4[]>(strataCount*2);
        std::fill(texturingConstants.get(), &texturingConstants[strataCount*2], Float4(1.f, 1.f, 1.f, 1.f));

            //  Each texture is stored as a separate file on disk. But we need to copy them
            //  all into a texture array.

        using namespace BufferUploads;
        BufferDesc desc;
        desc._type = BufferDesc::Type::Texture;
        desc._bindFlags = BindFlag::ShaderResource;
        desc._cpuAccess = 0;
        desc._gpuAccess = GPUAccess::Read;
        desc._allocationRules = 0;
        XlCopyString(desc._name, "TerrainMaterialTextures");

        const auto texturesPerStrata = dimof(((TerrainMaterialScaffold::Strata*)nullptr)->_texture);

            // todo -- there are some SRGB problems here!
            //          should we be using SRGB input texture format?
        desc._textureDesc = BufferUploads::TextureDesc::Plain2D(
            scaffold._diffuseDims[0], scaffold._diffuseDims[1], NativeFormat::BC1_UNORM, 
            (uint8)IntegerLog2(std::max(scaffold._diffuseDims[0], scaffold._diffuseDims[1]))+1, uint8(texturesPerStrata * strataCount));
        auto diffuseTextureArray = GetBufferUploads()->Transaction_Immediate(desc, nullptr)->AdoptUnderlying();

        desc._textureDesc = BufferUploads::TextureDesc::Plain2D(
            scaffold._normalDims[0], scaffold._normalDims[1], NativeFormat::BC5_UNORM, 
            (uint8)IntegerLog2(std::max(scaffold._normalDims[0], scaffold._normalDims[1]))+1, uint8(texturesPerStrata * strataCount));
        auto normalTextureArray = GetBufferUploads()->Transaction_Immediate(desc, nullptr)->AdoptUnderlying();

        desc._textureDesc = BufferUploads::TextureDesc::Plain2D(
            scaffold._paramDims[0], scaffold._paramDims[1], NativeFormat::BC1_UNORM, 
            (uint8)IntegerLog2(std::max(scaffold._paramDims[0], scaffold._paramDims[1]))+1, uint8(texturesPerStrata * strataCount));
        auto specularityTextureArray = GetBufferUploads()->Transaction_Immediate(desc, nullptr)->AdoptUnderlying();

        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, &scaffold.GetDependencyValidation());

        unsigned strataIndex = 0;
        for (auto s=scaffold._strata.cbegin(); s!=scaffold._strata.cend(); ++s, ++strataIndex) {

            for (unsigned t=0; t<texturesPerStrata; ++t) {
                    //  This is a input texture. We need to build the 
                    //  diffuse, specularity and normal map names from this texture name
                TRY { 
                    ::Assets::ResChar resolvedFile[MaxPath];
                    scaffold._searchRules.ResolveFile(resolvedFile, dimof(resolvedFile), StringMeld<MaxPath, ::Assets::ResChar>() << s->_texture[t] << "_df.dds");
                    if (resolvedFile[0]) {
                        LoadTextureIntoArray(diffuseTextureArray.get(), resolvedFile, (strataIndex * texturesPerStrata) + t);
                        RegisterFileDependency(_validationCallback, resolvedFile);
                    }
                } CATCH (const ::Assets::Exceptions::InvalidResource&) {}
                CATCH_END
                            
                TRY { 
                    ::Assets::ResChar resolvedFile[MaxPath];
                    scaffold._searchRules.ResolveFile(resolvedFile, dimof(resolvedFile), StringMeld<MaxPath, ::Assets::ResChar>() << s->_texture[t] << "_ddn.dds");
                    if (resolvedFile[0]) {
                        LoadTextureIntoArray(normalTextureArray.get(), resolvedFile, (strataIndex * texturesPerStrata) + t, true);
                        RegisterFileDependency(_validationCallback, resolvedFile);
                    }
                } CATCH (const ::Assets::Exceptions::InvalidResource&) {}
                CATCH_END
                                
                TRY { 
                    ::Assets::ResChar resolvedFile[MaxPath];
                    scaffold._searchRules.ResolveFile(resolvedFile, dimof(resolvedFile), StringMeld<MaxPath, ::Assets::ResChar>() << s->_texture[t] << "_sp.dds");
                    if (resolvedFile[0]) {
                        LoadTextureIntoArray(specularityTextureArray.get(), resolvedFile, (strataIndex * texturesPerStrata) + t, true);
                        RegisterFileDependency(_validationCallback, resolvedFile);
                    }
                } CATCH (const ::Assets::Exceptions::InvalidResource&) {}
                CATCH_END
            }

            texturingConstants[strataIndex] = Float4(s->_endHeight, s->_endHeight, s->_endHeight, s->_endHeight);

            Float4 mappingConstant = Float4(1.f, 1.f, 1.f, 1.f);
            for (unsigned c=0; c<texturesPerStrata; ++c) {
                texturingConstants[strataCount + strataIndex][c] = 1.f / s->_mappingConstant[c];
            }
        }

        RenderCore::Metal::ShaderResourceView diffuseSrv(diffuseTextureArray.get());
        RenderCore::Metal::ShaderResourceView normalSrv(normalTextureArray.get());
        RenderCore::Metal::ShaderResourceView specularitySrv(specularityTextureArray.get());
        RenderCore::Metal::ConstantBuffer texContBuffer(texturingConstants.get(), sizeof(Float4)*strataCount*2);

        _textureArray[Diffuse] = std::move(diffuseTextureArray);
        _textureArray[Normal] = std::move(normalTextureArray);
        _textureArray[Specularity] = std::move(specularityTextureArray);
        _srv[Diffuse] = std::move(diffuseSrv);
        _srv[Normal] = std::move(normalSrv);
        _srv[Specularity] = std::move(specularitySrv);
        _texturingConstants = std::move(texContBuffer);
        _strataCount = strataCount;
    }

    TerrainMaterialTextures::~TerrainMaterialTextures() {}

    //////////////////////////////////////////////////////////////////////////////////////////
    class CellAndPosition { public: TerrainCellId _id; };
    class TerrainManager::Pimpl
    {
    public:
        std::shared_ptr<TerrainCellRenderer> _renderer;
        std::unique_ptr<TerrainSurfaceHeightsProvider> _heightsProvider;
        std::unique_ptr<TerrainUberSurfaceInterface> _uberSurfaceInterface;
        std::shared_ptr<ITerrainFormat> _ioFormat;

        std::vector<CellAndPosition> _cells;
        TerrainCoordinateSystem _coords;
        TerrainConfig _cfg;

        std::unique_ptr<TerrainMaterialTextures> _textures;

        void CullNodes(
            DeviceContext* context, LightingParserContext& parserContext, 
            TerrainRenderingContext& terrainContext);
    };

    void TerrainConfig::GetCellFilename(
        char buffer[], unsigned bufferCount,
        UInt2 cellIndex, FileType::Enum fileType) const
    {
        if (_filenamesMode == Legacy) {
                // note -- cell xy flipped
                //      This is a related to the terrain format used in Archeage
            switch (fileType) {
            case FileType::Heightmap:
                _snprintf_s(buffer, bufferCount, _TRUNCATE, "%s/cells/%03i_%03i/client/terrain/heightmap.dat_new", 
                    _baseDir.c_str(), cellIndex[1], cellIndex[0]);
                break;
            case FileType::ShadowCoverage:
                _snprintf_s(buffer, bufferCount, _TRUNCATE, "%s/cells/%03i_%03i/client/terrain/shadow.ctc", 
                    _baseDir.c_str(), cellIndex[1], cellIndex[0]);
                break;
            case FileType::ArchiveHeightmap:
                _snprintf_s(buffer, bufferCount, _TRUNCATE, "%s/cells/%03i_%03i/client/terrain/heightmap.dat", 
                    _baseDir.c_str(), cellIndex[1], cellIndex[0]);
                break;
            default:
                buffer[0] = '\0';
                break;
            }
        } else if (_filenamesMode == XLE) {
            switch (fileType) {
            case FileType::Heightmap:
                _snprintf_s(buffer, bufferCount, _TRUNCATE, "%s/c%02i_%02i/height.terr", 
                    _baseDir.c_str(), cellIndex[0], cellIndex[1]);
                break;
            case FileType::ShadowCoverage:
                _snprintf_s(buffer, bufferCount, _TRUNCATE, "%s/c%02i_%02i/shadow.terr", 
                    _baseDir.c_str(), cellIndex[0], cellIndex[1]);
                break;
            case FileType::ArchiveHeightmap:
                _snprintf_s(buffer, bufferCount, _TRUNCATE, "%s/c%02i_%02i/archiveheights.terr", 
                    _baseDir.c_str(), cellIndex[0], cellIndex[1]);
                break;
            default:
                buffer[0] = '\0';
                break;
            }
        }
    }

    void TerrainConfig::GetUberSurfaceFilename(
        char buffer[], unsigned bufferCount,
        FileType::Enum fileType) const
    {
        XlCopyString(buffer, bufferCount, _baseDir.c_str());
        switch (fileType) {
        case FileType::Heightmap:
            XlCatString(buffer, bufferCount, "/ubersurface.dat");
            break;
        case FileType::ShadowCoverage:
            XlCatString(buffer, bufferCount, "/ubershadowingsurface.dat");
            break;
        default:
            buffer[0] = '\0';
            break;
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    void ExecuteTerrainConversion(
        const TerrainConfig& outputConfig, 
        std::shared_ptr<ITerrainFormat> outputIOFormat,
        const TerrainConfig& inputConfig, 
        std::shared_ptr<ITerrainFormat> inputIOFormat)
    {
        assert(outputIOFormat);

        char uberSurfaceFile[MaxPath], uberShadowingFile[MaxPath];
        outputConfig.GetUberSurfaceFilename(uberSurfaceFile, dimof(uberSurfaceFile), TerrainConfig::FileType::Heightmap);
        outputConfig.GetUberSurfaceFilename(uberShadowingFile, dimof(uberShadowingFile), TerrainConfig::FileType::ShadowCoverage);

        char path[MaxPath];
        XlDirname(path, dimof(path), uberSurfaceFile);
        CreateDirectoryRecursive(path);

        //////////////////////////////////////////////////////////////////////////////////////
            // If we don't have an uber surface file, then we should create it
        if (!DoesFileExist(uberSurfaceFile) && inputIOFormat) {
            BuildUberSurfaceFile(
                uberSurfaceFile, inputConfig, inputIOFormat.get(), 
                0, 0, inputConfig._cellCount[0], inputConfig._cellCount[1]);
        }

        //////////////////////////////////////////////////////////////////////////////////////
            // write all of the shadow.ctc shadow layers from the "uber shadowing file"
        if (DoesFileExist(uberShadowingFile)) {
                //  open and destroy the uber shadowing surface before we open the uber heights surface
                //  (opening them both at the same time requires too much memory)
            TerrainUberShadowingSurface uberShadowingSurface(uberShadowingFile);
            for (unsigned y=0; y<outputConfig._cellCount[1]; ++y)
                for (unsigned x=0; x<outputConfig._cellCount[0]; ++x) {
                    char shadowFile[MaxPath];
                    outputConfig.GetCellFilename(shadowFile, dimof(shadowFile), 
                        UInt2(x, y), TerrainConfig::FileType::ShadowCoverage);
                    if (!DoesFileExist(shadowFile)) {
                        XlDirname(path, dimof(path), shadowFile);
                        CreateDirectoryRecursive(path);
                        TRY {
                            auto cellOrigin = outputConfig.CellBasedCoordsToTerrainCoords(Float2(float(x), float(y)));
                            auto cellMaxs = outputConfig.CellBasedCoordsToTerrainCoords(Float2(float(x+1), float(y+1)));
                            outputIOFormat->WriteCellCoverage_Shadow(
                                shadowFile, uberShadowingSurface, 
                                AsUInt2(cellOrigin), AsUInt2(cellMaxs), outputConfig.CellTreeDepth(), 1);
                        } CATCH(...) { // sometimes throws (eg, if the directory doesn't exist)
                        } CATCH_END
                    }
                }
        }

        //////////////////////////////////////////////////////////////////////////////////////
            //  load the uber height surface, and uber surface interface (but only temporarily
            //  while we initialise the data)
        TerrainUberHeightsSurface heightsData(uberSurfaceFile);
        TerrainUberSurfaceInterface uberSurfaceInterface(heightsData, outputIOFormat);

        //////////////////////////////////////////////////////////////////////////////////////
        for (unsigned y=0; y<outputConfig._cellCount[1]; ++y) {
            for (unsigned x=0; x<outputConfig._cellCount[0]; ++x) {
                char heightMapFile[MaxPath];
                outputConfig.GetCellFilename(heightMapFile, dimof(heightMapFile), 
                    UInt2(x, y), TerrainConfig::FileType::Heightmap);
                if (!DoesFileExist(heightMapFile)) {
                    XlDirname(path, dimof(path), heightMapFile);
                    CreateDirectoryRecursive(path);
                    TRY {
                        auto cellOrigin = outputConfig.CellBasedCoordsToTerrainCoords(Float2(float(x), float(y)));
                        auto cellMaxs = outputConfig.CellBasedCoordsToTerrainCoords(Float2(float(x+1), float(y+1)));
                        outputIOFormat->WriteCell(
                            heightMapFile, *uberSurfaceInterface.GetUberSurface(), 
                            AsUInt2(cellOrigin), AsUInt2(cellMaxs), outputConfig.CellTreeDepth(), outputConfig.NodeOverlap());
                    } CATCH(...) { // sometimes throws (eg, if the directory doesn't exist)
                    } CATCH_END
                }
            }
        }

        //////////////////////////////////////////////////////////////////////////////////////
            // build the uber shadowing file, and then write out the shadowing textures for each node
        // auto uberShadowingFile = terrainConfig._baseDir + "/ubershadowingsurface.dat";
        if (!DoesFileExist(uberShadowingFile)) {
            // Int2 interestingMins((9-1) * 16 * 32, (19-1) * 16 * 32), interestingMaxs((9+4) * 16 * 32, (19+4) * 16 * 32);
            Int2 interestingMins(0, 0);
            Int2 interestingMaxs = UInt2(
                outputConfig._cellCount[0] * outputConfig.CellDimensionsInNodes()[0] * outputConfig.NodeDimensionsInElements()[0],
                outputConfig._cellCount[1] * outputConfig.CellDimensionsInNodes()[1] * outputConfig.NodeDimensionsInElements()[1]);

            float xyScale = 10.f;
            Float2 sunDirectionOfMovement = Normalize(Float2(1.f, 0.33f));
            uberSurfaceInterface.BuildShadowingSurface(uberShadowingFile, interestingMins, interestingMaxs, sunDirectionOfMovement, xyScale);
        }
    }

    static void RegisterShortCircuitUpdate(
        const TerrainConfig& terrainCfg,
        unsigned overlap,
        TerrainUberSurfaceInterface* uberInterface,
        std::shared_ptr<TerrainCellRenderer> renderer)
    {
            //  Register cells for short-circuit update... Do we need to do this for every single cell
            //  or just those that are within the limited area we're going to load?
        for (unsigned y=0; y<terrainCfg._cellCount[1]; ++y) {
            for (unsigned x=0; x<terrainCfg._cellCount[0]; ++x) {
                char heightMapFile[256];
                terrainCfg.GetCellFilename(heightMapFile, dimof(heightMapFile), UInt2(x, y), TerrainConfig::FileType::Heightmap);
                std::string filename = heightMapFile;

                auto cellOrigin = AsUInt2(terrainCfg.CellBasedCoordsToTerrainCoords(Float2(float(x), float(y))));
                auto cellMax = AsUInt2(terrainCfg.CellBasedCoordsToTerrainCoords(Float2(float(x+1), float(y+1))));
                uberInterface->RegisterCell(
                    heightMapFile, cellOrigin, cellMax, overlap,
                    std::bind(&DoHeightMapShortCircuitUpdate, filename, renderer, cellOrigin, cellMax, std::placeholders::_1));
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    TerrainManager::TerrainManager(
        const TerrainConfig& cfg,
        std::shared_ptr<ITerrainFormat> ioFormat, 
        BufferUploads::IManager* bufferUploads,
        Int2 cellMin, Int2 cellMax, Float3 worldSpaceOrigin)
    {
        auto pimpl = std::make_unique<Pimpl>();
        
            // These are the determining parameters for the terrain:
            //      cellSize: 
            //          Size of the a cell (in meters)
            //          This determines the scaling that is applied to convert to world coords
            //      cellElementCount: 
            //          Number of height samples per cell, for width and height (ie, 512 means cells are 512x512)
            //      cellTreeDepth: 
            //          The depth of tree of a single cell. Higher numbers allow for more aggressive lodding on distant LODs
            //      overlap: 
            //          Overlap elements for each node (eg, 2 means an extra 2 rows and an extra 2 columns is added to each node)
            //          Overlap rows and columns store the same value as adjacent nodes -- they are required to make sure the edges match
            //          Eg, if nodes are 32x32 and overlap=2, each node will actually store 34x34 height samples
            //
            //      The number of samples per node is determined by "cellElementCount" and "cellTreeDepth"
            //      In general, we want to balance the number of height samples per node so that is it convenient for 
            //      the shader (since the shaders always work on a node-by-node basis). The number of samples per
            //      node should be small enough that the height texture can fit in the GPU cache, and the GPU
            //      tessellation can apply enough tessellation. But it should also be large enough that the "overlap"
            //      parts are not excessive.

            //      \todo -- these terrain configuration values should come from the data image
            //      The "uber-surface" is effectively the "master" data for the terrain. These variables determine
            //      how we break the uber surface down into separate cells. As a result, the cell data is almost
            //      just a cached version of the uber surface, with these configuration values.
        const auto cellSize = cfg.CellDimensionsInNodes()[0] * cfg.NodeDimensionsInElements()[0] * 10.f;
        const unsigned overlap = cfg.NodeOverlap();
        const auto cellNodeSize = cellSize * std::pow(2.f, -float(cfg.CellTreeDepth()-1));      // size, in m, of a single node
        
        pimpl->_coords = TerrainCoordinateSystem(
            worldSpaceOrigin, cellNodeSize, cfg);

        char uberSurfaceFile[MaxPath];
        cfg.GetUberSurfaceFilename(uberSurfaceFile, dimof(uberSurfaceFile), TerrainConfig::FileType::Heightmap);

            // uber-surface is a special-case asset, because we can edit it without a "DivergentAsset". But to do that, we must const_cast here
        auto& uberSurface = const_cast<TerrainUberHeightsSurface&>(Assets::GetAsset<TerrainUberHeightsSurface>(uberSurfaceFile));
        pimpl->_uberSurfaceInterface = std::make_unique<TerrainUberSurfaceInterface>(std::ref(uberSurface), ioFormat);

        ////////////////////////////////////////////////////////////////////////////
            // decide on the list of terrain cells we're going to render
            //  The caller should be deciding this -- what cells to prepare, and any offset information
        for (int cellY=cellMin[1]; cellY<cellMax[1]; ++cellY) {
            for (int cellX=cellMin[0]; cellX<cellMax[0]; ++cellX) {
                CellAndPosition cell;
                cfg.GetCellFilename(cell._id._heightMapFilename, dimof(cell._id._heightMapFilename), UInt2(cellX, cellY), TerrainConfig::FileType::Heightmap);
                cfg.GetCellFilename(cell._id._coverageFilename[0], dimof(cell._id._coverageFilename[0]), UInt2(cellX, cellY), TerrainConfig::FileType::ShadowCoverage);

                auto t = cfg.CellBasedCoordsToTerrainCoords(Float2(float(cellX), float(cellY)));
                auto cellOrigin = pimpl->_coords.TerrainCoordsToWorldSpace(t);
                cell._id._cellToWorld = Float4x4(
                    cellSize, 0.f, 0.f, cellOrigin[0],
                    0.f, cellSize, 0.f, cellOrigin[1],
                    0.f, 0.f, 1.f, pimpl->_coords.TerrainOffset()[2],
                    0.f, 0.f, 0.f, 1.f);

                    //  Calculate the bounding box. Note that we have to actually read the
                    //  height map file to get this information. Perhaps we can cache this
                    //  somewhere, to avoid having to load the scaffold for every cell on
                    //  startup
                auto& heights = ioFormat->LoadHeights(cell._id._heightMapFilename);
                float minHeight = FLT_MAX, maxHeight = -FLT_MAX;
                for (auto i=heights._nodes.cbegin(); i!=heights._nodes.cend(); ++i) {
                    float zScale = (*i)->_localToCell(2, 2);
                    float zOffset = (*i)->_localToCell(2, 3);
                    minHeight = std::min(minHeight, zOffset);
                    maxHeight = std::max(maxHeight, zOffset + zScale);
                }

                cell._id._aabbMin = Expand(cellOrigin, minHeight + pimpl->_coords.TerrainOffset()[2]);
                cell._id._aabbMax = Expand(Float2(cellOrigin + Float2(cellSize, cellSize)), maxHeight + pimpl->_coords.TerrainOffset()[2]);
                pimpl->_cells.push_back(cell);
            }
        }
        ////////////////////////////////////////////////////////////////////////////

        const Int2 heightMapElementSize = cfg.NodeDimensionsInElements() + Int2(overlap, overlap);
        pimpl->_renderer = std::make_shared<TerrainCellRenderer>(ioFormat, bufferUploads, heightMapElementSize);
        pimpl->_heightsProvider = std::make_unique<TerrainSurfaceHeightsProvider>(pimpl->_renderer, cfg, pimpl->_coords);
        pimpl->_ioFormat = std::move(ioFormat);
        pimpl->_cfg = cfg;
        RegisterShortCircuitUpdate(
            cfg, overlap,
            pimpl->_uberSurfaceInterface.get(), pimpl->_renderer);
        
        MainSurfaceHeightsProvider = pimpl->_heightsProvider.get();
        _pimpl = std::move(pimpl);
    }

    TerrainManager::~TerrainManager()
    {
        MainSurfaceHeightsProvider = nullptr;
    }

    void TerrainManager::Pimpl::CullNodes(
        DeviceContext* context, 
        LightingParserContext& parserContext, TerrainRenderingContext& terrainContext)
    {
        TerrainCollapseContext collapseContext;
        collapseContext._startLod = Tweakable("TerrainLOD", 1);
        collapseContext._screenSpaceEdgeThreshold = Tweakable("TerrainEdgeThreshold", 384.f);
        for (auto i=_cells.begin(); i!=_cells.end(); i++) {
            _renderer->CullNodes(context, parserContext, terrainContext, collapseContext, i->_id);
        }

        for (unsigned c=collapseContext._startLod; c<(TerrainCollapseContext::MaxLODLevels-1); ++c) {
            collapseContext.AttemptLODPromote(c, terrainContext);
        }

        _renderer->WriteQueuedNodes(terrainContext, collapseContext);
    }

    void TerrainManager::Render(DeviceContext* context, LightingParserContext& parserContext, unsigned techniqueIndex)
    {
        assert(_pimpl && _pimpl->_renderer);
        auto* renderer = _pimpl->_renderer.get();

            //  we need to enable the rendering state once, for all cells. The state should be
            //  more or less the same for every cell, so we don't need to do it every time
        TerrainRenderingContext state(true, renderer->GetElementSize());
        state._queuedNodes.erase(state._queuedNodes.begin(), state._queuedNodes.end());
        state._queuedNodes.reserve(2048);
        state._currentViewport = ViewportDesc(*context);
        _pimpl->CullNodes(context, parserContext, state);

        renderer->CompletePendingUploads();
        renderer->QueueUploads(state);

        if (!_pimpl->_textures || _pimpl->_textures->GetDependencyValidation().GetValidationIndex() > 0) {
            _pimpl->_textures.reset();
            auto& scaffold = Assets::GetAssetDep<TerrainMaterialScaffold>(_pimpl->_cfg._textureCfgName.c_str());
            _pimpl->_textures = std::make_unique<TerrainMaterialTextures>(scaffold);
        }

        context->BindPS(MakeResourceList(8, 
            _pimpl->_textures->_srv[TerrainMaterialTextures::Diffuse], 
            _pimpl->_textures->_srv[TerrainMaterialTextures::Normal], 
            _pimpl->_textures->_srv[TerrainMaterialTextures::Specularity]));

        auto mode = 
            (techniqueIndex==5)
            ? TerrainRenderingContext::Mode_VegetationPrepare
            : TerrainRenderingContext::Mode_Normal;

        auto shadowSoftness = Tweakable("ShadowSoftness", 15.f);
        float terrainLightingConstants[] = { SunDirectionAngle / float(.5f * M_PI), shadowSoftness, 0.f, 0.f };
        ConstantBuffer lightingConstantsBuffer(terrainLightingConstants, sizeof(terrainLightingConstants));
        context->BindPS(MakeResourceList(5, _pimpl->_textures->_texturingConstants, lightingConstantsBuffer));
        if (mode == TerrainRenderingContext::Mode_VegetationPrepare) {
                // this cb required in the geometry shader for vegetation prepare mode!
            context->BindGS(MakeResourceList(6, lightingConstantsBuffer));  
        }

        state.EnterState(context, parserContext, *_pimpl->_textures, mode);
        renderer->Render(context, parserContext, state);
        state.ExitState(context, parserContext);
    }

    unsigned TerrainManager::CalculateIntersections(
        IntersectionResult intersections[], unsigned maxIntersections,
        std::pair<Float3, Float3> ray,
        RenderCore::Metal::DeviceContext* context,
        LightingParserContext& parserContext)
    {
            // we can only do this on the immediate context (because we need to execute
            // and readback GPU data)
        assert(context->GetUnderlying()->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);

            //  we can use the same culling as the rendering part. But ideally we want to cull nodes
            //  that are outside of the camera frustum, or that don't intersect the ray
            //      first pass -- normal culling
        TerrainRenderingContext state(true, _pimpl->_renderer->GetElementSize());
        state._queuedNodes.erase(state._queuedNodes.begin(), state._queuedNodes.end());
        state._queuedNodes.reserve(2048);
        state._currentViewport = ViewportDesc(*context);        // (accurate viewport is required to get the lodding right)
        _pimpl->CullNodes(context, parserContext, state);

            //  second pass -- remove nodes that don't intersect the ray
        for (auto i=state._queuedNodes.begin(); i!=state._queuedNodes.end();) {
            auto sourceNode = i->_cell->_sourceCell->_nodes[i->_absNodeIndex].get();
            auto localToWorld = Combine(sourceNode->_localToCell, i->_cellToWorld);
            auto result = RayVsAABB(ray, localToWorld, Float3(0.f, 0.f, 0.f), Float3(1.f, 1.f, float(0xffff)));
            if (!result) {
                i = state._queuedNodes.erase(i);
            } else {
                ++i;
            }
        }

            // \todo -- all this rendering code would be better placed in another class

            //  we should have a set of nodes that are visible on screen, and that might intersect the ray
            //  execute the intersections test shader to look for intersections with the terrain triangles
            //  we need to create a structured buffer for the result, and bind it to the geometry shader
        _pimpl->_renderer->CompletePendingUploads();
        _pimpl->_renderer->QueueUploads(state);

        const unsigned resultsBufferSize = 4 * 1024;

        intrusive_ptr<ID3D::Buffer> gpuOutput;
        intrusive_ptr<ID3D::Buffer> stagingCopy;
        {
            using namespace BufferUploads;
            BufferDesc desc;
            desc._type = BufferDesc::Type::LinearBuffer;
            desc._bindFlags = BindFlag::StreamOutput;
            desc._cpuAccess = 0;
            desc._gpuAccess = GPUAccess::Write;
            desc._allocationRules = 0;
            desc._linearBufferDesc._sizeInBytes = resultsBufferSize;
            desc._linearBufferDesc._structureByteSize = 0;
            XlCopyString(desc._name, "TriangleResult");

            auto pkt = BufferUploads::CreateEmptyPacket(desc);
            XlSetMemory(pkt->GetData(), 0x0, pkt->GetDataSize());

            auto& uploads = *GetBufferUploads();
            auto resource = uploads.Transaction_Immediate(desc, pkt.get())->AdoptUnderlying();

            gpuOutput = QueryInterfaceCast<ID3D::Buffer>(resource.get());
            if (gpuOutput) {
                ID3D11Buffer* bufferPtr = gpuOutput.get();
                unsigned offsets = 0;
                context->GetUnderlying()->SOSetTargets(1, &bufferPtr, &offsets);

                    //  make a staging buffer copy of this resource -- so we can access the
                    //  results from the CPU
                desc._cpuAccess = CPUAccess::Read;
                desc._gpuAccess = 0;
                desc._bindFlags = 0;
                resource = uploads.Transaction_Immediate(desc, nullptr)->AdoptUnderlying();

                stagingCopy = QueryInterfaceCast<ID3D::Buffer>(resource.get());
            }
        }

        struct RayTestBuffer
        {
            Float3 _rayStart; float _dummy0;
            Float3 _rayEnd; float _dummy1;
        } rayTestBuffer = { ray.first, 0.f, ray.second, 0.f };
        context->BindGS(MakeResourceList(2, ConstantBuffer(&rayTestBuffer, sizeof(rayTestBuffer))));

        state.EnterState(context, parserContext, TerrainMaterialTextures(), TerrainRenderingContext::Mode_RayTest);
        _pimpl->_renderer->Render(context, parserContext, state);
        state.ExitState(context, parserContext);

        {
                // clear SO targets
            ID3D11Buffer* bufferPtr = nullptr; unsigned offsets = 0;
            context->GetUnderlying()->SOSetTargets(0, &bufferPtr, &offsets);
        }

        unsigned resultCount = 0;
        if (gpuOutput && stagingCopy) {
            context->GetUnderlying()->CopyResource(stagingCopy.get(), gpuOutput.get());

            D3D11_MAPPED_SUBRESOURCE subres;
            auto hresult = context->GetUnderlying()->Map(stagingCopy.get(), 0, D3D11_MAP_READ, 0, &subres);
            if (SUCCEEDED(hresult) && subres.pData) {
                    //  results are in the buffer we mapped... But how do we know how may
                    //  results there are? There is a counter associated with the buffer, but it's
                    //  inaccessible to us. Just read along until we get a zero.
                Float4* resultArray = (Float4*)subres.pData;
                Float4* res = resultArray;
                while (res < PtrAdd(subres.pData, resultsBufferSize)) {
                    if ((*res)[0] == 0.f)
                        break;
                    ++res;
                }
                resultCount = unsigned(res - resultArray);
                std::sort(resultArray, &resultArray[resultCount], 
                    [](const Float4& lhs, const Float4& rhs) { return lhs[0] < rhs[0]; });

                for (unsigned c=0; c<std::min(resultCount, maxIntersections); ++c) {
                    intersections[c]._intersectionPoint = 
                        LinearInterpolate(ray.first, ray.second, resultArray[c][0]);
                    intersections[c]._cellCoordinates = Float2(resultArray[c][1], resultArray[c][2]);
                    intersections[c]._fullTerrainCoordinates = Float2(0.f, 0.f);    // (we don't actually know which node it hit yet)
                }

                context->GetUnderlying()->Unmap(stagingCopy.get(), 0);
            }
        }

        return resultCount;
    }

    void TerrainManager::SetWorldSpaceOrigin(const Float3& origin)
    {
        auto change = origin - _pimpl->_coords.TerrainOffset();
        _pimpl->_coords.SetTerrainOffset(origin);

            //  We have to update the _cellToWorld transforms
            //  Note that we could get some floating point creep if we
            //  do this very frequently! This method is fine for tools, but
            //  could be a problem if attempting to move the terrain origin
            //  in-game.
        for (auto& i:_pimpl->_cells) {
            Combine_InPlace(i._id._cellToWorld, change);
            i._id._aabbMin += change;
            i._id._aabbMax += change;
        }
    }

    Float2  TerrainCoordinateSystem::WorldSpaceToTerrainCoords(const Float2& worldSpacePosition) const
    {
        return Float2(
            (worldSpacePosition[0] - _terrainOffset[0]) * float(_config.NodeDimensionsInElements()[0]) / _nodeSizeMeters, 
            (worldSpacePosition[1] - _terrainOffset[1]) * float(_config.NodeDimensionsInElements()[1]) / _nodeSizeMeters);
    }

    float TerrainCoordinateSystem::WorldSpaceDistanceToTerrainCoords(float distance) const
    {
            //  if the scale factors for X and Y are different, we can only end up with an
            //  approximation (since we don't know if this distance is the distance along
            //  a straight line, or around a curve, or what direction it falls... etc)
        float scale = std::min(
            float(_config.NodeDimensionsInElements()[0]) / _nodeSizeMeters, 
            float(_config.NodeDimensionsInElements()[1]) / _nodeSizeMeters);

        return scale * distance;
    }

    Float2  TerrainCoordinateSystem::TerrainCoordsToWorldSpace(const Float2& terrainCoords) const
    {
            //  calculate world space coords (excluding height) that correspond
            //  to the terrain coords given.
            //  Note that the returned "Z" is always 0.f
        return Float2(
            terrainCoords[0] * _nodeSizeMeters / float(_config.NodeDimensionsInElements()[0]) + _terrainOffset[0], 
            terrainCoords[1] * _nodeSizeMeters / float(_config.NodeDimensionsInElements()[1]) + _terrainOffset[1]);
    }

    Float3      TerrainCoordinateSystem::TerrainOffset() const { return _terrainOffset; }
    void        TerrainCoordinateSystem::SetTerrainOffset(const Float3& newOffset) { _terrainOffset = newOffset; }

    Float2 TerrainConfig::TerrainCoordsToCellBasedCoords(const Float2& terrainCoords) const
    {
        auto t = NodeDimensionsInElements();
        auto t2 = CellDimensionsInNodes();
        return Float2(terrainCoords[0] / float(t[0] * t2[0]), terrainCoords[1] / float(t[1] * t2[1]));
    }

    Float2 TerrainConfig::CellBasedCoordsToTerrainCoords(const Float2& cellBasedCoords) const
    {
        auto t = NodeDimensionsInElements();
        auto t2 = CellDimensionsInNodes();
        return Float2(cellBasedCoords[0] * float(t[0]*t2[0]), cellBasedCoords[1] * float(t[1]*t2[1]));
    }

    UInt2 TerrainConfig::CellDimensionsInNodes() const
    {
        unsigned t = 1<<(_cellTreeDepth-1);
        return UInt2(t, t);
    }

    UInt2 TerrainConfig::NodeDimensionsInElements() const
    {
        return UInt2(_nodeDimsInElements, _nodeDimsInElements);
    }

    TerrainConfig::TerrainConfig(
		const ::Assets::rstring& baseDir, UInt2 cellCount,
        Filenames filenamesMode, 
        unsigned nodeDimsInElements, unsigned cellTreeDepth, unsigned nodeOverlap)
    : _baseDir(baseDir), _cellCount(cellCount), _filenamesMode(filenamesMode)
    , _nodeDimsInElements(nodeDimsInElements), _cellTreeDepth(cellTreeDepth), _nodeOverlap(nodeOverlap) 
    {
        {
            ::Assets::ResChar buffer[MaxPath];
            XlConcatPath(buffer, dimof(buffer), _baseDir.c_str(), "terraintextures/textures.txt");
            _textureCfgName = buffer;
        }
    }

    TerrainConfig::TerrainConfig(const std::string& baseDir)
    : _baseDir(baseDir), _filenamesMode(XLE)
    , _cellCount(0,0), _nodeDimsInElements(0)
    , _cellTreeDepth(0)
    {
        size_t fileSize = 0;
        auto sourceFile = LoadFileAsMemoryBlock(StringMeld<MaxPath>() << baseDir << "\\world.cfg", &fileSize);

        Data data;
        data.Load((const char*)sourceFile.get(), int(fileSize));
        auto* c = data.ChildWithValue("TerrainConfig");
        if (c) {
            auto* filenames = c->StrAttribute("Filenames");
            if (!XlCompareStringI(filenames, "Legacy")) { _filenamesMode = Legacy; }

            _nodeDimsInElements = c->IntAttribute("NodeDims", _nodeDimsInElements);
            _cellTreeDepth = c->IntAttribute("CellTreeDepth", _cellTreeDepth);
            _nodeOverlap = c->IntAttribute("NodeOverlap", _nodeOverlap);

            _cellCount = Deserialize(c, "CellCount", _cellCount);
        }

        {
            ::Assets::ResChar buffer[MaxPath];
            XlConcatPath(buffer, dimof(buffer), _baseDir.c_str(), "terraintextures/textures.txt");
            _textureCfgName = buffer;
        }
    }

    void TerrainConfig::Save()
    {
            // write this configuration file out to disk
            //  simple serialisation via the "Data" class
        auto terrainConfig = std::make_unique<Data>("TerrainConfig");
        if (_filenamesMode == Legacy)   { terrainConfig->SetAttribute("Filenames", "Legacy"); }
        else                            { terrainConfig->SetAttribute("Filenames", "XLE"); }
        terrainConfig->SetAttribute("NodeDims", _nodeDimsInElements);
        terrainConfig->SetAttribute("CellTreeDepth", _cellTreeDepth);
        terrainConfig->SetAttribute("NodeOverlap", _nodeOverlap);

        auto cellCount = std::make_unique<Data>("CellCount");
        cellCount->Add(new Data(StringMeld<32>() << _cellCount[0]));
        cellCount->Add(new Data(StringMeld<32>() << _cellCount[1]));
        terrainConfig->Add(cellCount.release());

        auto parentNode = std::make_unique<Data>();
        parentNode->Add(terrainConfig.release());
        parentNode->Save(StringMeld<MaxPath>() << _baseDir << "\\world.cfg");
    }

    const TerrainCoordinateSystem&  TerrainManager::GetCoords() const       { return _pimpl->_coords; }
    TerrainUberSurfaceInterface* TerrainManager::GetUberSurfaceInterface()  { return _pimpl->_uberSurfaceInterface.get(); }
    ISurfaceHeightsProvider* TerrainManager::GetHeightsProvider()           { return _pimpl->_heightsProvider.get(); }

    const TerrainConfig& TerrainManager::GetConfig() const
    {
        return _pimpl->_cfg;
    }
    const std::shared_ptr<ITerrainFormat>& TerrainManager::GetFormat() const
    {
        return _pimpl->_ioFormat;
    }
}


