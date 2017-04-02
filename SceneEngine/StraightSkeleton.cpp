// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "StraightSkeleton.h"
#include "../Math/Geometry.h"

namespace SceneEngine { namespace StraightSkeleton 
{
	static const unsigned BoundaryVertexFlag = 1u<<31u;

	enum WindingType { Left, Right, Straight };
	WindingType CalculateWindingType(Float2 zero, Float2 one, Float2 two, float threshold);

	// float sign (fPoint p1, fPoint p2, fPoint p3)
	WindingType CalculateWindingType(Float2 zero, Float2 one, Float2 two, float threshold)
	{
		float sign = (zero[1] - two[1]) * (one[1] - two[1]) - (one[1] - two[1]) * (zero[1] - two[1]);
		if (sign < -threshold) return Left;
		if (sign > threshold) return Right;
		return Straight;
	}

	static Float2 CalculateVertexVelocity(Float2 vex0, Float2 vex1, Float2 vex2)
	{
		// Calculate the velocity of vertex v1, assuming segments vex0->vex1 && vex1->vex2
		// are moving at a constant velocity inwards.
		// Note that the winding order is important. We're assuming these are polygon edge vertices
		// arranged in a clockwise order. This means that v1 will move towards the left side of the
		// segments.

		// let segment 1 be v0->v1
		// let segment 2 be v1->v2
		// let m1,m2 = gradient of segments
		// let u1,u2 = speed in X axis of points on segments
		// let v1,v1 = speed in Y axis of points on segments
		//
		// We're going to center our coordinate system on the initial intersection point, v0
		// We want to know where the intersection point of the 2 segments will be after time 't'
		// (since the intersection point will move in a straight line, we only need to calculate
		// it for t=1)
		//
		// I've calculated this out using basic algebra -- but we may be able to get a more efficient
		// method using vector math.

#if 0
		auto o1 = vex1-vex0;
		auto o2 = vex2-vex1;
		auto m1 = o1[1]/o1[0];
		auto m2 = o2[1]/o2[0];

		// note that we have 2 ways to for transform gradient -> velocity (position can be either left side or right side)
		auto v1 = 1/std::sqrt(m1*m1+1);
		auto u1 = -m1*v1;
		auto v2 = 1/std::sqrt(m2*m2+1);
		auto u2 = -m2*v2;

		// (now, line 1 is y-v1*t=m1(x-u1*t)
		
		const auto t = 1.0f;
		auto x = t * (-u1*m1+v1+u2*m2-v2) / (m2-m1);
		auto y = m1 * (x - u1*t) + v1*t;

		return Float2(x,y);
#endif
		auto t0 = vex1-vex0;
		auto t1 = vex2-vex1;

		// create normal pointing in direction of movement
		auto N0 = Normalize(Float2(-t0[1], t0[0]));
		auto N1 = Normalize(Float2(-t1[1], t1[0]));
		auto a = N0[0], b = N0[1];
		auto c = N1[0], d = N1[1];
		const auto t = 1.0f;

		// Now, line1 is 0 = xa + yb - t and line2 is 0 = xc + yd - t

		// we can calculate the intersection of the lines using this formula...
		auto D = (c-a)/(b-d);
		auto x = t / (a + b*D);
		auto y = D*x;
		return Float2(x, y);
	}

	Graph BuildGraphFromVertexLoop(IteratorRange<const Float2*> vertices)
	{
		assert(vertices.size() >= 2);
		const float threshold = 1e-6f;

		// Construct the starting point for the straight skeleton calculations
		// We're expecting the input vertices to be a closed loop, in counter-clockwise order
		// The first and last vertices should *not* be the same vertex; there is an implied
		// segment between the first and last.
		Graph result;
		result._edgeSegments.reserve(vertices.size());
		result._vertices.reserve(vertices.size());
		for (size_t v=0; v<vertices.size(); ++v) {
			// Each segment of the polygon becomes an "edge segment" in the graph
			auto v0 = (v+vertices.size()-1)%vertices.size();
			auto v1 = v;
			auto v2 = (v+1)%vertices.size();
			result._edgeSegments.emplace_back(Graph::Segment{(unsigned)v, (unsigned)v2});

			// We must calculate the velocity for each vertex, based on which segments it belongs to...
			auto velocity = CalculateVertexVelocity(vertices[v0], vertices[v1], vertices[v2]);
			result._vertices.emplace_back(Vertex{vertices[v], BoundaryVertexFlag|unsigned(v), 0.0f, velocity});
		}

		// Each reflex vertex in the graph must result in a "motocycle segment".
		// We already know the velocity of the head of the motorcycle; and it has a fixed tail that
		// stays at the original position
		for (size_t v=0; v<vertices.size(); ++v) {
			auto v0 = (v+vertices.size()-1)%vertices.size();
			auto v1 = v;
			auto v2 = (v+1)%vertices.size();

			// Since we're expecting counter-clockwise inputs, if "v1" is a convex vertex, we should
			// wind around to the left when going v0->v1->v2
			// If we wind to the right then it's a reflex vertex, and we must add a motorcycle edge
			if (CalculateWindingType(vertices[v0], vertices[v1], vertices[v2], threshold) == WindingType::Right) {
				auto fixedVertex = (unsigned)(result._vertices.size());
				result._vertices.emplace_back(Vertex{vertices[v], BoundaryVertexFlag|unsigned(v), 0.0f, Zero<Float2>()});
				result._motorcycleSegments.emplace_back(Graph::MotorcycleSegment{(unsigned)v, (unsigned)fixedVertex});
			}
		}

		return result;
	}

	static const float epsilon = 1e-6f;

	static float CalculateCollapseTime(const StraightSkeleton::Vertex& v0, const StraightSkeleton::Vertex& v1)
	{
		// At some point the trajectories of v0 & v1 may intersect
		// Let's assume that the velocities are either zero or unit length
		//		(which should be the case for an unweighted graph)
		// float d = Dot(v0._velocity, v1._velocity);

		// If either velocity is zero, we should get d=0 here
		// Alternatively, if the intersection point is in the reverse direction, we will get
		// a negative number.
		// if (d > -epsilon && d < epsilon) return std::numeric_limits<float>::max();

		// auto o = Truncate(v1._position) - Truncate(v0._position);
		// float d2 = Dot(0, )
		
		// 0 = p0.x + t * v0._velocity.x - p1.x - t * v1._velocity.x
		//	(p1.x - p0.x) / (v0._velocity.x - v1._velocity.x) = t
		
		auto d0x = v0._velocity[0] - v1._velocity[0];
		auto d0y = v0._velocity[1] - v1._velocity[1];
		// We need to pick out a specific time on the timeline, and find both v0 and v1 at that
		// time. This can be any time, but it may be convnient to use time 0
		auto p0 = Float2(v0._position - v0._initialTime * v0._velocity);
		auto p1 = Float2(v1._position - v1._initialTime * v1._velocity);
		if (std::abs(d0x) > std::abs(d0y)) {
			auto  t = (p1[0] - p0[0]) / d0x;

			auto ySep = p0[1] + t * v0._velocity[1]
					   - p1[1] - t * v1._velocity[1];
			if (ySep < epsilon) return t;	// (todo -- we could refine with the y results?
		} else {
			if (std::abs(d0y) < epsilon) return std::numeric_limits<float>::max();
			auto t = (p1[1] - p0[1]) / d0y;

			auto xSep = p0[0] + t * v0._velocity[0]
					   - p1[0] - t * v1._velocity[0];
			if (xSep < epsilon) return t;	// (todo -- we could refine with the y results?
		}

		return std::numeric_limits<float>::max();
	}

	static void ReplaceVertex(IteratorRange<Graph::Segment*> segs, unsigned oldVertex, unsigned newVertex)
	{
		for (auto& s:segs) {
			if (s._head == oldVertex) s._head = newVertex;
			if (s._tail == oldVertex) s._tail = newVertex;
		}
	}

	static unsigned AddSteinerVertex(Skeleton& skeleton, const Float3& vertex, float threshold)
	{
		auto existing = std::find_if(skeleton._steinerVertices.begin(), skeleton._steinerVertices.end(),
			[vertex, threshold](const Float3& v) { return Equivalent(v, vertex, threshold); });
		if (existing != skeleton._steinerVertices.end())
			return (unsigned)std::distance(skeleton._steinerVertices.begin(), existing);
		auto result = (unsigned)skeleton._steinerVertices.size();
		skeleton._steinerVertices.push_back(vertex);
		return result;
	}

	Skeleton Graph::GenerateSkeleton() 
	{
		Skeleton result;
		for (;;) {
			// Find the next event to occur
			//		-- either a edge collapse or a motorcycle collision
			float bestCollapseTime = std::numeric_limits<float>::max();
			std::vector<std::pair<float, size_t>> bestCollapse;
			for (auto e=_edgeSegments.begin(); e!=_edgeSegments.end(); ++e) {
				const auto& v0 = _vertices[e->_head];
				const auto& v1 = _vertices[e->_tail];
				float collapseTime = CalculateCollapseTime(v0, v1);
				// todo -- handle equal collapse times
				if (collapseTime < (bestCollapseTime - epsilon)) {
					bestCollapse.clear();
					bestCollapse.push_back(std::make_pair(collapseTime, std::distance(_edgeSegments.begin(), e)));
					bestCollapseTime = collapseTime;
				} else if (collapseTime < (bestCollapseTime + epsilon)) {
					bestCollapse.push_back(std::make_pair(collapseTime, std::distance(_edgeSegments.begin(), e)));
					bestCollapseTime = std::min(collapseTime, bestCollapseTime);
				}
			}

			// Always ensure that every entry in "bestCollapse" is within
			// "epsilon" of bestCollapseTime -- this can become untrue if there
			// are chains of events with very small gaps in between them
			bestCollapse.erase(
				std::remove_if(
					bestCollapse.begin(), bestCollapse.end(),
					[bestCollapseTime](const std::pair<float, size_t>& e)
					{
						return !(e.first < bestCollapseTime + epsilon);
					}), 
				bestCollapse.end());

			if (bestCollapse.empty()) break;

			// Process the "edge" events
			auto r = _edgeSegments.end()-1;
			for (auto i=bestCollapse.rbegin(); i!=bestCollapse.rend(); ++i) {
				if (i!=bestCollapse.rbegin()) --r;

				// Generate an edge in the skeleton. This edge will represent the motion that the 
				// vertices on either side of the segment took before they collapsed at the collision
				// point
				// It's possible that we may have multiple simulteous collapses of the same vertex
				// at this point. When this happens, ideally we still want to write only a single edge
				// to the output.
				const auto& seg = _edgeSegments[i->second];
				unsigned vs[] = { seg._head, seg._tail };

				// treat the average of the head & tail collision pts as the authoriative collision pt
				auto collisionPt = 
					Expand(
						0.5f * (_vertices[seg._head]._position + (i->first - _vertices[seg._head]._initialTime) * _vertices[seg._head]._velocity
							  + _vertices[seg._tail]._position + (i->first - _vertices[seg._tail]._initialTime) * _vertices[seg._tail]._velocity),
						i->first);
				auto collisionVertId = AddSteinerVertex(result, collisionPt, epsilon);

				for (auto& v:vs) {
					const auto& vert = _vertices[v];
					if (vert._skeletonVertexId != ~0u) {
						result._unplacedEdges.push_back({vert._skeletonVertexId, collisionVertId});
					} else {
						result._unplacedEdges.push_back({
							AddSteinerVertex(result, Expand(vert._position, vert._initialTime), epsilon), 
							collisionVertId});
					}
				}

				assert(!_vertices[seg._tail]._noLongerActive);
				_vertices[seg._tail]._noLongerActive = true;

				// Reassign the head vertex so that it starts from the collision pt and moves out from here
				_vertices[seg._head]._initialTime = i->first;
				_vertices[seg._head]._position = Truncate(collisionPt);
				_vertices[seg._head]._skeletonVertexId = collisionVertId;

				// First; swap the ones we're going to remove to the end of the list (note that we loose ordering
				// for the list as a whole...
				std::swap(*r, _edgeSegments[i->second]);
			}

			// We must remove one vertex of every collapsed edge. We'll
			// remove the tail, and replace references to it with the head
			for (auto i=r; i!=_edgeSegments.end(); ++i) {
				ReplaceVertex(MakeIteratorRange(AsPointer(_edgeSegments.begin()), AsPointer(r)), i->_tail, i->_head);
			}

			_edgeSegments.erase(r, _edgeSegments.end());
		}

		return result;
	}

}}
