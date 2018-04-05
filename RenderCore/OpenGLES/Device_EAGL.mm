
#include "Device_EAGL.h"
#include "Metal/DeviceContext.h"
#include "Metal/ObjectFactory.h"
#include "Metal/Resource.h"
#include "Metal/Format.h"
#include "../IAnnotator.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/StringFormat.h"
#include "../../Core/Exceptions.h"
#include <type_traits>
#include <iostream>
#include <assert.h>
#include "IncludeGLES.h"

#include <OpenGLES/EAGL.h>
#include <UIKit/UIView.h>

namespace RenderCore { namespace ImplOpenGLES
{
    static Metal_OpenGLES::FeatureSet::BitField AsGLESFeatureSet(EAGLRenderingAPI api)
    {
        Metal_OpenGLES::FeatureSet::BitField featureSet = Metal_OpenGLES::FeatureSet::GLES200;
        switch (api) {
        default:
        case kEAGLRenderingAPIOpenGLES1: assert(0); break;
        case kEAGLRenderingAPIOpenGLES2: featureSet = Metal_OpenGLES::FeatureSet::GLES200 | Metal_OpenGLES::FeatureSet::ETC1TC; break;
        case kEAGLRenderingAPIOpenGLES3: featureSet = Metal_OpenGLES::FeatureSet::GLES200 | Metal_OpenGLES::FeatureSet::GLES300 | Metal_OpenGLES::FeatureSet::ETC1TC | Metal_OpenGLES::FeatureSet::ETC2TC; break;
        }
        // All Apple / EAGL devices can support PVR textures
        featureSet |= Metal_OpenGLES::FeatureSet::PVRTC;
        return featureSet;
    }

    IResourcePtr    ThreadContext::BeginFrame(IPresentationChain& presentationChain)
    {
        assert(!_activeFrameContext);
        _activeFrameContext = nullptr;

        auto& presChain = *checked_cast<PresentationChain*>(&presentationChain);
        _activeFrameContext = presChain.GetEAGLContext();
        _activeFrameRenderbuffer = presChain.GetFrameRenderbuffer();
        if (!_activeFrameContext)
            _activeFrameContext = _sharedContext;

        [EAGLContext setCurrentContext:_activeFrameContext.get()];

        _activeFrameBuffer = Metal_OpenGLES::GetObjectFactory().CreateFrameBuffer();
        glBindFramebuffer(GL_FRAMEBUFFER, _activeFrameBuffer->AsRawGLHandle());
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _activeFrameRenderbuffer->GetRenderBuffer()->AsRawGLHandle());
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            Throw(std::runtime_error("Framebuffer not complete in PresentationChain::PresentationChain"));

        return _activeFrameRenderbuffer;
    }

    void        ThreadContext::Present(IPresentationChain& presentationChain)
    {
        auto& presChain = *checked_cast<PresentationChain*>(&presentationChain);
        assert(!presChain.GetEAGLContext() || presChain.GetEAGLContext() == _activeFrameContext);
        if (_activeFrameContext) {
            glBindRenderbuffer(GL_RENDERBUFFER, _activeFrameRenderbuffer->GetRenderBuffer()->AsRawGLHandle());
            [_activeFrameContext.get() presentRenderbuffer:GL_RENDERBUFFER];
        }
        _activeFrameRenderbuffer.reset();
        _activeFrameBuffer.reset();
        _activeFrameContext = nullptr;
        [EAGLContext setCurrentContext:_sharedContext.get()];
    }

    bool                        ThreadContext::IsImmediate() const { return false; }
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

    void*                       ThreadContext::QueryInterface(size_t guid) {
        if (guid == typeid(EAGLContext).hash_code()) {
            return (EAGLContext*)_sharedContext;
        }
        return nullptr;
    }

    ThreadContext::ThreadContext(EAGLContext* sharedContext, const std::shared_ptr<Device>& device)
    : _device(device)
    , _sharedContext(sharedContext)
    {}

    ThreadContext::~ThreadContext() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    const std::shared_ptr<Metal_OpenGLES::DeviceContext>&  ThreadContextOpenGLES::GetDeviceContext()
    {
        return _deviceContext;
    }

    void*       ThreadContextOpenGLES::QueryInterface(size_t guid)
    {
        if (guid == typeid(IThreadContextOpenGLES).hash_code()) {
            return (IThreadContextOpenGLES*)this;
        }
        return ThreadContext::QueryInterface(guid);
    }

    ThreadContextOpenGLES::ThreadContextOpenGLES(EAGLContext* sharedContext, const std::shared_ptr<Device>& device)
    : ThreadContext(sharedContext, device)
    {
        auto featureSet = AsGLESFeatureSet(sharedContext.API);
        _deviceContext = std::make_shared<Metal_OpenGLES::DeviceContext>(featureSet);
    }

    ThreadContextOpenGLES::~ThreadContextOpenGLES() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    Device::Device()
    {
        auto* t = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
        _sharedContext = TBC::moveptr(t);
        [EAGLContext setCurrentContext:_sharedContext.get()];
        auto featureSet = AsGLESFeatureSet(_sharedContext.get().API);
        _objectFactory = std::make_shared<Metal_OpenGLES::ObjectFactory>(featureSet);
    }

    Device::~Device()
    {
    }

    std::unique_ptr<IPresentationChain>   Device::CreatePresentationChain(const void* platformValue, unsigned width, unsigned height)
    {
        return std::make_unique<PresentationChain>(
            *_objectFactory,
            _sharedContext.get(),
            platformValue, width, height);
    }

    void* Device::QueryInterface(size_t guid)
    {
        return nullptr;
    }

    IResourcePtr Device::CreateResource(const ResourceDesc& desc, const ResourceInitializer& init)
    {
        return Metal_OpenGLES::CreateResource(*_objectFactory, desc, init);
    }

    DeviceDesc Device::GetDesc()
    {
        return DeviceDesc { "OpenGLES-EAGL", "", "" };
    }

    FormatCapability Device::QueryFormatCapability(Format format, BindFlag::BitField bindingType)
    {
        auto activeFeatureSet = _objectFactory->GetFeatureSet();
        auto glFmt = Metal_OpenGLES::AsTexelFormatType(format);
        if (glFmt._internalFormat == GL_NONE)
            return FormatCapability::NotSupported;

        bool supported = true;
        if (bindingType & BindFlag::ShaderResource) {
            supported = (activeFeatureSet & glFmt._textureFeatureSet);
        } else if ((bindingType & BindFlag::RenderTarget) || (bindingType & BindFlag::DepthStencil)) {
            supported = (activeFeatureSet & glFmt._renderbufferFeatureSet);
        }

        return supported ? FormatCapability::Supported : FormatCapability::NotSupported;
    }

    std::unique_ptr<IThreadContext>   Device::CreateDeferredContext()
    {
        return std::make_unique<ThreadContextOpenGLES>(_sharedContext, shared_from_this());
    }

    std::shared_ptr<IThreadContext>   Device::GetImmediateContext()
    {
        if (!_immediateContext)
            _immediateContext = std::make_shared<ThreadContextOpenGLES>(_sharedContext, shared_from_this());
        return _immediateContext;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    void* DeviceOpenGLES::QueryInterface(size_t guid)
    {
        if (guid == typeid(IDeviceOpenGLES).hash_code()) {
            return (IDeviceOpenGLES*)this;
        }
        return nullptr;
    }

    Metal_OpenGLES::DeviceContext * DeviceOpenGLES::GetImmediateDeviceContext()
    {
        return nullptr;
    }

    DeviceOpenGLES::DeviceOpenGLES() {}
    DeviceOpenGLES::~DeviceOpenGLES() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    PresentationChain::PresentationChain(
        Metal_OpenGLES::ObjectFactory& objFactory,
        EAGLContext* eaglContext,
        const void* platformValue, unsigned width, unsigned height)
    {
        if (![((NSObject*)platformValue) isKindOfClass:UIView.class])
            Throw(std::runtime_error("Platform value in PresentationChain::PresentationChain is not a UIView"));

        auto* view = (UIView*)platformValue;
        if (![view.layer conformsToProtocol:@protocol(EAGLDrawable)])
            Throw(std::runtime_error("Layer in UIView passed to PresentationChain::PresentationChain does not conform to EAGLDrawable protocol"));

        auto eaglDrawable = (id<EAGLDrawable>)view.layer;
        eaglDrawable.drawableProperties =
            [NSDictionary dictionaryWithObjectsAndKeys:
                [NSNumber numberWithBool:FALSE], kEAGLDrawablePropertyRetainedBacking,
                kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat,
                nil];

        auto frameRenderbuffer = objFactory.CreateRenderBuffer();
        glBindRenderbuffer(GL_RENDERBUFFER, frameRenderbuffer->AsRawGLHandle());

        auto res = [eaglContext renderbufferStorage: GL_RENDERBUFFER fromDrawable: eaglDrawable];
        if (!res)
            Throw(std::runtime_error("Failed to allocate renderbuffer storage for EAGL drawable in PresentationChain::PresentationChain"));

        _frameRenderbuffer = std::make_shared<Metal_OpenGLES::Resource>(frameRenderbuffer);

        _desc = std::make_shared<PresentationChainDesc>();
        auto resDesc = ExtractDesc(*_frameRenderbuffer);
        assert(resDesc._type == ResourceDesc::Type::Texture);
        _desc->_width = resDesc._textureDesc._width;
        _desc->_height = resDesc._textureDesc._height;
        _desc->_format = resDesc._textureDesc._format;
        _desc->_samples = resDesc._textureDesc._samples;
    }

    PresentationChain::~PresentationChain()
    {
    }

    void PresentationChain::Resize(unsigned newWidth, unsigned newHeight)
    {
        assert(0);      // unimplemented
    }

    const std::shared_ptr<PresentationChainDesc>& PresentationChain::GetDesc() const
    {
        return _desc;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    render_dll_export std::shared_ptr<IDevice> CreateDevice()
    {
        return std::make_shared<DeviceOpenGLES>();
    }

}}

namespace RenderCore
{
    IDeviceOpenGLES::~IDeviceOpenGLES() {}
    IThreadContextOpenGLES::~IThreadContextOpenGLES() {}
}

