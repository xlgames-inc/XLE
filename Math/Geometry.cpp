// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Geometry.h"
#include "Transformations.h"
#include "../Core/Prefix.h"

namespace Math
{

    bool RayVsAABB(const std::pair<Float3, Float3>& worldSpaceRay, const Float4x4& aabbToWorld, const Float3& mins, const Float3& maxs)
    {
            //  Does this ray intersect the aabb? 
            //  transform the ray back into aabb space, and do tests against the edge planes of the bounding box

        auto worldToAabb = Inverse(aabbToWorld);     // maybe not be orthonormal input. This is used for terrain, which has scale on aabbToWorld
        auto ray = std::make_pair(TransformPoint(worldToAabb, worldSpaceRay.first), TransformPoint(worldToAabb, worldSpaceRay.second));

            // if both points are rejected by the same plane, then it's an early out
        unsigned inside = 0;
        for (unsigned c=0; c<3; ++c) {
            if (    (ray.first[c] < mins[c] && ray.second[c] < mins[c])
                ||  (ray.first[c] > maxs[c] && ray.second[c] > maxs[c]))
                return false;

            inside |= unsigned(ray.first[c] >= mins[c] && ray.first[c] <= maxs[c] && ray.second[c] >= mins[c] && ray.second[c] <= maxs[c]) << c;
        }

            // if completely inside, let's consider it an intersection
        if (inside == 7)
            return true;

            //  there's a potential intersection. Find the planes that the ray crosses, and find the intersection 
            //  point. If the intersection point is inside the aabb, then we have an intersection
        for (unsigned c=0; c<3; ++c) {

            {
                float a =  ray.first[c] - mins[c];
                float b = ray.second[c] - mins[c];
                if ((a<0) != (b<0)) {
                    float alpha = a / (a-b);
                    Float3 intersection = LinearInterpolate(ray.first, ray.second, alpha);
                        // don't test element "c", because we might get floating point creep
                    if (    intersection[(c+1)%3] >= mins[(c+1)%3] && intersection[(c+1)%3] <= maxs[(c+1)%3]
                        &&  intersection[(c+2)%3] >= mins[(c+2)%3] && intersection[(c+2)%3] <= maxs[(c+2)%3])
                        return true;
                }
            }

            {
                float a =  ray.first[c] - maxs[c];
                float b = ray.second[c] - maxs[c];
                if ((a<0) != (b<0)) {
                    float alpha = a / (a-b);
                    Float3 intersection = LinearInterpolate(ray.first, ray.second, alpha);
                    if (    intersection[(c+1)%3] >= mins[(c+1)%3] && intersection[(c+1)%3] <= maxs[(c+1)%3]
                        &&  intersection[(c+2)%3] >= mins[(c+2)%3] && intersection[(c+2)%3] <= maxs[(c+2)%3])
                        return true;
                }
            }

        }

        return false;
    }


    std::pair<Float3, Float3> TransformBoundingBox(const Float3x4& transformation, std::pair<Float3, Float3> boundingBox)
    {
        Float3 corners[] = 
        {
            Float3(  boundingBox.first[0], boundingBox.first[1],  boundingBox.first[2] ),
            Float3( boundingBox.second[0], boundingBox.first[1],  boundingBox.first[2] ),
            Float3(  boundingBox.first[0], boundingBox.second[1], boundingBox.first[2] ),
            Float3( boundingBox.second[0], boundingBox.second[1], boundingBox.first[2] ),

            Float3(  boundingBox.first[0], boundingBox.first[1],  boundingBox.second[2] ),
            Float3( boundingBox.second[0], boundingBox.first[1],  boundingBox.second[2] ),
            Float3(  boundingBox.first[0], boundingBox.second[1], boundingBox.second[2] ),
            Float3( boundingBox.second[0], boundingBox.second[1], boundingBox.second[2] )
        };

        for (unsigned c=0; c<dimof(corners); ++c) {
            corners[c] = TransformPoint(transformation, corners[c]);
        }

        Float3 mins(FLT_MAX, FLT_MAX, FLT_MAX), maxs(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        for (unsigned c=0; c<dimof(corners); ++c) {
            mins[0] = std::min(mins[0], corners[c][0]);
            mins[1] = std::min(mins[1], corners[c][1]);
            mins[2] = std::min(mins[2], corners[c][2]);

            maxs[0] = std::max(maxs[0], corners[c][0]);
            maxs[1] = std::max(maxs[1], corners[c][1]);
            maxs[2] = std::max(maxs[2], corners[c][2]);
        }

        return std::make_pair(mins, maxs);
    }

}