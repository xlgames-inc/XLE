// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "StraightSkeleton.h"
#include "../Math/Geometry.h"
#include <stack>

#if defined(_DEBUG)
	// #define EXTRA_VALIDATION
#endif

#pragma warning(disable:4505) // 'SceneEngine::StraightSkeleton::ReplaceVertex': unreferenced local function has been removed

namespace SceneEngine { namespace StraightSkeleton 
{
	static const float epsilon = 1e-5f;
	static const unsigned BoundaryVertexFlag = 1u<<31u;

	enum WindingType { Left, Right, Straight };
	WindingType CalculateWindingType(Float2 zero, Float2 one, Float2 two, float threshold);

	// float sign (fPoint p1, fPoint p2, fPoint p3)
	WindingType CalculateWindingType(Float2 zero, Float2 one, Float2 two, float threshold)
	{
		float sign = (one[0] - zero[0]) * (two[1] - zero[1]) - (two[0] - zero[0]) * (one[1] - zero[1]);
		if (sign < -threshold) return Right;
		if (sign > threshold) return Left;
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

		if (Equivalent(vex0, vex2, epsilon)) return Zero<Float2>();

		auto t0 = Float2(vex1-vex0);
		auto t1 = Float2(vex2-vex1);

		if (Equivalent(t0, Zero<Float2>(), epsilon)) return Zero<Float2>();
		if (Equivalent(t1, Zero<Float2>(), epsilon)) return Zero<Float2>();

		// create normal pointing in direction of movement
		auto N0 = Normalize(Float2(-t0[1], t0[0]));
		auto N1 = Normalize(Float2(-t1[1], t1[0]));
		auto a = N0[0], b = N0[1];
		auto c = N1[0], d = N1[1];
		const auto t = 1.0f;		// time = 1.0f, because we're calculating the velocity

		// Now, line1 is 0 = xa + yb - t and line2 is 0 = xc + yd - t

		// we can calculate the intersection of the lines using this formula...
		float B0 = 0.0f, B1 = 0.0f;
		if (d<-epsilon || d>epsilon) B0 = a - b*c/d;
		if (c<-epsilon || c>epsilon) B1 = b - a*d/c;

		float x, y;
		if (std::abs(B0) > std::abs(B1)) {
			if (B0 > -epsilon && B0 < epsilon) return Zero<Float2>();
			auto A = 1.0f - b/d;
			x = t * A / B0;
			y = (t - x*c) / d;
		} else {
			if (B1 > -epsilon && B1 < epsilon) return Zero<Float2>();
			auto A = 1.0f - a/c;
			y = t * A / B1;
			x = (t - y*d) / c;
		}

		assert(Dot(Float2(x, y), N0+N1) > 0.0f);

		assert(!isnan(x) && isfinite(x) && (x==x));
		assert(!isnan(y) && isfinite(y) && (y==y));
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
		result._wavefrontEdges.reserve(vertices.size());
		result._vertices.reserve(vertices.size());
		for (size_t v=0; v<vertices.size(); ++v) {
			// Each segment of the polygon becomes an "edge segment" in the graph
			auto v0 = (v+vertices.size()-1)%vertices.size();
			auto v1 = v;
			auto v2 = (v+1)%vertices.size();
			result._wavefrontEdges.emplace_back(Graph::Segment{unsigned(v2), unsigned(v), ~0u, unsigned(v)});

			// We must calculate the velocity for each vertex, based on which segments it belongs to...
			auto velocity = CalculateVertexVelocity(vertices[v0], vertices[v1], vertices[v2]);
			assert(!Equivalent(velocity, Zero<Float2>(), epsilon));
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
				result._motorcycleSegments.emplace_back(Graph::MotorcycleSegment{unsigned(v), unsigned(fixedVertex), unsigned(v0), unsigned(v1)});
			}
		}

		result._boundaryPoints = std::vector<Float2>(vertices.begin(), vertices.end());
		return result;
	}

	static float CalculateCollapseTime(Float2 p0, Float2 v0, Float2 p1, Float2 v1)
	{
		auto d0x = v0[0] - v1[0];
		auto d0y = v0[1] - v1[1];
		if (std::abs(d0x) > std::abs(d0y)) {
			if (std::abs(d0x) < epsilon) return std::numeric_limits<float>::max();
			auto t = (p1[0] - p0[0]) / d0x;

			auto ySep = p0[1] + t * v0[1] - p1[1] - t * v1[1];
			if (std::abs(ySep) < 1e-3f) {
				// assert(std::abs(p0[0] + t * v0[0] - p1[0] - t * v1[0]) < epsilon);
				return t;	// (todo -- we could refine with the x results?
			}
		} else {
			if (std::abs(d0y) < epsilon) return std::numeric_limits<float>::max();
			auto t = (p1[1] - p0[1]) / d0y;

			auto xSep = p0[0] + t * v0[0] - p1[0] - t * v1[0];
			if (std::abs(xSep) < 1e-3f) {
				// sassert(std::abs(p0[1] + t * v0[1] - p1[1] - t * v1[1]) < epsilon);
				return t;	// (todo -- we could refine with the y results?
			}
		}

		return std::numeric_limits<float>::max();
	}

	static float CalculateCollapseTime(const StraightSkeleton::Vertex& v0, const StraightSkeleton::Vertex& v1)
	{
		// hack -- if one side is frozen, we must collapse immediately
		if (Equivalent(v0._velocity, Zero<Float2>(), epsilon)) return std::numeric_limits<float>::max();
		if (Equivalent(v1._velocity, Zero<Float2>(), epsilon)) return std::numeric_limits<float>::max();

		// At some point the trajectories of v0 & v1 may intersect
		// We need to pick out a specific time on the timeline, and find both v0 and v1 at that
		// time. 
		float calcTime = std::min(v0._initialTime, v1._initialTime);
		auto p0 = Float2(v0._position + (calcTime-v0._initialTime) * v0._velocity);
		auto p1 = Float2(v1._position + (calcTime-v1._initialTime) * v1._velocity);
		return calcTime + CalculateCollapseTime(p0, v0._velocity, p1, v1._velocity);
	}

	static void ReplaceVertex(IteratorRange<Graph::Segment*> segs, unsigned oldVertex, unsigned newVertex)
	{
		for (auto& s:segs) {
			if (s._head == oldVertex) s._head = newVertex;
			if (s._tail == oldVertex) s._tail = newVertex;
		}
	}

	static unsigned AddSteinerVertex(Skeleton& skeleton, const Float3& vertex)
	{
		assert(vertex[2] != 0.0f);
		assert(isfinite(vertex[0]) && isfinite(vertex[1]) && isfinite(vertex[2]));
		assert(!isnan(vertex[0]) && !isnan(vertex[1]) && !isnan(vertex[2]));
		assert(vertex[0] == vertex[0] && vertex[1] == vertex[1] && vertex[2] == vertex[2]);
		assert(vertex[0] != std::numeric_limits<float>::max() && vertex[1] != std::numeric_limits<float>::max() && vertex[2] != std::numeric_limits<float>::max());
		auto existing = std::find_if(skeleton._steinerVertices.begin(), skeleton._steinerVertices.end(),
			[vertex](const Float3& v) { return Equivalent(v, vertex, epsilon); });
		if (existing != skeleton._steinerVertices.end()) {
			return (unsigned)std::distance(skeleton._steinerVertices.begin(), existing);
		}
		#if defined(_DEBUG)
			auto test = std::find_if(skeleton._steinerVertices.begin(), skeleton._steinerVertices.end(),
				[vertex](const Float3& v) { return Equivalent(Truncate(v), Truncate(vertex), epsilon); });
			assert(test == skeleton._steinerVertices.end());
		#endif
		auto result = (unsigned)skeleton._steinerVertices.size();
		skeleton._steinerVertices.push_back(vertex);
		return result;
	}

	static Float2 PositionAtTime(const Vertex& v, float time)
	{
		auto result = v._position + v._velocity * (time - v._initialTime);
		assert(!isnan(result[0]) && !isnan(result[1]));
		assert(isfinite(result[0]) && isfinite(result[1]));
		assert(result[0] == result[0] && result[1] == result[1]);
		return result;
	}

	static Float3 ClampedPositionAtTime(const Vertex& v, float time)
	{
		if (Equivalent(v._velocity, Zero<Float2>(), epsilon))
			return Expand(v._position, v._initialTime);
		return Expand(PositionAtTime(v, time), time);
	}

	struct CrashEvent
	{
		float _time;
		unsigned _edgeSegment;
	};

	static CrashEvent CalculateCrashTime(
		Graph& graph, Float2 position, Float2 velocity, float initialTime, 
		const Graph::MotorcycleSegment* cycleToSkip = nullptr, unsigned boundaryPtToSkip = ~0u)
	{
		auto p2 = Float2(position - initialTime * velocity);
		auto v2 = velocity;
		CrashEvent bestCollisionEvent { std::numeric_limits<float>::max(), ~0u };

		// Look for an intersection with _wavefrontEdges
		for (const auto&e:graph._wavefrontEdges) {
			const auto& head = graph._vertices[e._head];
			const auto& tail = graph._vertices[e._tail];

			// Since the edge segments are moving, the solution is a little complex
			// We can create a triangle between head, tail & the motorcycle head
			// If there is a collision, the triangle area will be zero at that point.
			// So we can search for a time when the triangle area is zero, and check to see
			// if a collision has actually occurred at that time.
			auto p0 = Float2(head._position - head._initialTime * head._velocity);
			auto p1 = Float2(tail._position - tail._initialTime * tail._velocity);
			auto v0 = head._velocity;
			auto v1 = tail._velocity;

			// 2 * signed triangle area = 
			//		(p1[0]-p0[0]) * (p2[1]-p0[1]) - (p2[0]-p0[0]) * (p1[1]-p0[1])
			//
			// A =	(p1[0]+t*v1[0]-p0[0]-t*v0[0]) * (p2[1]+t*v2[1]-p0[1]-t*v0[1])
			// B =  (p2[0]+t*v2[0]-p0[0]-t*v0[0]) * (p1[1]+t*v1[1]-p0[1]-t*v0[1]);
			//
			// A =   (p1[0]-p0[0]) * (p2[1]+t*v2[1]-p0[1]-t*v0[1])
			//	 + t*(v1[0]-v0[0]) * (p2[1]+t*v2[1]-p0[1]-t*v0[1])
			//
			// A =   (p1[0]-p0[0]) * (p2[1]-p0[1]+t*(v2[1]-v0[1]))
			//	 + t*(v1[0]-v0[0]) * (p2[1]-p0[1]+t*(v2[1]-v0[1]))
			//
			// A =   (p1[0]-p0[0])*(p2[1]-p0[1]) + t*(p1[0]-p0[0])*(v2[1]-v0[1])
			//	 + t*(v1[0]-v0[0])*(p2[1]-p0[1]) + t*t*(v1[0]-v0[0])*(v2[1]-v0[1])
			//
			// B =   (p2[0]-p0[0])*(p1[1]-p0[1]) + t*(p2[0]-p0[0])*(v1[1]-v0[1])
			//	 + t*(v2[0]-v0[0])*(p1[1]-p0[1]) + t*t*(v2[0]-v0[0])*(v1[1]-v0[1])
			//
			// 0 = t*t*a + t*b + c
			// c = (p1[0]-p0[0])*(p2[1]-p0[1]) - (p2[0]-p0[0])*(p1[1]-p0[1])
			// b = (p1[0]-p0[0])*(v2[1]-v0[1]) + (v1[0]-v0[0])*(p2[1]-p0[1]) - (p2[0]-p0[0])*(v1[1]-v0[1]) - (v2[0]-v0[0])*(p1[1]-p0[1])
			// a = (v1[0]-v0[0])*(v2[1]-v0[1]) - (v2[0]-v0[0])*(v1[1]-v0[1])

			auto c = (p1[0]-p0[0])*(p2[1]-p0[1]) - (p2[0]-p0[0])*(p1[1]-p0[1]);
			auto b = (p1[0]-p0[0])*(v2[1]-v0[1]) + (v1[0]-v0[0])*(p2[1]-p0[1]) - (p2[0]-p0[0])*(v1[1]-v0[1]) - (v2[0]-v0[0])*(p1[1]-p0[1]);
			auto a = (v1[0]-v0[0])*(v2[1]-v0[1]) - (v2[0]-v0[0])*(v1[1]-v0[1]);

			// x = (-b +/- sqrt(b*b - 4*a*c)) / 2*a
			auto Q = sqrt(b*b - 4*a*c);
			float t[] = {
				(-b + Q) / (2.0f*a),
				(-b - Q) / (2.0f*a)
			};

			// Is there is a viable collision at either t0 or t1?
			// All 3 points should be on the same line at this point -- so we just need to check if
			// the motorcycle is between them (or intersecting a vertex)
			for (unsigned c=0; c<2; ++c) {
				if (t[c] > bestCollisionEvent._time || t[c] < std::max(head._initialTime, tail._initialTime)) continue;	// don't need to check collisions that happen too late
				auto P0 = Float2(p0 + t[c]*v0);
				auto P1 = Float2(p1 + t[c]*v1);
				auto P2 = Float2(p2 + t[c]*v2);
				if ((Dot(P1-P0, P2-P0) > 0.0f) && (Dot(P0-P1, P2-P1) > 0.0f)) {
					// good collision
					bestCollisionEvent._time = t[c];
					bestCollisionEvent._edgeSegment = unsigned(&e - AsPointer(graph._wavefrontEdges.begin()));
				} else if (Equivalent(P0, P2, epsilon) || Equivalent(P1, P2, epsilon)) {
					// collided with vertex (or close enough)
					bestCollisionEvent._time = t[c];
					bestCollisionEvent._edgeSegment = unsigned(&e - AsPointer(graph._wavefrontEdges.begin()));
				}
			}
		}

		return bestCollisionEvent;
	}

	static std::pair<Graph::Segment*, Graph::Segment*> FindInAndOut(IteratorRange<Graph::Segment*> edges, unsigned pivotVertex)
	{
		std::pair<Graph::Segment*, Graph::Segment*> result(nullptr, nullptr);
		for  (auto&s:edges) {
			if (s._head == pivotVertex) {
				assert(!result.first);
				result.first = &s;
			} else if (s._tail == pivotVertex) {
				assert(!result.second);
				result.second = &s;
			}
		}
		return result;
	}

	static bool IsFrozen(const Vertex& v) { return Equivalent(v._velocity, Zero<Float2>(), 0.0f); }
	static void FreezeInPlace(Vertex& v, float atTime)
	{
		assert(atTime != 0.0f);
		v._position = PositionAtTime(v, atTime);
		v._initialTime = atTime;
		v._skeletonVertexId = ~0u;
		v._velocity = Zero<Float2>();
	}

	static void AddUnique(std::vector<Skeleton::Edge>& dst, const Skeleton::Edge& edge)
	{
		auto existing = std::find_if(dst.begin(), dst.end(), 
			[&edge](const Skeleton::Edge&e) { return e._head == edge._head && e._tail == edge._tail; });

		if (existing == dst.end()) {
			dst.push_back(edge);
		} else {
			assert(existing->_type == edge._type);
		}
	}

	static void AddEdge(Skeleton& dest, unsigned headVertex, unsigned tailVertex, unsigned leftEdge, unsigned rightEdge, Skeleton::EdgeType type)
	{
		if (headVertex == tailVertex) return;

		// todo -- edge ordering may be flipped here....?
		if (rightEdge != ~0u) {
			AddUnique(dest._faces[rightEdge]._edges, {headVertex, tailVertex, type});
		} else {
			AddUnique(dest._unplacedEdges, {headVertex, tailVertex, type});
		}
		if (leftEdge != ~0u) {
			AddUnique(dest._faces[leftEdge]._edges, {tailVertex, headVertex, type});
		} else {
			AddUnique(dest._unplacedEdges, {tailVertex, headVertex, type});
		}
	}

	Skeleton Graph::GenerateSkeleton(float maxTime)
	{
		Skeleton result;
		result._faces.resize(_boundaryPoints.size());

		std::vector<std::pair<float, size_t>> bestCollapse;
		std::vector<std::pair<CrashEvent, size_t>> bestMotorcycleCrash;
		bestCollapse.reserve(8);

		float lastEventTime = 0.0f;
		unsigned lastEvent = 0;

		for (;;) {
			#if defined(EXTRA_VALIDATION)
				// validate vertex velocities
				for (unsigned v=0; v<_vertices.size(); ++v) {
					Segment* in, *out;
					std::tie(in, out) = FindInAndOut(MakeIteratorRange(_wavefrontEdges), v);
					if (in && out) {
						auto calcTime = (_vertices[in->_tail]._initialTime + _vertices[v]._initialTime + _vertices[out->_head]._initialTime) / 3.0f;
						auto v0 = PositionAtTime(_vertices[in->_tail], calcTime);
						auto v1 = PositionAtTime(_vertices[v], calcTime);
						auto v2 = PositionAtTime(_vertices[out->_head], calcTime);
						auto expectedVelocity = CalculateVertexVelocity(v0, v1, v2);
						assert(Equivalent(_vertices[v]._velocity, expectedVelocity, 1e-3f));
					}
				}
				// every wavefront edge must have a collapse time (assuming it's vertices are not frozen)
				for (const auto&e:_wavefrontEdges) {
					if (IsFrozen(_vertices[e._head]) || IsFrozen(_vertices[e._tail])) continue;
					auto collapseTime = CalculateCollapseTime(_vertices[e._head], _vertices[e._tail]);
					assert(collapseTime != std::numeric_limits<float>::max());		//it can be negative; because some edges are expanding
				}
			#endif

			// Find the next event to occur
			//		-- either a edge collapse or a motorcycle collision
			float bestCollapseTime = std::numeric_limits<float>::max();
			bestCollapse.clear();
			for (auto e=_wavefrontEdges.begin(); e!=_wavefrontEdges.end(); ++e) {
				const auto& v0 = _vertices[e->_head];
				const auto& v1 = _vertices[e->_tail];
				auto collapseTime = CalculateCollapseTime(v0, v1);
				if (collapseTime < 0.0f) continue;
				assert(collapseTime >= lastEventTime);
				if (collapseTime < (bestCollapseTime - epsilon)) {
					bestCollapse.clear();
					bestCollapse.push_back(std::make_pair(collapseTime, std::distance(_wavefrontEdges.begin(), e)));
					bestCollapseTime = collapseTime;
				} else if (collapseTime < (bestCollapseTime + epsilon)) {
					bestCollapse.push_back(std::make_pair(collapseTime, std::distance(_wavefrontEdges.begin(), e)));
					bestCollapseTime = std::min(collapseTime, bestCollapseTime);
				}
			}

			// Always ensure that every entry in "bestCollapse" is within
			// "epsilon" of bestCollapseTime -- this can become untrue if there
			// are chains of events with very small gaps in between them
			bestCollapse.erase(
				std::remove_if(
					bestCollapse.begin(), bestCollapse.end(),
					[bestCollapseTime](const std::pair<float, size_t>& e) { return !(e.first < bestCollapseTime + epsilon); }), 
				bestCollapse.end());

			// Also check for motorcycles colliding.
			//		These can collide with segments in the _wavefrontEdges list, or 
			//		other motorcycles, or boundary polygon edges
			float bestMotorcycleCrashTime = std::numeric_limits<float>::max();
			bestMotorcycleCrash.clear();
			for (auto m=_motorcycleSegments.begin(); m!=_motorcycleSegments.end(); ++m) {
				const auto& head = _vertices[m->_head];
				if (Equivalent(head._velocity, Zero<Float2>(), epsilon)) continue;
				assert(head._initialTime == 0.0f);
				auto crashEvent = CalculateCrashTime(*this, head._position, head._velocity, head._initialTime, AsPointer(m), head._skeletonVertexId);
				if (crashEvent._time < 0.0f) continue;
				assert(crashEvent._time >= lastEventTime);

				// If our best motorcycle collision happens before our best collapse, then we
				// must do the motorcycle first, and recalculate edge collapses afterwards
				// But if they happen at the same time, we should do the edge collapse first,
				// and then recalculate the motorcycle collisions afterwards (ie, even if there's
				// a motorcycle collision at around the same time as the edge collapses, we're
				// going to ignore it for now)
				if (crashEvent._time < (bestCollapseTime + epsilon)) {
					if (crashEvent._time < (bestMotorcycleCrashTime - epsilon)) {
						bestMotorcycleCrash.clear();
						bestMotorcycleCrash.push_back(std::make_pair(crashEvent, std::distance(_motorcycleSegments.begin(), m)));
						bestMotorcycleCrashTime = crashEvent._time;
					} else if (crashEvent._time < (bestMotorcycleCrashTime + epsilon)) {
						bestMotorcycleCrash.push_back(std::make_pair(crashEvent, std::distance(_motorcycleSegments.begin(), m)));
						bestMotorcycleCrashTime = std::min(crashEvent._time, bestMotorcycleCrashTime);
					}
				}
			}

			// If we get some motorcycle crashes, we're going to ignore the collapse events
			// and just process the motorcycle events
			if (!bestMotorcycleCrash.empty()) {
				if (bestMotorcycleCrashTime > maxTime) break;

				bestMotorcycleCrash.erase(
					std::remove_if(
						bestMotorcycleCrash.begin(), bestMotorcycleCrash.end(),
						[bestMotorcycleCrashTime](const std::pair<CrashEvent, size_t>& e) { return !(e.first._time < bestMotorcycleCrashTime + epsilon); }), 
					bestMotorcycleCrash.end());

				// we can only process a single crash event at a time currently
				// only the first event in bestMotorcycleCrashwill be processed (note that
				// this isn't necessarily the first event!)
				assert(bestMotorcycleCrash.size() == 1);
				auto crashEvent = bestMotorcycleCrash[0].first;
				const auto& motor = _motorcycleSegments[bestMotorcycleCrash[0].second];
				assert(crashEvent._edgeSegment != ~0u);

				auto crashPt = PositionAtTime(_vertices[motor._head], crashEvent._time);
				auto crashPtSkeleton = AddSteinerVertex(result, Float3(crashPt, crashEvent._time));

				auto crashSegment = _wavefrontEdges[crashEvent._edgeSegment];
				Segment newSegment0{ motor._head, motor._head, motor._leftFace, crashSegment._rightFace};
				Segment newSegment1{ motor._head, motor._head, crashSegment._rightFace, motor._rightFace };
				auto calcTime = crashEvent._time;

				// is there volume on the "tout" side?
				{
					auto* tout = FindInAndOut(MakeIteratorRange(_wavefrontEdges), motor._head).second;
					assert(tout);

					auto v0 = ClampedPositionAtTime(_vertices[crashSegment._tail], calcTime);
					auto v2 = ClampedPositionAtTime(_vertices[tout->_head], calcTime);
					if (tout->_head == crashSegment._tail || Equivalent(v0, v2, epsilon)) {
						// no longer need crashSegment or tout
						assert(crashSegment._leftFace == ~0u && tout->_leftFace == ~0u);
						auto endPt = AddSteinerVertex(result, (v0+v2)/2.0f);
						AddEdge(result, endPt, crashPtSkeleton, crashSegment._rightFace, tout->_rightFace, Skeleton::EdgeType::VertexPath);
						// tout->_head & crashSegment._tail end here. We must draw the skeleton segment 
						// tracing out their path
						AddEdgeForVertexPath(result, tout->_head, endPt);
						AddEdgeForVertexPath(result, crashSegment._tail, endPt);
						// todo -- there may be a chain of collapsing that occurs now... we should follow it along...
						// We still need to add a wavefront edge to close and the loop, and ensure we don't leave
						// stranded edges. Without this we can easily get a single edge without anything looping
						// it back around (or just an unclosed loop)
						auto toutHead = tout->_head;
						_wavefrontEdges.erase(_wavefrontEdges.begin()+(tout-AsPointer(_wavefrontEdges.begin())));
						if (toutHead != crashSegment._tail) {
							auto existing = std::find_if(_wavefrontEdges.begin(), _wavefrontEdges.end(), 
								[toutHead, crashSegment](const Segment&s) { return s._head == crashSegment._tail && s._tail == toutHead; });
							if (existing != _wavefrontEdges.end()) {
								_wavefrontEdges.push_back({toutHead, crashSegment._tail, existing->_rightFace, existing->_leftFace});
							} else {
								_wavefrontEdges.push_back({toutHead, crashSegment._tail, ~0u, ~0u});
							}
						}
					} else {
						auto newVertex = (unsigned)_vertices.size();
						tout->_tail = newVertex;
						_wavefrontEdges.push_back({newVertex, crashSegment._tail, crashSegment._leftFace, crashSegment._rightFace});	// (hin)

						_vertices.push_back(Vertex{crashPt, crashPtSkeleton, crashEvent._time, CalculateVertexVelocity(Truncate(v0), crashPt, Truncate(v2))});
						newSegment1._head = newVertex;
					}
				}

				// is there volume on the "tin" side?
				{
					auto* tin = FindInAndOut(MakeIteratorRange(_wavefrontEdges), motor._head).first;
					assert(tin);

					auto v0 = ClampedPositionAtTime(_vertices[tin->_tail], calcTime);
					auto v2 = ClampedPositionAtTime(_vertices[crashSegment._head], calcTime);
					if (tin->_tail == crashSegment._head || Equivalent(v0, v2, epsilon)) {
						// no longer need "crashSegment" or tin
						assert(crashSegment._leftFace == ~0u && tin->_leftFace == ~0u);
						auto endPt = AddSteinerVertex(result, (v0+v2)/2.0f);
						AddEdge(result, endPt, crashPtSkeleton, tin->_rightFace, crashSegment._rightFace, Skeleton::EdgeType::VertexPath);
						// tin->_tail & crashSegment._head end here. We must draw the skeleton segment 
						// tracing out their path
						AddEdgeForVertexPath(result, tin->_tail, endPt);
						AddEdgeForVertexPath(result, crashSegment._head, endPt);
						// todo -- there may be a chain of collapsing that occurs now... we should follow it along...
						// We still need to add a wavefront edge to close and the loop, and ensure we don't leave
						// stranded edges. Without this we can easily get a single edge without anything looping
						// it back around (or just an unclosed loop)
						auto tinTail = tin->_tail;
						_wavefrontEdges.erase(_wavefrontEdges.begin()+(tin-AsPointer(_wavefrontEdges.begin())));
						if (tinTail != crashSegment._head) {
							auto existing = std::find_if(_wavefrontEdges.begin(), _wavefrontEdges.end(), 
								[tinTail, crashSegment](const Segment&s) { return s._head == tinTail && s._tail == crashSegment._head; });
							if (existing != _wavefrontEdges.end()) {
								_wavefrontEdges.push_back({crashSegment._head, tinTail, existing->_rightFace, existing->_leftFace});
							} else
								_wavefrontEdges.push_back({crashSegment._head, tinTail, ~0u, ~0u});
						}
					} else {
						auto newVertex = (unsigned)_vertices.size();
						tin->_head = newVertex;
						_wavefrontEdges.push_back({crashSegment._head, newVertex, crashSegment._leftFace, crashSegment._rightFace});	// (hout)

						_vertices.push_back(Vertex{crashPt, crashPtSkeleton, crashEvent._time, CalculateVertexVelocity(Truncate(v0), crashPt, Truncate(v2))});
						newSegment0._head = newVertex;
					}
				}

				// if (newSegment0._head != newSegment0._tail) _wavefrontEdges.push_back(newSegment0);
				// if (newSegment1._head != newSegment1._tail) _wavefrontEdges.push_back(newSegment1);

				// note -- we can't erase this edge too soon, because it's used to calculate left and right faces
				// when calling AddEdgeForVertexPath 
				_wavefrontEdges.erase(
					std::remove_if(	_wavefrontEdges.begin(), _wavefrontEdges.end(), 
									[crashSegment](const Segment& s) { return s._head == crashSegment._head && s._tail == crashSegment._tail; }), 
					_wavefrontEdges.end());

				// add skeleton edge from the  
				assert(_vertices[motor._tail]._skeletonVertexId != ~0u);
				AddEdge(result, 
						crashPtSkeleton, _vertices[motor._tail]._skeletonVertexId,
						motor._leftFace, motor._rightFace, Skeleton::EdgeType::VertexPath);
				FreezeInPlace(_vertices[motor._head], crashEvent._time);

				_motorcycleSegments.erase(_motorcycleSegments.begin() + bestMotorcycleCrash[0].second);

				#if defined(EXTRA_VALIDATION)
					{
						for (auto m=_motorcycleSegments.begin(); m!=_motorcycleSegments.end(); ++m) {
							const auto& head = _vertices[m->_head];
							if (Equivalent(head._velocity, Zero<Float2>(), epsilon)) continue;
							auto nextCrashEvent = CalculateCrashTime(*this, head._position, head._velocity, head._initialTime, AsPointer(m), head._skeletonVertexId);
							if (nextCrashEvent._time < 0.0f) continue;
							assert(nextCrashEvent._time >= crashEvent._time);
						}
					}
				#endif

				lastEventTime = crashEvent._time;
				lastEvent = 1;
			} else {
				if (bestCollapse.empty()) break;
				if (bestCollapseTime > maxTime) break;

				// Process the "edge" events... first separate the edges into collapse groups
				// Each collapse group collapses onto a single vertex. We will search through all
				// of the collapse events we're processing, and separate them into discrete groups.
				std::vector<unsigned> collapseGroups(bestCollapse.size(), ~0u);
				struct CollapseGroupInfo { unsigned _head, _tail, _newVertex; };
				std::vector<CollapseGroupInfo> collapseGroupInfos;
				unsigned nextCollapseGroup = 0;
				for (size_t c=0; c<bestCollapse.size(); ++c) {
					if (collapseGroups[c] != ~0u) continue;

					collapseGroups[c] = nextCollapseGroup;

					// got back as far as possible, from tail to tail
					auto searchingTail =_wavefrontEdges[bestCollapse[c].second]._tail;
					for (;;) {
						auto i = std::find_if(bestCollapse.begin(), bestCollapse.end(),
							[searchingTail, this](const std::pair<float, size_t>& t)
							{ return _wavefrontEdges[t.second]._head == searchingTail; });
						if (i == bestCollapse.end()) break;
						if (collapseGroups[std::distance(bestCollapse.begin(), i)] == nextCollapseGroup) break;
						assert(collapseGroups[std::distance(bestCollapse.begin(), i)] == ~0u);
						collapseGroups[std::distance(bestCollapse.begin(), i)] = nextCollapseGroup;
						searchingTail = _wavefrontEdges[i->second]._tail;
					}

					// also go forward head to head
					auto searchingHead =_wavefrontEdges[bestCollapse[c].second]._head;
					for (;;) {
						auto i = std::find_if(bestCollapse.begin(), bestCollapse.end(),
							[searchingHead, this](const std::pair<float, size_t>& t)
							{ return _wavefrontEdges[t.second]._tail == searchingHead; });
						if (i == bestCollapse.end()) break;
						if (collapseGroups[std::distance(bestCollapse.begin(), i)] == nextCollapseGroup) break;
						assert(collapseGroups[std::distance(bestCollapse.begin(), i)] == ~0u);
						collapseGroups[std::distance(bestCollapse.begin(), i)] = nextCollapseGroup;
						searchingHead = _wavefrontEdges[i->second]._head;
					}

					++nextCollapseGroup;
					collapseGroupInfos.push_back({searchingHead, searchingTail, ~0u});
				}

				// Each collapse group becomes a single new vertex. We can collate them together
				// now, and write out some segments to the output skeleton
				std::vector<unsigned> collapseGroupNewVertex(nextCollapseGroup, ~0u);
				for (auto collapseGroup=0u; collapseGroup<nextCollapseGroup; ++collapseGroup) {
					Float2 collisionPt(0.0f, 0.0f);
					unsigned contributors = 0;
					for (size_t c=0; c<bestCollapse.size(); ++c) {
						if (collapseGroups[c] != collapseGroup) continue;
						const auto& seg = _wavefrontEdges[bestCollapse[c].second];
						collisionPt += PositionAtTime(_vertices[seg._head], bestCollapseTime);
						collisionPt += PositionAtTime(_vertices[seg._tail], bestCollapseTime);
						contributors += 2;

						// at this point they should not be frozen (but they will all be frozen later)
						assert(!IsFrozen(_vertices[seg._tail]));
						assert(!IsFrozen(_vertices[seg._head]));
					}
					collisionPt /= float(contributors);

					// Validate that our "collisionPt" is close to all of the collapsing points
					#if defined(_DEBUG)
						for (size_t c=0; c<bestCollapse.size(); ++c) {
							if (collapseGroups[c] != collapseGroup) continue;
							const auto& seg = _wavefrontEdges[bestCollapse[c].second];
							auto one = PositionAtTime(_vertices[seg._head], bestCollapseTime);
							auto two = PositionAtTime(_vertices[seg._tail], bestCollapseTime);
							assert(Equivalent(one, collisionPt, 1e-3f));
							assert(Equivalent(two, collisionPt, 1e-3f));
						}
					#endif

					// add a steiner vertex into the output
					auto collisionVertId = AddSteinerVertex(result, Float3(collisionPt, bestCollapseTime));

					// connect up edges in the output graph
					// Note that since we're connecting both head and tail, we'll end up doubling up each edge
					for (size_t c=0; c<bestCollapse.size(); ++c) {
						if (collapseGroups[c] != collapseGroup) continue;
						const auto& seg = _wavefrontEdges[bestCollapse[c].second];
						unsigned vs[] = { seg._head, seg._tail };
						for (auto& v:vs) {
							AddEdgeForVertexPath(result, v, collisionVertId);
						}
					
						FreezeInPlace(_vertices[seg._tail], bestCollapseTime);
						FreezeInPlace(_vertices[seg._head], bestCollapseTime);
					}

					// create a new vertex in the graph to connect the edges to either side of the collapse
					collapseGroupInfos[collapseGroup]._newVertex = (unsigned)_vertices.size();
					_vertices.push_back(Vertex{collisionPt, collisionVertId, bestCollapseTime, Float2(0.0f,0.0f)});
				}

				// Remove all of the collapsed edges (by shifting them to the end)
				// (note, expecting bestCollapse to be sorted by "second")
				auto r = _wavefrontEdges.end()-1;
				for (auto i=bestCollapse.rbegin(); i!=bestCollapse.rend(); ++i) {
					if (i!=bestCollapse.rbegin()) --r;
					// Swap the ones we're going to remove to the end of the list (note that we loose ordering
					// for the list as a whole...
					std::swap(*r, _wavefrontEdges[i->second]);
				}
				_wavefrontEdges.erase(r, _wavefrontEdges.end());

				// For each collapse group, there should be one tail edge, and one head edge
				// We need to find these edges in order to calculate the velocity of the point in between
				// Let's resolve that now...
				for (const auto& group:collapseGroupInfos) {
					if (group._head == group._tail) continue;	// if we remove an entire loop, let's assume that there are no external links to it

					auto tail = FindInAndOut(MakeIteratorRange(_wavefrontEdges), group._tail).first;
					auto head = FindInAndOut(MakeIteratorRange(_wavefrontEdges), group._head).second;
					assert(tail && head);

					tail->_head = group._newVertex;
					head->_tail = group._newVertex;
					auto calcTime = _vertices[group._newVertex]._initialTime;
					auto v0 = PositionAtTime(_vertices[tail->_tail], calcTime);
					auto v1 = _vertices[group._newVertex]._position;
					auto v2 = PositionAtTime(_vertices[head->_head], calcTime);
					_vertices[group._newVertex]._velocity = CalculateVertexVelocity(v0, v1, v2);

					#if defined(EXTRA_VALIDATION)
						{
							assert(CalculateCollapseTime(_vertices[tail->_tail], _vertices[group._newVertex]) >= _vertices[group._newVertex]._initialTime);
							assert(CalculateCollapseTime(_vertices[group._newVertex], _vertices[head->_head]) >= _vertices[group._newVertex]._initialTime);

							auto calcTime = (_vertices[tail->_tail]._initialTime + _vertices[group._newVertex]._initialTime + _vertices[head->_head]._initialTime) / 3.0f;
							auto v0 = PositionAtTime(_vertices[tail->_tail], calcTime);
							auto v1 = PositionAtTime(_vertices[group._newVertex], calcTime);
							auto v2 = PositionAtTime(_vertices[head->_head], calcTime);
							
							auto validatedVelocity = CalculateVertexVelocity(v0, v1, v2);
							assert(Equivalent(validatedVelocity, _vertices[group._newVertex]._velocity, epsilon));
						}
					#endif
				}
			
				lastEventTime = bestCollapseTime;
				lastEvent = 2;
			}
		}

		if (maxTime == std::numeric_limits<float>::max())
			maxTime = lastEventTime;
		WriteWavefront(result, maxTime);

		return result;
	}

	static float ClosestPointOnLine2D(Float2 rayStart, Float2 rayEnd, Float2 testPt)
	{
		auto o = testPt - rayStart;
		auto l = rayEnd - rayStart;
		return Dot(o, l) / MagnitudeSquared(l);
	}

	static bool DoColinearLinesIntersect(Float2 AStart, Float2 AEnd, Float2 BStart, Float2 BEnd)
	{
		// return false if the lines share a point, but otherwise do not intersect
		// but returns true if the lines overlap completely (even if the lines have zero length)
		auto closestBStart = ClosestPointOnLine2D(AStart, AEnd, BStart);
		auto closestBEnd = ClosestPointOnLine2D(AStart, AEnd, BEnd);
		return ((closestBStart > epsilon) && (closestBStart > 1.0f-epsilon))
			|| ((closestBEnd > epsilon) && (closestBEnd > 1.0f-epsilon))
			|| (Equivalent(AStart, BStart, epsilon) && Equivalent(AEnd, BEnd, epsilon))
			|| (Equivalent(AEnd, BStart, epsilon) && Equivalent(AStart, BEnd, epsilon))
			;
	}

	void Graph::WriteWavefront(Skeleton& result, float time)
	{
		// Write the current wavefront to the destination skeleton. Each edge in 
		// _wavefrontEdges comes a segment in the output
		// However, we must check for overlapping / intersecting edges
		//	-- these happen very frequently
		// The best way to remove overlapping edges is just to go through the list of segments, 
		// and for each one look for other segments that intersect

		std::vector<Segment> filteredSegments;
		std::stack<Segment> segmentsToTest;

		// We need to combine overlapping points at this stage, also
		// (2 different vertices could end up at the same location at time 'time')

		for (auto i=_wavefrontEdges.begin(); i!=_wavefrontEdges.end(); ++i) {
			auto A = ClampedPositionAtTime(_vertices[i->_head], time);
			auto B = ClampedPositionAtTime(_vertices[i->_tail], time);
			auto v0 = AddSteinerVertex(result, A);
			auto v1 = AddSteinerVertex(result, B);
			if (v0 != v1)
				segmentsToTest.push(Segment{v0, v1, i->_leftFace, i->_rightFace});
		}

		while (!segmentsToTest.empty()) {
			auto seg = segmentsToTest.top();
			segmentsToTest.pop();

			auto A = Truncate(result._steinerVertices[seg._head]);
			auto B = Truncate(result._steinerVertices[seg._tail]);
			bool filterOutSeg = false;

			// Compare against all edges already in "filteredSegments"
			for (auto i2=filteredSegments.begin(); i2!=filteredSegments.end();++i2) {

				if (i2->_head == seg._head && i2->_tail == seg._tail) {
					if (i2->_leftFace == ~0u) i2->_leftFace = seg._leftFace;
					if (i2->_rightFace == ~0u) i2->_rightFace = seg._rightFace;
					filterOutSeg = true; 
					break; // (overlap completely)
				} else if (i2->_head == seg._tail && i2->_tail == seg._head) {
					if (i2->_leftFace == ~0u) i2->_leftFace = seg._rightFace;
					if (i2->_rightFace == ~0u) i2->_rightFace = seg._leftFace;
					filterOutSeg = true; 
					break; // (overlap completely)
				}

				// If they intersect, they should be colinear, and at least one 
				// vertex if i2 should lie on i
				auto C = Truncate(result._steinerVertices[i2->_head]);
				auto D = Truncate(result._steinerVertices[i2->_tail]);
				auto closestC = ClosestPointOnLine2D(A, B, C);
				auto closestD = ClosestPointOnLine2D(A, B, D);

				bool COnLine = closestC > 0.0f && closestC < 1.0f && MagnitudeSquared(LinearInterpolate(A, B, closestC) - C) < epsilon;
				bool DOnLine = closestD > 0.0f && closestD < 1.0f && MagnitudeSquared(LinearInterpolate(A, B, closestD) - D) < epsilon;
				if (!COnLine && !DOnLine) { continue; }

				float m0 = (B[1] - A[1]) / (B[0] - A[0]);
				float m1 = (D[1] - C[1]) / (D[0] - C[0]);
				if (!Equivalent(m0, m1, epsilon)) { continue; }

				if (i2->_head == seg._head) {
					if (closestD < 1.0f) {
						seg._head = i2->_tail;
					} else {
						i2->_head = seg._tail;
					}
				} else if (i2->_head == seg._tail) {
					if (closestD > 0.0f) {
						seg._tail = i2->_tail;
					} else {
						i2->_head = seg._head;
					}
				} else if (i2->_tail == seg._head) {
					if (closestC < 1.0f) {
						seg._head = i2->_head;
					} else {
						i2->_tail = seg._tail;
					}
				} else if (i2->_tail == seg._tail) {
					if (closestC > 0.0f) {
						seg._tail = i2->_head;
					} else {
						i2->_tail = seg._head;
					}
				} else {
					// The lines are colinear, and at least one point of i2 is on i
					// We must separate these 2 segments into 3 segments.
					// Replace i2 with something that is strictly with i2, and then schedule
					// the remaining split parts for intersection tests.
					Segment newSeg;
					if (closestC < 0.0f) {
						if (closestD > 1.0f) newSeg = {seg._tail, i2->_tail};
						else { newSeg = {i2->_tail, seg._tail}; seg._tail = i2->_tail; }
						i2->_tail = seg._head;
					} else if (closestD < 0.0f) {
						if (closestC > 1.0f) newSeg = {seg._tail, i2->_head};
						else { newSeg = {i2->_head, seg._tail}; seg._tail = i2->_head; }
						i2->_head = seg._head;
					} else if (closestC < closestD) {
						if (closestD > 1.0f) newSeg = {seg._tail, i2->_tail};
						else { newSeg = {i2->_tail, seg._tail}; seg._tail = i2->_tail; }
						seg._tail = i2->_head;
					} else {
						if (closestC > 1.0f) newSeg = {seg._tail, i2->_head};
						else { newSeg = {i2->_head, seg._tail}; seg._tail = i2->_head; }
						seg._tail = i2->_tail;
					}

					assert(!DoColinearLinesIntersect(
						Truncate(result._steinerVertices[newSeg._head]),
						Truncate(result._steinerVertices[newSeg._tail]),
						Truncate(result._steinerVertices[seg._head]),
						Truncate(result._steinerVertices[seg._tail])));
					assert(!DoColinearLinesIntersect(
						Truncate(result._steinerVertices[newSeg._head]),
						Truncate(result._steinerVertices[newSeg._tail]),
						Truncate(result._steinerVertices[i2->_head]),
						Truncate(result._steinerVertices[i2->_tail])));
					assert(!DoColinearLinesIntersect(
						Truncate(result._steinerVertices[i2->_head]),
						Truncate(result._steinerVertices[i2->_tail]),
						Truncate(result._steinerVertices[seg._head]),
						Truncate(result._steinerVertices[seg._tail])));
					assert(newSeg._head != newSeg._tail);
					assert(i2->_head != i2->_tail);
					assert(seg._head != seg._tail);

					// We will continue testing "seg", and we will push "newSeg" onto the stack to
					// be tested later.
					// i2 has also been changed; it is now shorter and no longer intersects 'seg'
					segmentsToTest.push(newSeg);
				}

				// "seg" has changed, so we need to calculate the end points
				A = Truncate(result._steinerVertices[seg._head]);
				B = Truncate(result._steinerVertices[seg._tail]);
			}

			if (!filterOutSeg)
				filteredSegments.push_back(seg);
		}

		// add all of the segments in "filteredSegments" to the skeleton
		for (const auto&seg:filteredSegments) {
			assert(seg._head != seg._tail);
			AddEdge(result, seg._head, seg._tail, seg._leftFace, seg._rightFace, Skeleton::EdgeType::Wavefront);
		}

		// Also have to add the traced out path of the each vertex (but only if it doesn't already exist in the result)
		for (const auto&seg:_wavefrontEdges) {
			unsigned vs[] = {seg._head, seg._tail};
			for (auto v:vs) {
				const auto& vert = _vertices[v];
				AddEdgeForVertexPath(result, v, AddSteinerVertex(result, ClampedPositionAtTime(vert, time)));
			}
		}
	}

	void Graph::AddEdgeForVertexPath(Skeleton& dst, unsigned v, unsigned finalVertId)
	{
		const Vertex& vert = _vertices[v];
		auto inAndOut = FindInAndOut(MakeIteratorRange(_wavefrontEdges), v);
		unsigned leftFace = ~0u, rightFace = ~0u;
		if (inAndOut.first) leftFace = inAndOut.first->_rightFace;
		if (inAndOut.second) rightFace = inAndOut.second->_rightFace;
		if (vert._skeletonVertexId != ~0u) {
			if (vert._skeletonVertexId&BoundaryVertexFlag) {
				auto q = vert._skeletonVertexId&(~BoundaryVertexFlag);
				AddEdge(dst, finalVertId, vert._skeletonVertexId, unsigned((q+_boundaryPoints.size()-1) % _boundaryPoints.size()), q, Skeleton::EdgeType::VertexPath);
			}
			AddEdge(dst, finalVertId, vert._skeletonVertexId, leftFace, rightFace, Skeleton::EdgeType::VertexPath);
		} else {
			AddEdge(dst,
				finalVertId, AddSteinerVertex(dst, Expand(vert._position, vert._initialTime)),
				leftFace, rightFace, Skeleton::EdgeType::VertexPath);
		}
	}


}}
