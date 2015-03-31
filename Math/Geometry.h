// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Vector.h"
#include "Matrix.h"
#include <utility>

namespace XLEMath
{
    float   SignedDistance(const Float3& pt, const Float4& plane);
    float   RayVsPlane(const Float3& rayStart, const Float3& rayEnd, const Float4& plane);

        /// <summary>Tests a ray against an AABB</summary>
        /// <param name="worldSpaceRay">The ray in world space. Start and end position</param>
    bool    RayVsAABB(const std::pair<Float3, Float3>& worldSpaceRay, const Float4x4& aabbToWorld, const Float3& mins, const Float3& maxs);
    bool    RayVsAABB(const std::pair<Float3, Float3>& localSpaceRay, const Float3& mins, const Float3& maxs);

    std::pair<Float3, Float3> TransformBoundingBox(const Float3x4& transformation, std::pair<Float3, Float3> boundingBox);

    /// <summary>Conversion from cartesian to spherical polar coordinates</summary>
    /// Returns a 3 component vector with:
    ///     [0] = theta
    ///     [1] = phi
    ///     [2] = distance
    ///
    /// See description in wikipedia:
    /// http://en.wikipedia.org/wiki/Spherical_coordinate_system
    Float3 CartesianToSpherical(Float3 direction);
    Float3 SphericalToCartesian(Float3 spherical);

        ////////////////////////////////////////////////////////////////////////////////////////////////
            //      I N L I N E   I M P L E M E N T A T I O N S
        ////////////////////////////////////////////////////////////////////////////////////////////////

    inline float SignedDistance(const Float3& pt, const Float4& plane)
    {
        return Dot(pt, Truncate(plane)) + plane[3];
    }

    inline float RayVsPlane(const Float3& rayStart, const Float3& rayEnd, const Float4& plane)
    {
        float a = SignedDistance(rayStart, plane);
        float b = SignedDistance(rayEnd, plane);
        return a / (a - b);
    }

    inline int XlSign(int input)
    {
        // std::signbit would be useful here!
        return (input<0)?-1:1;
    }

        /// <summary>Iterator through a grid, finding that edges that intersect with a line segment</summary>
        /// The callback "opr" will be called for each grid edge that intersects with given line segment.
        /// Here, the grid is assumed to be made up of 1x1 elements on integer boundaries.
    template<typename Operator>
        void GridEdgeIterator(Float2 start, Float2 end, Operator& opr)
        {
            Int2 s = Int2(int(XlFloor(start[0])), int(XlFloor(start[1])));
            Int2 e = Int2(int(XlFloor(end[0])), int(XlFloor(end[1])));

		    int w = e[0] - s[0];
		    int h = e[1] - s[1];

            int ystep = XlSign(h); 
            h = XlAbs(h); 
            int xstep = XlSign(w); 
            w = XlAbs(w); 
            int ddy = 2 * h;  // We may not need to double this (because we're starting from the corner of the pixel)
            int ddx = 2 * w; 

            int errorprev = 0, error = 0; // (start from corner. we don't want to start in the middle of the grid element)
            int x = s[0], y = s[1];
            if (ddx >= ddy) {
                for (int i=0; i<w; ++i) {
                    x += xstep; 
                    error += ddy; 
                    
                    Int2 e0, e1;
                    float edgeAlpha;

                    if (error >= ddx) {

                        y += ystep; 
                        error -= ddx; 

                            //  The cases for what happens here. Each case defines different edges
                            //  we need to check
                        if (error != 0) {
                            e0 = Int2(x, y); e1 = Int2(x, y+ystep);
                            edgeAlpha = error / float(ddx);

                            Int2 e0b(x-xstep, y), e1b(x, y);
                            int tri0 = ddx - errorprev;
                            int tri1 = error;
                            opr(e0b, e1b, tri0 / float(tri0+tri1));
                        } else {
                                // passes directly though the corner. Easiest case.
                            e0 = e1 = Int2(x, y);
                            edgeAlpha = 0.f;
                        }

                    } else {
                            // simple -- y isn't changing, just moving to the next "x" grid
                        e0 = Int2(x, y); e1 = Int2(x, y+ystep);
                        edgeAlpha = error / float(ddx);
                    }

                    opr(e0, e1, edgeAlpha);
                    errorprev = error; 
                }
            } else {
                for (int i=0; i<h; ++i) {
                    y += ystep;
                    error += ddx;

                    Int2 e0, e1;
                    float edgeAlpha;

                    if (error >= ddy) {

                        x += xstep; 
                        error -= ddy; 

                            //  The cases for what happens here. Each case defines different edges
                            //  we need to check
                        if (error != 0) {
                            e0 = Int2(x, y); e1 = Int2(x+xstep, y);
                            edgeAlpha = error / float(ddy);

                            Int2 e0b(x, y-ystep), e1b(x, y);
                            int tri0 = ddy - errorprev;
                            int tri1 = error;
                            opr(e0b, e1b, tri0 / float(tri0+tri1));
                        } else {
                                // passes directly though the corner. Easiest case.
                            e0 = e1 = Int2(x, y);
                            edgeAlpha = 0.f;
                        }

                    } else {
                            // simple -- y isn't changing, just moving to the next "x" grid
                        e0 = Int2(x, y); e1 = Int2(x+xstep, y);
                        edgeAlpha = error / float(ddy);
                    }

                    opr(e0, e1, edgeAlpha);
                    errorprev = error; 
                }
            }
        }
}


