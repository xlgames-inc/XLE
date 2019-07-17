// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Device.h"
#include "../IAnnotator.h"
#include <assert.h>

#include "Metal/IncludeAppleMetal.h"
#include "Metal/Format.h"
#include "Metal/DeviceContext.h" // for ObjectFactory
#include <QuartzCore/CAMetalLayer.h>

#if PLATFORMOS_TARGET == PLATFORMOS_OSX
    #include <AppKit/NSView.h>
#else
    #include <UIKit/UIView.h>
#endif

namespace RenderCore { namespace ImplAppleMetal
{

    IResourcePtr    ThreadContext::BeginFrame(IPresentationChain& presentationChain)
    {
        assert(!_activeFrameDrawable);
        assert(_immediateCommandQueue);     // we can only do BeginFrame/Present on the "immediate" context
        _activeFrameDrawable = nullptr;

        auto& presChain = *checked_cast<PresentationChain*>(&presentationChain);

        // note -- nextDrawable can stall if the CPU is running too fast
        id<CAMetalDrawable> nextDrawable = [presChain.GetUnderlyingLayer() nextDrawable];

        id<MTLTexture> texture = nextDrawable.texture;

        _activeFrameDrawable = nextDrawable;

        // Each thread will have its own command buffer, which is provided to the device context to create command encoders
        _commandBuffer = [_immediateCommandQueue.get() commandBuffer];

        GetDeviceContext()->HoldCommandBuffer(_commandBuffer);

        // KenD -- This is constructing a RenderBuffer, but we don't really differentiate between RenderBuffer and Texture really.  The binding is specified as RenderTarget.
        return std::make_shared<Metal_AppleMetal::Resource>(texture, Metal_AppleMetal::ExtractRenderBufferDesc(texture));
    }

    void        ThreadContext::Present(IPresentationChain& presentationChain)
    {
        assert(_commandBuffer);
        assert(_immediateCommandQueue);
        if (_activeFrameDrawable) {
            [_commandBuffer.get() presentDrawable:_activeFrameDrawable.get()];
        }
        [_commandBuffer.get() commit];

        GetDeviceContext()->ReleaseCommandBuffer();

        _activeFrameDrawable = nullptr;
        _commandBuffer = nullptr;
    }

    bool                        ThreadContext::IsImmediate() const { return _immediateCommandQueue != nullptr; }
    ThreadContextStateDesc      ThreadContext::GetStateDesc() const { return {}; }
    std::shared_ptr<IDevice>    ThreadContext::GetDevice() const { return _device.lock(); }
    void                        ThreadContext::IncrFrameId() {}
    void                        ThreadContext::InvalidateCachedState() const {}

    IAnnotator&                 ThreadContext::GetAnnotator()
    {
        if (!_annotator) {
            auto d = _device.lock();
            assert(d);
            assert(0);
            // _annotator = CreateAnnotator(*d);         DavidJ -- update to new IAnnotator interface
        }
        return *_annotator;
    }

    void*                       ThreadContext::QueryInterface(size_t guid)
    {
        if (guid == typeid(IThreadContextAppleMetal).hash_code())
            return (IThreadContextAppleMetal*)this;
        return nullptr;
    }

    const std::shared_ptr<Metal_AppleMetal::DeviceContext>&  ThreadContext::GetDeviceContext()
    {
        assert(_devContext);
        return _devContext;
    }

    ThreadContext::ThreadContext(
        id<MTLCommandQueue> immediateCommandQueue,
        const std::shared_ptr<Device>& device)
    : _immediateCommandQueue(immediateCommandQueue)
    , _device(device)
    {
        _devContext = std::make_shared<Metal_AppleMetal::DeviceContext>();

        // KenD -- needed a way for the device context to access the MTLDevice
        _devContext->HoldDevice(device->GetUnderlying());
    }

    ThreadContext::ThreadContext(
        id<MTLCommandBuffer> commandBuffer,
        const std::shared_ptr<Device>& device)
    : _device(device)
    , _commandBuffer(commandBuffer)
    {
        _devContext = std::make_shared<Metal_AppleMetal::DeviceContext>();

        // KenD -- needed a way for the device context to access the MTLDevice
        _devContext->HoldDevice(device->GetUnderlying());
    }

    ThreadContext::~ThreadContext() {
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    static id<MTLDevice> CreateUnderlyingDevice()
    {
        auto underlying = MTLCreateSystemDefaultDevice();
        if (!underlying)
            Throw(::Exceptions::BasicLabel("Could not initialize Apple Metal, because the API isn't supported on this device"));
        return underlying;
    }

    Device::Device() : Device(CreateUnderlyingDevice())
    {
    }

    Device::Device(id<MTLDevice> underlying)
    {
        _underlying = underlying;
        _immediateCommandQueue = [_underlying.get() newCommandQueue];
        _objectFactory = std::make_shared<Metal_AppleMetal::ObjectFactory>(_underlying.get());
    }

    Device::~Device()
    {
    }

    std::unique_ptr<IPresentationChain>   Device::CreatePresentationChain(const void* platformValue, const PresentationChainDesc &desc)
    {
        return std::make_unique<PresentationChain>(_underlying.get(), platformValue, desc);
    }

    void* Device::QueryInterface(size_t guid)
    {
        if (guid == typeid(Device).hash_code()) {
            return this;
        }
        return nullptr;
    }

    IResourcePtr Device::CreateResource(const ResourceDesc& desc, const ResourceInitializer& init)
    {
        return Metal_AppleMetal::CreateResource(*_objectFactory, desc, init);
    }

    std::shared_ptr<ILowLevelCompiler>        Device::CreateShaderCompiler()
    {
        assert(0);      // DavidJ -- unimplemented
        return nullptr;
    }

    DeviceDesc Device::GetDesc()
    {
        return DeviceDesc { "AppleMetal", "", "" };
    }

    FormatCapability Device::QueryFormatCapability(Format format, BindFlag::BitField bindingType)
    {
        auto mtlFormat = Metal_AppleMetal::AsMTLPixelFormat(format);
        return (mtlFormat != MTLPixelFormatInvalid) ? FormatCapability::Supported : FormatCapability::NotSupported;
    }

    std::unique_ptr<IThreadContext>   Device::CreateDeferredContext()
    {
        return std::make_unique<ThreadContext>(
            [_immediateCommandQueue.get() commandBuffer],
            shared_from_this());
    }

    std::shared_ptr<IThreadContext>   Device::GetImmediateContext()
    {
        if (!_immediateContext) {
            _immediateContext = std::make_shared<ThreadContext>(
                (id<MTLCommandQueue>)_immediateCommandQueue.get(),
                shared_from_this());
        }
        return _immediateContext;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    PresentationChain::PresentationChain(id<MTLDevice> device, const void* platformValue, const PresentationChainDesc &desc)
    {
        #if PLATFORMOS_TARGET == PLATFORMOS_OSX
            if (![((NSObject*)platformValue) isKindOfClass:NSView.class])
                Throw(std::runtime_error("Platform value in PresentationChain::PresentationChain is not a NSView"));

            auto* view = (NSView*)platformValue;
        #else
            if (![((NSObject*)platformValue) isKindOfClass:UIView.class])
                Throw(std::runtime_error("Platform value in PresentationChain::PresentationChain is not a UIView"));

            auto* view = (UIView*)platformValue;
        #endif

        if (![view.layer isKindOfClass:CAMetalLayer.class])
            Throw(std::runtime_error("Layer in UIView passed to PresentationChain::PresentationChain is not of type CAMetalLayer"));

        _desc = std::make_shared<PresentationChainDesc>(desc);

        /* KenD -- Metal Hack -- a bit of a hack to double the width and height for higher resolation (mimics retina displays);
         * tried changing the contentsScale of the CAMetalLayer, but that didn't have a noticeable effect. */
        _desc->_width *= 2.0;
        _desc->_height *= 2.0;

        auto* metalLayer = (CAMetalLayer*)view.layer;
        metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm; /* Metal TODO -- Currently fixed to using LDR format */
        metalLayer.framebufferOnly = YES;
        metalLayer.drawableSize = CGSizeMake(_desc->_width, _desc->_height);
        metalLayer.device = device;
        // metalLayer.colorSpace = nil;     <-- only OSX?

        _layer = metalLayer;
    }

    PresentationChain::~PresentationChain()
    {
    }

    void PresentationChain::Resize(unsigned newWidth, unsigned newHeight)
    {
        _layer.get().drawableSize = CGSizeMake(newWidth, newHeight);
    }

    const std::shared_ptr<PresentationChainDesc>& PresentationChain::GetDesc() const
    {
        return _desc;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    render_dll_export std::shared_ptr<IDevice> TryCreateDevice()
    {
        auto* underlyingDevice = MTLCreateSystemDefaultDevice();
        if (!underlyingDevice)
            return nullptr;
        return std::make_shared<Device>(underlyingDevice);
    }

}}

namespace RenderCore {
    IThreadContextAppleMetal::~IThreadContextAppleMetal() {}
}

