// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ResourceUtils.h"
#include "ResourceDesc.h"
#include "Format.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/PtrUtils.h"
#include <algorithm>

namespace RenderCore
{
    bool operator==(const Box2D& lhs, const Box2D& rhs) 
    {
        return      lhs._left == rhs._left
                &&  lhs._top == rhs._top
                &&  lhs._right == rhs._right
                &&  lhs._bottom == rhs._bottom
                ;
    }

    static const unsigned BlockCompDim = 4;
    static unsigned    RoundBCDim(unsigned input)
    {
        auto result = (input + 3) & ~3;
        assert(!(result%BlockCompDim));
        return result;
    }

    static bool IsBlockCompressed(Format format) { return GetCompressionType(format) == FormatCompressionType::BlockCompression; }

    unsigned CopyMipLevel(
        void* destination, size_t destinationDataSize, TexturePitches dstPitches,
        const TextureDesc& mipMapDesc,
        const SubResourceInitData& srcData)
    {
        // copy a single mip level in a single depth slice in a single array layer
        // todo -- we should expand this to support depth slices
        auto originalDest = destination;
        (void)originalDest;

        auto copiedBytes = 0u;
        if (dstPitches._rowPitch != srcData._pitches._rowPitch) {
            // unsigned sourceRowPitch;
            // unsigned rows;
            // if (isDXTCompressed) {
            //     sourceRowPitch = ByteCount(RoundBCDim(mipMapDesc._width), BlockCompDim, 1, 1, mipMapDesc._format);
            //     rows = (mipMapDesc._height + BlockCompDim - 1) / BlockCompDim;
            // } else {
            //     sourceRowPitch = ByteCount(mipMapDesc._width, 1, 1, 1, mipMapDesc._format);
            //     rows = mipMapDesc._height;
            // }

            auto sourceRowPitch = srcData._pitches._rowPitch;
            auto rows = mipMapDesc._height;
            if (IsBlockCompressed(mipMapDesc._format))
                rows /= BlockCompDim;  // in block compressed formats, we're dealing with 4x4 blocks of texels

            // prevent reading off the end of our src data, or writing past our dst data
            rows = std::min(rows, unsigned(srcData._size/sourceRowPitch));
            rows = std::min(rows, unsigned(destinationDataSize/dstPitches._rowPitch));

            auto sourceData = srcData._data;
            for (unsigned j = 0; j < rows; j++) {
                assert((size_t(destination) + sourceRowPitch - size_t(originalDest)) <= size_t(originalDest) + destinationDataSize);
                XlCopyMemoryAlign16(destination, sourceData, sourceRowPitch);
                sourceData = PtrAdd(sourceData, sourceRowPitch);
                destination = PtrAdd(destination, dstPitches._rowPitch);
            }

            copiedBytes = rows * sourceRowPitch;

        } else {
            int nPitch = ByteCount(mipMapDesc._width, 1, 1, 1, mipMapDesc._format);
            assert(srcData._size % nPitch == 0); (void)nPitch;
            assert(size_t(destination) + srcData._size <= size_t(originalDest) + destinationDataSize);
            XlCopyMemoryAlign16((uint8*)destination, srcData._data, srcData._size);
            copiedBytes = (unsigned)srcData._size;
        }

        return copiedBytes;
    }

    unsigned CopyMipLevel(
        void* destination, size_t destinationDataSize, TexturePitches dstPitches,
        const TextureDesc& dstDesc,
        const Box2D& dst2D,
        const SubResourceInitData& srcData)
    {
        // note -- we could simplify this by dividing widths and box dimensions by 4 when it's BlockCompressed
        //          (ie, just treat it as a smaller texture with larger pixels)
        auto blockCompressed = IsBlockCompressed(dstDesc._format);

        Box2D adjustedBox = dst2D;
        adjustedBox._top = std::min(adjustedBox._top, int(dstDesc._height));
        adjustedBox._bottom = std::min(adjustedBox._bottom, int(dstDesc._height));

        auto* srcIterator = srcData._data;
        if (adjustedBox._top < 0) {
            if (blockCompressed) {
                assert((-adjustedBox._top)%BlockCompDim == 0);
                srcIterator = PtrAdd(srcIterator, (-adjustedBox._top)/BlockCompDim * srcData._pitches._rowPitch);
            } else {
                srcIterator = PtrAdd(srcIterator, -adjustedBox._top * srcData._pitches._rowPitch);
            }
            adjustedBox._top = 0;
        }

        if (adjustedBox._bottom <= adjustedBox._top) return 0 ;

        if (blockCompressed)
            assert(adjustedBox._top%BlockCompDim == 0);

        auto bbp = BitsPerPixel(dstDesc._format);

        auto totalCopiedBytes = 0u;
        for (int r=adjustedBox._top; r<adjustedBox._bottom;) {
            auto left = dst2D._left;
            auto right = dst2D._right;
            auto* dstRowStart = PtrAdd(destination, r*dstPitches._rowPitch);
            auto* srcRowStart = srcIterator;

            if (left < 0) {
                if (blockCompressed) {
                    assert((-left)%BlockCompDim==0);
                    srcRowStart = PtrAdd(srcRowStart, (-left) / BlockCompDim * bbp * BlockCompDim * BlockCompDim / 8);
                } else {
                    srcRowStart = PtrAdd(srcRowStart, (-left) * bbp / 8);
                }
                left = 0;
            }
            right = std::min(right, int(dstDesc._width));

            if (left < right) {

                void* dstStart; size_t copyBytes;
                if (blockCompressed) {
                    assert(left%BlockCompDim==0 && right%BlockCompDim == 0);
                    dstStart = PtrAdd(dstRowStart, left / BlockCompDim * bbp * BlockCompDim * BlockCompDim / 8);
                    copyBytes = (right - left) / BlockCompDim * bbp * BlockCompDim * BlockCompDim / 8;
                } else {
                    dstStart = PtrAdd(dstRowStart, left * bbp / 8);
                    copyBytes = (right - left) * bbp / 8;
                }

                assert((size_t(dstStart) + copyBytes) <= (size_t(destination) + destinationDataSize));
                assert((size_t(srcRowStart) + copyBytes) <= (size_t(srcData._data) + srcData._size));
                XlCopyMemory(dstStart, srcRowStart, copyBytes);
                totalCopiedBytes += (unsigned)copyBytes;
            }

            r += blockCompressed ? BlockCompDim : 1;
            srcIterator = PtrAdd(srcIterator, srcData._pitches._rowPitch);
        }

        return totalCopiedBytes;
    }

    TextureDesc  CalculateMipMapDesc(const TextureDesc& topMostMipDesc, unsigned mipMapIndex)
    {
        assert(mipMapIndex<topMostMipDesc._mipCount);
        TextureDesc result = topMostMipDesc;
        result._width    = std::max(result._width  >> mipMapIndex, 1u); 
        result._height   = std::max(result._height >> mipMapIndex, 1u);
        if (IsBlockCompressed(topMostMipDesc._format)) { 
            result._width = RoundBCDim(result._width);
            result._height = RoundBCDim(result._height);
        }
        //result._depth  = std::max(minDimension, result._depth>>mipMapIndex); 
        result._mipCount -= uint8(mipMapIndex);
        return result;
    }
    
    unsigned ByteCount(unsigned nWidth, unsigned nHeight, unsigned nDepth, unsigned mipCount, Format format)
    {
        if (format == Format::Unknown) {
            return 0;
        }

        const bool dxt = IsBlockCompressed(format);
        const auto bbp = BitsPerPixel(format);

        mipCount = std::max(mipCount, 1u);
        unsigned result = 0;
        for (unsigned mipIterator = 0; (mipIterator < mipCount) && (nWidth || nHeight || nDepth); ++mipIterator) {
            if (dxt) {
                auto blockWidth = std::max((nWidth + BlockCompDim - 1u) / BlockCompDim, 1u);
                auto blockHeight = std::max((nHeight + BlockCompDim - 1u) / BlockCompDim, 1u);
                result += blockWidth * blockHeight * std::max(nDepth, 1u) * bbp * 16u / 8u;
            } else {
                result += std::max(nWidth, 1u) * std::max(nHeight, 1u) * std::max(nDepth, 1u) * bbp / 8u;
            }

            nWidth >>= 1;
            nHeight >>= 1;
            nDepth >>= 1;
        }

        return result;
    }

    unsigned ByteCount(const TextureDesc& tDesc)
    {
        return 
            ByteCount(tDesc._width, tDesc._height, tDesc._depth, tDesc._mipCount, tDesc._format) 
            * std::max(1u, unsigned(tDesc._arrayCount));
    }

    unsigned ByteCount(const ResourceDesc& desc)
    {
        if (desc._type == ResourceDesc::Type::LinearBuffer) {
            return desc._linearBufferDesc._sizeInBytes;
        } else if (desc._type == ResourceDesc::Type::Texture) {
            return ByteCount(desc._textureDesc);
        }
        return 0;
    }

    TexturePitches MakeTexturePitches(const TextureDesc& desc)
    {
        TexturePitches result = {0u, 0u, 0u};
        result._slicePitch = ByteCount(
            desc._width, desc._height, 1, 1, 
            desc._format);
            
            //  row pitch calculation is a little platform-specific here...
            //  (eg, DX9 and DX11 use different systems)
            //  Perhaps this could be moved into the platform interface layer
        bool isDXT = GetCompressionType(desc._format) 
            == RenderCore::FormatCompressionType::BlockCompression;
        if (isDXT) {
            result._rowPitch = ByteCount(
                RoundBCDim(desc._width), BlockCompDim, 1, 1, 
                desc._format);
        } else {
            result._rowPitch = ByteCount(
                desc._width, 1, 1, 1, 
				desc._format);
        }

        return result;
    }
}
