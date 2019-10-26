// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "State.h"
#include "DeviceContext.h"
#include "ObjectFactory.h"

#include <Metal/MTLSampler.h>
#include <Metal/MTLRenderCommandEncoder.h>

namespace RenderCore { namespace Metal_AppleMetal
{

    static MTLSamplerAddressMode AsMTLenum(AddressMode mode)
    {
        switch (mode) {
            case AddressMode::Wrap: return MTLSamplerAddressModeRepeat;
            case AddressMode::Mirror: return MTLSamplerAddressModeMirrorRepeat;
            case AddressMode::Clamp: return MTLSamplerAddressModeClampToEdge;
#if TARGET_OS_OSX
            case AddressMode::Border: return MTLSamplerAddressModeClampToBorderColor;
#endif
            default:
                assert(0);
                return MTLSamplerAddressModeRepeat;
        }
    }

    static MTLCompareFunction AsMTLenum(CompareOp comparison)
    {
        switch (comparison) {
            case CompareOp::Less:           return MTLCompareFunctionLess;
            case CompareOp::Equal:          return MTLCompareFunctionEqual;
            case CompareOp::LessEqual:      return MTLCompareFunctionLessEqual;
            case CompareOp::Greater:        return MTLCompareFunctionGreater;
            case CompareOp::NotEqual:       return MTLCompareFunctionNotEqual;
            case CompareOp::GreaterEqual:   return MTLCompareFunctionGreaterEqual;
            case CompareOp::Always:         return MTLCompareFunctionAlways;
            default:
            case CompareOp::Never:          return MTLCompareFunctionNever;
        }
    }

    class SamplerState::Pimpl
    {
    public:
        TBC::OCPtr<AplMtlSamplerState> _underlyingSamplerMipmaps; // <MTLSamplerState>
        TBC::OCPtr<AplMtlSamplerState> _underlyingSamplerNoMipmaps; // <MTLSamplerState>
        bool _enableMipmaps = true;
    };

    SamplerState::SamplerState(
        FilterMode filter,
        AddressMode addressU, AddressMode addressV, AddressMode addressW,
        CompareOp comparison,
        bool enableMipmaps)
    {
        _pimpl = std::make_shared<Pimpl>();
        _pimpl->_enableMipmaps = enableMipmaps;

        TBC::OCPtr<MTLSamplerDescriptor> desc = TBC::moveptr([[MTLSamplerDescriptor alloc] init]);

        desc.get().rAddressMode = AsMTLenum(addressW);
        desc.get().sAddressMode = AsMTLenum(addressU);
        desc.get().tAddressMode = AsMTLenum(addressV);

        MTLSamplerMinMagFilter minFilter;
        MTLSamplerMinMagFilter magFilter;
        MTLSamplerMipFilter mipFilter;

        switch (filter) {
            case FilterMode::Bilinear:
            case FilterMode::ComparisonBilinear:
                minFilter = MTLSamplerMinMagFilterLinear;
                magFilter = MTLSamplerMinMagFilterLinear;
                mipFilter = MTLSamplerMipFilterNearest;
                break;

            case FilterMode::Trilinear:
            case FilterMode::Anisotropic:
                minFilter = MTLSamplerMinMagFilterLinear;
                magFilter = MTLSamplerMinMagFilterLinear;
                mipFilter = MTLSamplerMipFilterLinear;
                break;

            default:
            case FilterMode::Point:
                minFilter = MTLSamplerMinMagFilterNearest;
                magFilter = MTLSamplerMinMagFilterNearest;
                mipFilter = MTLSamplerMipFilterNearest;
                break;
        }

        desc.get().minFilter = minFilter;
        desc.get().magFilter = magFilter;
        desc.get().mipFilter = mipFilter;

        desc.get().compareFunction = AsMTLenum(comparison);
        if (filter != FilterMode::ComparisonBilinear) {
            desc.get().compareFunction = MTLCompareFunctionNever;
        }

        auto& factory = GetObjectFactory();
        if (!(factory.GetFeatureSet() & FeatureSet::Flags::SamplerComparisonFn)) {
            // Not all Metal feature sets allow you to define a framework-side sampler comparison function for a MTLSamplerState object.
            // All feature sets support shader-side sampler comparison functions, as described in the Metal Shading Language Guide.
            desc.get().compareFunction = MTLCompareFunctionNever;
        }

        _pimpl->_underlyingSamplerMipmaps = factory.CreateSamplerState(desc);

        desc.get().mipFilter = MTLSamplerMipFilterNotMipmapped;
        _pimpl->_underlyingSamplerNoMipmaps = factory.CreateSamplerState(desc);
    }

    SamplerState::SamplerState()
    {
        // Default constructor is intentionally inexpensive and incomplete - it's called, for example, when resizing a vector
        _pimpl = std::make_shared<Pimpl>();
        _pimpl->_enableMipmaps = false;
        _pimpl->_underlyingSamplerNoMipmaps = GetObjectFactory().StandInSamplerState();
    }

    void SamplerState::Apply(DeviceContext& context, bool textureHasMipmaps, unsigned samplerIndex, ShaderStage stage) const never_throws
    {
        assert(_pimpl);

        id<MTLSamplerState> mtlSamplerState = nil;
        if (_pimpl->_enableMipmaps && textureHasMipmaps) {
            mtlSamplerState = _pimpl->_underlyingSamplerMipmaps.get();
        } else {
            mtlSamplerState = _pimpl->_underlyingSamplerNoMipmaps.get();
        }
        assert(mtlSamplerState);

        id<MTLRenderCommandEncoder> cmdEncoder = context.GetCommandEncoder();
        if (stage == ShaderStage::Vertex) {
            [cmdEncoder setVertexSamplerState:mtlSamplerState atIndex:(NSUInteger)samplerIndex];
        } else if (stage == ShaderStage::Pixel) {
            [cmdEncoder setFragmentSamplerState:mtlSamplerState atIndex:(NSUInteger)samplerIndex];
        }
    }

    BlendState::BlendState() {}
    void BlendState::Apply() const never_throws
    {
        assert(0);
    }

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

}}
