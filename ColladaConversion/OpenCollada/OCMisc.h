// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../ConversionCore.h"
#include "../../Math/Matrix.h"
#include "../../Math/Vector.h"
#include "../../RenderCore/Assets/RawAnimationCurve.h"

namespace COLLADABU
{
    namespace Math { class Matrix4; class Vector3; }
}

namespace COLLADAFW { class Animation; class UniqueId; class Image; class SkinControllerData; }

namespace RenderCore { namespace Assets { class RawAnimationCurve; } }

namespace RenderCore { namespace Assets { using AnimationParameterId = uint32; }}

namespace RenderCore { namespace ColladaConversion
{
    class ReferencedTexture;
    class ReferencedMaterial;
    class UnboundSkinController;

    Float4x4    AsFloat4x4  (const COLLADABU::Math::Matrix4& matrix);
    Float3      AsFloat3    (const COLLADABU::Math::Vector3& vector);
    Assets::RawAnimationCurve   Convert(const COLLADAFW::Animation& animation);

    ObjectGuid              Convert(const COLLADAFW::UniqueId& input);
    ReferencedTexture       Convert(const COLLADAFW::Image* image);
    UnboundSkinController   Convert(const COLLADAFW::SkinControllerData* input);

    Assets::AnimationParameterId BuildAnimParameterId(const COLLADAFW::UniqueId& input);
}}

