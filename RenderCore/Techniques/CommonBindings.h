// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Assets/AssetsCore.h"
#include "../../Math/Vector.h"
#include "../../Utility/MemoryUtils.h"

namespace RenderCore { namespace Techniques
{
    namespace ObjectCB
    {
        static const auto LocalTransform = Hash64("LocalTransform");
        static const auto BasicMaterialConstants = Hash64("BasicMaterialConstants");
        static const auto Globals = Hash64("$Globals");
    }

    /// <summary>Technique type binding indicies</summary>
    /// We use a hard coded set of technique indices. This non-ideal in the sense that it limits
    /// the number of different ways we can render things. But it's also important for
    /// performance, since technique lookups can happen very frequently. It's hard to
    /// find a good balance between performance and flexibility for this case.
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
        static const auto StochasticTransparency = 10u;
        static const auto DepthWeightedTransparency = 11u;

        static const auto Max = 12u;
    };

	namespace AttachmentSemantics
	{
		const uint64_t MultisampleDepth = ConstHash64<'Mult', 'isam', 'pleD', 'epth'>::Value;
		const uint64_t GBufferDiffuse	= ConstHash64<'GBuf', 'ferD', 'iffu', 'se'>::Value;
		const uint64_t GBufferNormal	= ConstHash64<'GBuf', 'ferN', 'orma', 'ls'>::Value;
		const uint64_t GBufferParameter = ConstHash64<'GBuf', 'ferP', 'aram', 'eter'>::Value;

		const uint64_t ColorLDR			= ConstHash64<'ldrc', 'olor'>::Value;
		const uint64_t ColorHDR			= ConstHash64<'hdrc', 'olor'>::Value;
		const uint64_t Depth			= ConstHash64<'dept', 'h'>::Value;

		const uint64_t ShadowDepthMap	= ConstHash64<'shad', 'owdm'>::Value;
	}
}}

