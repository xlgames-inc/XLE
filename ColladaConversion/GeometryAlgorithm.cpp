// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "GeometryAlgorithm.h"
#include "../RenderCore/Assets/MeshDatabase.h"
#include "../Math/Geometry.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/StringUtils.h"

namespace RenderCore { namespace ColladaConversion
{
    using namespace RenderCore::Assets::GeoProc;

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
        MeshDatabase& mesh, 
        unsigned normalMapTextureCoordinateSemanticIndex,
        const void* rawIb, size_t indexCount, Metal::NativeFormat::Enum ibFormat)
	{
        using namespace RenderCore::Metal;

            // testing -- remove existing tangents & normals
        // mesh.RemoveStream(mesh.FindElement("NORMAL"));
        // mesh.RemoveStream(mesh.FindElement("TEXTANGENT"));
        // mesh.RemoveStream(mesh.FindElement("TEXBITANGENT"));

        auto tcElement = mesh.FindElement("TEXCOORD", normalMapTextureCoordinateSemanticIndex);
        if (tcElement == ~0u) return;   // if there are no texture coordinates, we could generate normals, but we can't generate tangents

        bool hasNormals = !!(mesh.HasElement("NORMAL") & 0x1);
        bool hasTangents = !!(mesh.HasElement("TEXTANGENT") & 0x1);
        bool hasBitangents = !!(mesh.HasElement("TEXBITANGENT") & 0x1);
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
        std::vector<Float3> normals(mesh.GetUnifiedVertexCount(), Zero<Float3>());
		std::vector<Float4> tangents(mesh.GetUnifiedVertexCount(), Zero<Float4>());
		std::vector<Float3> bitangents(mesh.GetUnifiedVertexCount(), Zero<Float3>());

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
            for (size_t c=0; c<mesh.GetUnifiedVertexCount(); c++)
                normals[c] = Normalize(normals[c]);
        
            mesh.AddStream(
                CreateRawDataSource(
                    AsPointer(normals.cbegin()), AsPointer(normals.cend()),
                    Metal::NativeFormat::R32G32B32_FLOAT),
                std::vector<unsigned>(),
                "NORMAL", 0);
        }

        if (!hasTangents) {

            unsigned normalsElement = mesh.FindElement("NORMAL");

                //  normals and tangents will have fallen out of orthogonality by the blending above.
			    //  we can re-orthogonalize using the Gram-Schmidt process -- we won't modify the normal, we'd rather lift the tangent and bitangent
			    //  off the triangle surface that distort the normal direction too much.
                //  Note that we don't need to touch the bitangent here... We're not going to write the bitangent
                //  to the output, so it doesn't matter right now. All we need to do is calculate the "handiness"
                //  value and write it to the "w" part of the tangent vector.
            for (size_t c=0; c<mesh.GetUnifiedVertexCount(); c++) {
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
                "TEXTANGENT", 0);

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
        //         "TEXBITANGENT", 0);
        // 
        // }
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

    void CopyVertexElements(     
        void* destinationBuffer,            size_t destinationVertexStride,
        const void* sourceBuffer,           size_t sourceVertexStride,
        const Assets::VertexElement* destinationLayoutBegin,  const Assets::VertexElement* destinationLayoutEnd,
        const Assets::VertexElement* sourceLayoutBegin,       const Assets::VertexElement* sourceLayoutEnd,
        const uint16* reorderingBegin,      const uint16* reorderingEnd )
    {
        uint32      elementReordering[32];
        signed      maxSourceLayout = -1;
        for (auto source=sourceLayoutBegin; source!=sourceLayoutEnd; ++source) {
                //      look for the same element in the destination layout (or put ~uint16(0x0) if it's not there)
            elementReordering[source-sourceLayoutBegin] = ~uint32(0x0);
            for (auto destination=destinationLayoutBegin; destination!=destinationLayoutEnd; ++destination) {
                if (    !XlCompareString(destination->_semanticName, source->_semanticName)
                    &&  destination->_semanticIndex  == source->_semanticIndex
                    &&  destination->_nativeFormat   == source->_nativeFormat) {

                    elementReordering[source-sourceLayoutBegin] = uint32(destination-destinationLayoutBegin);
                    maxSourceLayout = std::max(maxSourceLayout, signed(source-sourceLayoutBegin));
                    break;
                }
            }
        }

        if (maxSourceLayout<0) return;

        size_t vertexCount = reorderingEnd - reorderingBegin; (void)vertexCount;

        #if defined(_DEBUG)
                    //  fill in some dummy values
            std::fill((uint8*)destinationBuffer, (uint8*)PtrAdd(destinationBuffer, vertexCount*destinationVertexStride), 0xaf);
        #endif

            ////////////////     copy each vertex (slowly) piece by piece       ////////////////
        for (auto reordering = reorderingBegin; reordering!=reorderingEnd; ++reordering) {
            size_t sourceIndex               = reordering-reorderingBegin, destinationIndex = *reordering;
            void* destinationVertexStart     = PtrAdd(destinationBuffer, destinationIndex*destinationVertexStride);
            const void* sourceVertexStart    = PtrAdd(sourceBuffer, sourceIndex*sourceVertexStride);
            for (unsigned c=0; c<=(unsigned)maxSourceLayout; ++c) {
                if (elementReordering[c] != ~uint16(0x0)) {
                    const auto& destinationElement = destinationLayoutBegin[elementReordering[c]]; assert(&destinationElement < destinationLayoutEnd);
                    const auto& sourceElement = sourceLayoutBegin[c]; assert(&sourceElement < sourceLayoutEnd);
                    size_t elementSize = Metal::BitsPerPixel(Metal::NativeFormat::Enum(destinationElement._nativeFormat))/8;
                    assert(elementSize == Metal::BitsPerPixel(Metal::NativeFormat::Enum(sourceElement._nativeFormat))/8);
                    assert(destinationElement._alignedByteOffset + elementSize <= destinationVertexStride);
                    assert(sourceElement._alignedByteOffset + elementSize <= sourceVertexStride);
                    assert(PtrAdd(destinationVertexStart, destinationElement._alignedByteOffset+elementSize) <= PtrAdd(destinationVertexStart, vertexCount*destinationVertexStride));
                    assert(PtrAdd(sourceVertexStart, sourceElement._alignedByteOffset+elementSize) <= PtrAdd(sourceVertexStart, vertexCount*sourceVertexStride));

                    XlCopyMemory(
                        PtrAdd(destinationVertexStart, destinationElement._alignedByteOffset),
                        PtrAdd(sourceVertexStart, sourceElement._alignedByteOffset),
                        elementSize);
                }
            }
        }
    }

    unsigned CalculateVertexSize(
        const Assets::VertexElement* layoutBegin,  
        const Assets::VertexElement* layoutEnd)
    {
        unsigned result = 0;
        for (auto l=layoutBegin; l!=layoutEnd; ++l)
            result += Metal::BitsPerPixel(Metal::NativeFormat::Enum(l->_nativeFormat));
        return result/8;
    }

    unsigned CalculateVertexSize(
        const Metal::InputElementDesc* layoutBegin,  
        const Metal::InputElementDesc* layoutEnd)
    {
        unsigned result = 0;
        for (auto l=layoutBegin; l!=layoutEnd; ++l)
            result += Metal::BitsPerPixel(Metal::NativeFormat::Enum(l->_nativeFormat));
        return result/8;
    }

}}

