// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Core/Prefix.h"
#include "RawGeometry.h"
#include "ProcessingUtil.h"
#include "ColladaConversion.h"
#include "../Assets/BlockSerializer.h"
#include "../RenderCore/RenderUtils.h"
#include "../Math/Geometry.h"
#include "../Utility/IteratorUtils.h"
#include "../Utility/StringUtils.h"
#include "../Utility/MemoryUtils.h"

namespace Serialization
{
    void Serialize( NascentBlockSerializer& serializer, 
                    const RenderCore::Metal::InputElementDesc&  object)
    {
        Serialize(serializer, object._semanticName);
        Serialize(serializer, object._semanticIndex);
        Serialize(serializer, unsigned(object._nativeFormat));
        Serialize(serializer, object._inputSlot);
        Serialize(serializer, object._alignedByteOffset);
        Serialize(serializer, unsigned(object._inputSlotClass));
        Serialize(serializer, object._instanceDataStepRate);
    }
}

namespace RenderCore { namespace ColladaConversion
{
    using ::Assets::Exceptions::FormatError;

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

///////////////////////////////////////////////////////////////////////////////////////////////////

    class RawVertexSourceDataAdapter : public IVertexSourceData
    {
    public:
        std::vector<uint8>          _rawData;
        Metal::NativeFormat::Enum   _fmt;

        const void* GetData() const { return AsPointer(_rawData.cbegin()); }
        size_t GetDataSize() const { return _rawData.size(); }
        RenderCore::Metal::NativeFormat::Enum GetFormat() const { return _fmt; }
        size_t GetStride() const { return RenderCore::Metal::BitsPerPixel(_fmt) / 8; }
        size_t GetCount() const { return GetDataSize() / GetStride(); }
        ProcessingFlags::BitField GetProcessingFlags() const { return 0; }

        RawVertexSourceDataAdapter() { _fmt = Metal::NativeFormat::Unknown; }
        RawVertexSourceDataAdapter(const void* start, const void* end, Metal::NativeFormat::Enum fmt)
        : _fmt(fmt), _rawData((const uint8*)start, (const uint8*)end) {}
    };

    IVertexSourceData::~IVertexSourceData() {}

    std::shared_ptr<IVertexSourceData> CreateRawDataSource(
        const void* dataBegin, const void* dataEnd, 
        Metal::NativeFormat::Enum srcFormat)
    {
        return std::make_shared<RawVertexSourceDataAdapter>(dataBegin, dataEnd, srcFormat);
    }

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

    std::unique_ptr<uint8[]>  MeshDatabaseAdapter::BuildNativeVertexBuffer(NativeVBLayout& outputLayout) const
    {
        outputLayout = BuildDefaultLayout();

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

    Float3 CorrectAxisDirection(const Float3& input, const Float3& p0, const Float3& p1, const Float3& p2, float t0, float t1, float t2)
    {
        float A0 = Dot(p0 - p1, input);
        float A1 = Dot(p1 - p2, input);
        float A2 = Dot(p2 - p0, input);
        float a0 = t0 - t1;
        float a1 = t1 - t2;
        float a2 = t2 - t0;

        float w0 = XlAbs(A0 * a0);
        float w1 = XlAbs(A1 * a1);
        float w2 = XlAbs(A2 * a2);
        if (w0 > w1) {
            if (w0 > w2) return ((A0 > 0.f) == (a0 > 0.f)) ? input : -input;
            return ((A2 > 0.f) == (a2 > 0.f)) ? input : -input;
        } else {
            if (w1 > w2) return ((A1 > 0.f) == (a1 > 0.f)) ? input : -input;
            return ((A2 > 0.f) == (a2 > 0.f)) ? input : -input;
        }
    }

    void GenerateNormalsAndTangents( 
        MeshDatabaseAdapter& mesh, 
        unsigned normalMapTextureCoordinateSemanticIndex,
        const void* rawIb, size_t indexCount, Metal::NativeFormat::Enum ibFormat)
	{
        using namespace RenderCore::Metal;

            // testing -- remove existing tangents & normals
        // mesh.RemoveStream(mesh.FindElement("NORMAL"));
        // mesh.RemoveStream(mesh.FindElement("TANGENT"));
        // mesh.RemoveStream(mesh.FindElement("BITANGENT"));

        auto tcElement = mesh.FindElement("TEXCOORD", normalMapTextureCoordinateSemanticIndex);
        if (tcElement == ~0u) return;   // if there are no texture coordinates, we could generate normals, but we can't generate tangents

        bool hasNormals = !!(mesh.HasElement("NORMAL") & 0x1);
        bool hasTangents = !!(mesh.HasElement("TANGENT") & 0x1);
        bool hasBitangents = !!(mesh.HasElement("BITANGENT") & 0x1);
        if ((hasNormals && hasTangents) || (hasTangents && hasBitangents)) return;

        auto posElement = mesh.FindElement("POSITION");

		struct Triangle
		{
			Float3 normal;
			Float3 tangent;
			Float3 bitangent;
		};
		
            //
            //      Note that when building normals and tangents, there are some
            //      cases were we might want to split a vertex into two...
            //      This can happen if we want to create a sharp edge in the model,
            //      or a seam in the texturing.
            //      However, this method never splits vertices... We only modify the
            //      input vertices. This can create stretching or warping in some 
            //      models -- that can only be fixed by changing the input data.
            //
            //      Also note that this is an unweighted method... Which means that
            //      each vertex is influenced by all triangles it is part of evenly.
            //      Some other methods will weight the influence of triangles such
            //      that larger or more important triangles have a larger influence.
            //
        std::vector<Float3> normals(mesh._unifiedVertexCount, Zero<Float3>());
		std::vector<Float4> tangents(mesh._unifiedVertexCount, Zero<Float4>());
		std::vector<Float3> bitangents(mesh._unifiedVertexCount, Zero<Float3>());

        unsigned indexStride = 2;
        unsigned indexMask = 0xffff;
        if (ibFormat == Metal::NativeFormat::R32_UINT) { indexStride = 4; indexMask = 0xffffffff; }
        auto* ib = (const uint32*)rawIb;

		auto triangleCount = indexCount / 3;   // assuming index buffer is triangle-list format
		for (size_t c=0; c<triangleCount; c++) {
            unsigned v0 = (*ib) & indexMask; ib = PtrAdd(ib, indexStride);
            unsigned v1 = (*ib) & indexMask; ib = PtrAdd(ib, indexStride);
            unsigned v2 = (*ib) & indexMask; ib = PtrAdd(ib, indexStride);

            assert(v0 != v1 && v1 != v2 && v1 != v2);

			auto p0 = mesh.GetUnifiedElement<Float3>(v0, posElement);
			auto p1 = mesh.GetUnifiedElement<Float3>(v1, posElement);
			auto p2 = mesh.GetUnifiedElement<Float3>(v2, posElement);

            Float4 plane;
            if (PlaneFit_Checked(&plane, p0, p1, p2)) {
				Triangle tri;
                tri.normal = -Truncate(plane);

					/*	There is one natural tangent and one natural bitangent for each triangle, on the v=0 and u=0 axes 
						in 3 space. We'll calculate them for this triangle here and then use a composite of triangle tangents
						for the vertex tangents below.

						Here's a good reference:
						http://www.terathon.com/code/tangent.html
						from "Mathematics for 3D Game Programming and Computer Graphics, 2nd ed."

						These equations just solve for v=0 and u=0 on the triangle surface.
					*/
				const auto UV0 = mesh.GetUnifiedElement<Float2>(v0, tcElement);
				const auto UV1 = mesh.GetUnifiedElement<Float2>(v1, tcElement);
				const auto UV2 = mesh.GetUnifiedElement<Float2>(v2, tcElement);
				auto Q1 = p1 - p0;
				auto Q2 = p2 - p0;
				auto st1 = UV1 - UV0;
				auto st2 = UV2 - UV0;
				float rr = (st1[0] * st2[1] + st2[0] * st1[1]);
				if (Equivalent(rr, 0.f, 1e-10f)) { tri.tangent = tri.bitangent = Zero<Float3>(); }
				else
				{
					float r = 1.f / rr;
					Float3 sAxis( (st2[1] * Q1 - st1[1] * Q2) * r );
					Float3 tAxis( (st1[0] * Q2 - st2[0] * Q1) * r );

                        // We may need to flip the direction of the s or t axis
                        // check the texture coordinates to find the correct direction
                        // for these axes...
                    sAxis = CorrectAxisDirection(sAxis, p0, p1, p2, UV0[0], UV1[0], UV2[0]);
                    tAxis = CorrectAxisDirection(tAxis, p0, p1, p2, UV0[1], UV1[1], UV2[1]);

                    auto sMagSq = MagnitudeSquared(sAxis);
                    auto tMagSq = MagnitudeSquared(tAxis);
                    
                    float recipSMag, recipTMag;
                    if (XlRSqrt_Checked(&recipSMag, sMagSq) && XlRSqrt_Checked(&recipTMag, tMagSq)) {
                        tri.tangent = sAxis * recipSMag;
						tri.bitangent = tAxis * recipTMag;
                    } else {
                        tri.tangent = tri.bitangent = Zero<Float3>();
                    }

                    tri.tangent = sAxis;
                    tri.bitangent = tAxis;
				}

				assert( tri.tangent[0] == tri.tangent[0] );

                    // We add the influence of this triangle to all vertices
                    // each vertex should get an even balance of influences from
                    // all triangles it is part of.
                normals[v0] += tri.normal; normals[v1] += tri.normal; normals[v2] += tri.normal;
                tangents[v0] += Expand(tri.tangent, 0.f); tangents[v1] += Expand(tri.tangent, 0.f); tangents[v2] += Expand(tri.tangent, 0.f);
                bitangents[v0] += tri.bitangent; bitangents[v1] += tri.bitangent; bitangents[v2] += tri.bitangent;
			} else {
				    /* this triangle is so small we can't derive any useful information from it */
			}
		}

            //  Create new streams for the normal & tangent, and write the results to the mesh database
            //  If we already have tangents or normals, don't write the new ones

        if (!hasNormals) {
            for (size_t c=0; c<mesh._unifiedVertexCount; c++)
                normals[c] = Normalize(normals[c]);
        
            mesh.AddStream(
                CreateRawDataSource(
                    AsPointer(normals.cbegin()), AsPointer(normals.cend()),
                    Metal::NativeFormat::R32G32B32_FLOAT),
                std::vector<unsigned>(),
                "NORMAL", 0,
                Use16BitFloats ? Metal::NativeFormat::R16G16B16A16_FLOAT : Metal::NativeFormat::R32G32B32_FLOAT);
        }

        if (!hasTangents) {

            unsigned normalsElement = mesh.FindElement("NORMAL");

                //  normals and tangents will have fallen out of orthogonality by the blending above.
			    //  we can re-orthogonalize using the Gram-Schmidt process -- we won't modify the normal, we'd rather lift the tangent and bitangent
			    //  off the triangle surface that distort the normal direction too much.
                //  Note that we don't need to touch the bitangent here... We're not going to write the bitangent
                //  to the output, so it doesn't matter right now. All we need to do is calculate the "handiness"
                //  value and write it to the "w" part of the tangent vector.
            for (size_t c=0; c<mesh._unifiedVertexCount; c++) {
                auto t3 = Truncate(tangents[c]);
                auto handinessValue = 0.f;

                    // if we already had normals in the mesh, we should prefex
                    // those normals (over the ones we generated here)
                auto n = normals[c];
                if (hasNormals) n = mesh.GetUnifiedElement<Float3>(c, normalsElement);

                if (Normalize_Checked(&t3, Float3(t3 - n * Dot(n, t3)))) {
                    // handinessValue = Dot( Cross( bitangents[c], t3 ), n ) < 0.f ? -1.f : 1.f;
                    handinessValue = Dot( Cross( t3, n ), bitangents[c] ) < 0.f ? -1.f : 1.f;
                } else {
                    t3 = Zero<Float3>();
                }

                tangents[c] = Expand(t3, handinessValue);
            }

            mesh.AddStream(
                CreateRawDataSource(
                    AsPointer(tangents.begin()), AsPointer(tangents.cend()), Metal::NativeFormat::R32G32B32A32_FLOAT),
                std::vector<unsigned>(),
                "TANGENT", 0,
                Use16BitFloats ? Metal::NativeFormat::R16G16B16A16_FLOAT : Metal::NativeFormat::R32G32B32A32_FLOAT);

        }

        // if (!hasBitangents) {
        // 
        //     unsigned normalsElement = mesh.FindElement("NORMAL");
        // 
        //     for (size_t c=0; c<mesh._unifiedVertexCount; c++) {
        //         auto t3 = bitangents[c];
        // 
        //             // if we already had normals in the mesh, we should prefex
        //             // those normals (over the ones we generated here)
        //         auto n = normals[c];
        //         if (hasNormals) n = mesh.GetUnifiedElement<Float3>(c, normalsElement);
        // 
        //         if (Normalize_Checked(&t3, Float3(t3 - n * Dot(n, t3)))) {
        //         } else {
        //             t3 = Zero<Float3>();
        //         }
        // 
        //         bitangents[c] = t3;
        //     }
        // 
        //     mesh.AddStream(
        //         AsPointer(bitangents.begin()), AsPointer(bitangents.cend()), Metal::NativeFormat::R32G32B32_FLOAT,
        //         Use16BitFloats ? Metal::NativeFormat::R16G16B16A16_FLOAT : Metal::NativeFormat::R32G32B32_FLOAT,
        //         "BITANGENT", 0);
        // 
        // }
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

    NativeVBLayout MeshDatabaseAdapter::BuildDefaultLayout() const
    {
        unsigned accumulatingOffset = 0;

        NativeVBLayout result;
        result._elements.resize(_streams.size());

        for (size_t c = 0; c<_streams.size(); ++c) {
            auto& nativeElement = result._elements[c];
            auto& stream = _streams[c];
            nativeElement._semanticName         = stream._semanticName;
            nativeElement._semanticIndex        = stream._semanticIndex;

                // Note --  There's a problem here with texture coordinates. Sometimes texture coordinates
                //          have 3 components in the Collada file. But only 2 components are actually used
                //          by mapping. The last component might just be redundant. The only way to know 
                //          for sure that the final component is redundant is to look at where the geometry
                //          is used, and how this vertex element is bound to materials. But in this function
                //          call we only have access to the "Geometry" object, without any context information.
                //          We don't yet know how it will be bound to materials.
            nativeElement._nativeFormat = stream._finalVBFormat;
            nativeElement._inputSlot            = 0;
            nativeElement._alignedByteOffset    = accumulatingOffset;
            nativeElement._inputSlotClass       = Metal::InputClassification::PerVertex;
            nativeElement._instanceDataStepRate = 0;

            accumulatingOffset += Metal::BitsPerPixel(nativeElement._nativeFormat)/8;
        }

        result._vertexStride = accumulatingOffset;
        return std::move(result);
    }

    void    MeshDatabaseAdapter::AddStream(
        std::shared_ptr<IVertexSourceData> dataSource,
        std::vector<unsigned>&& vertexMap,
        const char semantic[], unsigned semanticIndex,
        Metal::NativeFormat::Enum finalVBFormat)
    {
        auto count = vertexMap.size() ? vertexMap.size() : dataSource->GetCount();
        assert(count > 0);
        if (!_unifiedVertexCount) { _unifiedVertexCount = count; }
        else _unifiedVertexCount = std::min(_unifiedVertexCount, count);

        _streams.push_back(
            Stream{std::move(dataSource), std::move(vertexMap), semantic, semanticIndex, finalVBFormat});
    }

    MeshDatabaseAdapter::MeshDatabaseAdapter()
    {
        _unifiedVertexCount = 0;
    }

    MeshDatabaseAdapter::~MeshDatabaseAdapter() {}

///////////////////////////////////////////////////////////////////////////////////////////////////


    GeometryInputAssembly::GeometryInputAssembly(   std::vector<Metal::InputElementDesc>&& vertexInputLayout,
                                                    unsigned vertexStride)
    :       _vertexStride(vertexStride)
    ,       _vertexInputLayout(vertexInputLayout)
    {
    }

    GeometryInputAssembly::GeometryInputAssembly() 
    :   _vertexStride(0)
    {
    }

    void    GeometryInputAssembly::Serialize(Serialization::NascentBlockSerializer& outputSerializer, unsigned slotFilter) const
    {
        Serialization::NascentBlockSerializer subBlock;
        unsigned elementCount = 0;
        for (auto i=_vertexInputLayout.begin(); i!=_vertexInputLayout.end(); ++i) {
            if (slotFilter == ~unsigned(0x0) || i->_inputSlot == slotFilter) {
                char semantic[16];
                XlZeroMemory(semantic);     // make sure unused space is 0
                XlCopyNString(semantic, AsPointer(i->_semanticName.begin()), i->_semanticName.size());
                semantic[dimof(semantic)-1] = '\0';
                for (unsigned c=0; c<dimof(semantic); ++c) { subBlock.SerializeValue((uint8)semantic[c]); }
                subBlock.SerializeValue(i->_semanticIndex);
                subBlock.SerializeValue(unsigned(i->_nativeFormat));
                subBlock.SerializeValue(i->_alignedByteOffset);
                ++elementCount;
            }
        }
        outputSerializer.SerializeSubBlock(subBlock);
        outputSerializer.SerializeValue(elementCount);
        outputSerializer.SerializeValue(_vertexStride);
    }





    NascentRawGeometry::NascentRawGeometry(DynamicArray<uint8>&&    vb,
                                DynamicArray<uint8>&&               ib,
                                GeometryInputAssembly&&             mainDrawInputAssembly,
                                Metal::NativeFormat::Enum           indexFormat,
                                std::vector<NascentDrawCallDesc>&&  mainDrawCalls,
                                DynamicArray<uint32>&&              unifiedVertexIndexToPositionIndex,
                                std::vector<uint64>&&               materials)
    :       _vertices(std::forward<DynamicArray<uint8>>(vb))
    ,       _indices(std::forward<DynamicArray<uint8>>(ib))
    ,       _mainDrawCalls(std::forward<std::vector<NascentDrawCallDesc>>(mainDrawCalls))
    ,       _mainDrawInputAssembly(std::forward<GeometryInputAssembly>(mainDrawInputAssembly))
    ,       _indexFormat(indexFormat)
    ,       _unifiedVertexIndexToPositionIndex(std::forward<DynamicArray<uint32>>(unifiedVertexIndexToPositionIndex))
    ,       _materials(std::forward<std::vector<uint64>>(materials))
    {
    }

    NascentRawGeometry::NascentRawGeometry(NascentRawGeometry&& moveFrom)
    :       _vertices(std::move(moveFrom._vertices))
    ,       _indices(std::move(moveFrom._indices))
    ,       _mainDrawInputAssembly(std::move(moveFrom._mainDrawInputAssembly))
    ,       _indexFormat(moveFrom._indexFormat)
    ,       _mainDrawCalls(std::move(moveFrom._mainDrawCalls))
    ,       _unifiedVertexIndexToPositionIndex(std::move(moveFrom._unifiedVertexIndexToPositionIndex))
    ,       _materials(std::move(moveFrom._materials))
    {
    }

    NascentRawGeometry& NascentRawGeometry::operator=(NascentRawGeometry&& moveFrom)
    {
        _vertices = std::move(moveFrom._vertices);
        _indices = std::move(moveFrom._indices);
        _mainDrawInputAssembly = std::move(moveFrom._mainDrawInputAssembly);
        _indexFormat = moveFrom._indexFormat;
        _mainDrawCalls = std::move(moveFrom._mainDrawCalls);
        _unifiedVertexIndexToPositionIndex = std::move(moveFrom._unifiedVertexIndexToPositionIndex);
        _materials = std::move(moveFrom._materials);
        return *this;
    }

    NascentRawGeometry::NascentRawGeometry()
    : _vertices(nullptr, 0)
    , _indices(nullptr, 0)
    , _unifiedVertexIndexToPositionIndex(nullptr, 0)
    {
        _indexFormat = Metal::NativeFormat::Unknown;
    }

    void    NascentRawGeometry::Serialize(Serialization::NascentBlockSerializer& outputSerializer, std::vector<uint8>& largeResourcesBlock) const
    {
            //  We're going to write the index and vertex buffer data to the "large resources block"
            //  class members and scaffold structure get written to the serialiser, but the very large stuff
            //  should end up in a separate pool

        auto vbOffset = largeResourcesBlock.size();
        auto vbSize = _vertices.size();
        largeResourcesBlock.insert(largeResourcesBlock.end(), _vertices.begin(), _vertices.end());

        auto ibOffset = largeResourcesBlock.size();
        auto ibSize = _indices.size();
        largeResourcesBlock.insert(largeResourcesBlock.end(), _indices.begin(), _indices.end());

        _mainDrawInputAssembly.Serialize(outputSerializer);
        outputSerializer.SerializeValue(unsigned(vbOffset));
        outputSerializer.SerializeValue(unsigned(vbSize));
        outputSerializer.SerializeValue(unsigned(_indexFormat));
        outputSerializer.SerializeValue(unsigned(ibOffset));
        outputSerializer.SerializeValue(unsigned(ibSize));
        
        outputSerializer.SerializeSubBlock(AsPointer(_mainDrawCalls.begin()), AsPointer(_mainDrawCalls.end()));
        outputSerializer.SerializeValue(_mainDrawCalls.size());
    }




    void    NascentDrawCallDesc::Serialize(Serialization::NascentBlockSerializer& outputSerializer) const
    {
        outputSerializer.SerializeValue(_firstIndex);
        outputSerializer.SerializeValue(_indexCount);
        outputSerializer.SerializeValue(_firstVertex);
        outputSerializer.SerializeValue(_subMaterialIndex);
        outputSerializer.SerializeValue(unsigned(_topology));
    }

}}


