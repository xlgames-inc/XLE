// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MeshDatabaseAdapter.h"
#include "../RenderCore/Metal/Format.h"
#include "../Utility/StringUtils.h"
#include "../Utility/MemoryUtils.h"
#include "../Foreign/half-1.9.2/include/half.hpp"

namespace RenderCore { namespace ColladaConversion
{
    using ::Assets::Exceptions::FormatError;

    enum class ComponentType { Float32, Float16, UNorm8 };
    static std::pair<ComponentType, unsigned> BreakdownFormat(Metal::NativeFormat::Enum fmt);
    static unsigned short AsFloat16(float input);

///////////////////////////////////////////////////////////////////////////////////////////////////

    unsigned    MeshDatabaseAdapter::HasElement(const char name[]) const
    {
        unsigned result = 0;
        for (auto i = _streams.cbegin(); i != _streams.cend(); ++i) {
            if (!XlCompareStringI(i->_semanticName.c_str(), name)) {
                assert((result & (1 << i->_semanticIndex)) == 0);
                result |= (1 << i->_semanticIndex);
            }
        }
        return result;
    }

    unsigned    MeshDatabaseAdapter::FindElement(const char name[], unsigned semanticIndex) const
    {
        for (auto i = _streams.cbegin(); i != _streams.cend(); ++i)
            if (i->_semanticIndex == semanticIndex && !XlCompareStringI(i->_semanticName.c_str(), name))
                return unsigned(std::distance(_streams.cbegin(), i));
        return ~0u;
    }

    void        MeshDatabaseAdapter::RemoveStream(unsigned elementIndex)
    {
        if (elementIndex < _streams.size())
            _streams.erase(_streams.begin() + elementIndex);
    }

    static inline void GetVertData(
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

    template<> Float3 MeshDatabaseAdapter::GetUnifiedElement(size_t vertexIndex, unsigned elementIndex) const
    {
        auto& stream = _streams[elementIndex];
        auto indexInStream = stream._vertexMap[vertexIndex];
        auto& sourceData = *stream._sourceData;
        // assert(((indexInStream+1) * sourceData._stride) <= sourceData._vertexData->getFloatValues()->getCount());
        // const auto* sourceStart = &sourceData._vertexData->getFloatValues()->getData()[indexInStream * sourceData._stride];
        auto stride = sourceData.GetStride();
        const auto* sourceStart = PtrAdd(sourceData.GetData(), indexInStream * stride);
        
        float input[4];
        GetVertData(input, (const float*)sourceStart, unsigned(stride), sourceData.GetProcessingFlags());
        return Float3(input[0], input[1], input[2]);
    }

    template<> Float2 MeshDatabaseAdapter::GetUnifiedElement(size_t vertexIndex, unsigned elementIndex) const
    {
        auto& stream = _streams[elementIndex];
        auto indexInStream = stream._vertexMap[vertexIndex];
        auto& sourceData = *stream._sourceData;
        // assert(((indexInStream+1) * sourceData._stride) <= sourceData._vertexData->getFloatValues()->getCount());
        // const auto* sourceStart = &sourceData._vertexData->getFloatValues()->getData()[indexInStream * sourceData._stride];
        auto stride = sourceData.GetStride();
        const auto* sourceStart = PtrAdd(sourceData.GetData(), indexInStream * stride);

        float input[4];
        GetVertData(input, (const float*)sourceStart, unsigned(stride), sourceData.GetProcessingFlags());
        return Float2(input[0], input[1]);
    }

    std::unique_ptr<uint32[]> MeshDatabaseAdapter::BuildUnifiedVertexIndexToPositionIndex() const
    {
            //      Collada has this idea of "vertex index"; which is used to map
            //      on the vertex weight information. But that seems to be lost in OpenCollada.
            //      All we can do is use the position index as a subtitute.

        auto unifiedVertexIndexToPositionIndex = std::make_unique<uint32[]>(_unifiedVertexCount);
        
        if (!_streams[0]._vertexMap.empty()) {
            for (size_t v=0; v<_unifiedVertexCount; ++v) {
                // assuming the first element is the position
                auto attributeIndex = _streams[0]._vertexMap[v];
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

    static void CopyVertexData(
        const void* dst, Metal::NativeFormat::Enum dstFmt, size_t dstStride,
        const void* src, Metal::NativeFormat::Enum srcFmt, size_t srcStride,
        std::vector<unsigned> mapping,
        unsigned count, ProcessingFlags::BitField processingFlags)
    {
        auto dstFormat = BreakdownFormat(dstFmt);
        auto srcFormat = BreakdownFormat(srcFmt);

            //      This could be be made more efficient with a smarter loop..
        if (srcFormat.first == ComponentType::Float32) {

            if (dstFormat.first == ComponentType::Float32) {  ////////////////////////////////////////////////

                for (unsigned v = 0; v<count; ++v, dst = PtrAdd(dst, dstStride)) {
                    auto srcIndex = (v < mapping.size()) ? mapping[v] : v;
                    auto* srcV = PtrAdd(src, srcIndex * srcStride);

                    float input[4];
                    GetVertData(input, (const float*)srcV, srcFormat.second, processingFlags);

                    for (unsigned c=0; c<dstFormat.second; ++c)
                        ((float*)dst)[c] = input[c];
                }

            } else if (dstFormat.first == ComponentType::Float16) {  ////////////////////////////////////////////////

                for (unsigned v = 0; v<count; ++v, dst = PtrAdd(dst, dstStride)) {
                    auto srcIndex = (v < mapping.size()) ? mapping[v] : v;
                    auto* srcV = PtrAdd(src, srcIndex * srcStride);

                    float input[4];
                    GetVertData(input, (const float*)srcV, srcFormat.second, processingFlags);

                    for (unsigned c=0; c<dstFormat.second; ++c)
                        ((unsigned short*)dst)[c] = AsFloat16(input[c]);
                }

            } else if (dstFormat.first == ComponentType::UNorm8) {  ////////////////////////////////////////////////

                for (unsigned v = 0; v<count; ++v, dst = PtrAdd(dst, dstStride)) {
                    auto srcIndex = (v < mapping.size()) ? mapping[v] : v;
                    auto* srcV = PtrAdd(src, srcIndex * srcStride);

                    float input[4];
                    GetVertData(input, (const float*)srcV, srcFormat.second, processingFlags);

                    for (unsigned c=0; c<dstFormat.second; ++c)
                        ((unsigned char*)dst)[c] = (unsigned char)Clamp(((float*)input)[c]*255.f, 0.f, 255.f);
                }

            } else {
                ThrowException(FormatError("Error while copying vertex data. Unexpected format for destination parameter."));
            }
        } else {
            ThrowException(FormatError("Error while copying vertex data. Only float type data is supported."));
        }
    }

    void MeshDatabaseAdapter::WriteStream(
        const Stream& stream,
        const void* dst, Metal::NativeFormat::Enum dstFormat, size_t dstStride) const
    {
        const auto& sourceData = *stream._sourceData;
        auto stride = sourceData.GetStride();
        CopyVertexData(
            dst, dstFormat, dstStride,
            sourceData.GetData(), sourceData.GetFormat(), stride,
            stream._vertexMap, (unsigned)_unifiedVertexCount, sourceData.GetProcessingFlags());
    }

    std::unique_ptr<uint8[]>  MeshDatabaseAdapter::BuildNativeVertexBuffer(const NativeVBLayout& outputLayout) const
    {
            //
            //      Write the data into the vertex buffer
            //
        auto finalVertexBuffer = std::make_unique<uint8[]>(outputLayout._vertexStride * _unifiedVertexCount);

        for (unsigned elementIndex = 0; elementIndex <_streams.size(); ++elementIndex) {
            const auto& nativeElement     = outputLayout._elements[elementIndex];
            const auto& stream            = _streams[elementIndex];
            WriteStream(
                stream, PtrAdd(finalVertexBuffer.get(), nativeElement._alignedByteOffset),
                nativeElement._nativeFormat, outputLayout._vertexStride);
        }

        return std::move(finalVertexBuffer);
    }

    void    MeshDatabaseAdapter::AddStream(
        std::shared_ptr<IVertexSourceData> dataSource,
        std::vector<unsigned>&& vertexMap,
        const char semantic[], unsigned semanticIndex)
    {
        auto count = vertexMap.size() ? vertexMap.size() : dataSource->GetCount();
        assert(count > 0);
        if (!_unifiedVertexCount) { _unifiedVertexCount = count; }
        else _unifiedVertexCount = std::min(_unifiedVertexCount, count);

        _streams.push_back(
            Stream{std::move(dataSource), std::move(vertexMap), semantic, semanticIndex});
    }

    MeshDatabaseAdapter::MeshDatabaseAdapter()
    {
        _unifiedVertexCount = 0;
    }

    MeshDatabaseAdapter::~MeshDatabaseAdapter() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

        static RenderCore::Metal::NativeFormat::Enum CalculateFinalVBFormat(const IVertexSourceData& source)
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

        auto parameterCount = RenderCore::Metal::GetComponentCount(
            RenderCore::Metal::GetComponents(source.GetFormat()));
        if (!parameterCount) return Metal::NativeFormat::Unknown;

        if (source.GetFormatHint() & FormatHint::IsColor) {
            if (parameterCount == 1)        return Metal::NativeFormat::R8_UNORM;
            else if (parameterCount == 2)   return Metal::NativeFormat::R8G8_UNORM;
            else                            return Metal::NativeFormat::R8G8B8A8_UNORM;
        }

        if (constant_expression<Use16BitFloats>::result()) {
            if (parameterCount == 1)        return Metal::NativeFormat::R16_FLOAT;
            else if (parameterCount == 2)   return Metal::NativeFormat::R16G16_FLOAT;
            else                            return Metal::NativeFormat::R16G16B16A16_FLOAT;
        } else {
            if (parameterCount == 1)        return Metal::NativeFormat::R32_FLOAT;
            else if (parameterCount == 2)   return Metal::NativeFormat::R32G32_FLOAT;
            else if (parameterCount == 3)   return Metal::NativeFormat::R32G32B32_FLOAT;
            else                            return Metal::NativeFormat::R32G32B32A32_FLOAT;
        }
    }

    NativeVBLayout BuildDefaultLayout(MeshDatabaseAdapter& mesh)
    {
        unsigned accumulatingOffset = 0;

        NativeVBLayout result;
        result._elements.resize(mesh._streams.size());

        for (size_t c = 0; c<mesh._streams.size(); ++c) {
            auto& nativeElement = result._elements[c];
            auto& stream = mesh._streams[c];
            nativeElement._semanticName         = stream._semanticName;
            nativeElement._semanticIndex        = stream._semanticIndex;

                // Note --  There's a problem here with texture coordinates. Sometimes texture coordinates
                //          have 3 components in the Collada file. But only 2 components are actually used
                //          by mapping. The last component might just be redundant. The only way to know 
                //          for sure that the final component is redundant is to look at where the geometry
                //          is used, and how this vertex element is bound to materials. But in this function
                //          call we only have access to the "Geometry" object, without any context information.
                //          We don't yet know how it will be bound to materials.
            nativeElement._nativeFormat         = CalculateFinalVBFormat(*stream._sourceData);
            nativeElement._inputSlot            = 0;
            nativeElement._alignedByteOffset    = accumulatingOffset;
            nativeElement._inputSlotClass       = Metal::InputClassification::PerVertex;
            nativeElement._instanceDataStepRate = 0;

            accumulatingOffset += Metal::BitsPerPixel(nativeElement._nativeFormat)/8;
        }

        result._vertexStride = accumulatingOffset;
        return std::move(result);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class RawVertexSourceDataAdapter : public IVertexSourceData
    {
    public:
        std::vector<uint8>              _rawData;
        Metal::NativeFormat::Enum       _fmt;

        const void* GetData() const     { return AsPointer(_rawData.cbegin()); }
        size_t GetDataSize() const      { return _rawData.size(); }
        size_t GetStride() const        { return RenderCore::Metal::BitsPerPixel(_fmt) / 8; }
        size_t GetCount() const         { return GetDataSize() / GetStride(); }

        RenderCore::Metal::NativeFormat::Enum GetFormat() const     { return _fmt; }
        ProcessingFlags::BitField GetProcessingFlags() const        { return 0; }
        FormatHint::BitField GetFormatHint() const                  { return 0; }

        RawVertexSourceDataAdapter()    { _fmt = Metal::NativeFormat::Unknown; }
        RawVertexSourceDataAdapter(const void* start, const void* end, Metal::NativeFormat::Enum fmt)
        : _fmt(fmt), _rawData((const uint8*)start, (const uint8*)end) {}
    };

    IVertexSourceData::~IVertexSourceData() {}

    std::shared_ptr<IVertexSourceData>
        CreateRawDataSource(
            const void* dataBegin, const void* dataEnd, 
            Metal::NativeFormat::Enum srcFormat)
    {
        return std::make_shared<RawVertexSourceDataAdapter>(dataBegin, dataEnd, srcFormat);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    size_t CreateTriangleWindingFromPolygon(size_t polygonVertexCount, unsigned buffer[], size_t bufferCount)
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

        return half_float::detail::float2half<std::round_to_nearest>(input);
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

}}

