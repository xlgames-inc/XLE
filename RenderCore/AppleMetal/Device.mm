// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Device.h"
#include "../IAnnotator.h"
#include <assert.h>

#include "Metal/IncludeAppleMetal.h"
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
        (void)texture;
        // todo return this texture as an IResourcePtr

        _activeFrameDrawable = nextDrawable;

        _commandBuffer = [_immediateCommandQueue.get() commandBuffer];
        return nullptr;
    }

    void        ThreadContext::Present(IPresentationChain& presentationChain)
    {
        assert(_commandBuffer);
        assert(_immediateCommandQueue);
        if (_activeFrameDrawable) {
            [_commandBuffer.get() presentDrawable:_activeFrameDrawable.get()];
        }
        [_commandBuffer.get() commit];

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
            _annotator = CreateAnnotator(*d);
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
        return _devContext;
    }

    ThreadContext::ThreadContext(
        id<MTLCommandQueue> immediateCommandQueue,
        const std::shared_ptr<Device>& device)
    : _immediateCommandQueue(immediateCommandQueue)
    , _device(device)
    {}

    ThreadContext::ThreadContext(
        id<MTLCommandBuffer> commandBuffer,
        const std::shared_ptr<Device>& device)
    : _device(device)
    , _commandBuffer(commandBuffer)
    {
    }

    ThreadContext::~ThreadContext() {}

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
    }

    Device::~Device()
    {
    }

    std::unique_ptr<IPresentationChain>   Device::CreatePresentationChain(
        const void* platformValue, unsigned width, unsigned height)
    {
        return std::make_unique<PresentationChain>(
            _underlying.get(),
            platformValue, width, height);
    }

    void* Device::QueryInterface(size_t guid)
    {
        return nullptr;
    }

    IResourcePtr Device::CreateResource(const ResourceDesc& desc, const ResourceInitializer& init)
    {
        // hack -- only getting a single subresource here!
        // return std::make_shared<Metal_OpenGLES::Resource>(*_objectFactory, desc, init);
        return nullptr;
    }

    DeviceDesc Device::GetDesc()
    {
        return DeviceDesc { "AppleMetal", "", "" };
    }

    std::unique_ptr<IThreadContext>   Device::CreateDeferredContext()
    {
        return std::make_unique<ThreadContext>(
            [_immediateCommandQueue.get() commandBuffer],
            shared_from_this());
    }

    std::shared_ptr<IThreadContext>   Device::GetImmediateContext()
    {
        if (!_immediateContext)
            _immediateContext = std::make_shared<ThreadContext>(
                (id<MTLCommandQueue>)_immediateCommandQueue.get(),
                shared_from_this());
        return _immediateContext;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    PresentationChain::PresentationChain(
        id<MTLDevice> device,
        const void* platformValue, unsigned width, unsigned height)
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

        // todo -- configure layer here
        auto* metalLayer = (CAMetalLayer*)view.layer;
        metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
        metalLayer.framebufferOnly = YES;
        metalLayer.drawableSize = CGSizeMake(width, height);
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
        static std::shared_ptr<PresentationChainDesc> dummy;
        return dummy;
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
