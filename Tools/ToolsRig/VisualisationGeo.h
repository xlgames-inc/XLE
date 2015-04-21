// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderCore/Metal/Forward.h"
#include "../../Math/Vector.h"
#include <vector>

namespace ToolsRig
{
    namespace Internal
    {
        #pragma pack(push)
        #pragma pack(1)
        class Vertex2D
        {
        public:
            Float2      _position;
            Float2      _texCoord;
        };

        class Vertex3D
        {
        public:
            Float3      _position;
            Float3      _normal;
            Float2      _texCoord;
            Float3      _tangent;
            Float3      _bitangent;
        };
        #pragma pack(pop)
    }

    extern std::pair<const RenderCore::Metal::InputElementDesc*, size_t> Vertex2D_InputLayout;
    extern std::pair<const RenderCore::Metal::InputElementDesc*, size_t> Vertex3D_InputLayout;

    std::vector<Internal::Vertex3D>     BuildGeodesicSphere(int detail = 4);
    std::vector<Internal::Vertex3D>     BuildCube();
}

