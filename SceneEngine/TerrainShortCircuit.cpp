// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainRender.h"
#include "TextureTileSet.h"
#include "TerrainUberSurface.h"
#include "TerrainScaffold.h"

#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/InputLayout.h"

#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/DataPacket.h"
#include "../BufferUploads/ResourceLocator.h"

#include "../Math/Transformations.h"
#include "../Assets/Assets.h"
#include "../Utility/StringFormat.h"

namespace SceneEngine
{
    using namespace RenderCore;

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

    static BufferUploads::BufferDesc RWTexture2DDesc(unsigned width, unsigned height, Metal::NativeFormat::Enum format)
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

    void TerrainCellRenderer::ShortCircuitTileUpdate(
        const TextureTile& tile, unsigned coverageLayerIndex, 
        UInt2 nodeMin, UInt2 nodeMax, unsigned downsample, bool encodedGradientFlags,
        Float4x4& localToCell, const ShortCircuitUpdate& upd)
    {
        TRY 
        {
            unsigned format = 0;
            TextureTileSet* tileSet = _heightMapTileSet.get();
            if (coverageLayerIndex < _coverageTileSet.size()) {
                tileSet = _coverageTileSet[coverageLayerIndex].get();
                format = tileSet->GetFormat();
            }
            if (!tileSet) return;

            const auto Filter_Bilinear = 1u;
            const auto Filter_Max = 2u;
            unsigned filterType = Filter_Bilinear;
            if (format == 62) filterType = Filter_Max;      // (use "max" filter for integer types)

            const ::Assets::ResChar firstPassShader[] = "game/xleres/ui/copyterraintile.sh:WriteToMidway:cs_*";
            const ::Assets::ResChar secondPassShader[] = "game/xleres/ui/copyterraintile.sh:CommitToFinal:cs_*";
            StringMeld<64, char> defines; 
            defines << "VALUE_FORMAT=" << format << ";FILTER_TYPE=" << filterType;
            if (encodedGradientFlags) defines << ";ENCODED_GRADIENT_FLAGS=1";
            const auto compressedHeightMask = CompressedHeightMask(encodedGradientFlags);

            auto& byteCode = ::Assets::GetAssetDep<CompiledShaderByteCode>(firstPassShader, defines.get());
            auto& cs0 = ::Assets::GetAssetDep<Metal::ComputeShader>(firstPassShader, defines.get());
            auto& cs1 = ::Assets::GetAssetDep<Metal::ComputeShader>(secondPassShader, defines.get());
            auto& cs2 = ::Assets::GetAssetDep<Metal::ComputeShader>("game/xleres/ui/copyterraintile.sh:DirectToFinal:cs_*", defines.get());

            float temp = FLT_MAX;
            const float heightOffsetValue = 5000.f; // (height values are shifted by this constant in the shader to get around issues with negative heights
            const float elementSpacing = 2.f;       // used when calculating the gradient flags; represents the distance between grid elements in world space units
            struct TileCoords
            {
                float minHeight, heightScale;
                unsigned workingMinHeight, workingMaxHeight;
                float elementSpacing; float heightOffsetValue;
                unsigned dummy[2];
            } tileCoords = { 
                localToCell(2, 3), localToCell(2, 2), 
                *reinterpret_cast<unsigned*>(&temp), 0x0u,
                elementSpacing, heightOffsetValue,
                0, 0
            };

            auto& uploads = tileSet->GetBufferUploads();
            auto tileCoordsBuffer = uploads.Transaction_Immediate(
                RWBufferDesc(sizeof(TileCoords), sizeof(TileCoords)), 
                BufferUploads::CreateBasicPacket(sizeof(tileCoords), &tileCoords).get())->AdoptUnderlying();
            Metal::UnorderedAccessView tileCoordsUAV(tileCoordsBuffer.get());

            struct Parameters
            {
                Int2 _sourceMin, _sourceMax;
                Int2 _updateMin, _updateMax;
                Int3 _dstTileAddress;
                int _sampleArea;
                UInt2 _tileSize;
                unsigned _dummy[2];
            } parameters = {
                Int2(upd._resourceMins) - Int2(nodeMin),
                Int2(upd._resourceMaxs) - Int2(nodeMin),
                Int2(upd._updateAreaMins) - Int2(nodeMin),
                Int2(upd._updateAreaMaxs) - Int2(nodeMin),
                Int3(tile._x, tile._y, tile._arrayIndex),
                1<<downsample, Int2(tile._width, tile._height)
            };
            Metal::ConstantBufferPacket pkts[] = { RenderCore::MakeSharedPkt(parameters) };
            const Metal::ShaderResourceView* srv[] = { upd._srv.get(), &tileSet->GetShaderResource() };

            auto context = RenderCore::Metal::DeviceContext::Get(*upd._context);

            Metal::BoundUniforms boundLayout(byteCode);
            boundLayout.BindConstantBuffers(1, {"Parameters"});
            boundLayout.BindShaderResources(1, {"Input", "OldHeights"});
            boundLayout.Apply(*context, Metal::UniformsStream(), Metal::UniformsStream(pkts, srv));

            const unsigned threadGroupWidth = 6;
            if (format == 0) {
                    // go via a midway buffer and handle the min/max quantization
                auto midwayBuffer = uploads.Transaction_Immediate(
                    RWTexture2DDesc(tile._width, tile._height, Metal::NativeFormat::R32_FLOAT))->AdoptUnderlying();
                Metal::UnorderedAccessView midwayBufferUAV(midwayBuffer.get());

                auto midwayGradFlagsBuffer = uploads.Transaction_Immediate(
                    RWTexture2DDesc(tile._width, tile._height, Metal::NativeFormat::R32_UINT))->AdoptUnderlying();
                Metal::UnorderedAccessView midwayGradFlagsBufferUAV(midwayGradFlagsBuffer.get());

                context->BindCS(MakeResourceList(1, midwayBufferUAV, midwayGradFlagsBufferUAV, tileCoordsUAV));

                context->Bind(cs0);
                context->Dispatch(   unsigned(XlCeil(tile._width /float(threadGroupWidth))), 
                                    unsigned(XlCeil(tile._height/float(threadGroupWidth))));

                    //  if everything is ok up to this point, we can commit to the final
                    //  output --
                context->BindCS(MakeResourceList(tileSet->GetUnorderedAccessView()));
                context->Bind(cs1);
                context->Dispatch(   unsigned(XlCeil(tile._width /float(threadGroupWidth))), 
                                    unsigned(XlCeil(tile._height/float(threadGroupWidth))));

                    //  We need to read back the new min/max heights
                    //  we could write these back to the original terrain cell -- but it
                    //  would be better to keep them cached only in the NodeRenderInfo
                auto readback = uploads.Resource_ReadBack(BufferUploads::ResourceLocator(tileCoordsBuffer.get()));
                float* readbackData = (float*)readback->GetData();
                if (readbackData) {
                    float newHeightOffset = readbackData[2] - heightOffsetValue;
                    float newHeightScale = (readbackData[3] - readbackData[2]) / float(compressedHeightMask);
                    localToCell(2,2) = newHeightScale;
                    localToCell(2,3) = newHeightOffset;
                }
            } else {
                    // just write directly
                context->BindCS(MakeResourceList(tileSet->GetUnorderedAccessView()));
                context->Bind(cs2);
                context->Dispatch(   unsigned(XlCeil(tile._width /float(threadGroupWidth))), 
                                    unsigned(XlCeil(tile._height/float(threadGroupWidth))));
            }

            context->UnbindCS<Metal::UnorderedAccessView>(0, 3);
        } CATCH (...) {
            // note, it's a real problem when we get a invalid resource get... 
            //  We should ideally stall until all the required resources are loaded
        } CATCH_END
    }

    void    TerrainCellRenderer::ShortCircuit(uint64 cellHash, TerrainCoverageId layerId, UInt2 cellOrigin, UInt2 cellMax, const ShortCircuitUpdate& upd)
    {
            //      We need to find the CellRenderInfo objects associated with the terrain cell with this name.
            //      Then, for any completed height map tiles within that object, we must copy in the data
            //      from our update information (sometimes doing the downsample along the way).
            //      This will update the tiles with new data, without hitting the disk or requiring a re-upload

        auto i = LowerBound(_renderInfos, cellHash);
        if (i == _renderInfos.end() || i->first != cellHash) return;

        auto& cri = *i->second;
        auto& sourceCell = *i->second->_sourceCell;

        TextureTileSet* tileSet = nullptr;
        std::vector<NodeCoverageInfo>* tiles = nullptr;
        unsigned coverageLayerIndex = ~unsigned(0);
        
        if (layerId == CoverageId_Heights) {
            tileSet = _heightMapTileSet.get();
            tiles = &cri._heightTiles;
        } else {
            for (unsigned c=0; c<_coverageIds.size(); ++c)
                if (_coverageIds[c] == layerId) { coverageLayerIndex = c; break; }
            if (coverageLayerIndex >= unsigned(_coverageTileSet.size())) return;
            tileSet = _coverageTileSet[coverageLayerIndex].get();
            tiles = &cri._coverage[coverageLayerIndex]._tiles;
        }

        if (!tileSet || !tiles) return;

            //  Got a match. Find all with completed tiles (ignoring the pending tiles) and 
            //  write over that data.
        for (auto ni=tiles->begin(); ni!=tiles->end(); ++ni) {

                // todo -- cancel any pending tiles, because they can cause problems

            if (tileSet->IsValid(ni->_tile)) {
                auto nodeIndex = std::distance(tiles->begin(), ni);
                auto& sourceNode = sourceCell._nodes[nodeIndex];

                    //  We need to transform the coordinates for this node into
                    //  the uber-surface coordinate system. If there's an overlap
                    //  between the node coords and the update box, we need to do
                    //  a copy.
                const unsigned compressedHeightMask = CompressedHeightMask(sourceCell.EncodedGradientFlags());

                Float3 nodeMinInCell = TransformPoint(sourceNode->_localToCell, Float3(0.f, 0.f, 0.f));
                Float3 nodeMaxInCell = TransformPoint(sourceNode->_localToCell, Float3(1.f, 1.f, float(compressedHeightMask)));

                UInt2 nodeMin(
                    (unsigned)LinearInterpolate(float(cellOrigin[0]), float(cellMax[0]), nodeMinInCell[0]),
                    (unsigned)LinearInterpolate(float(cellOrigin[1]), float(cellMax[1]), nodeMinInCell[1]));
                UInt2 nodeMax(
                    (unsigned)LinearInterpolate(float(cellOrigin[0]), float(cellMax[0]), nodeMaxInCell[0]),
                    (unsigned)LinearInterpolate(float(cellOrigin[1]), float(cellMax[1]), nodeMaxInCell[1]));

                const int overlap = 1;
                if (    (int(nodeMin[0])-overlap) <= int(upd._updateAreaMaxs[0]) && (int(nodeMax[0])+overlap) >= int(upd._updateAreaMins[0])
                    &&  (int(nodeMin[1])-overlap) <= int(upd._updateAreaMaxs[1]) && (int(nodeMax[1])+overlap) >= int(upd._updateAreaMins[1])) {

                        // downsampling required depends on which field we're in.
                    auto fi = std::find_if(sourceCell._nodeFields.cbegin(), sourceCell._nodeFields.cend(),
                        [=](const TerrainCell::NodeField& field) { return unsigned(nodeIndex) >= field._nodeBegin && unsigned(nodeIndex) < field._nodeEnd; });
                    size_t fieldIndex = std::distance(sourceCell._nodeFields.cbegin(), fi);
                    unsigned downsample = unsigned(4-fieldIndex);

                    ShortCircuitTileUpdate(ni->_tile, coverageLayerIndex, nodeMin, nodeMax, downsample, sourceCell.EncodedGradientFlags(), sourceNode->_localToCell, upd);

                }
            }
        }
    }


    void DoShortCircuitUpdate(
        uint64 cellHash, TerrainCoverageId layerId, std::weak_ptr<TerrainCellRenderer> renderer,
        TerrainCellId::UberSurfaceAddress uberAddress, const ShortCircuitUpdate& upd)
    {
        auto r = renderer.lock();
        if (r)
            r->ShortCircuit(cellHash, layerId, uberAddress._mins, uberAddress._maxs, upd);
    }
}

