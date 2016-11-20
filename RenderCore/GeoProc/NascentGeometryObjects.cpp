// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NascentGeometryObjects.h"
#include "NascentCommandStream.h"
#include "GeoProcUtil.h"
#include "../Format.h"

namespace RenderCore { namespace Assets { namespace GeoProc
{
	unsigned NascentGeometryObjects::GetGeo(ObjectGuid id)
	{
		for (const auto& i:_rawGeos)
			if (i.first == id) return unsigned(&i - AsPointer(_rawGeos.cbegin()));
		return ~0u;
	}

	unsigned NascentGeometryObjects::GetSkinnedGeo(ObjectGuid id)
	{
		for (const auto& i:_skinnedGeos)
			if (i.first == id) return unsigned(&i - AsPointer(_skinnedGeos.cbegin()));
		return ~0u;
	}

	std::pair<Float3, Float3> NascentGeometryObjects::CalculateBoundingBox
		(
			const NascentModelCommandStream& scene,
			IteratorRange<const Float4x4*> transforms
		) const
	{
		//
		//      For all the parts of the model, calculate the bounding box.
		//      We just have to go through each vertex in the model, and
		//      transform it into model space, and calculate the min and max values
		//      found;
		//
		auto result = InvalidBoundingBox();
		// const auto finalMatrices = 
		//     _skeleton.GetTransformationMachine().GenerateOutputTransforms(
		//         _animationSet.BuildTransformationParameterSet(0.f, nullptr, _skeleton, _objects));

		//
		//      Do the unskinned geometry first
		//

		for (auto i=scene._geometryInstances.cbegin(); i!=scene._geometryInstances.cend(); ++i) {
			const NascentModelCommandStream::GeometryInstance& inst = *i;

			if (inst._id >= _rawGeos.size()) continue;
			const auto* geo = &_rawGeos[inst._id].second;

			Float4x4 localToWorld = Identity<Float4x4>();
			if (inst._localToWorldId < transforms.size())
				localToWorld = transforms[inst._localToWorldId];

			const void*         vertexBuffer = geo->_vertices.get();
			const unsigned      vertexStride = geo->_mainDrawInputAssembly._vertexStride;

			auto positionDesc = FindPositionElement(
				AsPointer(geo->_mainDrawInputAssembly._elements.begin()),
				geo->_mainDrawInputAssembly._elements.size());

			if (positionDesc._nativeFormat != Format::Unknown && vertexStride) {
				AddToBoundingBox(
					result, vertexBuffer, vertexStride, 
					geo->_vertices.size() / vertexStride, positionDesc, localToWorld);
			}
		}

		//
		//      Now also do the skinned geometry. But use the default pose for
		//      skinned geometry (ie, don't apply the skinning transforms to the bones).
		//      Obvious this won't give the correct result post-animation.
		//

		for (auto i=scene._skinControllerInstances.cbegin(); i!=scene._skinControllerInstances.cend(); ++i) {
			const NascentModelCommandStream::SkinControllerInstance& inst = *i;

			if (inst._id >= _skinnedGeos.size()) continue;
			const auto* controller = &_skinnedGeos[inst._id].second;
			if (!controller) continue;

			Float4x4 localToWorld = Identity<Float4x4>();
			if (inst._localToWorldId < transforms.size())
				localToWorld = transforms[inst._localToWorldId];

			//  We can't get the vertex position data directly from the vertex buffer, because
			//  the "bound" object is already using an opaque hardware object. However, we can
			//  transform the local space bounding box and use that.

			const unsigned indices[][3] = 
			{
				{0,0,0}, {0,1,0}, {1,0,0}, {1,1,0},
				{0,0,1}, {0,1,1}, {1,0,1}, {1,1,1}
			};

			const Float3* A = (const Float3*)&controller->_localBoundingBox.first;
			for (unsigned c=0; c<dimof(indices); ++c) {
				Float3 position(A[indices[c][0]][0], A[indices[c][1]][1], A[indices[c][2]][2]);
				AddToBoundingBox(result, position, localToWorld);
			}
		}

		assert(!isinf(result.first[0]) && !isinf(result.first[1]) && !isinf(result.first[2]));
		assert(!isinf(result.second[0]) && !isinf(result.second[1]) && !isinf(result.second[2]));

		return result;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

    #include "../Utility/ExposeStreamOp.h"

    std::ostream& operator<<(std::ostream& stream, const NascentGeometryObjects& geos)
    {
        using namespace Operators;

        stream << " --- Geos:" << std::endl;
        unsigned c=0;
        for (const auto& g:geos._rawGeos)
            stream << "[" << c++ << "] (0x" << std::hex << g.first._objectId << std::dec << ") Geo --- " << std::endl << g.second << std::endl;

        stream << " --- Skinned Geos:" << std::endl;
        c=0;
        for (const auto& g:geos._skinnedGeos)
            stream << "[" << c++ << "] (0x" << std::hex << g.first._objectId << std::dec << ") Skinned geo --- " << std::endl << g.second << std::endl;
        return stream;
    }

}}}

