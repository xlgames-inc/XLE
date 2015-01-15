// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Math/Vector.h"
#include <vector>

namespace SceneEngine
{

    class LightingParserContext;

        ////////////////////////////////////////////////////////
    
    class IVolumeDensityFunction
    {
    public:
        typedef std::pair<Float3, Float3> Boundary;
        virtual Boundary    GetBoundary() const = 0;
        virtual float       GetDensity(const Float3& pt) const = 0;
        virtual Float3      GetNormal(const Float3& pt) const = 0;
    };

        ////////////////////////////////////////////////////////

    class DualContourMesh
    {
    public:
        class Vertex    { public: Float3 _pt; Float3 _normal; Vertex(Float3 pt, Float3 normal) : _pt(pt), _normal(normal) {} };
        class Quad      { public: unsigned _verts[4]; };

        std::vector<Vertex>     _vertices;
        std::vector<Quad>       _quads;

        DualContourMesh();
        DualContourMesh(DualContourMesh&& moveFrom);
        DualContourMesh& operator=(DualContourMesh&& moveFrom);
        ~DualContourMesh();
    };

        ////////////////////////////////////////////////////////

    DualContourMesh     DualContourMesh_Build(  unsigned samplingGridDimensions, 
                                                const IVolumeDensityFunction& fn);

        ////////////////////////////////////////////////////////

}

