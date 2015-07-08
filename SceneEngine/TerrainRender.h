// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainCoverageId.h"
#include "TextureTileSet.h"
#include "../RenderCore/Metal/Forward.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Metal/State.h"
#include "../Math/Vector.h"
#include "../Math/Matrix.h"
#include "../Utility/PtrUtils.h"
#include "../Core/Types.h"

namespace SceneEngine
{
    class LightingParserContext;
    class ITerrainFormat;
    using CoverageFormat = RenderCore::Metal::NativeFormat::Enum;

    class TerrainCellId
    {
    public:
        static const unsigned MaxCoverageCount = 5;
        char                _heightMapFilename[256];
        char                _coverageFilename[MaxCoverageCount][256];
        TerrainCoverageId   _coverageIds[MaxCoverageCount];
        Float4x4            _cellToWorld;
        Float3              _aabbMin, _aabbMax;

        class UberSurfaceAddress
        {
        public:
            UInt2 _mins, _maxs;
        };
        UberSurfaceAddress  _heightsToUber;
        UberSurfaceAddress  _coverageToUber[MaxCoverageCount];

        uint64      BuildHash() const;
        
        TerrainCellId()
        {
            _heightMapFilename[0] = '\0';
            for (unsigned c=0; c<MaxCoverageCount; ++c) {
                _coverageFilename[c][0] = '\0';
                _coverageIds[c] = ~TerrainCoverageId(0x0);
            }
            _cellToWorld = Identity<Float4x4>();
            _aabbMin = _aabbMax = Float3(0.f, 0.f, 0.f);
        }
    };


    class TerrainRenderingContext;
    class TerrainCollapseContext;
    class TerrainCell;
    class TerrainCellTexture;
    class ShortCircuitUpdate;

    class TerrainRendererConfig
    {
    public:
        class Layer
        {
        public:
            Int2 _tileSize;
            unsigned _cachedTileCount;
            CoverageFormat _format;
        };
        Layer _heights;
        std::vector<std::pair<TerrainCoverageId, Layer>> _coverageLayers;
    };

    bool IsCompatible(const TerrainRendererConfig& lhs, const TerrainRendererConfig& rhs);

///////////////////////////////////////////////////////////////////////////////////////////////////

    class TerrainCellRenderer
    {
    public:
        void CullNodes( RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext, 
                        TerrainRenderingContext& terrainContext, TerrainCollapseContext& collapseContext,
                        const TerrainCellId& cell);
        void WriteQueuedNodes(TerrainRenderingContext& renderingContext, TerrainCollapseContext& collapseContext);
        void CompletePendingUploads();
        void QueueUploads(TerrainRenderingContext& terrainContext);
        void Render(RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext, TerrainRenderingContext& terrainContext);

        Int2 GetHeightsElementSize() const                  { return _heightMapTileSet->GetTileSize(); }
        const TerrainCoverageId* GetCoverageIds() const     { return AsPointer(_coverageIds.cbegin()); }
        const CoverageFormat* GetCoverageFmts() const       { return AsPointer(_coverageFmts.cbegin()); }
        unsigned GetCoverageLayersCount() const             { return (unsigned)_coverageIds.size(); }
        const TerrainRendererConfig& GetConfig() const      { return _cfg; }

        void ShortCircuit(uint64 cellHash, TerrainCoverageId layerId, UInt2 cellOrigin, UInt2 cellMax, const ShortCircuitUpdate& upd);
        const bool IsShortCircuitAllowed() const { return _shortCircuitAllowed; }

        TerrainCellRenderer(
            const TerrainRendererConfig& cfg,
            std::shared_ptr<ITerrainFormat> ioFormat,
            bool allowShortCircuitModification);
        ~TerrainCellRenderer();

    private:
        class NodeCoverageInfo
        {
        public:
            TextureTile _tile;
            TextureTile _pendingTile;

            void Queue(TextureTileSet& coverageTileSet, const void* filePtr, unsigned fileOffset, unsigned fileSize);
            bool CompleteUpload(BufferUploads::IManager& bufferUploads);
            void EndTransactions(BufferUploads::IManager& bufferUploads);

            NodeCoverageInfo();
            NodeCoverageInfo(NodeCoverageInfo&& moveFrom);
            NodeCoverageInfo& operator=(NodeCoverageInfo&& moveFrom);
        };

        class CellRenderInfo
        {
        public:
            CellRenderInfo(const TerrainCell& cell, const TerrainCellTexture* const* cellCoverageBegin, const TerrainCellTexture* const* cellCoverageEnd);
            CellRenderInfo(CellRenderInfo&& moveFrom);
            CellRenderInfo& operator=(CellRenderInfo&& moveFrom) throw();
            ~CellRenderInfo();

            bool CompleteUpload(uint32 uploadId, BufferUploads::IManager& bufferUploads);

            const TerrainCell* _sourceCell;       // unguarded ptr... Perhaps keep a reference count?

                // height map
            const void* _heightMapStreamingFilePtr;
            std::vector<NodeCoverageInfo> _heightTiles;

                // coverage layers
            class CoverageLayer
            {
            public:
                const TerrainCellTexture*       _source;
                const void*                     _streamingFilePtr;
                std::vector<NodeCoverageInfo>   _tiles;
            };
            std::vector<CoverageLayer> _coverage;

        private:
            CellRenderInfo(const CellRenderInfo&) = delete;
            CellRenderInfo& operator=(const CellRenderInfo& ) = delete;
        };

        typedef std::pair<uint64, std::unique_ptr<CellRenderInfo>> CRIPair;

        std::unique_ptr<TextureTileSet> _heightMapTileSet;
        std::vector<std::unique_ptr<TextureTileSet>> _coverageTileSet;
        std::vector<TerrainCoverageId>  _coverageIds;
        std::vector<CoverageFormat>     _coverageFmts;
        std::vector<CRIPair>            _renderInfos;

        typedef std::pair<CellRenderInfo*, uint32> UploadPair;
        std::vector<UploadPair>         _pendingUploads;

        std::shared_ptr<ITerrainFormat> _ioFormat;
        TerrainRendererConfig           _cfg;

        bool                            _shortCircuitAllowed;

        friend class TerrainRenderingContext;
        friend class TerrainCollapseContext;
        friend class TerrainSurfaceHeightsProvider;

        void    CullNodes(RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext, TerrainRenderingContext& terrainContext, CellRenderInfo& cellRenderInfo, const Float4x4& localToWorld);
        void    RenderNode(RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext, TerrainRenderingContext& terrainContext, CellRenderInfo& cellRenderInfo, unsigned absNodeIndex, int8 neighbourLodDiffs[4]);

        void    CullNodes(
            TerrainRenderingContext& terrainContext, TerrainCollapseContext& collapseContext,
            const Float4x4& worldToProjection, const Float3& viewPositionWorld,
            CellRenderInfo& cellRenderInfo, const Float4x4& cellToWorld);

        void    ShortCircuitTileUpdate(const TextureTile& tile, unsigned layerIndex, UInt2 nodeMin, UInt2 nodeMax, unsigned downsample, bool encodedGradientFlags, Float4x4& localToCell, const ShortCircuitUpdate& upd);

        auto    BuildQueuedNodeFlags(const CellRenderInfo& cellRenderInfo, unsigned nodeIndex, unsigned lodField) const -> unsigned;

        TerrainCellRenderer(const TerrainCellRenderer&);
        TerrainCellRenderer& operator=(const TerrainCellRenderer&);
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////////////////////////

    class TerrainMaterialTextures;

    class TerrainRenderingContext
    {
    public:
        RenderCore::Metal::ConstantBuffer  _tileConstantsBuffer;
        RenderCore::Metal::ConstantBuffer  _localTransformConstantsBuffer;
        RenderCore::Metal::ViewportDesc    _currentViewport;
        unsigned        _indexDrawCount;

        // Int2            _elementSize;
        bool            _dynamicTessellation;
        bool            _encodedGradientFlags;

        TerrainCoverageId   _coverageLayerIds[TerrainCellId::MaxCoverageCount];
        CoverageFormat      _coverageFmts[TerrainCellId::MaxCoverageCount];
        unsigned            _coverageLayerCount;
        
        struct QueuedNode
        {
            TerrainCellRenderer::CellRenderInfo* _cell;
            unsigned    _fieldIndex;
            unsigned    _absNodeIndex;
            float       _priority;
            Float4x4    _cellToWorld;
            int8        _neighbourLODDiff[4];   // top, left, bottom, right

            struct Flags { 
                enum Enum { 
                    HasValidData = 1<<0, NeedsHeightMapUpload = 1<<1, 
                    NeedsCoverageUpload0 = 1<<2, NeedsCoverageUpload1 = 1<<3, NeedsCoverageUpload2 = 1<<4, NeedsCoverageUpload3 = 1<<5,
                    NeedsCoverageUploadMask = 0xFC }; 
                typedef unsigned BitField; 
            };
            Flags::BitField _flags;
        };
        static std::vector<QueuedNode> _queuedNodes;        // HACK -- static to avoid allocation!

        TerrainRenderingContext(
            const TerrainCoverageId* coverageLayers, 
            const CoverageFormat* coverageFmts, 
            unsigned coverageLayerCount,
            bool encodedGradientFlags);

        enum Mode { Mode_Normal, Mode_RayTest, Mode_VegetationPrepare };

        void    EnterState(RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext, const TerrainMaterialTextures& materials, UInt2 elementSize, Mode mode = Mode_Normal);
        void    ExitState(RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext);
    };

    void DoShortCircuitUpdate(
        uint64 cellHash, TerrainCoverageId layerId, std::weak_ptr<TerrainCellRenderer> renderer,
        TerrainCellId::UberSurfaceAddress uberAddress, const ShortCircuitUpdate& upd);
}

