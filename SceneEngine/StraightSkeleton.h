// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Math/Vector.h"
#include "../Utility/IteratorUtils.h"
#include <vector>
#include <limits>

namespace XLEMath
{
	class StraightSkeleton
	{
	public:
		using VertexId = unsigned;
		enum class EdgeType { VertexPath, Wavefront };
		struct Edge { VertexId _head; VertexId _tail; EdgeType _type; };
		struct Face { std::vector<Edge> _edges; };

		std::vector<Float3> _steinerVertices;
		std::vector<Face> _faces;
		std::vector<Edge> _unplacedEdges;

		std::vector<std::vector<unsigned>> WavefrontAsVertexLoops();
	};

	StraightSkeleton CalculateStraightSkeleton(
		IteratorRange<const Float2*> vertices, 
		float maxInset = std::numeric_limits<float>::max());
}
