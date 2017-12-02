// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "StraightSkeleton.h"
#include "../Math/Geometry.h"
#include <stack>

#pragma warning(disable:4505) // 'SceneEngine::StraightSkeleton::ReplaceVertex': unreferenced local function has been removed

namespace SceneEngine { namespace StraightSkeleton 
{
	static const float epsilon = 1e-6f;
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

		auto t0 = vex1-vex0;
		auto t1 = vex2-vex1;

		if (Equivalent(Float2(t0), Zero<Float2>(), epsilon)) return Zero<Float2>();
		if (Equivalent(Float2(t1), Zero<Float2>(), epsilon)) return Zero<Float2>();

		// create normal pointing in direction of movement
		auto N0 = Normalize(Float2(-t0[1], t0[0]));
		auto N1 = Normalize(Float2(-t1[1], t1[0]));
		auto a = N0[0], b = N0[1];
		auto c = N1[0], d = N1[1];
		const auto t = 1.0f;

		// Now, line1 is 0 = xa + yb - t and line2 is 0 = xc + yd - t

		// we can calculate the intersection of the lines using this formula...
		auto q1 = (b-d);
		if (q1 > -epsilon && q1 < epsilon) return Zero<Float2>();
		auto D = (c-a)/q1;

		auto q2 = (a + b*D);
		if (q2 > -epsilon && q2 < epsilon) return Zero<Float2>();
		auto x = t / q2;
		auto y = D*x;
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
			result._wavefrontEdges.emplace_back(Graph::Segment{unsigned(v), unsigned(v2), ~0u, unsigned(v)});

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
			auto  t = (p1[0] - p0[0]) / d0x;

			auto ySep = p0[1] + t * v0[1]
					   - p1[1] - t * v1[1];
			if (ySep < epsilon) return t;	// (todo -- we could refine with the y results?
		} else {
			if (std::abs(d0y) < epsilon) return std::numeric_limits<float>::max();
			auto t = (p1[1] - p0[1]) / d0y;

			auto xSep = p0[0] + t * v0[0]
					   - p1[0] - t * v1[0];
			if (xSep < epsilon) return t;	// (todo -- we could refine with the y results?
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
		// time. This can be any time, but it may be convnient to use time 0
		auto p0 = Float2(v0._position - v0._initialTime * v0._velocity);
		auto p1 = Float2(v1._position - v1._initialTime * v1._velocity);
		return CalculateCollapseTime(p0, v0._velocity, p1, v1._velocity);
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

	static Float2 PositionAtTime(const Vertex& v, float time)
	{
		auto result = v._position + v._velocity * (time - v._initialTime);
		assert(!isnan(result[0]) && !isnan(result[1]));
		assert(isfinite(result[0]) && isfinite(result[1]));
		assert(result[0] == result[0] && result[1] == result[1]);
		return result;
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
			for (unsigned q=0; q<2; ++q) {
				if (t[q] > bestCollisionEvent._time || t[q] < 0.0f) continue;	// don't need to check collisions that happen too late
				auto P0 = Float2(p0 + t[q]*v0);
				auto P1 = Float2(p1 + t[q]*v1);
				auto P2 = Float2(p2 + t[q]*v2);
				if ((Dot(P1-P0, P2-P0) > 0.0f) && (Dot(P0-P1, P2-P1) > 0.0f)) {
					// good collision
					bestCollisionEvent._time = t[q];
					bestCollisionEvent._edgeSegment = unsigned(&e - AsPointer(graph._wavefrontEdges.begin()));
				} else if (Equivalent(P0, P2, epsilon) || Equivalent(P1, P2, epsilon)) {
					// collided with vertex (or close enough)
					bestCollisionEvent._time = t[q];
					bestCollisionEvent._edgeSegment = unsigned(&e - AsPointer(graph._wavefrontEdges.begin()));
				}
			}
		}

		// Look for an intersection with other motorcycles
		for (auto i=graph._motorcycleSegments.begin(); i!=graph._motorcycleSegments.end(); ++i) {
			// we skip one cycle -- it's usually the one we're actually testing
			if (AsPointer(i) == cycleToSkip) continue;

			const auto& head = graph._vertices[i->_head];
			auto p0 = Float2(head._position - head._initialTime * head._velocity);
			auto collapseTime = CalculateCollapseTime(p0, head._velocity, p2, v2);
			if (collapseTime < bestCollisionEvent._time) {
				bestCollisionEvent._time = collapseTime;
				bestCollisionEvent._edgeSegment = ~0u;
			}
		}

		// Look for an intersection with boundary edges
		// These are easier, because the boundaries are static. We just need to see if the motorcycle is going
		// to hit them, and when
		auto prevPt = graph._boundaryPoints.size()-1;
		for (size_t c=0; c<graph._boundaryPoints.size(); ++c) {
			if ((unsigned(c)|BoundaryVertexFlag) == boundaryPtToSkip || (unsigned(prevPt)|BoundaryVertexFlag) == boundaryPtToSkip) { prevPt = c; continue; }
			
			auto p0 = graph._boundaryPoints[prevPt];
			auto p1 = graph._boundaryPoints[c];
			prevPt = c;	// (update before all of the continues below)

			// If the motorcycle velocity is unit length (which it is for unweighted straight skeleton calculation),
			// we can make this simple
			auto crs = Float2(-v2[1], v2[0]);
			auto d0 = Dot(p0-p2, crs);
			auto d1 = Dot(p1-p2, crs);
			auto a = d0 / (d0 - d1);
			if (a < -epsilon || a > (1.0f + epsilon)) continue;

			float t = (std::abs(v2[0]) > std::abs(v2[1])) ? (LinearInterpolate(p0[0], p1[0], a) - p2[0]) / v2[0] : (LinearInterpolate(p0[1], p1[1], a) - p2[1]) / v2[1];
			if (t < 0.f || t > bestCollisionEvent._time) continue;
			bestCollisionEvent._time = t;
			bestCollisionEvent._edgeSegment = ~0u;
		}

		// todo -- also check segmnents written into the output skeleton...?

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
		v._position = PositionAtTime(v, atTime);
		v._initialTime = atTime;
		v._skeletonVertexId = ~0u;
		v._velocity = Zero<Float2>();
	}

	Skeleton Graph::GenerateSkeleton(float maxTime)
	{
		Skeleton result;
		std::vector<std::pair<float, size_t>> bestCollapse;
		std::vector<std::pair<CrashEvent, size_t>> bestMotorcycleCrash;
		bestCollapse.reserve(8);

		float lastEventTime = 0.0f;

		for (;;) {
			// Find the next event to occur
			//		-- either a edge collapse or a motorcycle collision
			float bestCollapseTime = std::numeric_limits<float>::max();
			bestCollapse.clear();
			for (auto e=_wavefrontEdges.begin(); e!=_wavefrontEdges.end(); ++e) {
				const auto& v0 = _vertices[e->_head];
				const auto& v1 = _vertices[e->_tail];
				auto collapseTime = CalculateCollapseTime(v0, v1);
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
				auto crashEvent = CalculateCrashTime(*this, head._position, head._velocity, head._initialTime, AsPointer(m), head._skeletonVertexId);

				// If our best motorcycle collision happens before our best collapse, then we
				// must do the motorcycle first, and recalculate edge collapses afterwards
				// But if they happen at the same time, we should do the edge collapse first,
				// and then recalculate the motorcycle collisions afterwards (ie, even if there's
				// a motorcycle collision at around the same time as the edge collapses, we're
				// going to ignore it for now)
				/*if (crashEvent._time < (bestCollapseTime - epsilon))*/ {
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
				auto crashPtSkeleton = AddSteinerVertex(result, Float3(crashPt, bestCollapseTime), epsilon);

				auto crashSegment = _wavefrontEdges[crashEvent._edgeSegment];
				_wavefrontEdges.erase(_wavefrontEdges.begin() + crashEvent._edgeSegment);
				Segment newSegment0{ motor._head, motor._head, motor._leftFace, crashSegment._rightFace};
				Segment newSegment1{ motor._head, motor._head, crashSegment._rightFace, motor._rightFace };
				auto calcTime = crashEvent._time;

				// is there volume on the "tout" side?
				{
					auto* tout = FindInAndOut(MakeIteratorRange(_wavefrontEdges), motor._head).second;
					assert(tout);

					auto v0 = PositionAtTime(_vertices[crashSegment._tail], calcTime);
					auto v2 = PositionAtTime(_vertices[tout->_head], calcTime);
					if (tout->_head == crashSegment._tail || Equivalent(v0, v2, epsilon)) {
						// no longer need crashSegment or tout
						result._unplacedEdges.push_back({
							AddSteinerVertex(result, Expand((v0+v2)/2.0f, calcTime), epsilon), 
							crashPtSkeleton});
						// todo -- there may be a chain of collapsing that occurs now... we should follow it along...
						// We still need to add a wavefront edge to close and the loop, and ensure we don't leave
						// stranded edges. Without this we can easily get a single edge without anything looping
						// it back around (or just an unclosed loop)
						if (tout->_head != crashSegment._tail) {
							_wavefrontEdges.push_back({tout->_head, crashSegment._tail, ~0u, ~0u});
						}
						_wavefrontEdges.erase(_wavefrontEdges.begin()+(tout-AsPointer(_wavefrontEdges.begin())));
					} else {
						auto newVertex = (unsigned)_vertices.size();
						tout->_tail = newVertex;
						_wavefrontEdges.push_back({newVertex, crashSegment._tail, tout->_leftFace, crashSegment._rightFace});	// (hin)

						_vertices.push_back(Vertex{crashPt, crashPtSkeleton, crashEvent._time, CalculateVertexVelocity(v0, crashPt, v2)});
						newSegment1._head = newVertex;
					}
				}

				// is there volume on the "tin" side?
				{
					auto* tin = FindInAndOut(MakeIteratorRange(_wavefrontEdges), motor._head).first;
					assert(tin);

					auto v0 = PositionAtTime(_vertices[tin->_tail], calcTime);
					auto v2 = PositionAtTime(_vertices[crashSegment._head], calcTime);
					if (tin->_tail == crashSegment._head || Equivalent(v0, v2, epsilon)) {
						// no longer need "crashSegment" or tin
						result._unplacedEdges.push_back({
							AddSteinerVertex(result, Expand((v0+v2)/2.0f, calcTime), epsilon), 
							crashPtSkeleton});
						// todo -- there may be a chain of collapsing that occurs now... we should follow it along...
						// We still need to add a wavefront edge to close and the loop, and ensure we don't leave
						// stranded edges. Without this we can easily get a single edge without anything looping
						// it back around (or just an unclosed loop)
						if (tin->_tail != crashSegment._head) {
							_wavefrontEdges.push_back({crashSegment._head, tin->_tail, ~0u, ~0u});
						}
						_wavefrontEdges.erase(_wavefrontEdges.begin()+(tin-AsPointer(_wavefrontEdges.begin())));
					} else {
						auto newVertex = (unsigned)_vertices.size();
						tin->_head = newVertex;
						_wavefrontEdges.push_back({crashSegment._head, newVertex, tin->_leftFace, crashSegment._rightFace});	// (hout)

						_vertices.push_back(Vertex{crashPt, crashPtSkeleton, crashEvent._time, CalculateVertexVelocity(v0, crashPt, v2)});
						newSegment0._head = newVertex;
					}
				}

				// if (newSegment0._head != newSegment0._tail) _wavefrontEdges.push_back(newSegment0);
				// if (newSegment1._head != newSegment1._tail) _wavefrontEdges.push_back(newSegment1);

				// add skeleton edge from the  
				assert(_vertices[motor._tail]._skeletonVertexId != ~0u);
				result._unplacedEdges.push_back({_vertices[motor._tail]._skeletonVertexId, crashPtSkeleton});
				FreezeInPlace(_vertices[motor._head], crashEvent._time);

				_motorcycleSegments.erase(_motorcycleSegments.begin() + bestMotorcycleCrash[0].second);

				lastEventTime = crashEvent._time;
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
						collisionPt += _vertices[seg._head]._position + (bestCollapseTime - _vertices[seg._head]._initialTime) * _vertices[seg._head]._velocity;
						collisionPt += _vertices[seg._tail]._position + (bestCollapseTime - _vertices[seg._tail]._initialTime) * _vertices[seg._tail]._velocity;
						contributors += 2;

						// at this point they should not be frozen (but they will all be frozen later)
						assert(!IsFrozen(_vertices[seg._tail]));
						assert(!IsFrozen(_vertices[seg._head]));
					}
					collisionPt /= float(contributors);

					// add a steiner vertex into the output
					auto collisionVertId = AddSteinerVertex(result, Float3(collisionPt, bestCollapseTime), epsilon);

					// connect up edges in the output graph
					// Note that since we're connecting both head and tail, we'll end up doubling up each edge
					for (size_t c=0; c<bestCollapse.size(); ++c) {
						if (collapseGroups[c] != collapseGroup) continue;
						const auto& seg = _wavefrontEdges[bestCollapse[c].second];
						unsigned vs[] = { seg._head, seg._tail };
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
					
						FreezeInPlace(_vertices[seg._tail], bestCollapseTime);
						FreezeInPlace(_vertices[seg._head], bestCollapseTime);
					}

					// create a new vertex in the graph to connect the edges to either side of the collapse
					auto newVertex = (unsigned)_vertices.size();
					_vertices.push_back(Vertex{collisionPt, collisionVertId, bestCollapseTime, Float2(0.0f,0.0f)});
					collapseGroupInfos[collapseGroup]._newVertex = newVertex;
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

					auto tail = std::find_if(_wavefrontEdges.begin(), _wavefrontEdges.end(), 
						[&group](const Segment&seg) { return seg._head == group._tail;});
					assert(tail != _wavefrontEdges.end());
					assert(std::find_if(tail+1, _wavefrontEdges.end(), [&group](const Segment&seg) { return seg._head == group._tail;}) == _wavefrontEdges.end());

					auto head = std::find_if(_wavefrontEdges.begin(), _wavefrontEdges.end(), 
						[&group](const Segment&seg) { return seg._tail == group._head;});
					assert(head != _wavefrontEdges.end());
					assert(std::find_if(head+1, _wavefrontEdges.end(), [&group](const Segment&seg) { return seg._tail == group._head;}) == _wavefrontEdges.end());

					tail->_head = group._newVertex;
					head->_tail = group._newVertex;
					auto calcTime = _vertices[group._newVertex]._initialTime;
					auto v0 = PositionAtTime(_vertices[tail->_tail], calcTime);
					auto v1 = _vertices[group._newVertex]._position;
					auto v2 = PositionAtTime(_vertices[head->_head], calcTime);
					_vertices[group._newVertex]._velocity = CalculateVertexVelocity(v0, v1, v2);

					if (tail->_tail == head->_head)
						_vertices[tail->_tail]._velocity = Zero<Float2>();
				}
			
				lastEventTime = bestCollapseTime;
			}

#if 0
			// if we've created any overlapping edges (ie, if the same 2 vertices are
			// connected by two separate edges, in different directions), we can remove
			// them an just write a line into the output result
			auto r = _wavefrontEdges.end();
			for (auto i=_wavefrontEdges.begin(); i!=r;) {
				auto dupe = std::find_if(i+1, r,
					[i](const Segment&e) { return (e._head == i->_tail) && (e._tail == i->_head);});
				if (dupe != r) {
					result._unplacedEdges.push_back({
						AddSteinerVertex(result, Expand(PositionAtTime(_vertices[i->_head], lastEventTime), lastEventTime), epsilon), 
						AddSteinerVertex(result, Expand(PositionAtTime(_vertices[i->_tail], lastEventTime), lastEventTime), epsilon)});
					--r; std::swap(*r, *i);
					--r; std::swap(*r, *dupe);
				} else {
					++i;
				}
			}
			_wavefrontEdges.erase(r, _wavefrontEdges.end());
#endif
		}

		WriteWavefront(result, lastEventTime);

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
			auto A = PositionAtTime(_vertices[i->_head], time);
			auto B = PositionAtTime(_vertices[i->_tail], time);
			auto v0 = AddSteinerVertex(result, Expand(A, time), epsilon);
			auto v1 = AddSteinerVertex(result, Expand(B, time), epsilon);
			if (v0 != v1)
				segmentsToTest.push(Segment{v0, v1});
		}

		while (!segmentsToTest.empty()) {
			auto seg = segmentsToTest.top();
			segmentsToTest.pop();

			auto A = Truncate(result._steinerVertices[seg._head]);
			auto B = Truncate(result._steinerVertices[seg._tail]);
			bool filterOutSeg = false;

			// Compare against all edges already in "filteredSegments"
			for (auto i2=filteredSegments.begin(); i2!=filteredSegments.end();++i2) {

				if (	(i2->_head == seg._head && i2->_tail == seg._tail)
					||	(i2->_head == seg._tail && i2->_tail == seg._head)) {
					filterOutSeg = true; break; // (overlap completely)
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
			result._unplacedEdges.push_back({seg._head, seg._tail});
		}

		// Also have to add the traced out path of the each vertex (but only if it doesn't already exist in the result)
		for (const auto&seg:_wavefrontEdges) {
			unsigned vs[] = {seg._head, seg._tail};
			for (auto v:vs) {
				const auto& vert = _vertices[v];
				std::pair<unsigned, unsigned> s;
				s.first = vert._skeletonVertexId ? vert._skeletonVertexId : AddSteinerVertex(result, Expand(vert._position, vert._initialTime), epsilon);
				s.second = AddSteinerVertex(result, Expand(PositionAtTime(vert, time), time), epsilon); // todo -- this must be freeze time!
				auto existing = std::find(result._unplacedEdges.begin(), result._unplacedEdges.begin(), s);
				if (existing == result._unplacedEdges.end())
					result._unplacedEdges.push_back(s);
			}
		}
	}

}}
