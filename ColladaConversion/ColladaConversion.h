// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Types.h"
#include "../Core/Exceptions.h"
#include "../Math/Matrix.h"
#include "../Math/Vector.h"
#include "../Utility/Mixins.h"
#include "../RenderCore/Metal/InputLayout.h"

namespace COLLADABU
{
    namespace Math 
    {
        class Matrix4;
        class Vector3;
    }
}

namespace COLLADAFW { class Animation; }

namespace RenderCore { namespace Assets { class RawAnimationCurve; } }

namespace RenderCore { namespace ColladaConversion
{
    unsigned int    FloatBits(float input);
    unsigned int    FloatBits(double input);
    unsigned short  AsFloat16(float input);
    float           Float16AsFloat32(unsigned short input);

    extern bool ImportCameras;

    void AddToBoundingBox(  std::pair<Float3, Float3>& boundingBox,
                            const Float3& localPosition, const Float4x4& localToWorld);
    void AddToBoundingBox(  std::pair<Float3, Float3>& boundingBox,
                            const void* vertexData, size_t vertexStride, size_t vertexCount,
                            const Metal::InputElementDesc& elementDesc, 
                            const Float4x4& localToWorld);
    std::pair<Float3, Float3>   InvalidBoundingBox();

    Metal::InputElementDesc     FindPositionElement(const Metal::InputElementDesc elements[], size_t elementCount);

    Float4x4    AsFloat4x4  (const COLLADABU::Math::Matrix4& matrix);
    Float3      AsFloat3    (const COLLADABU::Math::Vector3& vector);
    Assets::RawAnimationCurve   Convert(const COLLADAFW::Animation& animation);

}}


