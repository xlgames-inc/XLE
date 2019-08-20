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
            default: assert(0);
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

        /* METAL TODO: restrictions on framework-side sampler comparison function
         The MTLFeatureSet_iOS_GPUFamily3_v1 and MTLFeatureSet_OSX_GPUFamily1_v1 feature sets allow you to define a framework-side sampler comparison function for a MTLSamplerState object. All feature sets support shader-side sampler comparison functions, as described in the Metal Shading Language Guide.
         */
        desc.get().compareFunction = AsMTLenum(comparison);
        if (filter != FilterMode::ComparisonBilinear) {
            desc.get().compareFunction = MTLCompareFunctionNever;
        }

        auto& factory = GetObjectFactory();
        _pimpl->_underlyingSamplerMipmaps = factory.CreateSamplerState(desc);

        desc.get().mipFilter = MTLSamplerMipFilterNotMipmapped;
        _pimpl->_underlyingSamplerNoMipmaps = factory.CreateSamplerState(desc);
    }

    SamplerState::SamplerState()
    {
        // Default constructor is intentionally inexpensive and incomplete - it's called, for example, when resizing a vector
        _pimpl = nullptr;
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

    ViewportDesc::ViewportDesc(DeviceContext& devContext)
    {
        *this = devContext.GetViewport();
    }
    
}}
