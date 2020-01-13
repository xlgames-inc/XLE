// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "StateDesc.h"
#include "../Utility/MemoryUtils.h"
#include <cassert>

namespace RenderCore
{
	uint64_t DepthStencilDesc::Hash() const
    {
        assert((unsigned(_depthTest) & ~0xfu) == 0);
        assert((unsigned(_stencilReadMask) & ~0xffu) == 0);
        assert((unsigned(_stencilWriteMask) & ~0xffu) == 0);
        assert((unsigned(_stencilReference) & ~0xffu) == 0);
        assert((unsigned(_frontFaceStencil._passOp) & ~0xfu) == 0);
        assert((unsigned(_frontFaceStencil._failOp) & ~0xfu) == 0);
        assert((unsigned(_frontFaceStencil._depthFailOp) & ~0xfu) == 0);
        assert((unsigned(_frontFaceStencil._comparisonOp) & ~0xfu) == 0);
        assert((unsigned(_backFaceStencil._passOp) & ~0xfu) == 0);
        assert((unsigned(_backFaceStencil._failOp) & ~0xfu) == 0);
        assert((unsigned(_backFaceStencil._depthFailOp) & ~0xfu) == 0);
        assert((unsigned(_backFaceStencil._comparisonOp) & ~0xfu) == 0);

        return  ((uint64_t(_depthTest) & 0xf) << 0ull)

            |   ((uint64_t(_frontFaceStencil._passOp) & 0xf) << 4ull)
            |   ((uint64_t(_frontFaceStencil._failOp) & 0xf) << 8ull)
            |   ((uint64_t(_frontFaceStencil._depthFailOp) & 0xf) << 12ull)
            |   ((uint64_t(_frontFaceStencil._comparisonOp) & 0xf) << 16ull)

            |   ((uint64_t(_backFaceStencil._passOp) & 0xf) << 20ull)
            |   ((uint64_t(_backFaceStencil._failOp) & 0xf) << 24ull)
            |   ((uint64_t(_backFaceStencil._depthFailOp) & 0xf) << 28ull)
            |   ((uint64_t(_backFaceStencil._comparisonOp) & 0xf) << 32ull)

            |   ((uint64_t(_stencilReadMask) & 0xf) << 36ull)
            |   ((uint64_t(_stencilWriteMask) & 0xf) << 44ull)
            |   ((uint64_t(_stencilReference) & 0xf) << 52ull)      // todo -- remove stencil reference

            |   ((uint64_t(_depthWrite) & 0x1) << 60ull)
            |   ((uint64_t(_stencilEnable) & 0x1) << 61ull)
            ;

    }

    uint64_t AttachmentBlendDesc::Hash() const
    {
        // Note that we're checking that each element fits in 4 bits, and then space them out
        // to give each 8 bits (well, there's room for expansion)
        assert((unsigned(_srcColorBlendFactor) & ~0xfu) == 0);
        assert((unsigned(_dstColorBlendFactor) & ~0xfu) == 0);
        assert((unsigned(_colorBlendOp) & ~0xfu) == 0);
        assert((unsigned(_srcAlphaBlendFactor) & ~0xfu) == 0);
        assert((unsigned(_dstAlphaBlendFactor) & ~0xfu) == 0);
        assert((unsigned(_alphaBlendOp) & ~0xfu) == 0);
        assert((unsigned(_writeMask) & ~0xfu) == 0);
        return  ((uint64_t(_blendEnable) & 1) << 0ull)

            |   ((uint64_t(_srcColorBlendFactor) & 0xf) << 8ull)
            |   ((uint64_t(_dstColorBlendFactor) & 0xf) << 16ull)
            |   ((uint64_t(_colorBlendOp) & 0xf) << 24ull)

            |   ((uint64_t(_srcAlphaBlendFactor) & 0xf) << 32ull)
            |   ((uint64_t(_dstAlphaBlendFactor) & 0xf) << 40ull)
            |   ((uint64_t(_alphaBlendOp) & 0xf) << 48ull)

            |   ((uint64_t(_writeMask) & 0xf) << 56ull)
            ;
    }

	static unsigned int FloatBits(float input)
    {
            // (or just use a reinterpret cast)
        union Converter { float f; unsigned int i; };
        Converter c; c.f = input; 
        return c.i;
    }

	uint64_t RasterizationDesc::Hash() const
	{
		assert((unsigned(_cullMode) & ~0xff) == 0);
        assert((unsigned(_frontFaceWinding) & ~0xff) == 0);
		uint64_t p0 = 
			  uint64_t(_cullMode) 
			| (uint64_t(_frontFaceWinding) << 8ull)
			| (uint64_t(FloatBits(_depthBiasConstantFactor)) << 32ull);
		uint64_t p1 = 
				uint64_t(FloatBits(_depthBiasClamp))
			|	(uint64_t(FloatBits(_depthBiasSlopeFactor)) << 32ull);
		return HashCombine(p0, p1);
	}

	StencilDesc StencilDesc::NoEffect { StencilOp::DontWrite, StencilOp::DontWrite, StencilOp::DontWrite, CompareOp::Always };
	StencilDesc StencilDesc::AlwaysWrite { StencilOp::Replace, StencilOp::DontWrite, StencilOp::DontWrite, CompareOp::Always };
}

