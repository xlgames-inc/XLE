// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DualContour.h"
#include "../Math/Matrix.h"
#include "../Math/Transformations.h"
#include "../Utility/PtrUtils.h"

#pragma warning(disable:4714)
#pragma push_macro("new")
#undef new
#include <Eigen/Dense>
#pragma pop_macro("new")

#pragma warning(disable:4127)       // conditional expression is constant

namespace SceneEngine
{
    class GridElement
    {
    public:
            //  Store the grid element values that we will
            //  need to combine together adjacent elements during
            //  the simplification
        Float3x3    _Ahat;      // (upper triangular -- maybe we don't have to store all of it?
        Float3      _Bhat;
        float       _r;
        Float3      _massPointAccum;
        unsigned    _massPointCount;

        GridElement() 
            : _Ahat(Zero<Float3x3>()), _Bhat(Zero<Float3>()), _r(0.f)
            , _massPointAccum(Zero<Float3>()), _massPointCount(0) {}
    };

    class EdgeIntersection
    {
    public:
        Float3 _pt;
        Float3 _normal;

        EdgeIntersection(const Float3& intersectionPt, const Float3& intersectionNormal)
            : _pt(intersectionPt), _normal(intersectionNormal) {}
    };

    static EdgeIntersection  TestEdge(  const Float3& e0, const Float3& e1, float d0, float d1, 
                                        const IVolumeDensityFunction& fn)
    {
            //  Test the edge between these points, and attempt to find the point where
            //  the surface passes through. 
            //
            //  The caller should have filtered out edges
            //  that don't pass through the surface.
        assert((d0 < 0.f) != (d1 < 0.f));

            //      It might be a good idea to further improve the
            //      result by taking a few steps to try to get to the
            //      smallest density value. note that a very strange
            //      density function might behave very non-linearly,
            //      and so produce strange results when trying to
            //      find the intersection (particularly if there are
            //      really multiple intersections).

        float x0 = 0.f, x1 = 1.f;
        float x = 1.f, d = FLT_MAX;
        unsigned maxImprovementSteps = 4;
        for (unsigned c=0; ; ++c) {
            float prevD = d; float prevX = x;
            x = LinearInterpolate(x0, x1, -d0 / (d1 - d0));
            d = fn.GetDensity(LinearInterpolate(e0, e1, x));

                // along noisy edges we could end up getting a worse result after a step
                //  In these cases, just give up at the last reasonable result
            if (XlAbs(d) > XlAbs(prevD)) {
                x = prevX;
                break;
            }

            if (d < 1e-6f) break;   // if we get close enough, just stop
            if ((c+1)>=maxImprovementSteps) break;

                //  We're going to attempt another improvement.
                //  Divide the search area again, depending on where
                //  the origin falls
            if ((d < 0.f) != (d1 < 0.f)) {
                x0 = x;
                d0 = d;
            } else {
                x1 = x;
                d1 = d;
            }
        }

        Float3 bestIntersection = LinearInterpolate(e0, e1, x);
        Float3 normal = fn.GetNormal(bestIntersection);     // note -- we might need to tell the function the sampling density
        return EdgeIntersection(bestIntersection, normal);
    }

    static void RotateToUpperTriangle(Eigen::Matrix<float,5,4>& A)
    {
        // Eigen::ColPivHouseholderQR<Eigen::Matrix<float,5,4>> qr(A);
        // A = qr.matrixR().triangularView<Eigen::Upper>();
        Eigen::HouseholderQR<Eigen::Matrix<float,5,4>> qr2(A);
        A = qr2.matrixQR().triangularView<Eigen::Upper>();
    }

    static void MergeInEdgeIntersection(GridElement& gridElement, const EdgeIntersection& intersection, const Float3& gridElementCenter)
    {
            //  The edge intersections defines the a plane through the grid element.
            //  The matrices in the grid element define a set of linear equations for
            //  calculating the best point for the given planes. We want to merge
            //  the plane from this new intersection into the equations already in
            //  the grid element.
            //  See the original dual contour paper for a description of this.
            //  Basically, we're using QR decomposition to define an orthogonal
            //  matrix and a upper triangular matrix. The "Q" orthogonal matrix
            //  gets factored out when solving the linear equations. It leaves only
            //  the "R" upper triangular matrix. 
            //  The original paper describes using "Given's rotations" to perform
            //  the QR decomposition. But I'm using the Householder method, here
            //  -- which is very similar, but requires fewer operations.
            //  Also note that everything is translated to end up relative
            //  to the grid center. This is just to guarantee small & simple numbers.

        typedef Eigen::Matrix<float,5,4> Float5x4;
        Float5x4 mat = Float5x4::Zero();

            // fill in the existing values from the grid element
        mat(0, 0) = gridElement._Ahat(0, 0);
        mat(0, 1) = gridElement._Ahat(0, 1);
        mat(0, 2) = gridElement._Ahat(0, 2);
        mat(1, 0) = gridElement._Ahat(1, 0);
        mat(1, 1) = gridElement._Ahat(1, 1);
        mat(1, 2) = gridElement._Ahat(1, 2);
        mat(2, 0) = gridElement._Ahat(2, 0);
        mat(2, 1) = gridElement._Ahat(2, 1);
        mat(2, 2) = gridElement._Ahat(2, 2);
        mat(0, 3) = gridElement._Bhat[0];
        mat(1, 3) = gridElement._Bhat[1];
        mat(2, 3) = gridElement._Bhat[2];
        mat(3, 3) = gridElement._r;

        mat(4, 0) = intersection._normal[0];        // normal can be positive or negative direction; we'll still get the same plane equation
        mat(4, 1) = intersection._normal[1];
        mat(4, 2) = intersection._normal[2];
        mat(4, 3) = Dot(intersection._pt - gridElementCenter, intersection._normal);
        RotateToUpperTriangle(mat);

            // extract new grid element values
        gridElement._Ahat(0, 0) = mat(0, 0);
        gridElement._Ahat(0, 1) = mat(0, 1);
        gridElement._Ahat(0, 2) = mat(0, 2);
        gridElement._Ahat(1, 0) = mat(1, 0);
        gridElement._Ahat(1, 1) = mat(1, 1);
        gridElement._Ahat(1, 2) = mat(1, 2);
        gridElement._Ahat(2, 0) = mat(2, 0);
        gridElement._Ahat(2, 1) = mat(2, 1);
        gridElement._Ahat(2, 2) = mat(2, 2);
        gridElement._Bhat[0] = mat(0, 3);
        gridElement._Bhat[1] = mat(1, 3);
        gridElement._Bhat[2] = mat(2, 3);
        gridElement._r = mat(3, 3);

        gridElement._massPointAccum += intersection._pt - gridElementCenter;
        ++gridElement._massPointCount;
    }

    static Float3 CalculateCellPoint(const GridElement& gridElement, const Float3& gridElementSize)
    {
        Float3 massPoint = gridElement._massPointAccum / float(gridElement._massPointCount);

        Eigen::Matrix<float,3,1> massPointVec;
        massPointVec(0,0) = massPoint[0];
        massPointVec(1,0) = massPoint[1];
        massPointVec(2,0) = 0.f;

        Eigen::Matrix<float,3,1> x;

        Eigen::Matrix<float,3,3> Ahat;
        Eigen::Matrix<float,3,1> Bhat;
        Ahat(0,0) = gridElement._Ahat(0,0);
        Ahat(0,1) = gridElement._Ahat(0,1);
        Ahat(0,2) = gridElement._Ahat(0,2);
        Ahat(1,0) = gridElement._Ahat(1,0);
        Ahat(1,1) = gridElement._Ahat(1,1);
        Ahat(1,2) = gridElement._Ahat(1,2);
        Ahat(2,0) = gridElement._Ahat(2,0);
        Ahat(2,1) = gridElement._Ahat(2,1);
        Ahat(2,2) = gridElement._Ahat(2,2);

        Bhat(0, 0) = gridElement._Bhat[0];
        Bhat(1, 0) = gridElement._Bhat[1];
        Bhat(2, 0) = gridElement._Bhat[2];

            //  The original dual contour multiplies through with the transpose of A. This might
            //  reduce the work when calculating the SVD, but the Eigen library doesn't seem to
            //  support this optimisation, however. So I'm not sure if we need to multiply
            //  through with AHatTranspose here.
        const bool useTranspose = true;
        if (useTranspose) {
            auto AhatTranspose = Ahat.transpose();
            Eigen::JacobiSVD<Eigen::Matrix<float, 3, 3>> svd(AhatTranspose * Ahat, Eigen::ComputeFullU | Eigen::ComputeFullV);
            svd.solve(AhatTranspose * Bhat - AhatTranspose * Ahat * massPointVec).evalTo(x);
        } else {
                //  note that if we know the mass point when we're calculating Ahat, we can probably
                //  just take into account the mass point then. However, if we're using a marching
                //  cubes-like algorithm to move through the data field, and so visiting each cube
                //  element multiple times, we might not be able to calculate the mass point until 
                //  after AHat has been fully built. But we can compensate during the solution, 
                //  as so ---
            Eigen::JacobiSVD<Eigen::Matrix<float, 3, 3>> svd(Ahat, Eigen::ComputeFullU | Eigen::ComputeFullV);
            svd.solve(Bhat - Ahat * massPointVec).evalTo(x);
        }

        auto result = Float3(x(0, 0) + massPoint[0], x(1, 0) + massPoint[1], x(2, 0) + massPoint[2]);

        static bool preventBadResults = false;
        if (preventBadResults) {
            Float3 halfSize = 0.5f * gridElementSize;
            if (result[0] < -halfSize[0] || result[0] > halfSize[0] ||  
                result[1] < -halfSize[1] || result[1] > halfSize[1] ||
                result[2] < -halfSize[2] || result[2] > halfSize[2]) {
                return massPoint;       // got a poor result from the QEF test -- too much curvature in a small area.
            }
        }

        return result;
    }

    static void CheckWindingOrder(DualContourMesh::Quad& q, const std::vector<DualContourMesh::Vertex>& vertices)
    {
            //  Check the winding order of the given quad, and correct if necessary
            //  There are various ways we could attempt to find the correct winding order. 
            //  But they all have problems. 
            //
            //  It's possible that that input quad could have a twist in it. We also have to
            //  be careful about input quads that are near-degenerate.
            //
            //  We want to avoid more queries of the density field to generate more normals. 
            //  So let's use the normals we already have access to. We can take some kind of 
            //  weighted mean of the vertex normals and use this as an approximation of the 
            //  face normal, Then we can test the vertex winding order to see if it agrees 
            //  with the face normal we've picked.
            //
            //  How do we weight the normals? We could use the interior angle of the corner.

            //  The vertices in our quads are arranged in a "Z" pattern... 
            //  We can't just +1 or -1 to get "prev" and "next". Use the following table:
        unsigned prevVertices[] = { 2, 0, 3, 1 };       
        unsigned nextVertices[] = { 1, 3, 0, 2 };

        const bool useWeightedMean = false;
        if (constant_expression<useWeightedMean>::result()) {

            Float3 faceNormal(0.f, 0.f, 0.f);
            for (unsigned c=0; c<4; ++c) {
                unsigned prevVertex = q._verts[prevVertices[c]];
                unsigned thisVertex = q._verts[c];
                unsigned nextVertex = q._verts[nextVertices[c]];
                float dot = Dot(
                    vertices[prevVertex]._pt - vertices[thisVertex]._pt,
                    vertices[nextVertex]._pt - vertices[thisVertex]._pt);
                float angle = XlACos(dot);  // because it's a quad, angle must be less than 180. An angle of 180 just gives a straight line
                assert(angle < gPI);
                faceNormal += angle * vertices[q._verts[c]]._normal;
            }
            faceNormal = Normalize(faceNormal);

                //  We can use the cross product to check if the winding is going
                //  in the correct direction. The cross product gives us directional
                //  information about the 2 input vectors. We can compare the result
                //  of the cross product to the direction of the face normal to see
                //  if the winding order is correct for this normal.
            unsigned correctCount = 0;
            for (unsigned c=0; c<4; ++c) {
                unsigned prevVertex = q._verts[prevVertices[c]];
                unsigned thisVertex = q._verts[c];
                unsigned nextVertex = q._verts[nextVertices[c]];

                auto crs = Cross(   
                    vertices[prevVertex]._pt - vertices[thisVertex]._pt, 
                    vertices[nextVertex]._pt - vertices[thisVertex]._pt);
                bool correct = Dot(faceNormal, crs) <= 0.f;
                correctCount += unsigned(correct);
            }

            if (correctCount < 2) {
                    //  Some tolerance if only one or two vertices are a problem.
                    //  But, otherwise, we need to reverse the order. Quads verts are
                    //  in a "Z" pattern... so just swap 1 & 2
                std::swap(q._verts[1], q._verts[2]);
            }

        } else {

            unsigned correctCount = 0;
            for (unsigned c=0; c<4; ++c) {
                unsigned prevVertex = q._verts[prevVertices[c]];
                unsigned thisVertex = q._verts[c];
                unsigned nextVertex = q._verts[nextVertices[c]];

                auto testDirection = Cross(
                    vertices[prevVertex]._pt - vertices[thisVertex]._pt, 
                    vertices[nextVertex]._pt - vertices[thisVertex]._pt);
                for (unsigned v=0; v<4; ++v) {
                    bool correct = Dot(vertices[q._verts[v]]._normal, testDirection) <= 0.f;
                    correctCount += unsigned(correct);
                }
            }

                // if we're correct less than half of the time, then flip
            if (correctCount < 4*4/2) {
                std::swap(q._verts[1], q._verts[2]);
            }

        }
        
    }

    DualContourMesh     DualContourMesh_Build(  unsigned samplingGridDimensions, 
                                                const IVolumeDensityFunction& fn)
    {
            //  Build a mesh of triangles from the given input function
            //      (using dual contouring method)
            //
            //  First we'll build a grid containing information for each
            //  voxel. Then we'll go through a calculate the QEF's at
            //  each grid point -- that will give us enough information
            //  to generate the triangles needed. Note that the algorithm
            //  should naturally build quads most of the time. They'll need
            //  to be split up into triangles.
            //
            //  Ideally, we would also do simplification before we calculate
            //  the QEF's and generate the triangles. But currently, no
            //  simplification.
        auto boundary = fn.GetBoundary();
        auto gridElements = std::make_unique<GridElement[]>(
            samplingGridDimensions*samplingGridDimensions*samplingGridDimensions);

            //  For each grid element, let's fill it in with the values from the
            //  density function. Note that we could reduce the work here slightly
            //  by finding a point within the grid that lies on the surface, and
            //  then marching along the surface, into each new grid that takes us.
            //
            //  The current method will create many redundant tests of the volume
            //  function -- because most grids are probably not on the surface of the 
            //  volume.
            //
            //  Let's find and test each edge. When we find a edge that crosses the 
            //  boundary, we can merge that into the QEF's for that adjacent grid
            //  elements.
            //
            //  It's a good idea to calculate the density results at each corner first
            //  This will help reduce the number of times we need to call the
            //  GetDensity() function.

        Float3x4 gridToSampleSpace = Zero<Float3x4>();
        gridToSampleSpace(0,0) = (boundary.second[0] - boundary.first[0]) / float(samplingGridDimensions);
        gridToSampleSpace(1,1) = (boundary.second[1] - boundary.first[1]) / float(samplingGridDimensions);
        gridToSampleSpace(2,2) = (boundary.second[2] - boundary.first[2]) / float(samplingGridDimensions);
        gridToSampleSpace(0,3) = boundary.first[0];
        gridToSampleSpace(1,3) = boundary.first[1];
        gridToSampleSpace(2,3) = boundary.first[2];
        
        auto densityResults = std::make_unique<float[]>(
            (samplingGridDimensions+1)*(samplingGridDimensions+1)*(samplingGridDimensions+1));
        for (int z=0; z<int(samplingGridDimensions+1); ++z)
            for (int y=0; y<int(samplingGridDimensions+1); ++y)
                for (int x=0; x<int(samplingGridDimensions+1); ++x) {
                    Float3 p0 = TransformPoint(gridToSampleSpace, Float3(float(x), float(y), float(z)));
                    densityResults[(z * (samplingGridDimensions+1) + y) * (samplingGridDimensions+1) + x] = fn.GetDensity(p0);
                }

            // note --  The order of the cell offsets here is important, because it 
            //          determines the order of the vertices in the quad. We 
        Int3 cellOffsetsX[] = { Int3(0, 0, 0), Int3(0, -1, 0), Int3(0, 0, -1), Int3(0, -1, -1) };
        Int3 cellOffsetsY[] = { Int3(0, 0, 0), Int3(-1, 0, 0), Int3(0, 0, -1), Int3(-1, 0, -1) };
        Int3 cellOffsetsZ[] = { Int3(0, 0, 0), Int3(-1, 0, 0), Int3(0, -1, 0), Int3(-1, -1, 0) };

        for (int z=0; z<int(samplingGridDimensions); ++z) {
            for (int y=0; y<int(samplingGridDimensions); ++y) {
                for (int x=0; x<int(samplingGridDimensions); ++x) {

                        //  For each grid element, we're going to test 3 edges.
                        //  we'll add the effect of those edges to adjacent grids
                        //  as well. This means each edges gets tested once.
                        //  However, some edges on the extreme positive boundary
                        //  of the sampling area will never be tested. We'll assume 
                        //  that the function doesn't go through these boundary edges.

                    Float3 p0 = TransformPoint(gridToSampleSpace, Float3(float(x), float(y), float(z)));
                    Float3 p1 = TransformPoint(gridToSampleSpace, Float3(float(x+1), float(y), float(z)));
                    Float3 p2 = TransformPoint(gridToSampleSpace, Float3(float(x), float(y+1), float(z)));
                    Float3 p3 = TransformPoint(gridToSampleSpace, Float3(float(x), float(y), float(z+1)));

                    float d0 = densityResults[(z * (samplingGridDimensions+1) + y) * (samplingGridDimensions+1) + x];
                    float d1 = densityResults[(z * (samplingGridDimensions+1) + y) * (samplingGridDimensions+1) + x + 1];
                    float d2 = densityResults[(z * (samplingGridDimensions+1) + y + 1) * (samplingGridDimensions+1) + x];
                    float d3 = densityResults[((z + 1) * (samplingGridDimensions+1) + y) * (samplingGridDimensions+1) + x];

                        //  Look for edges that contain intersections with the surface, and then merge 
                        //  those edge into all of the grid cells that contain them.
                        //  Note that TestEdge() will do extra calls to GetDensity to improve the 
                        //  intersection point.
                    
                    if ((d0 < 0.f) != (d1 < 0.f)) {
                        auto intersection = TestEdge(p0, p1, d0, d1, fn);
                        for (unsigned c=0; c<dimof(cellOffsetsX); ++c) {
                            Int3 g(x + cellOffsetsX[c][0], y + cellOffsetsX[c][1], z + cellOffsetsX[c][2]);
                            if (g[0] >= 0 && g[1] >= 0 && g[2] >= 0) {
                                const auto cellCenter = Float3(
                                    LinearInterpolate(boundary.first[0], boundary.second[0], (float(g[0]) + .5f) / float(samplingGridDimensions)),
                                    LinearInterpolate(boundary.first[1], boundary.second[1], (float(g[1]) + .5f) / float(samplingGridDimensions)),
                                    LinearInterpolate(boundary.first[2], boundary.second[2], (float(g[2]) + .5f) / float(samplingGridDimensions)));

                                MergeInEdgeIntersection(
                                    gridElements[(g[2] * samplingGridDimensions + g[1]) * samplingGridDimensions + g[0]],
                                    intersection, cellCenter);
                            }
                        }
                    }

                    if ((d0 < 0.f) != (d2 < 0.f)) {
                        auto intersection = TestEdge(p0, p2, d0, d2, fn);
                        for (unsigned c=0; c<dimof(cellOffsetsY); ++c) {
                            Int3 g(x + cellOffsetsY[c][0], y + cellOffsetsY[c][1], z + cellOffsetsY[c][2]);
                            if (g[0] >= 0 && g[1] >= 0 && g[2] >= 0) {
                                const auto cellCenter = Float3(
                                    LinearInterpolate(boundary.first[0], boundary.second[0], (float(g[0]) + .5f) / float(samplingGridDimensions)),
                                    LinearInterpolate(boundary.first[1], boundary.second[1], (float(g[1]) + .5f) / float(samplingGridDimensions)),
                                    LinearInterpolate(boundary.first[2], boundary.second[2], (float(g[2]) + .5f) / float(samplingGridDimensions)));

                                MergeInEdgeIntersection(
                                    gridElements[(g[2] * samplingGridDimensions + g[1]) * samplingGridDimensions + g[0]],
                                    intersection, cellCenter);
                            }
                        }
                    }

                    if ((d0 < 0.f) != (d3 < 0.f)) {
                        auto intersection = TestEdge(p0, p3, d0, d3, fn);
                        for (unsigned c=0; c<dimof(cellOffsetsZ); ++c) {
                            Int3 g(x + cellOffsetsZ[c][0], y + cellOffsetsZ[c][1], z + cellOffsetsZ[c][2]);
                            if (g[0] >= 0 && g[1] >= 0 && g[2] >= 0) {
                                const auto cellCenter = Float3(
                                    LinearInterpolate(boundary.first[0], boundary.second[0], (float(g[0]) + .5f) / float(samplingGridDimensions)),
                                    LinearInterpolate(boundary.first[1], boundary.second[1], (float(g[1]) + .5f) / float(samplingGridDimensions)),
                                    LinearInterpolate(boundary.first[2], boundary.second[2], (float(g[2]) + .5f) / float(samplingGridDimensions)));

                                MergeInEdgeIntersection(
                                    gridElements[(g[2] * samplingGridDimensions + g[1]) * samplingGridDimensions + g[0]],
                                    intersection, cellCenter);
                            }
                        }
                    }

                }
            }
        }

            //  Now, we've calculated the error functions for all of the grid elements.
            //  For each grid element, we can calculate the appropriate point for that
            //  element. Let's make sure we do this only one per grid element (because
            //  typically each vertex will be used in multiple quads.

        const auto cellSize = Float3(
            (boundary.second[0] - boundary.first[0]) / float(samplingGridDimensions),
            (boundary.second[1] - boundary.first[1]) / float(samplingGridDimensions),
            (boundary.second[2] - boundary.first[2]) / float(samplingGridDimensions));

        std::vector<DualContourMesh::Vertex> vertices;
        auto vertexIndices = std::make_unique<unsigned[]>(
            samplingGridDimensions*samplingGridDimensions*samplingGridDimensions);
        for (int z=0; z<int(samplingGridDimensions); ++z) {
            for (int y=0; y<int(samplingGridDimensions); ++y) {
                for (int x=0; x<int(samplingGridDimensions); ++x) {
                    auto index = (z * samplingGridDimensions + y) * samplingGridDimensions + x;
                    const auto& g = gridElements[index];
                    if (!g._massPointCount) {
                        vertexIndices[index] = 0xffffffff;
                        continue;
                    }

                    const auto cellCenter = Float3(
                        LinearInterpolate(boundary.first[0], boundary.second[0], (float(x) + .5f) / float(samplingGridDimensions)),
                        LinearInterpolate(boundary.first[1], boundary.second[1], (float(y) + .5f) / float(samplingGridDimensions)),
                        LinearInterpolate(boundary.first[2], boundary.second[2], (float(z) + .5f) / float(samplingGridDimensions)));
                    auto pt = CalculateCellPoint(g, cellSize) + cellCenter;

                        //  We need the normal at this location, also.
                        //  We've lost the locations of the edge intersections -- so we can't
                        //  just add together the normals from them. However. We can 
                        //  query the density field again to get the normal at this location.
                    auto normal = fn.GetNormal(pt);
                    vertices.push_back(DualContourMesh::Vertex(pt, normal));
                    vertexIndices[index] = unsigned(vertices.size()-1);
                }
            }
        }

            //  We just need to calculate the triangles. 
            //  For each edge with an intersection, we want to create a quad.
            //  we start at one here, because the edge cells have nothing to join
            //  on to.

        std::vector<DualContourMesh::Quad> quads;
        for (int z=1; z<int(samplingGridDimensions); ++z) {
            for (int y=1; y<int(samplingGridDimensions); ++y) {
                for (int x=1; x<int(samplingGridDimensions); ++x) {

                    float d0 = densityResults[(z * (samplingGridDimensions+1) + y) * (samplingGridDimensions+1) + x];
                    float d1 = densityResults[(z * (samplingGridDimensions+1) + y) * (samplingGridDimensions+1) + x + 1];
                    float d2 = densityResults[(z * (samplingGridDimensions+1) + y + 1) * (samplingGridDimensions+1) + x];
                    float d3 = densityResults[((z + 1) * (samplingGridDimensions+1) + y) * (samplingGridDimensions+1) + x];

                        //  If the edge has a intersection point. We want to create a 
                        //  quad by joining together all of the cells that use this edge.
                    if ((d0 < 0.f) != (d1 < 0.f)) {
                        DualContourMesh::Quad q;
                        for (unsigned c=0; c<4; ++c) {
                            Int3 g(x + cellOffsetsX[c][0], y + cellOffsetsX[c][1], z + cellOffsetsX[c][2]);
                            auto index = (g[2] * samplingGridDimensions + g[1]) * samplingGridDimensions + g[0];
                            q._verts[c] = vertexIndices[index];
                            assert(q._verts[c] < vertices.size());
                        }
                        CheckWindingOrder(q, vertices);
                        quads.push_back(q);
                    }

                    if ((d0 < 0.f) != (d2 < 0.f)) {
                        DualContourMesh::Quad q;
                        for (unsigned c=0; c<4; ++c) {
                            Int3 g(x + cellOffsetsY[c][0], y + cellOffsetsY[c][1], z + cellOffsetsY[c][2]);
                            auto index = (g[2] * samplingGridDimensions + g[1]) * samplingGridDimensions + g[0];
                            q._verts[c] = vertexIndices[index];
                            assert(q._verts[c] < vertices.size());
                        }
                        CheckWindingOrder(q, vertices);
                        quads.push_back(q);
                    }

                    if ((d0 < 0.f) != (d3 < 0.f)) {
                        DualContourMesh::Quad q;
                        for (unsigned c=0; c<4; ++c) {
                            Int3 g(x + cellOffsetsZ[c][0], y + cellOffsetsZ[c][1], z + cellOffsetsZ[c][2]);
                            auto index = (g[2] * samplingGridDimensions + g[1]) * samplingGridDimensions + g[0];
                            q._verts[c] = vertexIndices[index];
                            assert(q._verts[c] < vertices.size());
                        }
                        CheckWindingOrder(q, vertices);
                        quads.push_back(q);
                    }

                }
            }
        }

        DualContourMesh mesh;
        mesh._vertices = std::move(vertices);
        mesh._quads = std::move(quads);
        return mesh;
    }



    DualContourMesh::DualContourMesh() {}
    DualContourMesh::DualContourMesh(DualContourMesh&& moveFrom)
    : _vertices(std::move(moveFrom._vertices))
    , _quads(std::move(moveFrom._quads))
    {}
    DualContourMesh& DualContourMesh::operator=(DualContourMesh&& moveFrom)
    {
        _vertices = std::move(moveFrom._vertices);
        _quads = std::move(moveFrom._quads);
        return *this;
    }
    DualContourMesh::~DualContourMesh() {}

}

