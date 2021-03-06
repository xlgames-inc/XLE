// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MeshDatabase.h"
#include "../Metal/Format.h"
#include "../../Math/Vector.h"
#include "../../Math/Math.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/IteratorUtils.h"
#include "../../Utility/BitUtils.h"
#include "../../Foreign/half-1.9.2/include/half.hpp"
#include <iterator>

namespace RenderCore { namespace Assets { namespace GeoProc
{
    using ::Assets::Exceptions::FormatError;

    enum class ComponentType { Float32, Float16, UNorm8 };
    static std::pair<ComponentType, unsigned> BreakdownFormat(Metal::NativeFormat::Enum fmt);
    static unsigned short AsFloat16(float input);
    static float AsFloat32(unsigned short f16input);

///////////////////////////////////////////////////////////////////////////////////////////////////

    unsigned    MeshDatabase::HasElement(const char name[]) const
    {
        unsigned result = 0;
        for (auto i = _streams.cbegin(); i != _streams.cend(); ++i) {
            if (!XlCompareStringI(i->GetSemanticName().c_str(), name)) {
                assert((result & (1 << i->GetSemanticIndex())) == 0);
                result |= (1 << i->GetSemanticIndex());
            }
        }
        return result;
    }

    unsigned    MeshDatabase::FindElement(const char name[], unsigned semanticIndex) const
    {
        for (auto i = _streams.cbegin(); i != _streams.cend(); ++i)
            if (i->GetSemanticIndex() == semanticIndex && !XlCompareStringI(i->GetSemanticName().c_str(), name))
                return unsigned(std::distance(_streams.cbegin(), i));
        return ~0u;
    }

    void        MeshDatabase::RemoveStream(unsigned elementIndex)
    {
        if (elementIndex < _streams.size())
            _streams.erase(_streams.begin() + elementIndex);
    }

    template<typename Type> 
        Type MeshDatabase::GetUnifiedElement(size_t vertexIndex, unsigned elementIndex) const
    {
        auto& stream = _streams[elementIndex];
        auto indexInStream = stream.GetVertexMap()[vertexIndex];
        auto& sourceData = stream.GetSourceData();
        return GetVertex<Type>(sourceData, indexInStream);
    }

    template Float3 MeshDatabase::GetUnifiedElement(size_t vertexIndex, unsigned elementIndex) const;
    template Float2 MeshDatabase::GetUnifiedElement(size_t vertexIndex, unsigned elementIndex) const;

    std::unique_ptr<uint32[]> MeshDatabase::BuildUnifiedVertexIndexToPositionIndex() const
    {
            //      Collada has this idea of "vertex index"; which is used to map
            //      on the vertex weight information. But that seems to be lost in OpenCollada.
            //      All we can do is use the position index as a subtitute.

        auto unifiedVertexIndexToPositionIndex = std::make_unique<uint32[]>(_unifiedVertexCount);
        
        if (!_streams[0].GetVertexMap().empty()) {
            for (size_t v=0; v<_unifiedVertexCount; ++v) {
                // assuming the first element is the position
                auto attributeIndex = _streams[0].GetVertexMap()[v];
                // assert(!_streams[0]._sourceData.IsValid() || attributeIndex < _mesh->getPositions().getValuesCount());
                unifiedVertexIndexToPositionIndex[v] = (uint32)attributeIndex;
            }
        } else {
            for (size_t v=0; v<_unifiedVertexCount; ++v) {
                unifiedVertexIndexToPositionIndex[v] = (uint32)v;
            }
        }

        return std::move(unifiedVertexIndexToPositionIndex);
    }

    void MeshDatabase::WriteStream(
        const Stream& stream,
        const void* dst, Metal::NativeFormat::Enum dstFormat, size_t dstStride, size_t dstSize) const
    {
        const auto& sourceData = stream.GetSourceData();
        auto stride = sourceData.GetStride();
        CopyVertexData(
            dst, dstFormat, dstStride, dstSize,
            sourceData.GetData(), sourceData.GetFormat(), stride, sourceData.GetDataSize(),
            (unsigned)_unifiedVertexCount, 
            stream.GetVertexMap(), sourceData.GetProcessingFlags());
    }

    DynamicArray<uint8>  MeshDatabase::BuildNativeVertexBuffer(const NativeVBLayout& outputLayout) const
    {
            //
            //      Write the data into the vertex buffer
            //
        auto size = outputLayout._vertexStride * _unifiedVertexCount;
        auto finalVertexBuffer = std::make_unique<uint8[]>(size);
        XlSetMemory(finalVertexBuffer.get(), 0, size);

        for (unsigned elementIndex = 0; elementIndex <_streams.size(); ++elementIndex) {
            const auto& nativeElement     = outputLayout._elements[elementIndex];
            const auto& stream            = _streams[elementIndex];
            WriteStream(
                stream, PtrAdd(finalVertexBuffer.get(), nativeElement._alignedByteOffset),
                nativeElement._nativeFormat, outputLayout._vertexStride,
                size - nativeElement._alignedByteOffset);
        }

        return DynamicArray<uint8>(std::move(finalVertexBuffer), size);
    }

    unsigned    MeshDatabase::AddStream(
        std::shared_ptr<IVertexSourceData> dataSource,
        std::vector<unsigned>&& vertexMap,
        const char semantic[], unsigned semanticIndex)
    {
        return InsertStream(~0u, dataSource, std::move(vertexMap), semantic, semanticIndex);
    }

    unsigned    MeshDatabase::InsertStream(
        unsigned insertionPosition,
        std::shared_ptr<IVertexSourceData> dataSource,
        std::vector<unsigned>&& vertexMap,
        const char semantic[], unsigned semanticIndex)
    {
        auto count = vertexMap.size() ? vertexMap.size() : dataSource->GetCount();
        assert(count > 0);
        if (!_unifiedVertexCount) { _unifiedVertexCount = count; }
        else _unifiedVertexCount = std::min(_unifiedVertexCount, count);

        if (insertionPosition == ~0u) {
            _streams.push_back(
            Stream { std::move(dataSource), std::move(vertexMap), semantic, semanticIndex });
            return unsigned(_streams.size()-1);
        } else {
            _streams.insert(
                _streams.begin()+insertionPosition,
                Stream { std::move(dataSource), std::move(vertexMap), semantic, semanticIndex });
            return insertionPosition;
        }
    }

    MeshDatabase::MeshDatabase()
    {
        _unifiedVertexCount = 0;
    }

    MeshDatabase::MeshDatabase(MeshDatabase&& moveFrom) never_throws
    : _streams(std::move(moveFrom._streams))
    , _unifiedVertexCount(moveFrom._unifiedVertexCount)
    {}

    MeshDatabase& MeshDatabase::operator=(MeshDatabase&& moveFrom) never_throws
    {
        _streams = std::move(moveFrom._streams);
        _unifiedVertexCount = moveFrom._unifiedVertexCount;
        return *this;
    }

    MeshDatabase::~MeshDatabase() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    MeshDatabase::Stream::Stream() { _semanticIndex = 0; }
    MeshDatabase::Stream::Stream(
        std::shared_ptr<IVertexSourceData> sourceData, std::vector<unsigned> vertexMap, 
        const std::string& semanticName, unsigned semanticIndex)
    : _sourceData(std::move(sourceData)), _vertexMap(std::move(vertexMap))
    , _semanticName(semanticName), _semanticIndex(semanticIndex)
    {}

    MeshDatabase::Stream::Stream(Stream&& moveFrom) never_throws
    : _sourceData(std::move(moveFrom._sourceData))
    , _vertexMap(std::move(moveFrom._vertexMap))
    , _semanticName(std::move(moveFrom._semanticName))
    , _semanticIndex(moveFrom._semanticIndex)
    {
    }

    auto MeshDatabase::Stream::operator=(Stream&& moveFrom) never_throws -> Stream&
    {
        _sourceData = std::move(moveFrom._sourceData);
        _vertexMap = std::move(moveFrom._vertexMap);
        _semanticName = std::move(moveFrom._semanticName);
        _semanticIndex = moveFrom._semanticIndex;
        return *this;
    }

    MeshDatabase::Stream::~Stream() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    static RenderCore::Metal::NativeFormat::Enum CalculateFinalVBFormat(const IVertexSourceData& source, const NativeVBSettings& settings)
    {
            //
            //      Calculate a native format that matches this source data.
            //      Actually, there are a limited number of relevant native formats.
            //      So, it's easy to find one that works.
            //
            //      We don't support doubles in vertex buffers. So we can only choose from
            //
            //          R32G32B32A32_FLOAT
            //          R32G32B32_FLOAT
            //          R32G32_FLOAT
            //          R32_FLOAT
            //
            //          (assuming R9G9B9E5_SHAREDEXP, etc, not valid for vertex buffers)
            //          R10G10B10A2_UNORM   (ok for DX 11.1 -- but DX11??)
            //          R10G10B10A2_UINT    (ok for DX 11.1 -- but DX11??)
            //          R11G11B10_FLOAT     (ok for DX 11.1 -- but DX11??)
            //
            //          R8G8B8A8_UNORM      (SRGB can't be used)
            //          R8G8_UNORM
            //          R8_UNORM
            //          B8G8R8A8_UNORM
            //          B8G8R8X8_UNORM
            //
            //          B5G6R5_UNORM        (on some hardware)
            //          B5G5R5A1_UNORM      (on some hardware)
            //          B4G4R4A4_UNORM      (on some hardware)
            //
            //          R16G16B16A16_FLOAT
            //          R16G16_FLOAT
            //          R16_FLOAT
            //
            //          (or UINT, SINT, UNORM, SNORM versions of the same thing)
            //

        auto brkdn = BreakdownFormat(source.GetFormat());
        if (!brkdn.second) return Metal::NativeFormat::Unknown;

        if (source.GetFormatHint() & FormatHint::IsColor) {
            if (brkdn.second == 1)          return Metal::NativeFormat::R8_UNORM;
            else if (brkdn.second == 2)     return Metal::NativeFormat::R8G8_UNORM;
            else                            return Metal::NativeFormat::R8G8B8A8_UNORM;
        }

        if (brkdn.first == ComponentType::UNorm8) {
                // consider also Metal::FindFormat
            if (brkdn.second == 1)          return Metal::NativeFormat::R8_UNORM;
            else if (brkdn.second == 2)     return Metal::NativeFormat::R8G8_UNORM;
            else                            return Metal::NativeFormat::R8G8B8A8_UNORM;
        } else {
            if (settings._use16BitFloats) {
                if (brkdn.second == 1)          return Metal::NativeFormat::R16_FLOAT;
                else if (brkdn.second == 2)     return Metal::NativeFormat::R16G16_FLOAT;
                else                            return Metal::NativeFormat::R16G16B16A16_FLOAT;
            } else {
                if (brkdn.second == 1)          return Metal::NativeFormat::R32_FLOAT;
                else if (brkdn.second == 2)     return Metal::NativeFormat::R32G32_FLOAT;
                else if (brkdn.second == 3)     return Metal::NativeFormat::R32G32B32_FLOAT;
                else                            return Metal::NativeFormat::R32G32B32A32_FLOAT;
            }
        }
    }

    NativeVBLayout BuildDefaultLayout(MeshDatabase& mesh, const NativeVBSettings& settings)
    {
        unsigned accumulatingOffset = 0;

        NativeVBLayout result;
        result._elements.resize(mesh.GetStreams().size());

        unsigned c=0;
        for (const auto&stream : mesh.GetStreams()) {
            auto& nativeElement = result._elements[c++];
            nativeElement._semanticName         = stream.GetSemanticName();
            nativeElement._semanticIndex        = stream.GetSemanticIndex();

                // Note --  There's a problem here with texture coordinates. Sometimes texture coordinates
                //          have 3 components in the Collada file. But only 2 components are actually used
                //          by mapping. The last component might just be redundant. The only way to know 
                //          for sure that the final component is redundant is to look at where the geometry
                //          is used, and how this vertex element is bound to materials. But in this function
                //          call we only have access to the "Geometry" object, without any context information.
                //          We don't yet know how it will be bound to materials.
            nativeElement._nativeFormat         = CalculateFinalVBFormat(stream.GetSourceData(), settings);
            nativeElement._inputSlot            = 0;
            nativeElement._alignedByteOffset    = accumulatingOffset;
            nativeElement._inputSlotClass       = Metal::InputClassification::PerVertex;
            nativeElement._instanceDataStepRate = 0;

            accumulatingOffset += Metal::BitsPerPixel(nativeElement._nativeFormat)/8;
        }

        result._vertexStride = accumulatingOffset;

            // DirectX seems to require vertex strides that are multiples of 4.
            // I haven't verified this in documentation. But very small strides (such
            // as 1) cause debug layer errors.
        result._vertexStride = CeilToMultiple(result._vertexStride, 4u);

        return std::move(result);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class RawVertexSourceDataAdapter : public IVertexSourceData
    {
    public:
        const void* GetData() const     { return AsPointer(_rawData.cbegin()); }
        size_t GetDataSize() const      { return _rawData.size(); }
        size_t GetStride() const        { return _stride; } // RenderCore::Metal::BitsPerPixel(_fmt) / 8; }
        size_t GetCount() const         { return _count; } // GetDataSize() / GetStride(); }

        RenderCore::Metal::NativeFormat::Enum GetFormat() const     { return _fmt; }
        ProcessingFlags::BitField   GetProcessingFlags() const      { return 0; }
        FormatHint::BitField        GetFormatHint() const           { return 0; }

        RawVertexSourceDataAdapter()    { _fmt = Metal::NativeFormat::Unknown; _count = _stride = 0; }
        RawVertexSourceDataAdapter(
            const void* start, const void* end, 
            size_t count, size_t stride,
            Metal::NativeFormat::Enum fmt)
        : _fmt(fmt), _rawData((const uint8*)start, (const uint8*)end), _count(count), _stride(stride) {}

        RawVertexSourceDataAdapter(
            std::vector<uint8>&& rawData, 
            size_t count, size_t stride,
            Metal::NativeFormat::Enum fmt)
        : _rawData(std::move(rawData)), _fmt(fmt), _count(count), _stride(stride) {}

    protected:
        std::vector<uint8>              _rawData;
        Metal::NativeFormat::Enum       _fmt;
        size_t                          _count, _stride;
    };

    IVertexSourceData::~IVertexSourceData() {}

    std::shared_ptr<IVertexSourceData>
        CreateRawDataSource(
            const void* dataBegin, const void* dataEnd, 
            size_t count, size_t stride,
            Metal::NativeFormat::Enum srcFormat)
    {
        return std::make_shared<RawVertexSourceDataAdapter>(dataBegin, dataEnd, count, stride, srcFormat);
    }

    std::shared_ptr<IVertexSourceData>
        CreateRawDataSource(
            const void* dataBegin, const void* dataEnd, 
            Metal::NativeFormat::Enum srcFormat)
    {
        auto stride = RenderCore::Metal::BitsPerPixel(srcFormat) / 8;
        auto count = (size_t(dataEnd) - size_t(dataBegin)) / stride;
        return CreateRawDataSource(dataBegin, dataEnd, count, stride, srcFormat);
    }

    std::shared_ptr<IVertexSourceData>
        CreateRawDataSource(
            std::vector<uint8>&& data, 
            size_t count, size_t stride,
            Metal::NativeFormat::Enum srcFormat)
    {
        return std::make_shared<RawVertexSourceDataAdapter>(std::move(data), count, stride, srcFormat);
    }


///////////////////////////////////////////////////////////////////////////////////////////////////

    static inline void GetVertDataF32(
        float* dst, 
        const float* src, unsigned srcComponentCount,
        ProcessingFlags::BitField processingFlags)
    {
            // In Collada, the default for values not set is 0.f (or 1. for components 3 or greater)
        dst[0] = (srcComponentCount > 0) ? src[0] : 0.f;
        dst[1] = (srcComponentCount > 1) ? src[1] : 0.f;
        dst[2] = (srcComponentCount > 2) ? src[2] : 0.f;
        dst[3] = (srcComponentCount > 3) ? src[3] : 1.f;
        if (processingFlags & ProcessingFlags::Renormalize) {
            float scale;
            if (XlRSqrt_Checked(&scale, dst[0] * dst[0] + dst[1] * dst[1] + dst[2] * dst[2]))
                dst[0] *= scale; dst[1] *= scale; dst[2] *= scale;
        }

        if (processingFlags & ProcessingFlags::TexCoordFlip) {
            dst[1] = 1.0f - dst[1];
        } else if (processingFlags & ProcessingFlags::BitangentFlip) {
            dst[0] = -dst[0];
            dst[1] = -dst[1];
            dst[2] = -dst[2];
        } else if (processingFlags & ProcessingFlags::TangentHandinessFlip) {
            dst[3] = -dst[3];
        }
    }

    static inline void GetVertDataF16(
        float* dst, 
        const uint16* src, unsigned srcComponentCount,
        ProcessingFlags::BitField processingFlags)
    {
            // In Collada, the default for values not set is 0.f (or 1. for components 3 or greater)
        dst[0] = (srcComponentCount > 0) ? AsFloat32(src[0]) : 0.f;
        dst[1] = (srcComponentCount > 1) ? AsFloat32(src[1]) : 0.f;
        dst[2] = (srcComponentCount > 2) ? AsFloat32(src[2]) : 0.f;
        dst[3] = (srcComponentCount > 3) ? AsFloat32(src[3]) : 1.f;
        if (processingFlags & ProcessingFlags::Renormalize) {
            float scale;
            if (XlRSqrt_Checked(&scale, dst[0] * dst[0] + dst[1] * dst[1] + dst[2] * dst[2]))
                dst[0] *= scale; dst[1] *= scale; dst[2] *= scale;
        }

        if (processingFlags & ProcessingFlags::TexCoordFlip) {
            dst[1] = 1.0f - dst[1];
        } else if (processingFlags & ProcessingFlags::BitangentFlip) {
            dst[0] = -dst[0];
            dst[1] = -dst[1];
            dst[2] = -dst[2];
        } else if (processingFlags & ProcessingFlags::TangentHandinessFlip) {
            dst[3] = -dst[3];
        }
    }

    static inline void GetVertData(
        float* dst, 
        const void* src, std::pair<ComponentType, unsigned> fmt,
        ProcessingFlags::BitField processingFlags)
    {
        switch (fmt.first) {
        case ComponentType::Float32:
            GetVertDataF32(dst, (const float*)src, fmt.second, processingFlags);
            break;
        case ComponentType::Float16:
            GetVertDataF16(dst, (const uint16*)src, fmt.second, processingFlags);
            break;
        default:
            assert(0);
            break;
        }
    }

    template<> Float3 GetVertex(const IVertexSourceData& sourceData, size_t index)
    {
        auto stride = sourceData.GetStride();
        const auto* sourceStart = PtrAdd(sourceData.GetData(), index * stride);

        float input[4];
        GetVertData(input, (const float*)sourceStart, BreakdownFormat(sourceData.GetFormat()), sourceData.GetProcessingFlags());
        return Float3(input[0], input[1], input[2]);
    }

    template<> Float2 GetVertex(const IVertexSourceData& sourceData, size_t index)
    {
        auto stride = sourceData.GetStride();
        const auto* sourceStart = PtrAdd(sourceData.GetData(), index * stride);

        float input[4];
        GetVertData(input, (const float*)sourceStart, BreakdownFormat(sourceData.GetFormat()), sourceData.GetProcessingFlags());
        return Float2(input[0], input[1]);
    }

    template<> Float4 GetVertex(const IVertexSourceData& sourceData, size_t index)
    {
        auto stride = sourceData.GetStride();
        const auto* sourceStart = PtrAdd(sourceData.GetData(), index * stride);

        float input[4];
        GetVertData(input, (const float*)sourceStart, BreakdownFormat(sourceData.GetFormat()), sourceData.GetProcessingFlags());
        return Float4(input[0], input[1], input[2], input[3]);
    }

    template<> float GetVertex(const IVertexSourceData& sourceData, size_t index)
    {
        auto stride = sourceData.GetStride();
        const auto* sourceStart = PtrAdd(sourceData.GetData(), index * stride);

        float input[4];
        GetVertData(input, (const float*)sourceStart, BreakdownFormat(sourceData.GetFormat()), sourceData.GetProcessingFlags());
        return input[0];
    }

    void CopyVertexData(
        const void* dst, Metal::NativeFormat::Enum dstFmt, size_t dstStride, size_t dstDataSize,
        const void* src, Metal::NativeFormat::Enum srcFmt, size_t srcStride, size_t srcDataSize,
        unsigned count, 
        std::vector<unsigned> mapping,
        ProcessingFlags::BitField processingFlags)
    {
        auto dstFormat = BreakdownFormat(dstFmt);
        auto srcFormat = BreakdownFormat(srcFmt);

            //      This could be be made more efficient with a smarter loop..
        if (srcFormat.first == ComponentType::Float32) {

            if (dstFormat.first == ComponentType::Float32) {  ////////////////////////////////////////////////

                for (unsigned v = 0; v<count; ++v, dst = PtrAdd(dst, dstStride)) {
                    auto srcIndex = (v < mapping.size()) ? mapping[v] : v;
                    assert(srcIndex * srcStride + sizeof(float) <= srcDataSize);
                    auto* srcV = PtrAdd(src, srcIndex * srcStride);

                    float input[4];
                    GetVertDataF32(input, (const float*)srcV, srcFormat.second, processingFlags);

                    for (unsigned c=0; c<dstFormat.second; ++c) {
                        assert(&((float*)dst)[c+1] <= PtrAdd(dst, dstDataSize));
                        ((float*)dst)[c] = input[c];
                    }
                }

            } else if (dstFormat.first == ComponentType::Float16) {  ////////////////////////////////////////////////

                for (unsigned v = 0; v<count; ++v, dst = PtrAdd(dst, dstStride)) {
                    auto srcIndex = (v < mapping.size()) ? mapping[v] : v;
                    assert(srcIndex * srcStride + sizeof(float) <= srcDataSize);
                    auto* srcV = PtrAdd(src, srcIndex * srcStride);

                    float input[4];
                    GetVertDataF32(input, (const float*)srcV, srcFormat.second, processingFlags);

                    for (unsigned c=0; c<dstFormat.second; ++c) {
                        assert(&((unsigned short*)dst)[c+1] <= PtrAdd(dst, dstDataSize));
                        ((unsigned short*)dst)[c] = AsFloat16(input[c]);
                    }
                }

            } else if (dstFormat.first == ComponentType::UNorm8) {  ////////////////////////////////////////////////

                for (unsigned v = 0; v<count; ++v, dst = PtrAdd(dst, dstStride)) {
                    auto srcIndex = (v < mapping.size()) ? mapping[v] : v;
                    assert(srcIndex * srcStride + sizeof(float) <= srcDataSize);
                    auto* srcV = PtrAdd(src, srcIndex * srcStride);

                    float input[4];
                    GetVertDataF32(input, (const float*)srcV, srcFormat.second, processingFlags);

                    for (unsigned c=0; c<dstFormat.second; ++c) {
                        assert(&((unsigned char*)dst)[c+1] <= PtrAdd(dst, dstDataSize));
                        ((unsigned char*)dst)[c] = (unsigned char)Clamp(((float*)input)[c]*255.f, 0.f, 255.f);
                    }
                }

            } else {
                Throw(FormatError("Error while copying vertex data. Unexpected format for destination parameter."));
            }
        } else if (
                srcFormat.first == ComponentType::UNorm8 && dstFormat.first == ComponentType::UNorm8
            &&  srcFormat.second == dstFormat.second) {

                // simple copy of uint8 data
            for (unsigned v = 0; v<count; ++v, dst = PtrAdd(dst, dstStride)) {
                auto srcIndex = (v < mapping.size()) ? mapping[v] : v;
                assert(srcIndex * srcStride + sizeof(uint8) * dstFormat.second <= srcDataSize);
                auto* srcV = (uint8*)PtrAdd(src, srcIndex * srcStride);
                for (unsigned c=0; c<dstFormat.second; ++c) {
                    assert(&((uint8*)dst)[c+1] <= PtrAdd(dst, dstDataSize));
                    ((uint8*)dst)[c] = srcV[c];
                }
            }

        } else {
            Throw(FormatError("Error while copying vertex data. Format not supported."));
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static std::vector<std::pair<Int3, unsigned>> BuildQuantizedCoords(
        const IVertexSourceData& sourceStream,
        Float3 quantization, Float3 offset)
    {
        std::vector<std::pair<Int3, unsigned>> result;
        result.resize(sourceStream.GetCount());

        auto stride = sourceStream.GetStride();
        auto fmtBrkdn = BreakdownFormat(sourceStream.GetFormat());

        Float3 precisionMin(float(INT_MIN) * quantization[0], float(INT_MIN) * quantization[1], float(INT_MIN) * quantization[2]);
        Float3 precisionMax(float(INT_MAX) * quantization[0], float(INT_MAX) * quantization[1], float(INT_MAX) * quantization[2]);

        for (unsigned c=0; c<sourceStream.GetCount(); ++c) {
            const auto* sourceStart = PtrAdd(sourceStream.GetData(), c * stride);
        
            float input[4];
            GetVertData(input, (const float*)sourceStart, fmtBrkdn, sourceStream.GetProcessingFlags());
            
                // note that if we're using very small values for quantization,
                // or if the source data is very large numbers, we could run into
                // integer precision problems here. We could use uint64 instead?
            assert(input[0] > precisionMin[0] && input[0] < precisionMax[0]);
            assert(input[1] > precisionMin[1] && input[1] < precisionMax[1]);
            assert(input[2] > precisionMin[2] && input[2] < precisionMax[2]);
            Int3 q( int((input[0] + offset[0]) / quantization[0]),
                    int((input[1] + offset[1]) / quantization[1]),
                    int((input[2] + offset[2]) / quantization[2]));

            result[c] = std::make_pair(q, c);
        }

        return std::move(result);
    }

    static bool SortQuantizedSet(
        const std::pair<Int3, unsigned>& lhs,
        const std::pair<Int3, unsigned>& rhs)
    {
        if (lhs.first[0] < rhs.first[0]) return true;
        if (lhs.first[0] > rhs.first[0]) return false;
        if (lhs.first[1] < rhs.first[1]) return true;
        if (lhs.first[1] > rhs.first[1]) return false;
        if (lhs.first[2] < rhs.first[2]) return true;
        if (lhs.first[2] > rhs.first[2]) return false;
            // when the quantized coordinates are equal, sort by
            // vertex index.
        return lhs.second < rhs.second; 
    }
    
    static bool CompareVertexPair(
        const std::pair<unsigned, unsigned>& lhs,
        const std::pair<unsigned, unsigned>& rhs)
    {
        if (lhs.first < rhs.first) return true;
        if (lhs.first > rhs.first) return false;
        if (lhs.second < rhs.second) return true;
        return false;
    }

    static void FindVertexPairs(
        std::vector<std::pair<unsigned, unsigned>>& closeVertices,
        std::vector<std::pair<Int3, unsigned>> & quantizedSet,
        const IVertexSourceData& sourceStream, float threshold)
    {
        auto stride = sourceStream.GetStride();
        auto fmtBrkdn = BreakdownFormat(sourceStream.GetFormat());

        const float tsq = threshold*threshold;
        for (auto c=quantizedSet.cbegin(); c!=quantizedSet.cend(); ) {
            auto c2 = c+1;
            while (c2!=quantizedSet.cend() && c2->first == c->first) ++c2;

            // Every vertex in the range [c..c2) has equal quantized coordinates
            // We can now use a brute-force test to find if they are truly "close"
            float vert0[4], vert1[4];
            for (auto ct0=c; ct0<c2; ++ct0) {
                GetVertData(
                    vert0, (const float*)PtrAdd(sourceStream.GetData(), ct0->second * stride), 
                    fmtBrkdn, sourceStream.GetProcessingFlags());
                for (auto ct1=ct0+1; ct1<c2; ++ct1) {
                    GetVertData(
                        vert1, (const float*)PtrAdd(sourceStream.GetData(), ct1->second * stride), 
                        fmtBrkdn, sourceStream.GetProcessingFlags());

                    auto off = Float3(vert1[0]-vert0[0], vert1[1]-vert0[1], vert1[2]-vert0[2]);
                    float dstSq = MagnitudeSquared(off);

                    if (dstSq < tsq) {
                        assert(ct0->second < ct1->second); // first index should always be smaller
                        auto p = std::make_pair(ct0->second, ct1->second);
                        auto i = std::lower_bound(closeVertices.begin(), closeVertices.end(), p, CompareVertexPair);
                        if (i == closeVertices.end() || *i != p)
                            closeVertices.insert(i, p);
                    }

                }
            }

            c = c2;
        }
    }

    static unsigned FindClosestToAverage(
        const IVertexSourceData& sourceStream,
        const unsigned* chainStart, const unsigned* chainEnd)
    {
        if (chainEnd <= chainStart) { assert(0); return ~0u; }

        auto stride = sourceStream.GetStride();
        auto fmtBrkdn = BreakdownFormat(sourceStream.GetFormat());

        float ave[4] = {0.f, 0.f, 0.f, 0.f};
        for (auto c=chainStart; c!=chainEnd; ++c) {
            float b[4];
            GetVertData(
                b, (const float*)PtrAdd(sourceStream.GetData(), (*c) * stride), 
                fmtBrkdn, sourceStream.GetProcessingFlags());
            for (unsigned q=0; q<dimof(ave); ++q)
                ave[q] += b[q];
        }

        auto count = chainEnd - chainStart;
        for (unsigned q=0; q<dimof(ave); ++q)
            ave[q] /= float(count);

        float closestDifference = FLT_MAX;
        auto bestIndex = ~0u;
        for (auto c=chainStart; c!=chainEnd; ++c) {
            float b[4];
            GetVertData(
                b, (const float*)PtrAdd(sourceStream.GetData(), (*c) * stride), 
                fmtBrkdn, sourceStream.GetProcessingFlags());
            float dstSq = 0.f;
            for (unsigned q=0; q<dimof(ave); ++q) {
                float a = b[q] - ave[q];
                dstSq += a * a;
            }
            if (dstSq < closestDifference) {
                closestDifference = dstSq;
                bestIndex = *c;
            }
        }
        return bestIndex;
    }

    std::shared_ptr<IVertexSourceData>
        RemoveDuplicates(
            std::vector<unsigned>& outputMapping,
            const IVertexSourceData& sourceStream,
            const std::vector<unsigned>& originalMapping,
            float threshold)
    {
            // We need to find vertices that are close together...
            // The easiest way to do this is to quantize space into grids of size 2 * threshold.
            // 2 vertices that have the same quantized position may be "close".
            // We do this twice -- once with a offset of half the grid size.
            // We will keep a record of all vertices that are found to be "close". Afterwards,
            // we should combine these pairs into chains of vertices. These chains get combined
            // into a single vertex, which is the one that is closest to the averaged vertex.
        auto quant = Float3(2.f*threshold, 2.f*threshold, 2.f*threshold);
        auto quantizedSet0 = BuildQuantizedCoords(sourceStream, quant, Zero<Float3>());
        auto quantizedSet1 = BuildQuantizedCoords(sourceStream, quant, Float3(threshold, threshold, threshold));

            // sort our quantized vertices to make it easier to find duplicates
            // note that duplicates will be sorted with the lowest vertex index first,
            // which is important when building the pairs.
        std::sort(quantizedSet0.begin(), quantizedSet0.end(), SortQuantizedSet);
        std::sort(quantizedSet1.begin(), quantizedSet1.end(), SortQuantizedSet);
        
            // Find the pairs of close vertices
            // Note that in these pairs, the first index will always be smaller 
            // than the second index.
        std::vector<std::pair<unsigned, unsigned>> closeVertices;
        FindVertexPairs(closeVertices, quantizedSet0, sourceStream, threshold);
        FindVertexPairs(closeVertices, quantizedSet1, sourceStream, threshold);

            // We want to convert our pairs into chains of interacting vertices
            // Each chain will get merged into a single vertex.
            // While doing this, we will create a new IVertexSourceData
            // We want to try to keep the ordering in this new source data to be
            // similar to the old ordering.
        const auto vertexSize = Metal::BitsPerPixel(sourceStream.GetFormat()) / 8;
        std::vector<uint8> finalVB;
        finalVB.reserve(vertexSize * sourceStream.GetCount());
        size_t finalVBCount = 0;

        std::vector<unsigned> oldOrderingToNewOrdering(sourceStream.GetCount(), ~0u);

        std::vector<unsigned> chainBuffer;
        chainBuffer.reserve(32);

        auto i = closeVertices.cbegin();
        for (unsigned c=0; c<sourceStream.GetCount(); c++) {
            if (oldOrderingToNewOrdering[c] != ~0u) continue;

            while (i != closeVertices.end() && i->first < c) ++i;
            if (i != closeVertices.end() && i->first == c) {

                    // We have a chain of at least 2 vertices. We must
                    // follow the pairs to find the full chain
                chainBuffer.clear();    // clear without deallocate
                chainBuffer.push_back(c);
                chainBuffer.push_back(i->second);

                auto chainEnd = i->second;
                for (;;) {
                    auto i2 = LowerBound(closeVertices, chainEnd);
                    if (i2 == closeVertices.end() || i2->first != chainEnd) break;
                    chainBuffer.push_back(i2->second);
                    chainEnd = i2->second;
                }

                auto m = FindClosestToAverage(sourceStream, AsPointer(chainBuffer.cbegin()), AsPointer(chainBuffer.cend()));

                const auto* sourceVertex = PtrAdd(sourceStream.GetData(), m * sourceStream.GetStride());
                finalVB.insert(finalVB.end(), (const uint8*)sourceVertex, (const uint8*)PtrAdd(sourceVertex, vertexSize));

                    // the new median vertex will replace the first vertex in the chain
                for (auto q=chainBuffer.cbegin(); q!=chainBuffer.cend(); ++q)
                    oldOrderingToNewOrdering[*q] = (unsigned)finalVBCount;
                ++finalVBCount;

            } else {
                    // This vertex is not part of a chain.
                    // Just append to the finalVB
                const auto* sourceVertex = PtrAdd(sourceStream.GetData(), c * sourceStream.GetStride());
                finalVB.insert(finalVB.end(), (const uint8*)sourceVertex, (const uint8*)PtrAdd(sourceVertex, vertexSize));
                oldOrderingToNewOrdering[c] = (unsigned)finalVBCount;
                ++finalVBCount;
            }
        }

            // Build the new mapping -- we need to use oldOrderingToNewOrdering,
            // which represents how the vertex ordering has changed.
        outputMapping.clear();
        if (originalMapping.size()) {
            outputMapping.reserve(originalMapping.size());
            std::transform(
                originalMapping.begin(), originalMapping.end(),
                std::back_inserter(outputMapping),
                [&oldOrderingToNewOrdering](const unsigned i) { return oldOrderingToNewOrdering[i]; });
        } else {
            outputMapping.insert(
                outputMapping.end(),
                oldOrderingToNewOrdering.cbegin(), oldOrderingToNewOrdering.cend());
        }

            // finally, return the source data adapter
        return std::make_shared<RawVertexSourceDataAdapter>(
            std::move(finalVB), finalVBCount, vertexSize,  
            sourceStream.GetFormat());
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    size_t CreateTriangleWindingFromPolygon(unsigned buffer[], size_t bufferCount, size_t polygonVertexCount)
    {
            //
            //      Assuming simple convex polygon
            //      (nothing fancy required to convert to triangle list)
            //
        size_t outputIterator = 0;
        for (unsigned triangleCount = 0; triangleCount < polygonVertexCount - 2; ++triangleCount) {
                ////////        ////////
            unsigned v0, v1, v2;
            v0 = (triangleCount+1) / 2;
            if (triangleCount&0x1) {
                v1 = unsigned(polygonVertexCount - 2 - triangleCount/2);
            } else {
                v1 = unsigned(v0 + 1);
            }
            v2 = unsigned(polygonVertexCount - 1 - triangleCount/2);
                ////////        ////////
            assert((outputIterator+3) <= bufferCount);
            buffer[outputIterator++] = v0;
            buffer[outputIterator++] = v1;
            buffer[outputIterator++] = v2;
                ////////        ////////
        }
        return outputIterator/3;
    }

    std::pair<ComponentType, unsigned> BreakdownFormat(Metal::NativeFormat::Enum fmt)
    {
        if (fmt == Metal::NativeFormat::Unknown) return std::make_pair(ComponentType::Float32, 0);

        auto componentType = ComponentType::Float32;
        unsigned componentCount = Metal::GetComponentCount(Metal::GetComponents(fmt));

        auto type = Metal::GetComponentType(fmt);
        unsigned prec = Metal::GetComponentPrecision(fmt);

        switch (type) {
        case Metal::FormatComponentType::Float:
            assert(prec == 16 || prec == 32);
            componentType = (prec > 16) ? ComponentType::Float32 : ComponentType::Float16; 
            break;

        case Metal::FormatComponentType::UnsignedFloat16:
        case Metal::FormatComponentType::SignedFloat16:
            componentType = ComponentType::Float16;
            break;

        case Metal::FormatComponentType::UNorm:
        case Metal::FormatComponentType::SNorm:
        case Metal::FormatComponentType::UNorm_SRGB:
            assert(prec==8);
            componentType = ComponentType::UNorm8;
            break;
        }

        return std::make_pair(componentType, componentCount);
    }

    unsigned short AsFloat16(float input)
    {
        //
        //      Using "half" library
        //          http://sourceforge.net/projects/half/
        //
        //      It doesn't have vectorized conversions,
        //      and it looks like it doesn't support denormalized
        //      or overflowed numbers. But it has lots of rounding
        //      modes!
        //

        auto result = half_float::detail::float2half<std::round_to_nearest>(input);
        // assert(!isinf(half_float::detail::half2float(result)));
        return result;
    }

    float AsFloat32(unsigned short input)
    {
        return half_float::detail::half2float(input);
    }

    // static unsigned short AsFloat16_Fast(float input)
    // {
    //         //
    //         //      See stack overflow article:
    //         //          http://stackoverflow.com/questions/3026441/float32-to-float16
    //         //
    //         //      He suggests either using a table lookup or vectorising
    //         //      this code for further optimisation.
    //         //
    //     unsigned int fltInt32 = FloatBits(input);
    // 
    //     unsigned short fltInt16 = (fltInt32 >> 31) << 5;
    // 
    //     unsigned short tmp = (fltInt32 >> 23) & 0xff;
    //     tmp = (tmp - 0x70) & ((unsigned int)((int)(0x70 - tmp) >> 4) >> 27);
    // 
    //     fltInt16 = (fltInt16 | tmp) << 10;
    //     fltInt16 |= (fltInt32 >> 13) & 0x3ff;
    // 
    //     return fltInt16;
    // }

}}}

