// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Math/Vector.h"

namespace RenderCore { namespace Techniques
{
    namespace ObjectCB
    {
        static const auto LocalTransform = Hash64("LocalTransform");
        static const auto BasicMaterialConstants = Hash64("BasicMaterialConstants");
        static const auto Globals = Hash64("$Globals");
    }

    namespace TechniqueIndex
    {
        static const auto Forward       = 0u;
        static const auto DepthOnly     = 1u;
        static const auto Deferred      = 2u;
        static const auto ShadowGen     = 3u;
        static const auto OrderIndependentTransparency = 4u;
        static const auto PrepareVegetationSpawn = 5u;
        static const auto RayTest       = 6u;
        static const auto VisNormals    = 7u;
        static const auto VisWireframe  = 8u;
        static const auto WriteTriangleIndex = 9u;
    }
}}

