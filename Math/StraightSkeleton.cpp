// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "StraightSkeleton.h"
#include "../Math/Geometry.h"
#include <stack>
#include <cmath>
#include <random>	// (for validation testing)

#if defined(_DEBUG)
	#define EXTRA_VALIDATION
#endif

#pragma warning(disable:4505) // 'SceneEngine::StraightSkeleton::ReplaceVertex': unreferenced local function has been removed

// We can define the handiness of 2D space as such:
// If we wanted to rotate the X axis so that it lies on the Y axis, 
// which is the shortest direction to rotate in? Is it clockwise, or counterclockwise?
// "SPACE_HANDINESS_COUNTERCLOCKWISE" corresponds to a space in which +Y points up the page, and +X to the right
// "SPACE_HANDINESS_CLOCKWISE" corresponds to a space in which +Y points down the page, and +X to the right
#define SPACE_HANDINESS_CLOCKWISE 1
#define SPACE_HANDINESS_COUNTERCLOCKWISE 2
#define SPACE_HANDINESS SPACE_HANDINESS_COUNTERCLOCKWISE 

namespace XLEMath
{
	// static const float epsilon = 1e-4f;
	T1(Primitive) static constexpr Primitive GetEpsilon();
	template<> static constexpr float GetEpsilon<float>() { return 1e-4f; }
	template<> static constexpr double GetEpsilon<double>() { return 1e-8; }
	template<> static constexpr int GetEpsilon<int>() { return 1; }
	template<> static constexpr int64_t GetEpsilon<int64_t>() { return 1ll; }
	static const unsigned BoundaryVertexFlag = 1u<<31u;

	template<> inline const Vector2T<int64_t>& Zero<Vector2T<int64_t>>()
    {
        static Vector2T<int64_t> result(0ll, 0ll);
        return result;
    }

	template<> inline const Vector3T<int64_t>& Zero<Vector3T<int64_t>>()
    {
        static Vector3T<int64_t> result(0ll, 0ll, 0ll);
        return result;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////

	enum WindingType { Left, Right, Straight };
	T1(Primitive) WindingType CalculateWindingType(Vector2T<Primitive> zero, Vector2T<Primitive> one, Vector2T<Primitive> two, Primitive threshold)
	{
		auto sign = (one[0] - zero[0]) * (two[1] - zero[1]) - (two[0] - zero[0]) * (one[1] - zero[1]);
		#if SPACE_HANDINESS == SPACE_HANDINESS_CLOCKWISE
			if (sign > threshold) return Right;
			if (sign < -threshold) return Left;
		#else
			if (sign > threshold) return Left;
			if (sign < -threshold) return Right;
		#endif
		return Straight;
	}

	T1(Primitive) Vector2T<Primitive> EdgeTangentToMovementDir(Vector2T<Primitive> tangent)
	{
		#if SPACE_HANDINESS == SPACE_HANDINESS_CLOCKWISE
			return Vector2T<Primitive>(tangent[1], -tangent[0]);
		#else
			return Vector2T<Primitive>(-tangent[1], tangent[0]);
		#endif
	}

	T1(Primitive) Vector2T<Primitive> CalculateVertexVelocity_FirstMethod(Vector2T<Primitive> vex0, Vector2T<Primitive> vex1, Vector2T<Primitive> vex2)
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

		if (Equivalent(vex0, vex2, GetEpsilon<Primitive>())) return Zero<Vector2T<Primitive>>();

		auto t0 = Vector2T<Primitive>(vex1-vex0);
		auto t1 = Vector2T<Primitive>(vex2-vex1);

		if (Equivalent(t0, Zero<Vector2T<Primitive>>(), GetEpsilon<Primitive>())) return Zero<Vector2T<Primitive>>();
		if (Equivalent(t1, Zero<Vector2T<Primitive>>(), GetEpsilon<Primitive>())) return Zero<Vector2T<Primitive>>();

		// create normal pointing in direction of movement
		auto N0 = Normalize(EdgeTangentToMovementDir(t0));
		auto N1 = Normalize(EdgeTangentToMovementDir(t1));
		auto a = N0[0], b = N0[1];
		auto c = N1[0], d = N1[1];
		const auto t = Primitive(1);		// time = 1.0f, because we're calculating the velocity

		// Now, line1 is 0 = xa + yb - t and line2 is 0 = xc + yd - t

		// we can calculate the intersection of the lines using this formula...
		auto B0 = Primitive(0), B1 = Primitive(0);
		if (d<-GetEpsilon<Primitive>() || d>GetEpsilon<Primitive>()) B0 = a - b*c/d;
		if (c<-GetEpsilon<Primitive>() || c>GetEpsilon<Primitive>()) B1 = b - a*d/c;

		Primitive x, y;
		if (std::abs(B0) > std::abs(B1)) {
			if (B0 > -GetEpsilon<Primitive>() && B0 < GetEpsilon<Primitive>()) return Zero<Vector2T<Primitive>>();
			auto A = Primitive(1) - b/d;
			x = t * A / B0;
			y = (t - x*c) / d;
		} else {
			if (B1 > -GetEpsilon<Primitive>() && B1 < GetEpsilon<Primitive>()) return Zero<Vector2T<Primitive>>();
			auto A = Primitive(1) - a/c;
			y = t * A / B1;
			x = (t - y*d) / c;
		}

		assert(Dot(Vector2T<Primitive>(x, y), N0+N1) > Primitive(0));

		assert(IsFiniteNumber(x) && IsFiniteNumber(y));
		return Vector2T<Primitive>(x, y);
	}

	T1(Primitive) auto SetMagnitude(Vector2T<Primitive> input, Primitive mag)
		-> typename std::enable_if<!std::is_integral<Primitive>::value, Vector2T<Primitive>>::type
	{
		auto scale = std::hypot(input[0], input[1]);		// (note "scale" becomes promoted to double)
		using Promoted = decltype(scale);
		Vector2T<Primitive> result;
		for (unsigned c=0; c<2; ++c)
			result[c] = input[c] * mag / scale;
		return result;
	}
	
	T1(Primitive) auto SetMagnitude(Vector2T<Primitive> input, Primitive mag)
		-> typename std::enable_if<std::is_integral<Primitive>::value, Vector2T<Primitive>>::type
	{
		auto scale = std::hypot(input[0], input[1]);		// (note "scale" becomes promoted to double)
		using Promoted = decltype(scale);
		Vector2T<Primitive> result;
		for (unsigned c=0; c<2; ++c)
			result[c] = (Primitive)std::round(Promoted(input[c]) * Promoted(mag) / scale);
		return result;
	}

	T1(Primitive) struct PromoteIntegral { using Value = Primitive; };
	template<> struct PromoteIntegral<int> { using Value = int64_t; };
	template<> struct PromoteIntegral<int64_t> { using Value = double; };

	T1(Primitive) Vector2T<Primitive> LineIntersection(
		std::pair<Vector2T<Primitive>, Vector2T<Primitive>> zero,
		std::pair<Vector2T<Primitive>, Vector2T<Primitive>> one)
	{
		// Look for an intersection between infinite lines "zero" and "one".
		// Only parallel lines won't collide.
		// Try to do this so that it's still precise with integer coords

		// We can define the line A->B as: (here sign of result is arbitrary)
		//		x(By-Ay) + y(Ax-Bx) + AyBx - AxBy = 0
		//
		// If we also have line C->D
		//		x(Dy-Cy) + y(Cx-Dx) + CyDx - CxDy = 0
		//
		// Let's simplify:
		//	xu + yv + i = 0
		//  xs + yt + j = 0
		//
		// Solving for simultaneous equations.... If tu != sv, then:
		// x = (it - jv) / (sv - tu)
		// y = (ju - is) / (sv - tu)
		
		// For some primitive types we should promote to higher precision
		//			types here (eg, we will get int32_t overflows if we don't promote here)

		using WorkingPrim = PromoteIntegral<Primitive>::Value;
		auto Ax = (WorkingPrim)zero.first[0], Ay = (WorkingPrim)zero.first[1];
		auto Bx = (WorkingPrim)zero.second[0], By = (WorkingPrim)zero.second[1];
		auto Cx = (WorkingPrim)one.first[0], Cy = (WorkingPrim)one.first[1];
		auto Dx = (WorkingPrim)one.second[0], Dy = (WorkingPrim)one.second[1];
		
		auto u = By-Ay, v = Ax-Bx, i = Ay*Bx-Ax*By;
		auto s = Dy-Cy, t = Cx-Dx, j = Cy*Dx-Cx*Dy;

		auto d = s*v - t*u;
		if (!d) return Vector2T<Primitive>(std::numeric_limits<Primitive>::max(), std::numeric_limits<Primitive>::max());
		return { Primitive((i*t - j*v) / d), Primitive((j*u - i*s) / d) };
		// return { Primitive(i*t/d - j*v/d), Primitive(j*u/d - i*s/d) };
	}

	T1(Primitive) Vector2T<Primitive> CalculateVertexVelocity_LineIntersection(Vector2T<Primitive> vex0, Vector2T<Primitive> vex1, Vector2T<Primitive> vex2, Primitive movementTime)
	{
		// For integers, let's simplify the math to try to get the high precision result.
		// We'll simply calculate the two edges at 2 points in time, and find the intersection
		// points at both times (actually vex1 is already an intersection point). Since the intersection always moves in a straight path, we
		// can just use the difference between those intersections to calculate the velocity

		if (Equivalent(vex0, vex2, GetEpsilon<Primitive>())) return Zero<Vector2T<Primitive>>();

		auto t0 = Vector2T<Primitive>(vex1-vex0);
		auto t1 = Vector2T<Primitive>(vex2-vex1);

		if (Equivalent(t0, Zero<Vector2T<Primitive>>(), GetEpsilon<Primitive>())) return Zero<Vector2T<Primitive>>();
		if (Equivalent(t1, Zero<Vector2T<Primitive>>(), GetEpsilon<Primitive>())) return Zero<Vector2T<Primitive>>();

		auto N0 = SetMagnitude(EdgeTangentToMovementDir(t0), movementTime);
		auto N1 = SetMagnitude(EdgeTangentToMovementDir(t1), movementTime);
		if (Equivalent(N0, N1, GetEpsilon<Primitive>()) || Equivalent(N0, Vector2T<Primitive>(-N1), GetEpsilon<Primitive>())) return Zero<Vector2T<Primitive>>();

		auto A = vex0 - vex1 + N0;
		auto B = N0;
		auto C = N1;
		auto D = vex2 - vex1 + N1;

		// where do A->B and C->D intersect?
		auto intersection = LineIntersection<Primitive>({A, B}, {C, D});
		if (intersection == Vector2T<Primitive>(std::numeric_limits<Primitive>::max(), std::numeric_limits<Primitive>::max()))
			return Zero<Vector2T<Primitive>>();

		// Now, vex1->intersection is the distance travelled in "calcTime"
		return intersection;
	}

	T1(Primitive) auto CalculateVertexVelocity(Vector2T<Primitive> vex0, Vector2T<Primitive> vex1, Vector2T<Primitive> vex2)
		-> typename std::enable_if<!std::is_integral<Primitive>::value, Vector2T<Primitive>>::type
	{
		Vector2T<Primitive> firstMethod = CalculateVertexVelocity_FirstMethod(vex0, vex1, vex2);
		Vector2T<Primitive> lineIntersection = CalculateVertexVelocity_LineIntersection(vex0, vex1, vex2, Primitive(1));
		(void)firstMethod;
		// assert(Equivalent(firstMethod, lineIntersection, GetEpsilon<Primitive>()));
		assert(!Equivalent(lineIntersection, Zero<Vector2T<Primitive>>(), GetEpsilon<Primitive>()));
		return lineIntersection;
	}

	static const int static_velocityVectorScale = INT32_MAX; // 0x7fff;

	T1(Primitive) auto CalculateVertexVelocity(Vector2T<Primitive> vex0, Vector2T<Primitive> vex1, Vector2T<Primitive> vex2)
		-> typename std::enable_if<std::is_integral<Primitive>::value, Vector2T<Primitive>>::type
	{
		// "CalculateVertexVelocity_FirstMethod" is not accurate when using integer & fixed point.
		// We need to use the line intersection method. This also allow us to scale up the length of the 
		// velocity vector so we represent it using integers.
		return CalculateVertexVelocity_LineIntersection(vex0, vex1, vex2, Primitive(static_velocityVectorScale));
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////

	T1(Primitive) class Vertex
	{
	public:
		Vector2T<Primitive>	_position;
		Vector2T<Primitive>	_velocity;
		Primitive			_initialTime;
		unsigned			_skeletonVertexId;
	};

	class WavefrontEdge
	{
	public:
		unsigned	_head, _tail;
		unsigned	_leftFace, _rightFace;
	};

	class MotorcycleSegment
	{
	public:
		unsigned _head;
		unsigned _tail;		// (this is the fixed vertex)
		unsigned _leftFace, _rightFace;
	};

	T1(T) auto IsFiniteNumber(T value) -> typename std::enable_if<std::is_floating_point<T>::value, bool>::type
	{
		auto type = std::fpclassify(value);
		return ((type == FP_NORMAL) || (type == FP_SUBNORMAL) || (type == FP_ZERO)) && (value == value);
	}

	T1(T) auto IsFiniteNumber(T) -> typename std::enable_if<!std::is_floating_point<T>::value, bool>::type { return true; }

	T1(Primitive) auto PositionAtTime(const Vertex<Primitive>& v, Primitive time)
		-> typename std::enable_if<!std::is_integral<Primitive>::value, Vector2T<Primitive>>::type
	{
		auto result = v._position + v._velocity * (time - v._initialTime);
		assert(IsFiniteNumber(result[0]) && IsFiniteNumber(result[1]));
		return result;
	}

	T1(Primitive) auto PositionAtTime(const Vertex<Primitive>& v, Primitive time)
		-> typename std::enable_if<std::is_integral<Primitive>::value, Vector2T<Primitive>>::type
	{
		return v._position + v._velocity * (time - v._initialTime) / static_velocityVectorScale;
	}

	T1(Primitive) static Vector3T<Primitive> ClampedPositionAtTime(const Vertex<Primitive>& v, Primitive time)
	{
		if (Equivalent(v._velocity, Zero<Vector2T<Primitive>>(), GetEpsilon<Primitive>()))
			return Expand(v._position, v._initialTime);
		return Expand(PositionAtTime(v, time), time);
	}

	T1(Primitive) static Primitive CalculateCollapseTime_FirstMethod(Vector2T<Primitive> p0, Vector2T<Primitive> v0, Vector2T<Primitive> p1, Vector2T<Primitive> v1)
	{
		auto d0x = v0[0] - v1[0];
		auto d0y = v0[1] - v1[1];
		if (std::abs(d0x) > std::abs(d0y)) {
			if (std::abs(d0x) < GetEpsilon<Primitive>()) return std::numeric_limits<Primitive>::max();
			auto t = (p1[0] - p0[0]) / d0x;

			auto ySep = p0[1] + t * v0[1] - p1[1] - t * v1[1];
			if (t > 0.f && std::abs(ySep) < (10 * GetEpsilon<Primitive>())) {
				// assert(std::abs(p0[0] + t * v0[0] - p1[0] - t * v1[0]) < GetEpsilon<Primitive>());
				return t;	// (todo -- we could refine with the x results?
			}
		} else {
			if (std::abs(d0y) < GetEpsilon<Primitive>()) return std::numeric_limits<Primitive>::max();
			auto t = (p1[1] - p0[1]) / d0y;

			auto xSep = p0[0] + t * v0[0] - p1[0] - t * v1[0];
			if (t > 0.0f && std::abs(xSep) < (10 * GetEpsilon<Primitive>())) {
				// sassert(std::abs(p0[1] + t * v0[1] - p1[1] - t * v1[1]) < GetEpsilon<Primitive>());
				return t;	// (todo -- we could refine with the y results?
			}
		}

		return std::numeric_limits<Primitive>::max();
	}

	T1(Primitive) struct VelocityVectorScale { static const Primitive Value; };
	template<> struct VelocityVectorScale<int> { static const int Value = static_velocityVectorScale; };
	template<> struct VelocityVectorScale<int64_t> { static const int64_t Value = static_velocityVectorScale; };
	T1(Primitive) const Primitive VelocityVectorScale<Primitive>::Value = Primitive(1);

	T1(Primitive) static Primitive CalculateCollapseTime_LineIntersection(Vector2T<Primitive> p0, Vector2T<Primitive> v0, Vector2T<Primitive> p1, Vector2T<Primitive> v1)
	{
		// Attempt to find the collapse time for these 2 vertices
		// Since we're doing this with integer coordinates, we should try to pick a method that will work well at limited
		// precision
		// There another way to do this... Effectively we want to find where 3 moving edges intersect in time.
		// We can do that algebraically
		auto intr = LineIntersection<Primitive>({Zero<Vector2T<Primitive>>(), v0}, {p1-p0, p1-p0+v1});
		using PromotedType = Primitive; // typename PromoteIntegral<Primitive>::Value;
		auto t0 = std::numeric_limits<PromotedType>::max();
		auto scale = VelocityVectorScale<Primitive>::Value;
		if (std::abs(v0[0]) > std::abs(v0[1]))	t0 = PromotedType(intr[0]) * PromotedType(scale) / PromotedType(v0[0]);
		else									t0 = PromotedType(intr[1]) * PromotedType(scale) / PromotedType(v0[1]);

		auto t1 = std::numeric_limits<PromotedType>::max();
		if (std::abs(v1[0]) > std::abs(v1[1]))	t1 = PromotedType(intr[0] - p1[0] + p0[0]) * PromotedType(scale) / PromotedType(v1[0]);
		else									t1 = PromotedType(intr[1] - p1[1] + p0[1]) * PromotedType(scale) / PromotedType(v1[1]);

		if (std::abs(t0 - t1) < (50 * GetEpsilon<Primitive>())) {
			auto result = Primitive(t0+t1)/Primitive(2);
			auto test0 = Vector2T<Primitive>(	Primitive(PromotedType(p0[0]) + PromotedType(v0[0]) * PromotedType(result) / PromotedType(scale)),
												Primitive(PromotedType(p0[1]) + PromotedType(v0[1]) * PromotedType(result) / PromotedType(scale)));
			auto test1 = Vector2T<Primitive>(	Primitive(PromotedType(p1[0]) + PromotedType(v1[0]) * PromotedType(result) / PromotedType(scale)),
												Primitive(PromotedType(p1[1]) + PromotedType(v1[1]) * PromotedType(result) / PromotedType(scale)));
			assert(Equivalent(test0, Vector2T<Primitive>(intr + p0), 50 * GetEpsilon<Primitive>()));
			assert(Equivalent(test1, Vector2T<Primitive>(intr + p0), 50 * GetEpsilon<Primitive>()));
			(void)test0; (void)test1;
			return result;
		}
		return std::numeric_limits<Primitive>::max();
	}

	T1(Primitive) auto CalculateCollapseTime(Vector2T<Primitive> p0, Vector2T<Primitive> v0, Vector2T<Primitive> p1, Vector2T<Primitive> v1)
		-> typename std::enable_if<!std::is_integral<Primitive>::value, Primitive>::type
	{
		auto firstMethod = CalculateCollapseTime_FirstMethod(p0, v0, p1, v1);
		assert(firstMethod > 0.f);
		return firstMethod;
		/*auto lineIntersection = CalculateCollapseTime_LineIntersection(p0, v0, p1, v1);
		(void)firstMethod;
		assert(Equivalent(firstMethod, lineIntersection, GetEpsilon<Primitive>()));
		return lineIntersection;*/
	}

	T1(Primitive) auto CalculateCollapseTime(Vector2T<Primitive> p0, Vector2T<Primitive> v0, Vector2T<Primitive> p1, Vector2T<Primitive> v1)
		-> typename std::enable_if<std::is_integral<Primitive>::value, Primitive>::type
	{
		return CalculateCollapseTime_LineIntersection(p0, v0, p1, v1);
	}

	T1(Primitive) static Primitive CalculateCollapseTime(const Vertex<Primitive>& v0, const Vertex<Primitive>& v1)
	{
		// hack -- if one side is frozen, we must collapse immediately
		if (Equivalent(v0._velocity, Zero<Vector2T<Primitive>>(), GetEpsilon<Primitive>())) return std::numeric_limits<Primitive>::max();
		if (Equivalent(v1._velocity, Zero<Vector2T<Primitive>>(), GetEpsilon<Primitive>())) return std::numeric_limits<Primitive>::max();

		// At some point the trajectories of v0 & v1 may intersect
		// We need to pick out a specific time on the timeline, and find both v0 and v1 at that
		// time. 
		auto calcTime = std::min(v0._initialTime, v1._initialTime);
		auto p0 = PositionAtTime(v0, calcTime);
		auto p1 = PositionAtTime(v1, calcTime);
		return calcTime + CalculateCollapseTime(p0, v0._velocity, p1, v1._velocity);
	}

	T1(Primitive) static Vector3T<Primitive> CalculateTriangleCollapse(Vector2T<Primitive> p0, Vector2T<Primitive> p1, Vector2T<Primitive> p2)
	{
		Matrix3x3T<Primitive> M;
		Vector3T<Primitive> res;
		Vector2T<Primitive> As[] = { p0, p1, p2 };
		Vector2T<Primitive> Bs[] = { p1, p2, p0 };
		for (unsigned c=0; c<3; ++c) {
			auto mag = (Primitive)std::hypot(Bs[c][0] - As[c][0], Bs[c][1] - As[c][1]);
			auto Nx = Primitive((As[c][1] - Bs[c][1]) * VelocityVectorScale<Primitive>::Value / mag);
			auto Ny = Primitive((Bs[c][0] - As[c][0]) * VelocityVectorScale<Primitive>::Value / mag);
			M(c, 0) = Nx;
			M(c, 1) = Ny;
			M(c, 2) = -Nx*Nx-Ny*Ny;
			res[c]  = As[c][0] * Nx + As[c][1] * Ny;
		}
		return cml::inverse(M) * res;
	}

	T1(Primitive) static Vector3T<Primitive> CalculateTriangleCollapse_Offset(Vector2T<Primitive> p0, Vector2T<Primitive> p1, Vector2T<Primitive> p2)
	{
		Matrix3x3T<Primitive> M;
		Vector3T<Primitive> res;
		Vector2T<Primitive> As[] = { Zero<Vector2T<Primitive>>(), p1 - p0, p2 - p0 };
		Vector2T<Primitive> Bs[] = { p1 - p0, p2 - p0, Zero<Vector2T<Primitive>>() };
		for (unsigned c=0; c<3; ++c) {
			auto mag = (Primitive)std::hypot(Bs[c][0] - As[c][0], Bs[c][1] - As[c][1]);
			auto Nx = Primitive((As[c][1] - Bs[c][1]) * VelocityVectorScale<Primitive>::Value / mag);
			auto Ny = Primitive((Bs[c][0] - As[c][0]) * VelocityVectorScale<Primitive>::Value / mag);
			M(c, 0) = Nx;
			M(c, 1) = Ny;
			M(c, 2) = -Nx*Nx-Ny*Ny;
			res[c]  = As[c][0] * Nx + As[c][1] * Ny;
		}
		auto result = cml::inverse(M) * res;
		result[0] += p0[0];
		result[1] += p0[1];
		return result;
	}

	T1(Primitive) static bool InvertInplaceSafe(Matrix3x3T<Primitive>& M, Primitive threshold)
	{
		// note -- derived from "inverse.h" in CML.
		// This version will return false if the determinant of the matrix is zero (which means
		// there is no inverse)

        /* Compute cofactors for each entry: */
        auto m_00 = M(1,1)*M(2,2) - M(1,2)*M(2,1);
        auto m_01 = M(1,2)*M(2,0) - M(1,0)*M(2,2);
        auto m_02 = M(1,0)*M(2,1) - M(1,1)*M(2,0);

        auto m_10 = M(0,2)*M(2,1) - M(0,1)*M(2,2);
        auto m_11 = M(0,0)*M(2,2) - M(0,2)*M(2,0);
        auto m_12 = M(0,1)*M(2,0) - M(0,0)*M(2,1);

        auto m_20 = M(0,1)*M(1,2) - M(0,2)*M(1,1);
        auto m_21 = M(0,2)*M(1,0) - M(0,0)*M(1,2);
        auto m_22 = M(0,0)*M(1,1) - M(0,1)*M(1,0);

        /* Compute determinant from the minors: */
        auto Ddenom = (M(0,0)*m_00 + M(0,1)*m_01 + M(0,2)*m_02);
		if (Equivalent(Ddenom, Primitive(0), threshold))
			return false;

		assert(IsFiniteNumber(Ddenom));
        /* Assign the inverse as (1/D) * (cofactor matrix)^T: */
        M(0,0) = m_00/Ddenom;  M(0,1) = m_10/Ddenom;  M(0,2) = m_20/Ddenom;
        M(1,0) = m_01/Ddenom;  M(1,1) = m_11/Ddenom;  M(1,2) = m_21/Ddenom;
        M(2,0) = m_02/Ddenom;  M(2,1) = m_12/Ddenom;  M(2,2) = m_22/Ddenom;
		return true;
	}

	T1(Primitive) static Vector3T<Primitive> CalculateEdgeCollapse_Offset(Vector2T<Primitive> pm1, Vector2T<Primitive> p0, Vector2T<Primitive> p1, Vector2T<Primitive> p2)
	{
		Matrix3x3T<Primitive> M;
		Vector3T<Primitive> res;
		Vector2T<Primitive> As[] = { pm1 - p0, Zero<Vector2T<Primitive>>(), p1 - p0 };
		Vector2T<Primitive> Bs[] = { Zero<Vector2T<Primitive>>(), p1 - p0, p2 - p0 };
		for (unsigned c=0; c<3; ++c) {
			auto mag = (Primitive)std::hypot(Bs[c][0] - As[c][0], Bs[c][1] - As[c][1]);
			assert(IsFiniteNumber(mag));
			auto Nx = Primitive((As[c][1] - Bs[c][1]) * VelocityVectorScale<Primitive>::Value / mag);
			auto Ny = Primitive((Bs[c][0] - As[c][0]) * VelocityVectorScale<Primitive>::Value / mag);
			M(c, 0) = Nx;
			M(c, 1) = Ny;
			M(c, 2) = -Nx*Nx-Ny*Ny;
			res[c]  = As[c][0] * Nx + As[c][1] * Ny;
		}
		if (!InvertInplaceSafe(M, GetEpsilon<Primitive>()))
			return Vector3T<Primitive>(std::numeric_limits<Primitive>::max(), std::numeric_limits<Primitive>::max(), Primitive(-1));

		auto result = M * res;
		assert(IsFiniteNumber(result[0]) && IsFiniteNumber(result[0]));
		result[0] += p0[0];
		result[1] += p0[1];
		return result;
	}

	T1(Primitive) static Primitive CalculateTriangleCollapse_Area_Internal(
		Vector2T<Primitive> p0, Vector2T<Primitive> p1, Vector2T<Primitive> p2,
		Vector2T<Primitive> v0, Vector2T<Primitive> v1, Vector2T<Primitive> v2)
	{
		auto a = (v1[0]-v0[0])*(v2[1]-v0[1]) - (v2[0]-v0[0])*(v1[1]-v0[1]);
		if (Equivalent(a, Primitive(0), GetEpsilon<Primitive>())) return std::numeric_limits<Primitive>::max();
			
		auto c = (p1[0]-p0[0])*(p2[1]-p0[1]) - (p2[0]-p0[0])*(p1[1]-p0[1]);
		auto b = (p1[0]-p0[0])*(v2[1]-v0[1]) + (v1[0]-v0[0])*(p2[1]-p0[1]) - (p2[0]-p0[0])*(v1[1]-v0[1]) - (v2[0]-v0[0])*(p1[1]-p0[1]);
			
		// x = (-b +/- sqrt(b*b - 4*a*c)) / 2*a
		auto K = b*b - Primitive(4)*a*c;
		if (K < Primitive(0)) return std::numeric_limits<Primitive>::max();

		auto Q = std::sqrt(K);
		Primitive ts[] = {
			Primitive((-b + Q) / (decltype(Q)(2)*a)),
			Primitive((-b - Q) / (decltype(Q)(2)*a))
		};
		if (ts[0] > 0.0f && ts[0] < ts[1]) return ts[0];
		return ts[1];
	}

	T1(Primitive) static Primitive CalculateTriangleCollapse_Area(
		Vector2T<Primitive> p0, Vector2T<Primitive> p1, Vector2T<Primitive> p2,
		Vector2T<Primitive> v0, Vector2T<Primitive> v1, Vector2T<Primitive> v2)
	{
		auto test = CalculateTriangleCollapse_Area_Internal(p0, p1, p2, v0, v1, v2);
		if (test != std::numeric_limits<Primitive>::max())
			return test;

		test = CalculateTriangleCollapse_Area_Internal(p1, p2, p0, v1, v2, v0);
		if (test != std::numeric_limits<Primitive>::max())
			return test;

		test = CalculateTriangleCollapse_Area_Internal(p2, p0, p1, v2, v0, v1);
		if (test != std::numeric_limits<Primitive>::max())
			return test;

		return std::numeric_limits<Primitive>::max();
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////

	T1(Primitive) std::pair<Vector2T<Primitive>, Vector2T<Primitive>> AdvanceEdge(
		std::pair<Vector2T<Primitive>, Vector2T<Primitive>> input, Primitive time )
	{
		auto t0 = Vector2T<Primitive>(input.second-input.first);
		auto movement = SetMagnitude(EdgeTangentToMovementDir(t0), time);
		return { input.first + movement, input.second + movement };
	}

	T1(Primitive) Primitive TestTriangle(Vector2T<Primitive> p[3])
	{
		using VelocityType = decltype(CalculateVertexVelocity(p[0], p[1], p[2]));
		VelocityType velocities[] = 
		{
			CalculateVertexVelocity(p[2], p[0], p[1]),
			CalculateVertexVelocity(p[0], p[1], p[2]),
			CalculateVertexVelocity(p[1], p[2], p[0])
		};

		Primitive collapses[] = 
		{
			CalculateCollapseTime<Primitive>({p[0], velocities[0]}, {p[1], velocities[1]}),
			CalculateCollapseTime<Primitive>({p[1], velocities[1]}, {p[2], velocities[2]}),
			CalculateCollapseTime<Primitive>({p[2], velocities[2]}, {p[0], velocities[0]})
		};

		// Find the earliest collapse, and calculate the accuracy
		unsigned edgeToCollapse = 0; 
		if (collapses[0] < collapses[1]) {
			if (collapses[0] < collapses[2]) {
				edgeToCollapse = 0;
			} else {
				edgeToCollapse = 2;
			}
		} else if (collapses[1] < collapses[2]) {
			edgeToCollapse = 1;
		} else {
			edgeToCollapse = 2;
		}

		auto triCollapse = CalculateTriangleCollapse(p[0], p[1], p[2]);
		auto collapseTest = collapses[edgeToCollapse];
		auto collapseTest1 = triCollapse[2];
		auto collapseTest2 = CalculateTriangleCollapse_Area(p[0], p[1], p[2], velocities[0], velocities[1], velocities[2]);
		auto collapseTest3 = CalculateTriangleCollapse_Offset(p[0], p[1], p[2]);
		auto collapseTest4 = CalculateEdgeCollapse_Offset(p[2], p[0], p[1], p[2]);
		(void)collapseTest, collapseTest1, collapseTest2, collapseTest3, collapseTest4;

		// Advance forward to time "collapses[edgeToCollapse]" and look
		// at the difference in position of pts edgeToCollapse & (edgeToCollapse+1)%3
		auto zero = edgeToCollapse, one = (edgeToCollapse+1)%3, m1 = (edgeToCollapse+2)%3;
		auto zeroA = PositionAtTime<Primitive>({p[zero], velocities[zero]}, collapses[edgeToCollapse]);
		auto oneA = PositionAtTime<Primitive>({p[one], velocities[one]}, collapses[edgeToCollapse]);

		// Accurately move forward edges m1 -> zero & one -> m1
		// The intersection point of these 2 edges should be the intersection point
		auto e0 = AdvanceEdge({p[m1], p[zero]}, collapses[edgeToCollapse]);
		auto e1 = AdvanceEdge({p[one], p[m1]}, collapses[edgeToCollapse]);
		auto intr = LineIntersection(e0, e1);

		auto intr2 = Truncate(triCollapse);

		auto d0 = zeroA - intr;
		auto d1 = oneA - intr;
		return std::max(MagnitudeSquared(d0), MagnitudeSquared(d1));
	}

	void TestSSAccuracy()
	{
#if 0
		std::stringstream str;
		// generate random triangles, and 
		std::mt19937 rng;
		const unsigned tests = 10000;
		const Vector2T<double> mins{-10, -10};
		const Vector2T<double> maxs{ 10,  10};
		double iScale = (INT64_MAX >> 8) / (maxs[0] - mins[0]) / 2.0;
		for (unsigned c=0; c<tests; ++c) {
			Vector2T<double> d[3];
			Vector2T<float> f[3];
			Vector2T<int64_t> i[3];
			for (unsigned q=0; q<3; ++q) {
				d[q][0] = std::uniform_real_distribution<double>(mins[0], maxs[0])(rng);
				d[q][1] = std::uniform_real_distribution<double>(mins[1], maxs[1])(rng);
				f[q][0] = (float)d[q][0];
				f[q][1] = (float)d[q][1];
				i[q][0] = int64_t(d[q][0] * iScale);
				i[q][1] = int64_t(d[q][1] * iScale);
			}

			auto windingType = CalculateWindingType(d[0], d[1], d[2], GetEpsilon<double>());
			if (windingType == WindingType::Straight) continue;
			if (windingType == WindingType::Right) {
				std::swap(d[0], d[2]);
				std::swap(f[0], f[2]);
				std::swap(i[0], i[2]);
			}

			auto dq = TestTriangle(d);
			auto fq = TestTriangle(f);
			// auto iq = TestTriangle(i);

			// The difference between the double and float results can give us a sense of
			// the accuracy of the method
			auto triCollapseDouble = CalculateTriangleCollapse_Offset(d[0], d[1], d[2]);
			auto triCollapseFloat = CalculateTriangleCollapse_Offset(f[0], f[1], f[2]);
			auto jitter = triCollapseDouble[2] - double(triCollapseFloat[2]);

			str << "dq: " << dq
				<< ", fq: " << fq
				// << ", iq: " << double(iq) / iScale
				<< ", jitter:" << jitter
				<< std::endl
				;
		}

		auto result = str.str();
		printf("%s", result.c_str());
		(void)result;
#endif
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////

	T1(Primitive) static unsigned AddSteinerVertex(StraightSkeleton<Primitive>& skeleton, const Vector3T<Primitive>& vertex)
	{
		assert(vertex[2] != Primitive(0));
		assert(IsFiniteNumber(vertex[0]) && IsFiniteNumber(vertex[1]) && IsFiniteNumber(vertex[2]));
		assert(vertex[0] != std::numeric_limits<Primitive>::max() && vertex[1] != std::numeric_limits<Primitive>::max() && vertex[2] != std::numeric_limits<Primitive>::max());
		auto existing = std::find_if(skeleton._steinerVertices.begin(), skeleton._steinerVertices.end(),
			[vertex](const Vector3T<Primitive>& v) { return Equivalent(v, vertex, GetEpsilon<Primitive>()); });
		if (existing != skeleton._steinerVertices.end()) {
			return (unsigned)std::distance(skeleton._steinerVertices.begin(), existing);
		}
		#if defined(_DEBUG)
			auto test = std::find_if(skeleton._steinerVertices.begin(), skeleton._steinerVertices.end(),
				[vertex](const Vector3T<Primitive>& v) { return Equivalent(Truncate(v), Truncate(vertex), GetEpsilon<Primitive>()); });
			assert(test == skeleton._steinerVertices.end());
		#endif
		auto result = (unsigned)skeleton._steinerVertices.size();
		skeleton._steinerVertices.push_back(vertex);
		return result;
	}

	T1(Primitive) struct CrashEvent
	{
		Primitive _time;
		unsigned _edgeSegment;
	};

	T1(Primitive) static CrashEvent<Primitive> CalculateCrashTime(
		Vertex<Primitive> v, 
		IteratorRange<const WavefrontEdge*> segments,
		IteratorRange<const Vertex<Primitive>*> vertices)
	{
		CrashEvent<Primitive> bestCollisionEvent { std::numeric_limits<Primitive>::max(), ~0u };

		// Look for an intersection with _wavefrontEdges
		for (const auto&e:segments) {
			const auto& head = vertices[e._head];
			const auto& tail = vertices[e._tail];

			// Since the edge segments are moving, the solution is a little complex
			// We can create a triangle between head, tail & the motorcycle head
			// If there is a collision, the triangle area will be zero at that point.
			// So we can search for a time when the triangle area is zero, and check to see
			// if a collision has actually occurred at that time.
			const auto calcTime = std::max(std::max(head._initialTime, tail._initialTime), v._initialTime);
			auto p0 = PositionAtTime(head, calcTime);
			auto p1 = PositionAtTime(tail, calcTime);
			auto v0 = head._velocity;
			auto v1 = tail._velocity;

			auto p2 = PositionAtTime(v, calcTime);
			auto v2 = v._velocity;

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

			auto a = (v1[0]-v0[0])*(v2[1]-v0[1]) - (v2[0]-v0[0])*(v1[1]-v0[1]);
			if (Equivalent(a, Primitive(0), GetEpsilon<Primitive>())) continue;
			
			auto c = (p1[0]-p0[0])*(p2[1]-p0[1]) - (p2[0]-p0[0])*(p1[1]-p0[1]);
			auto b = (p1[0]-p0[0])*(v2[1]-v0[1]) + (v1[0]-v0[0])*(p2[1]-p0[1]) - (p2[0]-p0[0])*(v1[1]-v0[1]) - (v2[0]-v0[0])*(p1[1]-p0[1]);
			
			// x = (-b +/- sqrt(b*b - 4*a*c)) / 2*a
			auto K = b*b - Primitive(4)*a*c;
			if (K < Primitive(0)) continue;

			auto Q = std::sqrt(K);
			Primitive ts[] = {
				calcTime + Primitive((-b + Q) / (decltype(Q)(2)*a)),
				calcTime + Primitive((-b - Q) / (decltype(Q)(2)*a))
			};

			// Is there is a viable collision at either t0 or t1?
			// All 3 points should be on the same line at this point -- so we just need to check if
			// the motorcycle is between them (or intersecting a vertex)
			for (auto t:ts) {
				if (t > bestCollisionEvent._time || t <= std::max(head._initialTime, tail._initialTime)) continue;	// don't need to check collisions that happen too late
				auto P0 = PositionAtTime(head, t);
				auto P1 = PositionAtTime(tail, t);
				auto P2 = PositionAtTime(v, t);
				if ((Dot(P1-P0, P2-P0) > Primitive(0)) && (Dot(P0-P1, P2-P1) > Primitive(0))) {
					// good collision
					bestCollisionEvent._time = t;
					bestCollisionEvent._edgeSegment = unsigned(&e - AsPointer(segments.begin()));
				} else if (Equivalent(P0, P2, GetEpsilon<Primitive>()) || Equivalent(P1, P2, GetEpsilon<Primitive>())) {
					// collided with vertex (or close enough)
					bestCollisionEvent._time = t;
					bestCollisionEvent._edgeSegment = unsigned(&e - AsPointer(segments.begin()));
				}
			}
		}

		return bestCollisionEvent;
	}

	T1(Primitive) static bool IsFrozen(const Vertex<Primitive>& v) { return Equivalent(v._velocity, Zero<Vector2T<Primitive>>(), GetEpsilon<Primitive>()); }
	T1(Primitive) static void FreezeInPlace(Vertex<Primitive>& v, Primitive atTime)
	{
		assert(atTime != Primitive(0));
		v._position = PositionAtTime(v, atTime);
		v._initialTime = atTime;
		v._skeletonVertexId = ~0u;
		v._velocity = Zero<Vector2T<Primitive>>();
	}

	T1(EdgeType) static void AddUnique(std::vector<EdgeType>& dst, const EdgeType& edge)
	{
		auto existing = std::find_if(dst.begin(), dst.end(), 
			[&edge](const EdgeType&e) { return e._head == edge._head && e._tail == edge._tail; });

		if (existing == dst.end()) {
			dst.push_back(edge);
		} else {
			assert(existing->_type == edge._type);
		}
	}

	T1(Primitive) static void AddEdge(StraightSkeleton<Primitive>& dest, unsigned headVertex, unsigned tailVertex, unsigned leftEdge, unsigned rightEdge, typename StraightSkeleton<Primitive>::EdgeType type)
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

///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////

	T1(Primitive) class Graph
	{
	public:
		std::vector<Vertex<Primitive>> _vertices;

		class WavefrontLoop
		{
		public:
			std::vector<WavefrontEdge> _edges;
			std::vector<MotorcycleSegment> _motorcycleSegments;
			Primitive _lastEventTime = Primitive(0);
		};
		std::vector<WavefrontLoop> _loops;
		size_t _boundaryPointCount;

		StraightSkeleton<Primitive> CalculateSkeleton(Primitive maxTime);

	private:
		void WriteWavefront(StraightSkeleton<Primitive>& dest, const WavefrontLoop& loop, Primitive time);
		void AddEdgeForVertexPath(StraightSkeleton<Primitive>& dst, unsigned v, unsigned finalVertId);
		void ValidateState();

		Primitive ProcessMotorcycleCrashes(
			WavefrontLoop& loop,
			IteratorRange<const std::pair<CrashEvent<Primitive>, size_t>*> crashes,
			StraightSkeleton<Primitive>& result);

		struct EdgeEvent
		{
		public:
			size_t _edge;
			Vector2T<Primitive> _collapsePt;
			Primitive _time;
		};
		Primitive ProcessEdgeEvents(
			WavefrontLoop& loop,
			IteratorRange<const EdgeEvent*> collapses,
			StraightSkeleton<Primitive>& result);

		void FindCollapses(std::vector<EdgeEvent>& bestCollapse, const WavefrontLoop& loop);
	};

	T1(Primitive) void Graph<Primitive>::FindCollapses(std::vector<EdgeEvent>& bestCollapse, const WavefrontLoop& loop)
	{
		auto bestCollapseTime = std::numeric_limits<Primitive>::max();
		for (size_t e=0; e<loop._edges.size(); ++e) {
			const auto& seg0 = loop._edges[(e+loop._edges.size()-1)%loop._edges.size()];
			const auto& seg1 = loop._edges[e];
			const auto& seg2 = loop._edges[(e+1)%loop._edges.size()];
			assert(seg0._head == seg1._tail && seg1._head == seg2._tail);	// ensure segments are correctly ordered

			const auto& vm1 = _vertices[seg0._tail];
			const auto& v0 = _vertices[seg1._tail];
			const auto& v1 = _vertices[seg1._head];
			const auto& v2 = _vertices[seg2._head];
			const auto calcTime = std::min(std::min(std::min(vm1._initialTime, v0._initialTime), v1._initialTime), v2._initialTime);
			auto collapse = CalculateEdgeCollapse_Offset(PositionAtTime(vm1, calcTime), PositionAtTime(v0, calcTime), PositionAtTime(v1, calcTime), PositionAtTime(v2, calcTime));
			if (collapse[2] < Primitive(0)) continue;
			assert(!IsFrozen(v0) && !IsFrozen(v1));
			auto collapseTime = collapse[2] + calcTime;
			assert(collapseTime >= loop._lastEventTime);
			if (collapseTime < (bestCollapseTime - GetEpsilon<Primitive>())) {
				bestCollapse.clear();
				bestCollapse.push_back({e, Truncate(collapse), collapseTime});
				bestCollapseTime = collapseTime;
			} else if (collapseTime < (bestCollapseTime + GetEpsilon<Primitive>())) {
				bestCollapse.push_back({e, Truncate(collapse), collapseTime});
				bestCollapseTime = std::min(collapseTime, bestCollapseTime);
			}
		}

		// Always ensure that every entry in "bestCollapse" is within
		// "GetEpsilon<Primitive>()" of bestCollapseTime -- this can become untrue if there
		// are chains of events with very small gaps in between them
		bestCollapse.erase(
			std::remove_if(
				bestCollapse.begin(), bestCollapse.end(),
				[bestCollapseTime](const EdgeEvent& e) { return !(e._time < bestCollapseTime + GetEpsilon<Primitive>()); }), 
			bestCollapse.end());
	}

	T1(Primitive) StraightSkeleton<Primitive> Graph<Primitive>::CalculateSkeleton(Primitive maxTime)
	{
		StraightSkeleton<Primitive> result;
		result._faces.resize(_boundaryPointCount);

		std::vector<EdgeEvent> bestCollapse;
		std::vector<std::pair<CrashEvent<Primitive>, size_t>> bestMotorcycleCrash;
		bestCollapse.reserve(8);

		unsigned lastEvent = 0;
		std::vector<WavefrontLoop> completedLoops;

		while (!_loops.empty()) {
			ValidateState();

			auto& loop = _loops.front();
			if (loop._edges.size() <= 2) {
				completedLoops.emplace_back(std::move(loop));
				_loops.erase(_loops.begin());
				continue;
			}

			// Find the next event to occur
			//		-- either a edge collapse or a motorcycle collision
			bestCollapse.clear();
			FindCollapses(bestCollapse, loop);
			auto bestCollapseTime = std::numeric_limits<Primitive>::max();
			for (const auto&e:bestCollapse)
				bestCollapseTime = std::min(bestCollapseTime, e._time);

			// Also check for motorcycles colliding.
			//		These can collide with segments in the _wavefrontEdges list, or 
			//		other motorcycles, or boundary polygon edges
			auto bestMotorcycleCrashTime = std::numeric_limits<Primitive>::max();
			bestMotorcycleCrash.clear();
			for (auto m=loop._motorcycleSegments.begin(); m!=loop._motorcycleSegments.end(); ++m) {
				const auto& head = _vertices[m->_head];
				if (Equivalent(head._velocity, Zero<Vector2T<Primitive>>(), GetEpsilon<Primitive>())) continue;
				assert(head._initialTime == Primitive(0));
				auto crashEvent = CalculateCrashTime<Primitive>(head, MakeIteratorRange(loop._edges), MakeIteratorRange(_vertices));
				if (crashEvent._time < Primitive(0)) continue;
				assert(crashEvent._time >= loop._lastEventTime);

				// todo -- have to ensure the motorcycle doesn't crash into any other loops at an earlier point!

				// If our best motorcycle collision happens before our best collapse, then we
				// must do the motorcycle first, and recalculate edge collapses afterwards
				// But if they happen at the same time, we should do the edge collapse first,
				// and then recalculate the motorcycle collisions afterwards (ie, even if there's
				// a motorcycle collision at around the same time as the edge collapses, we're
				// going to ignore it for now)
				if (crashEvent._time < (bestCollapseTime + GetEpsilon<Primitive>())) {
					if (crashEvent._time < (bestMotorcycleCrashTime - GetEpsilon<Primitive>())) {
						bestMotorcycleCrash.clear();
						bestMotorcycleCrash.push_back(std::make_pair(crashEvent, std::distance(loop._motorcycleSegments.begin(), m)));
						bestMotorcycleCrashTime = crashEvent._time;
					} else if (crashEvent._time < (bestMotorcycleCrashTime + GetEpsilon<Primitive>())) {
						bestMotorcycleCrash.push_back(std::make_pair(crashEvent, std::distance(loop._motorcycleSegments.begin(), m)));
						bestMotorcycleCrashTime = std::min(crashEvent._time, bestMotorcycleCrashTime);
					}
				}
			}

			// If we get some motorcycle crashes, we're going to ignore the collapse events
			// and just process the motorcycle events
			if (!bestMotorcycleCrash.empty()) {
				if (bestMotorcycleCrashTime > maxTime) {
					loop._lastEventTime = maxTime;
					completedLoops.emplace_back(std::move(loop));
					_loops.erase(_loops.begin());
					continue;
				}

				bestMotorcycleCrash.erase(
					std::remove_if(
						bestMotorcycleCrash.begin(), bestMotorcycleCrash.end(),
						[bestMotorcycleCrashTime](const std::pair<CrashEvent<Primitive>, size_t>& e) { return !(e.first._time < bestMotorcycleCrashTime + GetEpsilon<Primitive>()); }), 
					bestMotorcycleCrash.end());

				ProcessMotorcycleCrashes(loop, MakeIteratorRange(bestMotorcycleCrash), result);

				#if defined(_DEBUG)
					if (loop._edges.size() > 2) {
						std::vector<EdgeEvent> newBestCollapse;
						FindCollapses(newBestCollapse, loop);
						for (const auto&e:newBestCollapse) {
							assert(e._time >= (loop._lastEventTime - GetEpsilon<Primitive>()));
						}
					}
				#endif
				lastEvent = 1;
			} else {
				if (bestCollapse.empty() || bestCollapseTime > maxTime) {
					if (bestCollapseTime > maxTime)
						loop._lastEventTime = maxTime;
					completedLoops.emplace_back(std::move(loop));
					_loops.erase(_loops.begin());
					continue;
				}

				loop._lastEventTime = ProcessEdgeEvents(loop, MakeIteratorRange(bestCollapse), result);
				lastEvent = 2;
			}
		}

		for (const auto&l:completedLoops)
			WriteWavefront(result, l, l._lastEventTime);

		return result;
	}

	T1(Primitive) Graph<Primitive> BuildGraphFromVertexLoop(IteratorRange<const Vector2T<Primitive>*> vertices)
	{
		assert(vertices.size() >= 2);
		const auto threshold = Primitive(1e-6f);

		// Construct the starting point for the straight skeleton calculations
		// We're expecting the input vertices to be a closed loop, in counter-clockwise order
		// The first and last vertices should *not* be the same vertex; there is an implied
		// segment between the first and last.
		Graph<Primitive> result;
		Graph<Primitive>::WavefrontLoop loop;
		loop._edges.reserve(vertices.size());
		result._vertices.reserve(vertices.size());
		for (size_t v=0; v<vertices.size(); ++v) {
			// Each segment of the polygon becomes an "edge segment" in the graph
			auto v0 = (v+vertices.size()-1)%vertices.size();
			auto v1 = v;
			auto v2 = (v+1)%vertices.size();
			loop._edges.emplace_back(WavefrontEdge{unsigned(v2), unsigned(v), ~0u, unsigned(v)});

			// We must calculate the velocity for each vertex, based on which segments it belongs to...
			auto velocity = CalculateVertexVelocity(vertices[v0], vertices[v1], vertices[v2]);
			assert(!Equivalent(velocity, Zero<Vector2T<Primitive>>(), GetEpsilon<Primitive>()));
			result._vertices.emplace_back(Vertex<Primitive>{vertices[v], velocity, Primitive(0), BoundaryVertexFlag|unsigned(v)});
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
				result._vertices.emplace_back(Vertex<Primitive>{vertices[v], Zero<Vector2T<Primitive>>(), Primitive(0), BoundaryVertexFlag|unsigned(v)});
				loop._motorcycleSegments.emplace_back(MotorcycleSegment{unsigned(v), unsigned(fixedVertex), unsigned(v0), unsigned(v1)});
			}
		}

		result._loops.emplace_back(std::move(loop));
		result._boundaryPointCount = vertices.size();
		return result;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////

	static auto FindInAndOut(IteratorRange<WavefrontEdge*> edges, unsigned pivotVertex) -> std::pair<WavefrontEdge*, WavefrontEdge*>
	{
		std::pair<WavefrontEdge*, WavefrontEdge*> result(nullptr, nullptr);
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

	static void ReplaceVertex(IteratorRange<WavefrontEdge*> segs, unsigned oldVertex, unsigned newVertex)
	{
		for (auto& s:segs) {
			if (s._head == oldVertex) s._head = newVertex;
			if (s._tail == oldVertex) s._tail = newVertex;
		}
	}

	T1(Primitive) void Graph<Primitive>::ValidateState()
	{
		#if defined(EXTRA_VALIDATION)
			for (auto&loop:_loops) {
				// validate vertex velocities
				for (unsigned v=0; v<_vertices.size(); ++v) {
					WavefrontEdge* in, *out;
					std::tie(in, out) = FindInAndOut(MakeIteratorRange(loop._edges), v);
					if (in && out) {
						if (in->_tail != out->_head) {
							auto calcTime = (_vertices[in->_tail]._initialTime + _vertices[v]._initialTime + _vertices[out->_head]._initialTime) / Primitive(3);
							auto v0 = PositionAtTime(_vertices[in->_tail], calcTime);
							auto v1 = PositionAtTime(_vertices[v], calcTime);
							auto v2 = PositionAtTime(_vertices[out->_head], calcTime);
							auto expectedVelocity = CalculateVertexVelocity(v0, v1, v2);
							assert(Equivalent(_vertices[v]._velocity, expectedVelocity, 10000*GetEpsilon<Primitive>()));
							assert(!Equivalent(expectedVelocity, Zero<Vector2T<Primitive>>(), GetEpsilon<Primitive>()));
						} else {
							assert(Equivalent(_vertices[v]._velocity, Zero<Vector2T<Primitive>>(), GetEpsilon<Primitive>()));
						}
					}
				}
				// Vertices in active loops should not be frozen
				if (loop._edges.size() > 2)
					for (const auto&e:loop._edges)
						assert(!IsFrozen(_vertices[e._head]) && !IsFrozen(_vertices[e._tail]));
				// every wavefront edge must have a collapse time (assuming it's vertices are not frozen)
				/*for (const auto&e:loop._edges) {
					if (IsFrozen(_vertices[e._head]) || IsFrozen(_vertices[e._tail])) continue;
					auto collapseTime = CalculateCollapseTime(_vertices[e._head], _vertices[e._tail]);
					assert(collapseTime != std::numeric_limits<Primitive>::max());		//it can be negative; because some edges are expanding
				}*/
			}
		#endif
	}

	T1(Primitive) Primitive Graph<Primitive>::ProcessMotorcycleCrashes(
		WavefrontLoop& loop,		// (this is the loop that contains the edge we crashed into)
		IteratorRange<const std::pair<CrashEvent<Primitive>, size_t>*> crashes,
		StraightSkeleton<Primitive>& result)
	{
		// we can only process a single crash event at a time currently
		// only the first event in bestMotorcycleCrashwill be processed (note that
		// this isn't necessarily the first event!)
		assert(crashes.size() == 1);
		auto crashEvent = crashes[0].first;
		const auto& motor = loop._motorcycleSegments[crashes[0].second];
		assert(crashEvent._edgeSegment != ~0u);

		auto crashPt = PositionAtTime(_vertices[motor._head], crashEvent._time);
		auto crashPtSkeleton = AddSteinerVertex(result, Vector3T<Primitive>(crashPt, crashEvent._time));

		auto crashSegment = loop._edges[crashEvent._edgeSegment];
		WavefrontEdge newSegment0{ motor._head, motor._head, motor._leftFace, crashSegment._rightFace};
		WavefrontEdge newSegment1{ motor._head, motor._head, crashSegment._rightFace, motor._rightFace };

		// We need to build 2 new WavefrontLoops -- one for the "tout" side and one for the "tin" side
		// In some cases, one side or the other than can be completely collapsed. But we're still going to
		// create it.
		WavefrontLoop outSide, inSide;
		outSide._lastEventTime = crashEvent._time;
		inSide._lastEventTime = crashEvent._time;
		{
			// Start at motor._head, and work around in order until we hit the crash segment.
			auto tout = std::find_if(loop._edges.begin(), loop._edges.end(),
				[&motor](const WavefrontEdge& test) { return test._tail == motor._head; });
			assert(tout != loop._edges.end());

			if (tout->_head != crashSegment._tail) {
				auto starti = tout+1;
				if (starti == loop._edges.end()) starti = loop._edges.begin();
				auto i=starti;
				while (i->_head!=crashSegment._tail) {
					outSide._edges.push_back(*i);
					++i;
					if (i == loop._edges.end()) i = loop._edges.begin();
				}
				outSide._edges.push_back(*i);
			}

			auto v0i = crashSegment._tail;
			auto v2i = tout->_head;
			auto v0 = PositionAtTime(_vertices[v0i], crashEvent._time);
			auto v2 = PositionAtTime(_vertices[v2i], crashEvent._time);

			// Note that we can end up with colinear vertices here in two cases. When there are
			// colinear vertices, we can't calculate the vertex velocity values accurately 
			//   1) we might produce a fully collapsed 2-edge loop
			//   2) when there are simultaneous motorcycle crashes
			// In the second case, the loop is not fully collapsed yet; but after the remaining
			// crashes have been processed, it will be.
			auto newVertex = (unsigned)_vertices.size();
			auto newVertexVel = (v0i != v2i) ? CalculateVertexVelocity(v0, crashPt, v2) : Zero<Vector2T<Primitive>>();
			_vertices.push_back(Vertex<Primitive>{crashPt, newVertexVel, crashEvent._time, crashPtSkeleton});

			outSide._edges.push_back({newVertex, v0i, crashSegment._leftFace, crashSegment._rightFace});	// (hin)
			outSide._edges.push_back({v2i, newVertex, tout->_leftFace, tout->_rightFace});					// tout

			if (outSide._edges.size() <= 2)
				for (const auto&e:outSide._edges) {
					FreezeInPlace(_vertices[e._head], crashEvent._time);
					FreezeInPlace(_vertices[e._tail], crashEvent._time);
				}
		}

		{
			// Start at crashSegment._head, and work around in order until we hit the motor vertex
			auto hout = std::find_if(loop._edges.begin(), loop._edges.end(),
				[&crashSegment](const WavefrontEdge& test) { return test._tail == crashSegment._head; });
			assert(hout != loop._edges.end());

			auto starti = hout;
			auto i=starti;
			while (i->_head!=motor._head) {
				inSide._edges.push_back(*i);
				++i;
				if (i == loop._edges.end())
					i = loop._edges.begin();
			}

			auto tin = i;

			auto v0i = tin->_tail;
			auto v2i = crashSegment._head;
			auto v0 = PositionAtTime(_vertices[v0i], crashEvent._time);
			auto v2 = PositionAtTime(_vertices[v2i], crashEvent._time);

			auto newVertex = (unsigned)_vertices.size();
			auto newVertexVel = (v0i != v2i) ? CalculateVertexVelocity(v0, crashPt, v2) : Zero<Vector2T<Primitive>>();
			_vertices.push_back(Vertex<Primitive>{crashPt, newVertexVel, crashEvent._time, crashPtSkeleton});

			inSide._edges.push_back({newVertex, v0i, tin->_leftFace, tin->_rightFace});
			inSide._edges.push_back({v2i, newVertex, crashSegment._leftFace, crashSegment._rightFace});

			if (inSide._edges.size() <= 2)
				for (const auto&e:inSide._edges) {
					FreezeInPlace(_vertices[e._head], crashEvent._time);
					FreezeInPlace(_vertices[e._tail], crashEvent._time);
				}
		}

#if 0
		// is there volume on the "tout" side?
		{
			auto* tout = FindInAndOut(MakeIteratorRange(_wavefrontEdges), motor._head).second;
			assert(tout);

			auto v0 = ClampedPositionAtTime(_vertices[crashSegment._tail], calcTime);
			auto v2 = ClampedPositionAtTime(_vertices[tout->_head], calcTime);
			if (tout->_head == crashSegment._tail || Equivalent(v0, v2, GetEpsilon<Primitive>())) {
				// no longer need crashSegment or tout
				assert(crashSegment._leftFace == ~0u && tout->_leftFace == ~0u);
				auto endPt = AddSteinerVertex<Primitive>(result, (v0+v2)/Primitive(2));
				AddEdge(result, endPt, crashPtSkeleton, crashSegment._rightFace, tout->_rightFace, StraightSkeleton<Primitive>::EdgeType::VertexPath);
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
						[toutHead, crashSegment](const WavefrontEdge&s) { return s._head == crashSegment._tail && s._tail == toutHead; });
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

				_vertices.push_back(Vertex<Primitive>{crashPt, CalculateVertexVelocity(Truncate(v0), crashPt, Truncate(v2)), crashEvent._time, crashPtSkeleton});
				newSegment1._head = newVertex;
			}
		}

		// is there volume on the "tin" side?
		{
			auto* tin = FindInAndOut(MakeIteratorRange(_wavefrontEdges), motor._head).first;
			assert(tin);

			auto v0 = ClampedPositionAtTime(_vertices[tin->_tail], calcTime);
			auto v2 = ClampedPositionAtTime(_vertices[crashSegment._head], calcTime);
			if (tin->_tail == crashSegment._head || Equivalent(v0, v2, GetEpsilon<Primitive>())) {
				// no longer need "crashSegment" or tin
				assert(crashSegment._leftFace == ~0u && tin->_leftFace == ~0u);
				auto endPt = AddSteinerVertex<Primitive>(result, (v0+v2)/Primitive(2));
				AddEdge(result, endPt, crashPtSkeleton, tin->_rightFace, crashSegment._rightFace, StraightSkeleton<Primitive>::EdgeType::VertexPath);
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
						[tinTail, crashSegment](const WavefrontEdge&s) { return s._head == tinTail && s._tail == crashSegment._head; });
					if (existing != _wavefrontEdges.end()) {
						_wavefrontEdges.push_back({crashSegment._head, tinTail, existing->_rightFace, existing->_leftFace});
					} else
						_wavefrontEdges.push_back({crashSegment._head, tinTail, ~0u, ~0u});
				}
			} else {
				auto newVertex = (unsigned)_vertices.size();
				tin->_head = newVertex;
				_wavefrontEdges.push_back({crashSegment._head, newVertex, crashSegment._leftFace, crashSegment._rightFace});	// (hout)

				_vertices.push_back(Vertex<Primitive>{crashPt, CalculateVertexVelocity(Truncate(v0), crashPt, Truncate(v2)), crashEvent._time, crashPtSkeleton});
				newSegment0._head = newVertex;
			}
		}

		// if (newSegment0._head != newSegment0._tail) _wavefrontEdges.push_back(newSegment0);
		// if (newSegment1._head != newSegment1._tail) _wavefrontEdges.push_back(newSegment1);

		// note -- we can't erase this edge too soon, because it's used to calculate left and right faces
		// when calling AddEdgeForVertexPath 
		_wavefrontEdges.erase(
			std::remove_if(	_wavefrontEdges.begin(), _wavefrontEdges.end(), 
							[crashSegment](const WavefrontEdge& s) { return s._head == crashSegment._head && s._tail == crashSegment._tail; }), 
			_wavefrontEdges.end());
#endif

		// add skeleton edge for vertex path along the motor cycle path  
		assert(_vertices[motor._tail]._skeletonVertexId != ~0u);
		AddEdge(result, 
				crashPtSkeleton, _vertices[motor._tail]._skeletonVertexId,
				motor._leftFace, motor._rightFace, StraightSkeleton<Primitive>::EdgeType::VertexPath);
		FreezeInPlace(_vertices[motor._head], crashEvent._time);

		// _motorcycleSegments.erase(_motorcycleSegments.begin() + crashes[0].second);
		// move the motorcycles from "loop" to "inSide" or "outSide" depending on which loop they are now
		// a part of.
		for (const auto&m:loop._motorcycleSegments) {
			if (&m == &motor) continue;		// (skip this one, it's just been processed)

			auto inSideI = std::find_if(inSide._edges.begin(), inSide._edges.end(),
				[&m](const WavefrontEdge&e) { return e._head == m._head || e._tail == m._head; });
			if (inSideI != inSide._edges.end()) {
				inSide._motorcycleSegments.push_back(m);
				continue;
			}

			auto outSideI = std::find_if(outSide._edges.begin(), outSide._edges.end(),
				[&m](const WavefrontEdge&e) { return e._head == m._head || e._tail == m._head; });
			if (outSideI != outSide._edges.end()) {
				outSide._motorcycleSegments.push_back(m);
				continue;
			}

			// we can sometimes get here if boths edges to the left and right of
			// the motor cycle collapse before the motorcycle crashes 
		}

		#if defined(EXTRA_VALIDATION)
			{
				for (auto m=outSide._motorcycleSegments.begin(); m!=outSide._motorcycleSegments.end(); ++m) {
					const auto& head = _vertices[m->_head];
					if (Equivalent(head._velocity, Zero<Vector2T<Primitive>>(), GetEpsilon<Primitive>())) continue;
					auto nextCrashEvent = CalculateCrashTime<Primitive>(head, MakeIteratorRange(outSide._edges), MakeIteratorRange(_vertices));
					if (nextCrashEvent._time < Primitive(0)) continue;
					assert(nextCrashEvent._time >= crashEvent._time);
				}
				for (auto m=inSide._motorcycleSegments.begin(); m!=inSide._motorcycleSegments.end(); ++m) {
					const auto& head = _vertices[m->_head];
					if (Equivalent(head._velocity, Zero<Vector2T<Primitive>>(), GetEpsilon<Primitive>())) continue;
					auto nextCrashEvent = CalculateCrashTime<Primitive>(head, MakeIteratorRange(inSide._edges), MakeIteratorRange(_vertices));
					if (nextCrashEvent._time < Primitive(0)) continue;
					assert(nextCrashEvent._time >= crashEvent._time);
				}
				// This motorcycle vertex is now frozen, and should not be a part of 
				// either active loop
				for (auto e:outSide._edges) assert(e._head != motor._head && e._tail != motor._head);
				for (auto e:inSide._edges) assert(e._head != motor._head && e._tail != motor._head);
			}
		#endif

		// Overwrite "loop" with outSide, and append inSide to the list of wavefront loops
		loop = std::move(outSide);
		_loops.emplace_back(std::move(inSide));

		return crashEvent._time;
	}

	T1(Primitive) Primitive Graph<Primitive>::ProcessEdgeEvents(
		WavefrontLoop& loop,
		IteratorRange<const EdgeEvent*> collapses,
		StraightSkeleton<Primitive>& result)
	{
		if (collapses.empty()) return std::numeric_limits<Primitive>::max();

		Primitive bestCollapseTime = std::numeric_limits<Primitive>::max();
		for (auto&c:collapses) bestCollapseTime = std::min(c._time, bestCollapseTime);

		// Process the "edge" events... first separate the edges into collapse groups
		// Each collapse group collapses onto a single vertex. We will search through all
		// of the collapse events we're processing, and separate them into discrete groups.
		std::vector<unsigned> collapseGroups(collapses.size(), ~0u);
		struct CollapseGroupInfo { unsigned _head, _tail, _newVertex; };
		std::vector<CollapseGroupInfo> collapseGroupInfos;
		unsigned nextCollapseGroup = 0;
		for (size_t c=0; c<collapses.size(); ++c) {
			if (collapseGroups[c] != ~0u) continue;

			collapseGroups[c] = nextCollapseGroup;

			// got back as far as possible, from tail to tail
			auto searchingTail = loop._edges[collapses[c]._edge]._tail;
			for (;;) {
				auto i = std::find_if(collapses.begin(), collapses.end(),
					[searchingTail, &loop](const EdgeEvent& t)
					{ return loop._edges[t._edge]._head == searchingTail; });
				if (i == collapses.end()) break;
				if (collapseGroups[std::distance(collapses.begin(), i)] == nextCollapseGroup) break;
				assert(collapseGroups[std::distance(collapses.begin(), i)] == ~0u);
				collapseGroups[std::distance(collapses.begin(), i)] = nextCollapseGroup;
				searchingTail = loop._edges[i->_edge]._tail;
			}

			// also go forward head to head
			auto searchingHead = loop._edges[collapses[c]._edge]._head;
			for (;;) {
				auto i = std::find_if(collapses.begin(), collapses.end(),
					[searchingHead, &loop](const EdgeEvent& t)
					{ return loop._edges[t._edge]._tail == searchingHead; });
				if (i == collapses.end()) break;
				if (collapseGroups[std::distance(collapses.begin(), i)] == nextCollapseGroup) break;
				assert(collapseGroups[std::distance(collapses.begin(), i)] == ~0u);
				collapseGroups[std::distance(collapses.begin(), i)] = nextCollapseGroup;
				searchingHead = loop._edges[i->_edge]._head;
			}

			++nextCollapseGroup;
			collapseGroupInfos.push_back({searchingHead, searchingTail, ~0u});
		}

		// Each collapse group becomes a single new vertex. We can collate them together
		// now, and write out some segments to the output skeleton
		std::vector<unsigned> collapseGroupNewVertex(nextCollapseGroup, ~0u);
		for (auto collapseGroup=0u; collapseGroup<nextCollapseGroup; ++collapseGroup) {
			Vector2T<Primitive> collisionPt(Primitive(0), Primitive(0));
			unsigned contributors = 0;
			for (size_t c=0; c<collapses.size(); ++c) {
				if (collapseGroups[c] != collapseGroup) continue;
				collisionPt += collapses[c]._collapsePt;
				contributors += 1;

				// at this point they should not be frozen (but they will all be frozen later)
				const auto& seg = loop._edges[collapses[c]._edge];
				assert(!IsFrozen(_vertices[seg._tail]));
				assert(!IsFrozen(_vertices[seg._head]));
			}
			collisionPt /= Primitive(contributors);

			// Validate that our "collisionPt" is close to all of the collapsing points
			#if defined(_DEBUG)
				for (size_t c=0; c<collapses.size(); ++c) {
					if (collapseGroups[c] != collapseGroup) continue;
					const auto& seg = loop._edges[collapses[c]._edge];
					auto one = PositionAtTime(_vertices[seg._head], bestCollapseTime);
					auto two = PositionAtTime(_vertices[seg._tail], bestCollapseTime);
					assert(Equivalent(one, collisionPt, 1000*GetEpsilon<Primitive>()));
					assert(Equivalent(two, collisionPt, 1000*GetEpsilon<Primitive>()));
				}
			#endif

			// add a steiner vertex into the output
			auto collisionVertId = AddSteinerVertex(result, Expand(collisionPt, bestCollapseTime));

			// connect up edges in the output graph
			// Note that since we're connecting both head and tail, we'll end up doubling up each edge
			for (size_t c=0; c<collapses.size(); ++c) {
				if (collapseGroups[c] != collapseGroup) continue;
				const auto& seg = loop._edges[collapses[c]._edge];
				unsigned vs[] = { seg._head, seg._tail };
				for (auto& v:vs)
					AddEdgeForVertexPath(result, v, collisionVertId);
					
				FreezeInPlace(_vertices[seg._tail], bestCollapseTime);
				FreezeInPlace(_vertices[seg._head], bestCollapseTime);
			}

			// create a new vertex in the graph to connect the edges to either side of the collapse
			collapseGroupInfos[collapseGroup]._newVertex = (unsigned)_vertices.size();
			_vertices.push_back(Vertex<Primitive>{collisionPt, Zero<Vector2T<Primitive>>(), bestCollapseTime, collisionVertId});
		}

		// Remove all of the collapsed edges (by shifting them to the end)
		// (note, expecting bestCollapse to be sorted by "second")
		//auto r = loop._edges.end()-1;
		for (auto i=collapses.size()-1;; --i) {
			//if (i!=collapses.size()-1) --r;
			// Swap the ones we're going to remove to the end of the list (note that we loose ordering
			// for the list as a whole...
			//std::swap(*r, loop._edges[collapses[i]._edge]);
			loop._edges.erase(loop._edges.begin() + collapses[i]._edge);
			if (i == 0) break;
		}
		//loop._edges.erase(r, loop._edges.end());

		// For each collapse group, there should be one tail edge, and one head edge
		// We need to find these edges in order to calculate the velocity of the point in between
		// Let's resolve that now...
		// todo -- this process can be simplified and combined with the above loop now; because we
		//			ensure that the wavefront loops are kept in winding order
		for (const auto& group:collapseGroupInfos) {
			if (group._head == group._tail) continue;	// if we remove an entire loop, let's assume that there are no external links to it

			auto tail = FindInAndOut(MakeIteratorRange(loop._edges), group._tail).first;
			auto head = FindInAndOut(MakeIteratorRange(loop._edges), group._head).second;
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

					auto calcTime = (_vertices[tail->_tail]._initialTime + _vertices[group._newVertex]._initialTime + _vertices[head->_head]._initialTime) / Primitive(3);
					auto v0 = PositionAtTime(_vertices[tail->_tail], calcTime);
					auto v1 = PositionAtTime(_vertices[group._newVertex], calcTime);
					auto v2 = PositionAtTime(_vertices[head->_head], calcTime);
							
					auto validatedVelocity = CalculateVertexVelocity(v0, v1, v2);
					assert(Equivalent(validatedVelocity, _vertices[group._newVertex]._velocity, 100*GetEpsilon<Primitive>()));
				}
			#endif
		}

		for (const auto&e:loop._edges)
			assert(!IsFrozen(_vertices[e._tail]) && !IsFrozen(_vertices[e._head]));

		return bestCollapseTime;
	}

	T1(Primitive) static Primitive ClosestPointOnLine2D(Vector2T<Primitive> rayStart, Vector2T<Primitive> rayEnd, Vector2T<Primitive> testPt)
	{
		auto o = testPt - rayStart;
		auto l = rayEnd - rayStart;
		return Dot(o, l) / MagnitudeSquared(l);
	}

	T1(Primitive) static bool DoColinearLinesIntersect(Vector2T<Primitive> AStart, Vector2T<Primitive> AEnd, Vector2T<Primitive> BStart, Vector2T<Primitive> BEnd)
	{
		// return false if the lines share a point, but otherwise do not intersect
		// but returns true if the lines overlap completely (even if the lines have zero length)
		auto closestBStart = ClosestPointOnLine2D(AStart, AEnd, BStart);
		auto closestBEnd = ClosestPointOnLine2D(AStart, AEnd, BEnd);
		return ((closestBStart > GetEpsilon<Primitive>()) && (closestBStart > Primitive(1)-GetEpsilon<Primitive>()))
			|| ((closestBEnd > GetEpsilon<Primitive>()) && (closestBEnd > Primitive(1)-GetEpsilon<Primitive>()))
			|| (Equivalent(AStart, BStart, GetEpsilon<Primitive>()) && Equivalent(AEnd, BEnd, GetEpsilon<Primitive>()))
			|| (Equivalent(AEnd, BStart, GetEpsilon<Primitive>()) && Equivalent(AStart, BEnd, GetEpsilon<Primitive>()))
			;
	}

	T1(Primitive) void Graph<Primitive>::WriteWavefront(StraightSkeleton<Primitive>& result, const WavefrontLoop& loop, Primitive time)
	{
		// Write the current wavefront to the destination skeleton. Each edge in 
		// _wavefrontEdges comes a segment in the output
		// However, we must check for overlapping / intersecting edges
		//	-- these happen very frequently
		// The best way to remove overlapping edges is just to go through the list of segments, 
		// and for each one look for other segments that intersect

		std::vector<WavefrontEdge> filteredSegments;
		std::stack<WavefrontEdge> segmentsToTest;

		// We need to combine overlapping points at this stage, also
		// (2 different vertices could end up at the same location at time 'time')

		for (auto i=loop._edges.begin(); i!=loop._edges.end(); ++i) {
			auto A = ClampedPositionAtTime(_vertices[i->_head], time);
			auto B = ClampedPositionAtTime(_vertices[i->_tail], time);
			auto v0 = AddSteinerVertex(result, A);
			auto v1 = AddSteinerVertex(result, B);
			if (v0 != v1)
				segmentsToTest.push(WavefrontEdge{v0, v1, i->_leftFace, i->_rightFace});
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

				bool COnLine = closestC > Primitive(0) && closestC < Primitive(1) && MagnitudeSquared(LinearInterpolate(A, B, closestC) - C) < GetEpsilon<Primitive>();
				bool DOnLine = closestD > Primitive(0) && closestD < Primitive(1) && MagnitudeSquared(LinearInterpolate(A, B, closestD) - D) < GetEpsilon<Primitive>();
				if (!COnLine && !DOnLine) { continue; }

				auto m0 = (B[1] - A[1]) / (B[0] - A[0]);
				auto m1 = (D[1] - C[1]) / (D[0] - C[0]);
				if (!Equivalent(m0, m1, GetEpsilon<Primitive>())) { continue; }

				if (i2->_head == seg._head) {
					if (closestD < Primitive(1)) {
						seg._head = i2->_tail;
					} else {
						i2->_head = seg._tail;
					}
				} else if (i2->_head == seg._tail) {
					if (closestD > Primitive(0)) {
						seg._tail = i2->_tail;
					} else {
						i2->_head = seg._head;
					}
				} else if (i2->_tail == seg._head) {
					if (closestC < Primitive(1)) {
						seg._head = i2->_head;
					} else {
						i2->_tail = seg._tail;
					}
				} else if (i2->_tail == seg._tail) {
					if (closestC > Primitive(0)) {
						seg._tail = i2->_head;
					} else {
						i2->_tail = seg._head;
					}
				} else {
					// The lines are colinear, and at least one point of i2 is on i
					// We must separate these 2 segments into 3 segments.
					// Replace i2 with something that is strictly with i2, and then schedule
					// the remaining split parts for intersection tests.
					WavefrontEdge newSeg;
					if (closestC < Primitive(0)) {
						if (closestD > Primitive(1)) newSeg = {seg._tail, i2->_tail};
						else { newSeg = {i2->_tail, seg._tail}; seg._tail = i2->_tail; }
						i2->_tail = seg._head;
					} else if (closestD < Primitive(0)) {
						if (closestC > Primitive(1)) newSeg = {seg._tail, i2->_head};
						else { newSeg = {i2->_head, seg._tail}; seg._tail = i2->_head; }
						i2->_head = seg._head;
					} else if (closestC < closestD) {
						if (closestD > Primitive(1)) newSeg = {seg._tail, i2->_tail};
						else { newSeg = {i2->_tail, seg._tail}; seg._tail = i2->_tail; }
						seg._tail = i2->_head;
					} else {
						if (closestC > Primitive(1)) newSeg = {seg._tail, i2->_head};
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
			AddEdge(result, seg._head, seg._tail, seg._leftFace, seg._rightFace, StraightSkeleton<Primitive>::EdgeType::Wavefront);
		}

		// Also have to add the traced out path of the each vertex (but only if it doesn't already exist in the result)
		for (const auto&seg:loop._edges) {
			unsigned vs[] = {seg._head, seg._tail};
			for (auto v:vs) {
				const auto& vert = _vertices[v];
				AddEdgeForVertexPath(result, v, AddSteinerVertex(result, ClampedPositionAtTime(vert, time)));
			}
		}
	}

	T1(Primitive) void Graph<Primitive>::AddEdgeForVertexPath(StraightSkeleton<Primitive>& dst, unsigned v, unsigned finalVertId)
	{
		const auto& vert = _vertices[v];
		unsigned leftFace = ~0u, rightFace = ~0u;

		for (auto&l:_loops) {
			auto inAndOut = FindInAndOut(MakeIteratorRange(l._edges), v);
			if (inAndOut.first) leftFace = inAndOut.first->_rightFace;
			if (inAndOut.second) rightFace = inAndOut.second->_rightFace;
		}

		if (vert._skeletonVertexId != ~0u) {
			if (vert._skeletonVertexId&BoundaryVertexFlag) {
				auto q = vert._skeletonVertexId&(~BoundaryVertexFlag);
				AddEdge(dst, finalVertId, vert._skeletonVertexId, unsigned((q+_boundaryPointCount-1) % _boundaryPointCount), q, StraightSkeleton<Primitive>::EdgeType::VertexPath);
			}
			AddEdge(dst, finalVertId, vert._skeletonVertexId, leftFace, rightFace, StraightSkeleton<Primitive>::EdgeType::VertexPath);
		} else {
			AddEdge(dst,
				finalVertId, AddSteinerVertex(dst, Expand(vert._position, vert._initialTime)),
				leftFace, rightFace, StraightSkeleton<Primitive>::EdgeType::VertexPath);
		}
	}


	T1(Primitive) StraightSkeleton<Primitive> CalculateStraightSkeleton(IteratorRange<const Vector2T<Primitive>*> vertices, Primitive maxInset)
	{
		auto graph = BuildGraphFromVertexLoop(vertices);
		return graph.CalculateSkeleton(maxInset);
	}

	std::vector<std::vector<unsigned>> AsVertexLoopsOrdered(
		IteratorRange<const std::pair<unsigned, unsigned>*> segments)
	{
		// From a line segment soup, generate vertex loops. This requires searching
		// for segments that join end-to-end, and following them around until we
		// make a loop.
		// Let's assume for the moment there are no 3-or-more way junctions (this would
		// require using some extra math to determine which is the correct path)
		std::vector<std::pair<unsigned, unsigned>> pool(segments.begin(), segments.end());
		std::vector<std::vector<unsigned>> result;
		while (!pool.empty()) {
			std::vector<unsigned> workingLoop;
			{
				auto i = pool.end()-1;
				workingLoop.push_back(i->first);
				workingLoop.push_back(i->second);
				pool.erase(i);
			}
			for (;;) {
				assert(!pool.empty());	// if we hit this, we have open segments
				auto searching = *(workingLoop.end()-1);
				auto hit = pool.end(); 
				for (auto i=pool.begin(); i!=pool.end(); ++i) {
					if (i->first == searching /*|| i->second == searching*/) {
						assert(hit == pool.end());
						hit = i;
					}
				}
				assert(hit != pool.end());
				auto newVert = hit->second; // (hit->first == searching) ? hit->second : hit->first;
				pool.erase(hit);
				if (std::find(workingLoop.begin(), workingLoop.end(), newVert) != workingLoop.end())
					break;	// closed the loop
				workingLoop.push_back(newVert);
			}
			result.push_back(std::move(workingLoop));
		}

		return result;
	}

	T1(Primitive) std::vector<std::vector<unsigned>> StraightSkeleton<Primitive>::WavefrontAsVertexLoops()
	{
		std::vector<std::pair<unsigned, unsigned>> segmentSoup;
		for (auto&f:_faces)
			for (auto&e:f._edges)
				if (e._type == EdgeType::Wavefront)
					segmentSoup.push_back({e._head, e._tail});
		// We shouldn't need the edges in _unplacedEdges, so long as each edge has been correctly
		// assigned to it's source face
		return AsVertexLoopsOrdered(MakeIteratorRange(segmentSoup));
	}

	template StraightSkeleton<float> CalculateStraightSkeleton<float>(IteratorRange<const Vector2T<float>*> vertices, float maxInset);
	template StraightSkeleton<double> CalculateStraightSkeleton<double>(IteratorRange<const Vector2T<double>*> vertices, double maxInset);
	template StraightSkeleton<int32_t> CalculateStraightSkeleton<int32_t>(IteratorRange<const Vector2T<int32_t>*> vertices, int32_t maxInset);
	template StraightSkeleton<int64_t> CalculateStraightSkeleton<int64_t>(IteratorRange<const Vector2T<int64_t>*> vertices, int64_t maxInset);
	template class StraightSkeleton<float>;
	template class StraightSkeleton<double>;
	template class StraightSkeleton<int32_t>;
	template class StraightSkeleton<int64_t>;

}
