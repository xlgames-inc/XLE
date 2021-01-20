// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainShadowOp.h"
#include "../../SceneEngine/TerrainUberSurface.h"
#include "../../Math/Geometry.h"
#include "../../Utility/ParameterBox.h"

namespace ToolsRig
{
    using namespace SceneEngine;

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ShadowingAngleOperator
    {
    public:
        void operator()(Float2 s0, Float2 s1, float edgeAlpha)
        {
                // for performance reasons, we don't do any boundary checking
                // on the input values. The input algorithm must guarantee that
                // we get reasonable results.
            Int2 is0 = Int2(int(s0[0]), int(s0[1])), is1 = Int2(int(s1[0]), int(s1[1]));
            assert(is0[0] >= 0 && is0[0] <= int(_surface->GetWidth()));
            assert(is0[1] >= 0 && is0[1] <= int(_surface->GetHeight()));
            assert(is1[0] >= 0 && is1[0] <= int(_surface->GetWidth()));
            assert(is1[1] >= 0 && is1[1] <= int(_surface->GetHeight()));

            // we need to find the height of the edge at the point we pass through it
            float h0 = _surface->GetValueFast(is0[0], is0[1]);
            float h1 = _surface->GetValueFast(is1[0], is1[1]);
            float finalHeight = LinearInterpolate(h0, h1, edgeAlpha);
            Float2 finalPos = LinearInterpolate(s0, s1, edgeAlpha);
            
                // this defines an angle. We can do "smaller than" comparisons on tanTheta to find the smallest theta
            float distance = Magnitude(finalPos - Truncate(_samplePt)); // * _xyScale;
            assert(distance > 0.f);
            float grad = (finalHeight - _samplePt[2]) / distance;
            _bestResult = BranchlessMax(grad, _bestResult);
        }

        ShadowingAngleOperator(TerrainUberHeightsSurface* surface, Float3 samplePt) { _bestResult = -std::numeric_limits<float>::max(); _surface = surface; _samplePt = samplePt; }

        float _bestResult;
    protected:
        TerrainUberHeightsSurface* _surface;
        Float3 _samplePt;
    };

    float GetInterpolatedValue(TerrainUberHeightsSurface& surface, Float2 pt)
    {
        Float2 floored(XlFloor(pt[0]), XlFloor(pt[1]));
        Float2 ceiled = floored + Float2(1.f, 1.f);
        Float2 alpha = pt - floored;
        float A = surface.GetValue((unsigned)floored[0], (unsigned)floored[1]);
        float B = surface.GetValue((unsigned) ceiled[0], (unsigned)floored[1]);
        float C = surface.GetValue((unsigned)floored[0], (unsigned) ceiled[1]);
        float D = surface.GetValue((unsigned) ceiled[0], (unsigned) ceiled[1]);
        return 
              A * (1.f - alpha[0]) * (1.f - alpha[1])
            + B * (      alpha[0]) * (1.f - alpha[1])
            + C * (1.f - alpha[0]) * (      alpha[1])
            + D * (      alpha[0]) * (      alpha[1])
            ;
    }

    static float CalculateShadowingGrad(
        TerrainUberHeightsSurface& surface, 
        Float2 rayStart, Float2 rayEnd)
    {
            //  Travel forward along the sunDirectionOfMovement and find the shadowing angle.
            //  It's important here that integer coordinates are on corners of the "pixels"
            //      -- ie, not the centers. This will keep the height map correctly aligned
            //  with the shadowing samples
        float sampleHeight = GetInterpolatedValue(surface, rayStart);

            //  Have to keep a border around the edge. Sometimes the interpolation generated 
            //  GridEdgeIterator2 will be just outside of the valid area. To avoid reading
            //  bad memory, we need to avoid the very edge.
        const float border = 1.f;
        if (rayEnd[0] < border) {
            rayEnd = LinearInterpolate(rayStart, rayEnd, (border-rayStart[0]) / (rayEnd[0]-rayStart[0]));
            rayEnd[0] = border;
        } else if (rayEnd[0] > float(surface.GetWidth()-2)) {
            rayEnd = LinearInterpolate(rayStart, rayEnd, (float(surface.GetWidth()-1)-border - rayStart[0]) / (rayEnd[0]-rayStart[0]));
            rayEnd[0] = float(surface.GetWidth()-1)-border;
        }
        
        if (rayEnd[1] < border) {
            rayEnd = LinearInterpolate(rayStart, rayEnd, (border-rayStart[1]) / (rayEnd[1]-rayStart[1]));
            rayEnd[1] = border;
        } else if (rayEnd[1] > float(surface.GetHeight()-1)-border) {
            rayEnd = LinearInterpolate(rayStart, rayEnd, (float(surface.GetHeight()-1)-border - rayStart[1]) / (rayEnd[1]-rayStart[1]));
            rayEnd[1] = float(surface.GetHeight()-1)-border;
        }

        assert(rayStart[0] >= 0.f && rayStart[0] < surface.GetWidth());
        assert(rayStart[1] >= 0.f && rayStart[1] < surface.GetHeight());
        assert(rayEnd[0] >= 0.f && rayEnd[0] < surface.GetWidth());
        assert(rayEnd[1] >= 0.f && rayEnd[1] < surface.GetHeight());

        ShadowingAngleOperator opr(&surface, Expand(rayStart, sampleHeight));
        GridEdgeIterator2(rayStart, rayEnd, opr);
        return opr._bestResult;
    }

    static float CalculateShadowingAngleForSun(
        TerrainUberHeightsSurface& surface, 
        Float2 samplePt, Float2 sunDirectionOfMovement, float searchDistance, float xyScale)
    {
            //  limit the maximum shadow distance (for efficiency while calculating the angles)
            //  As we get further away from the sample point, we're less likely to find the shadow
            //  caster... So just cut off at a given distance (helps performance greatly)
            //  It seems that around 1000 is needed for very long shadows. At 200, shadows get clipped off too soon.

            //  where will this iteration hit the edge of the valid area?
            //  start with a really long line, then clamp it to the valid region
        Float2 fe = samplePt + std::min(searchDistance, float(surface.GetWidth() + surface.GetHeight())) * sunDirectionOfMovement;

        float grad = CalculateShadowingGrad(surface, samplePt, fe);
        grad /= xyScale;
        return XlATan2(1.f, grad);
    }

    void AngleBasedShadowsOperator::Calculate(
        void* dst, Float2 coord, 
        SceneEngine::TerrainUberHeightsSurface& heightsSurface, float xyScale) const
    {
            //
            //      There are some limitations on the way the sun can move.
            //      It must move in a perfect circle arc, and pass through a
            //      point directly above. It must be as if our terrain is near
            //      the equator of the planet. The results won't be exactly physically
            //      correct for areas away from the equator; but maybe that's difficult
            //      for people to notice.
            //
            //      While this limitation is fairly significant, for many situations
            //      it's ok. It's difficult for the player to notice that the sun
            //      follows the same path every time and we still have control over 
            //      the time of sunrise and sunset, etc.

            //      For each point, we want to move both ways along the "sunDirectionOfMovement"
            //      vector. We want to find the angle at which shadowing starts. We can then
            //      assume that the point will always be in shadow when the sun is below that
            //      angle (this applies for the other direction, also).
            //
            //      We'll align the shadowing information perfectly with the height information.
            //      That means the shadowing samples happen on the corners of the quads that
            //      are generated by the height map.
            //
            //      For iterating along the line, we can use a fixed point method like Bresenham's method. 
            //      We want to pass through every height map quad that the line passes through. We'll then
            //      find the two points on the edges of that quad that are pierced by the line.
            //      The simplest way to do this, is to consider the grid as a set of grid lines, and look
            //      for all of the cases where the test line intersects those grid lines.
            //      Note that it's possible that the first shadow could be cast by the diagonal edge on
            //      the triangles for the quad (not an outside edge). This method will miss those cases.
            //      But the difference should be subtle.
            //
            //      We want to write to a real big file... So here's what we'll do.
            //          Lets do 1 line at a time, and write each line to the file in one write operation
            //
            //      If we wanted to improve this, we could twiddle the data in the texture. This would
            //      mean that samples that are close to each other in physical space are more likely to
            //      be close in the file.
            //

        float a0 = CalculateShadowingAngleForSun(
            heightsSurface, coord, -_sunDirectionOfMovement, _searchDistance, xyScale);
        float a1 = CalculateShadowingAngleForSun(
            heightsSurface, coord,  _sunDirectionOfMovement, _searchDistance, xyScale);

            // Both a0 and a1 should be positive. But we'll negate a0 before we use it for a comparison
        assert(a0 > 0.f && a1 > 0.f);

            //
            //      The "expansion constant" helps prevent shadows creaping up on peaks.
            //      Peaks (especially sharp peaks) shouldn't receive shadows until the sun is >90 degrees, or <-90 degrees.
            //      But if we clamp the direction at +-90, shadow will start to the creep
            //      up on the peak when the sun gets near 90 degrees. We want to prevent the
            //      shadow from behaving like this -- which we can do by clamping the angle
            //      beyond 90.
            //
        const float expansionConstant = 1.5f;
        const float conversionConstant = float(0xffff) / (.5f * expansionConstant * float(M_PI));

        *(ShadowSample*)dst = ShadowSample(
            (int16)Clamp(a0 * conversionConstant, 0.f, float(0xffff)),
            (int16)Clamp(a1 * conversionConstant, 0.f, float(0xffff)));
    }

    ImpliedTyping::TypeDesc AngleBasedShadowsOperator::GetOutputFormat() const
    {
        return ImpliedTyping::TypeDesc{
            ImpliedTyping::TypeCat::UInt16, 2};
    }

    void AngleBasedShadowsOperator::FillDefault(void* dst, unsigned count) const
    {
        std::fill(
            (ShadowSample*)dst, ((ShadowSample*)dst) + count,
            ShadowSample(0xffffui16, 0xffffui16));
    }

    const char* AngleBasedShadowsOperator::GetName() const
    {
        return "Generate Terrain Shadows";
    }

    AngleBasedShadowsOperator::AngleBasedShadowsOperator(Float2 sunDirectionOfMovement, float searchDistance)
    {
        _sunDirectionOfMovement = sunDirectionOfMovement;
        _searchDistance = searchDistance;
    }

    AngleBasedShadowsOperator::~AngleBasedShadowsOperator() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    ImpliedTyping::TypeDesc AOOperator::GetOutputFormat() const 
    { 
        return ImpliedTyping::TypeDesc{ImpliedTyping::TypeCat::UInt8}; 
    }

    void AOOperator::FillDefault(void* dst, unsigned count) const
    {
        auto* d = (AoSample*)dst;
        std::fill(d, d + count, (AoSample)~0);
    }

    void AOOperator::Calculate(
        void* dst, Float2 coord, 
        TerrainUberHeightsSurface& heightsSurface, float xyScale) const
    {
        // For the given point, we're going to calculate an ambient occlusion sample
        // We want our sample to be the proportion of the upper hemisphere that is visible to the sky
        // So it is a proportion of a solid angle (actually, a proportion of 2 steradians).
        //      Note -- is is worthwhile to go beyond 2 steradians? Geometry on the size of amount will
        //              be partially occluded in the upper hemisphere. But it could be getting reflected
        //              light from below...?
        //
        // To test this, we need to calculate test rays in 360 degrees around a circle (in a similar
        // way to how the shadows are calculated). That will tell us how exposed the geometry is in that
        // direction. 
        // When shooting out rays, we have 2 options:
        //      1) send rays to the vertices (but the geometry is most extreme on the vertices, so small
        //          details may have a more significant result)
        //      2) send rays to the midpoints between two vertices (this should give a smoother result,
        //          over all?)
        //
        // We will find the vertices in a rectangle around the input point. We need to weight the 
        // contribution of each point according to the angles to adjacent sample points (because we are
        // going around a rectangle, each point will not be equally spaced). The result might be a bit
        // distorted in some cases (particularly when there are more AO samples than height map
        // samples)

        Float2 baseCoord(XlFloor(coord[0]), XlFloor(coord[1]));

        float averageAngle = 0.f;
        for (const auto&p:_testPts) {
            float grad = CalculateShadowingGrad(heightsSurface, coord, baseCoord + Truncate(p));
            grad /= xyScale;
            float angle = XlATan2(1.f, grad);
            averageAngle += angle * p[2];   // weight in z element
        }

        float result = Clamp(averageAngle / (0.5f * gPI), 0.f, 1.f);
        result = std::pow(result, _power);
        *(AoSample*)dst = AoSample(0xff * result);
    }

    const char* AOOperator::GetName() const
    {
        return "Generate Terrain Ambient Occlusion";
    }

    AOOperator::AOOperator(unsigned testRadius, float power)
    {
        _testRadius = testRadius;
        _power = power;

        Float2 minTest = Float2(-float(_testRadius), -float(_testRadius));
        Float2 maxTest = Float2( float(_testRadius),  float(_testRadius));

        for (unsigned c=0; c<_testRadius; ++c)
            _testPts.push_back(Float3(minTest[0] + float(c) + 0.5f, minTest[1], 0.f));
        for (unsigned c=0; c<_testRadius; ++c)
            _testPts.push_back(Float3(maxTest[0], minTest[1] + float(c) + 0.5f, 0.f));
        for (unsigned c=0; c<_testRadius; ++c)
            _testPts.push_back(Float3(minTest[0] + float(_testRadius-c-1) + 0.5f, maxTest[1], 0.f));
        for (unsigned c=0; c<_testRadius; ++c)
            _testPts.push_back(Float3(minTest[0], minTest[1] + float(_testRadius-c-1) + 0.5f, 0.f));

        auto testPtsCount = _testPts.size(); 
        for (size_t p=0; p<testPtsCount; ++p) {
            auto prevPt = Truncate(_testPts[(p+testPtsCount-1)%testPtsCount]);
            auto nextPt = Truncate(_testPts[(p+1)%testPtsCount]);

            auto cosTheta = Dot(Normalize(prevPt), Normalize(nextPt));
            auto theta = XlACos(cosTheta);
            _testPts[p][2] = (.5f * theta) / (2.f * gPI); // this is the weight for this point
        }
    }

    AOOperator::~AOOperator() {}
}
