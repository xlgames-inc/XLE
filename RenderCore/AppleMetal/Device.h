// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IDeviceAppleMetal.h"
#include "../IDevice.h"
#include "../IThreadContext.h"
#include "../IAnnotator.h"
#include "../../../Externals/Misc/OCPtr.h"
#include <memory>

@class CAMetalLayer;
@protocol MTLDrawable;
@protocol MTLCommandQueue;
@protocol MTLCommandBuffer;
@protocol MTLDevice;

namespace RenderCore { namespace Metal_AppleMetal
{
    class ObjectFactory;
    class DeviceContext;
}}

namespace RenderCore { namespace ImplAppleMetal
{

////////////////////////////////////////////////////////////////////////////////

    class PresentationChain : public Base_PresentationChain
    {
    public:
        void                Resize(unsigned newWidth, unsigned newHeight) /*override*/;
        const std::shared_ptr<PresentationChainDesc>& GetDesc() const;

        CAMetalLayer* GetUnderlyingLayer() const { return _layer; }

        PresentationChain(
            id<MTLDevice> device,
            const void* platformValue, unsigned width, unsigned height);
        ~PresentationChain();

    private:
        TBC::OCPtr<CAMetalLayer> _layer;
        std::shared_ptr<PresentationChainDesc> _desc;
    };

////////////////////////////////////////////////////////////////////////////////

    class Device;

    class ThreadContext : public Base_ThreadContext, public IThreadContextAppleMetal
    {
    public:
        IResourcePtr BeginFrame(IPresentationChain& presentationChain);
        void        Present(IPresentationChain& presentationChain) /*override*/;

        void*                       QueryInterface(size_t guid);
        bool                        IsImmediate() const;
        ThreadContextStateDesc      GetStateDesc() const;
        std::shared_ptr<IDevice>    GetDevice() const;
        void                        IncrFrameId();
        void                        InvalidateCachedState() const;

        IAnnotator&                 GetAnnotator();

        const std::shared_ptr<Metal_AppleMetal::DeviceContext>&  GetDeviceContext();

        ThreadContext(
            id<MTLCommandQueue> immediateCommandQueue,
            const std::shared_ptr<Device>& device);
        ThreadContext(
            id<MTLCommandBuffer> commandBuffer,
            const std::shared_ptr<Device>& device);
        ~ThreadContext();

    private:
        TBC::OCPtr<id> _immediateCommandQueue;      // (id<MTLCommandQueue>)
        std::weak_ptr<Device> _device;  // (must be weak, because Device holds a shared_ptr to the immediate context)

        TBC::OCPtr<id> _activeFrameDrawable;        // (id<MTLDrawable>)
        TBC::OCPtr<id> _commandBuffer;              // (id<MTLCommandBuffer>)

        std::shared_ptr<IAnnotator> _annotator;

        std::shared_ptr<Metal_AppleMetal::DeviceContext> _devContext;
    };

////////////////////////////////////////////////////////////////////////////////

    class Device :  public Base_Device, public std::enable_shared_from_this<Device>
    {
    public:
        std::unique_ptr<IPresentationChain> CreatePresentationChain(
            const void* platformValue, unsigned width, unsigned height);
        void* QueryInterface(size_t guid);

        std::shared_ptr<IThreadContext> GetImmediateContext();
        std::unique_ptr<IThreadContext> CreateDeferredContext();

        using ResourceInitializer = std::function<SubResourceInitData(SubResourceId)>;
        IResourcePtr CreateResource(const ResourceDesc& desc, const ResourceInitializer& init);
        DeviceDesc GetDesc();
        FormatCapability QueryFormatCapability(Format format, BindFlag::BitField bindingType);

        id<MTLDevice> GetUnderlying() const { return _underlying; }

        Device();
        explicit Device(id<MTLDevice> underlying);
        ~Device();

    protected:
        TBC::OCPtr<id> _underlying;                 // id<MTLDevice>
        std::shared_ptr<Metal_AppleMetal::ObjectFactory> _objectFactory;
        TBC::OCPtr<id> _immediateCommandQueue;      // id<MTLCommandQueue>
        std::shared_ptr<ThreadContext> _immediateContext;
    };

////////////////////////////////////////////////////////////////////////////////
}}
