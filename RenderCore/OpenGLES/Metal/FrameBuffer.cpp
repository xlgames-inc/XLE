// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FrameBuffer.h"
#include "Format.h"
#include "Resource.h"
#include "TextureView.h"
#include "DeviceContext.h"
#include "ObjectFactory.h"
#include "../../Format.h"
#include "../../ResourceUtils.h"
#include "../../FrameBufferDesc.h"
#include "../../../Utility/MemoryUtils.h"
#include "../../../ConsoleRig/Log.h"
#include "../../../Core/Exceptions.h"

#include "IncludeGLES.h"

namespace RenderCore { namespace Metal_OpenGLES
{
    static void BindToFramebuffer(
        GLenum attachmentSlot,
        Resource& res, const TextureViewDesc& viewWindow)
    {
        const auto& desc = res.GetDesc();
        assert(desc._type == ResourceDesc::Type::Texture);

        if (    desc._textureDesc._dimensionality == TextureDesc::Dimensionality::T2D
            ||  desc._textureDesc._dimensionality == TextureDesc::Dimensionality::T1D) {
            if (res.GetRenderBuffer()) {
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachmentSlot, GL_RENDERBUFFER, res.GetRenderBuffer()->AsRawGLHandle());
            } else {
                if (desc._textureDesc._arrayCount > 1u) {
                    glFramebufferTextureLayer(
                        GL_FRAMEBUFFER, attachmentSlot,
                        res.GetTexture()->AsRawGLHandle(),
                        viewWindow._mipRange._min,
                        viewWindow._arrayLayerRange._min);
                } else {
                    glFramebufferTexture2D(
                        GL_FRAMEBUFFER, attachmentSlot, GL_TEXTURE_2D,
                        res.GetTexture()->AsRawGLHandle(),
                        viewWindow._mipRange._min);
                }
            }
        } else if (desc._textureDesc._dimensionality == TextureDesc::Dimensionality::T3D) {
            assert(!res.GetRenderBuffer());     // not rational in this case
            glFramebufferTextureLayer(
                GL_FRAMEBUFFER, attachmentSlot,
                res.GetTexture()->AsRawGLHandle(),
                viewWindow._mipRange._min,
                viewWindow._arrayLayerRange._min);
        } else if (desc._textureDesc._dimensionality == TextureDesc::Dimensionality::CubeMap) {
            assert(!res.GetRenderBuffer());     // not rational in this case
            assert(desc._textureDesc._arrayCount <= 1u);    // cannot render to arrays of cubemaps
            glFramebufferTexture2D(
                GL_FRAMEBUFFER, attachmentSlot, GL_TEXTURE_CUBE_MAP_POSITIVE_X + viewWindow._arrayLayerRange._min,
                res.GetTexture()->AsRawGLHandle(),
                viewWindow._mipRange._min);
        } else {
            assert(0);
        }
    }
    
    static bool HasPrimaryClear(LoadStore loadStoreFlags)
    {
        return loadStoreFlags == LoadStore::Clear
            || loadStoreFlags == LoadStore::Clear_ClearStencil
            || loadStoreFlags == LoadStore::Clear_RetainStencil
            ;
    }

    static bool HasClear(LoadStore loadStoreFlags)
    {
        return HasPrimaryClear(loadStoreFlags)
            || loadStoreFlags == LoadStore::DontCare_ClearStencil
            || loadStoreFlags == LoadStore::Retain_ClearStencil
            ;
    }

    FrameBuffer::FrameBuffer(
		ObjectFactory& factory,
        const FrameBufferDesc& fbDesc,
        const INamedAttachments& namedResources)
    {
        // We must create the frame buffer, including all resources and views required.
        // Here, some resources can come from the presentation chain. But other resources will
        // be created an attached to this object.
        auto subpasses = fbDesc.GetSubpasses();

		ViewPool<RenderTargetView> rtvPool;
        ViewPool<DepthStencilView> dsvPool;
		unsigned clearValueIterator = 0;
        
        _subpasses.resize(subpasses.size());
        for (unsigned c=0; c<(unsigned)subpasses.size(); ++c) {
			const auto& spDesc = subpasses[c];
			auto& sp = _subpasses[c];
			sp._rtvCount = (unsigned)std::min(spDesc._output.size(), dimof(Subpass::_rtvs));
			for (unsigned r = 0; r<sp._rtvCount; ++r) {
				const auto& attachmentView = spDesc._output[r];
				auto resource = namedResources.GetResource(attachmentView._resourceName);
				if (!resource)
					Throw(::Exceptions::BasicLabel("Could not find attachment resource for RTV in FrameBuffer::FrameBuffer"));
				sp._rtvs[r] = *rtvPool.GetView(resource, attachmentView._window);
                if (HasPrimaryClear(attachmentView._loadFromPreviousPhase)) {
				    sp._rtvLoad[r] = LoadStore::Clear;
                    sp._rtvClearValue[r] = clearValueIterator++;
                } else {
                    sp._rtvLoad[r] = attachmentView._loadFromPreviousPhase;
                    sp._rtvClearValue[r] = ~0u;
                }
			}

            sp._dsvHasDepth = sp._dsvHasStencil = false;

			if (spDesc._depthStencil._resourceName != ~0u) {
				auto resource = namedResources.GetResource(spDesc._depthStencil._resourceName);
				if (!resource)
					Throw(::Exceptions::BasicLabel("Could not find attachment resource for DSV in FrameBuffer::FrameBuffer"));
				sp._dsv = *dsvPool.GetView(resource, spDesc._depthStencil._window);
				sp._dsvLoad = spDesc._depthStencil._loadFromPreviousPhase;
				sp._dsvClearValue = HasClear(sp._dsvLoad) ? (clearValueIterator++) : ~0u;
                auto resolvedFormat = ResolveFormat(sp._dsv.GetResource()->GetDesc()._textureDesc._format, sp._dsv._window._format, FormatUsage::DSV);
                auto components = GetComponents(resolvedFormat);
                sp._dsvHasDepth = (components == FormatComponents::Depth) || (components == FormatComponents::DepthStencil);
                sp._dsvHasStencil = (components == FormatComponents::Stencil) || (components == FormatComponents::DepthStencil);
			}

            GLenum drawBuffers[dimof(Subpass::_rtvs)] = { GL_NONE, GL_NONE, GL_NONE, GL_NONE };
            unsigned colorAttachmentIterator = 0;

            #if defined(_DEBUG)
                if (factory.GetFeatureSet() & FeatureSet::GLES300) {
                    int maxDrawBuffers = 0;
                    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
                    assert(sp._rtvCount <= maxDrawBuffers);
                }
            #endif

            bool bindingToBackbuffer = false;
            for (unsigned rtv=0; rtv<sp._rtvCount; ++rtv) {
                assert(sp._rtvs[rtv].IsGood());

                auto& res = *sp._rtvs[rtv].GetResource();
                if (res.IsBackBuffer()) {
                    bindingToBackbuffer = true;
                    break;
                }
            }
            if (bindingToBackbuffer) {
                sp._frameBuffer = intrusive_ptr<GlObject<GlObject_Type::FrameBuffer> >(0);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            } else {
                sp._frameBuffer = factory.CreateFrameBuffer();
                glBindFramebuffer(GL_FRAMEBUFFER, sp._frameBuffer->AsRawGLHandle());
            }

            for (unsigned rtv=0; rtv<sp._rtvCount; ++rtv) {
                assert(sp._rtvs[rtv].IsGood());

                auto& res = *sp._rtvs[rtv].GetResource();
                if (res.IsBackBuffer()) {
                    #if !defined(GL_ES_VERSION_2_0) && !defined(GL_ES_VERSION_3_0) // i.e. desktop gl
                        drawBuffers[rtv] = GL_BACK_LEFT;
                    #else
                        drawBuffers[rtv] = GL_BACK;
                    #endif
                } else {
                    BindToFramebuffer(GL_COLOR_ATTACHMENT0 + colorAttachmentIterator, res, sp._rtvs[rtv]._window);
                    drawBuffers[rtv] = GL_COLOR_ATTACHMENT0 + colorAttachmentIterator;
                    ++colorAttachmentIterator;
                }
            }
            if (!bindingToBackbuffer && sp._dsv.IsGood()) {
                auto& res = *sp._dsv.GetResource();
                const auto& viewWindow = sp._dsv._window;

                // select either GL_DEPTH_STENCIL_ATTACHMENT, GL_DEPTH_ATTACHMENT or GL_STENCIL_ATTACHMENT
                // depending on the type of resource & the view window
                GLenum bindingPoint = GL_DEPTH_STENCIL_ATTACHMENT;
                assert(res.GetDesc()._type == ResourceDesc::Type::Texture);
                auto fmt = res.GetDesc()._textureDesc._format;
                auto components = GetComponents(viewWindow._format._explicitFormat != Format::Unknown ? viewWindow._format._explicitFormat : fmt);

                switch (components) {
                case FormatComponents::Depth:
                    bindingPoint = GL_DEPTH_ATTACHMENT;
                    break;

                case FormatComponents::Stencil:
                    bindingPoint = GL_STENCIL_ATTACHMENT;
                    break;

                default:
                    {
                        auto aspect = viewWindow._format._aspect;
                        if (aspect == TextureViewDesc::Aspect::Depth) {
                            bindingPoint = GL_DEPTH_ATTACHMENT;
                        } else if (aspect == TextureViewDesc::Aspect::Stencil) {
                            bindingPoint = GL_STENCIL_ATTACHMENT;
                        }
                        assert(!(viewWindow._flags & TextureViewDesc::Flags::JustDepth));
                        assert(!(viewWindow._flags & TextureViewDesc::Flags::JustStencil));
                    }
                }

                BindToFramebuffer(bindingPoint, res, viewWindow);
            }

            #if defined(GL_ES_VERSION_2_0) || defined(GL_ES_VERSION_3_0)
                // Don't call glDrawBuffers on GLES2
                if (factory.GetFeatureSet() & FeatureSet::GLES300) {
                    glDrawBuffers(sp._rtvCount, drawBuffers);
                }
            #else
                glDrawBuffers(sp._rtvCount, drawBuffers);
            #endif

            #if !defined(GL_ES_VERSION_2_0) && !defined(GL_ES_VERSION_3_0) // i.e. desktop gl
                // In desktop GL, we must do this to avoid FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER framebuffer status
                if (sp._rtvCount == 0) {
                    glDrawBuffer(GL_NONE);
                    glReadBuffer(GL_NONE);
                } else {
                    // XTODO: currently there are situations where colorAttachmentIterator is 0. Is this expected?
                    if (colorAttachmentIterator > 0) {
                        glReadBuffer(GL_COLOR_ATTACHMENT0);
                    }
                }
            #endif

            #if defined(_DEBUG)
                GLenum validationFlag = glCheckFramebufferStatus(GL_FRAMEBUFFER);
                assert(validationFlag == GL_FRAMEBUFFER_COMPLETE);
            #endif
        }
    }

    void FrameBuffer::BindSubpass(DeviceContext& context, unsigned subpassIndex, IteratorRange<const ClearValue*> clearValues) const
    {
        if (subpassIndex >= _subpasses.size())
            Throw(::Exceptions::BasicLabel("Attempting to set invalid subpass"));

        const auto& s = _subpasses[subpassIndex];
        // DavidJ -- hack because can't figure out how to get this working correctly using EGL
        bool fbZeroHack = false;
        if (s._rtvCount == 1 && s._rtvs[0].GetResource() && s._rtvs[0].GetResource()->IsBackBuffer()) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            fbZeroHack = true;
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, s._frameBuffer->AsRawGLHandle());
        }
        // glBindFramebuffer(GL_FRAMEBUFFER, 0);

        bool clearDepth = false, clearStencil = false;
        if (s._dsv.IsGood()) {
            clearDepth =
                    (s._dsvLoad == LoadStore::Clear_ClearStencil || s._dsvLoad == LoadStore::Clear || s._dsvLoad == LoadStore::Clear_RetainStencil)
                &&  (s._dsvHasDepth);
            clearStencil =
                    (s._dsvLoad == LoadStore::Clear_ClearStencil || s._dsvLoad == LoadStore::DontCare_ClearStencil || s._dsvLoad == LoadStore::Retain_ClearStencil)
                &&  (s._dsvHasStencil);
        }

        #if _DEBUG
            // write masks affect glClear and glClearBuffer calls
            // https://www.khronos.org/opengl/wiki/Write_Mask
            if (clearDepth) {
                GLboolean depthWriteMask;
                glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWriteMask);
                if (!depthWriteMask) {
                    Throw(::Exceptions::BasicLabel("Attempting to clear depth with depth mask off in subpass %d", (int)subpassIndex));
                }
            }
            if (clearStencil) {
                GLint stencilWriteMask;
                glGetIntegerv(GL_STENCIL_WRITEMASK, &stencilWriteMask);
                if (!(stencilWriteMask & 0xFF)) {
                    Throw(::Exceptions::BasicLabel("Attempting to clear stencil with stencil mask %u in subpass %d", stencilWriteMask, (int)subpassIndex));
                }
            }
        #endif

        // OpenGLES3 has glClearBuffer... functions that can clear specific targets.
        // For ES2 and GL2, we have to drop back to the older API
        bool useNewClearAPI = context.GetFeatureSet() & FeatureSet::GLES300;
        if (!fbZeroHack && useNewClearAPI) {
            for (unsigned rtv=0; rtv<s._rtvCount; ++rtv) {
                auto attachmentIdx = s._rtvs[rtv];
                auto load = s._rtvLoad[rtv];
                if (load == LoadStore::Clear) {
                    auto formatComponentType = FormatComponentType::Float;
                    if (s._rtvs[rtv].GetResource() && s._rtvs[rtv].GetResource()->GetDesc()._type == ResourceDesc::Type::Texture) {
                        formatComponentType = GetComponentType(s._rtvs[rtv].GetResource()->GetDesc()._textureDesc._format);
                    }

                    float defClear[] = {0.f, 0.f, 0.f, 1.0f};
                    unsigned defClearUInt[] = {0, 0, 0, 0};
                    signed defClearSInt[] = {0, 0, 0, 0};

                    switch (formatComponentType) {
                    case FormatComponentType::UInt:
                        {
                            const unsigned* clear = defClearUInt;
                            if (s._rtvClearValue[rtv] < clearValues.size())
                                clear = clearValues[s._rtvClearValue[rtv]]._uint;
                            glClearBufferuiv(GL_COLOR, rtv, clear);
                        }
                        break;
                    case FormatComponentType::SInt:
                        {
                            const signed* clear = defClearSInt;
                            if (s._rtvClearValue[rtv] < clearValues.size())
                                clear = clearValues[s._rtvClearValue[rtv]]._int;
                            glClearBufferiv(GL_COLOR, rtv, clear);
                        }
                        break;
                    case FormatComponentType::UNorm:
                    case FormatComponentType::SNorm:
                    case FormatComponentType::UNorm_SRGB:
                    case FormatComponentType::Typeless:
                    case FormatComponentType::Float:
                    case FormatComponentType::Exponential:
                    case FormatComponentType::UnsignedFloat16:
                    case FormatComponentType::SignedFloat16:
                        {
                            const float* clear = defClear;
                            if (s._rtvClearValue[rtv] < clearValues.size())
                                clear = clearValues[s._rtvClearValue[rtv]]._float;
                            glClearBufferfv(GL_COLOR, rtv, clear);
                        }
                        break;
                    }
                }
            }

            float depthClear = 1.0f; GLint stencilClear = 0;
            if (clearDepth && clearStencil) {
                if (s._dsvClearValue < clearValues.size()) {
                    depthClear = clearValues[s._dsvClearValue]._depthStencil._depth;
                    stencilClear = clearValues[s._dsvClearValue]._depthStencil._stencil;
                }
                glClearBufferfi(GL_DEPTH_STENCIL, 0, depthClear, stencilClear);
            } else if (clearDepth) {
                glClearBufferfv(GL_DEPTH, 0, &depthClear);
            } else if (clearStencil)  {
                glClearBufferiv(GL_STENCIL, 0, &stencilClear);
            }
        } else {
            assert(s._rtvCount <= 1);
            bool clearColor = (s._rtvCount != 0) && s._rtvLoad[0] == LoadStore::Clear;

            unsigned clearBits = 0;
            if (clearColor) {
                float defClear[] = {0.f, 0.f, 0.f, 1.0f};
                const float* clear = defClear;
                if (s._rtvClearValue[0] < clearValues.size())
                    clear = clearValues[s._rtvClearValue[0]]._float;
                glClearColor(clear[0], clear[1], clear[2], clear[3]);
                clearBits |= GL_COLOR_BUFFER_BIT;
            }
            if (clearDepth) {
                float clear = 1.0f;
                if (s._dsvClearValue < clearValues.size())
                    clear = clearValues[s._dsvClearValue]._depthStencil._depth;
                glClearDepthf(clear);
                clearBits |= GL_DEPTH_BUFFER_BIT;
            }
            if (clearStencil) {
                unsigned clear = 0;
                if (s._dsvClearValue < clearValues.size())
                    clear = clearValues[s._dsvClearValue]._depthStencil._stencil;
                glClearStencil(clear);
                clearBits |= GL_STENCIL_BUFFER_BIT;
            }

            if (clearBits)
                glClear(clearBits);
        }

        GLenum attachmentsToInvalidate[s_maxMRTs+1];
        unsigned invalidationCount = 0;
        if (fbZeroHack) {
            if (s._rtvCount > 0 && s._rtvLoad[0] == LoadStore::DontCare)
                attachmentsToInvalidate[invalidationCount++] = GL_COLOR;
        } else {
            for (unsigned rtv=0; rtv<s._rtvCount; ++rtv)
                if (s._rtvLoad[rtv] == LoadStore::DontCare)
                    attachmentsToInvalidate[invalidationCount++] = GL_COLOR_ATTACHMENT0 + rtv;
        }

        bool invalidateDepth = false, invalidateStencil = false;
        if (s._dsvHasDepth) {
            invalidateDepth |=
                   s._dsvLoad == LoadStore::DontCare
                || s._dsvLoad == LoadStore::DontCare_RetainStencil
                || s._dsvLoad == LoadStore::DontCare_ClearStencil;
        } else if (s._dsvHasStencil) {
            invalidateStencil |=
                       s._dsvLoad == LoadStore::DontCare
                    || s._dsvLoad == LoadStore::Retain
                    || s._dsvLoad == LoadStore::Clear;
        }

        if (fbZeroHack) {
            if (invalidateDepth) {
                attachmentsToInvalidate[invalidationCount++] = GL_DEPTH;
            }
            if (invalidateStencil) {
                attachmentsToInvalidate[invalidationCount++] = GL_STENCIL;
            }
        } else {
            if (invalidateDepth && invalidateStencil) {
                attachmentsToInvalidate[invalidationCount++] = GL_DEPTH_STENCIL_ATTACHMENT;
            } else if (invalidateDepth) {
                attachmentsToInvalidate[invalidationCount++] = GL_DEPTH_ATTACHMENT;
            } else if (invalidateStencil) {
                attachmentsToInvalidate[invalidationCount++] = GL_STENCIL_ATTACHMENT;
            }
        }

        if (invalidationCount) {
            glInvalidateFramebuffer(GL_FRAMEBUFFER, invalidationCount, attachmentsToInvalidate);
            CheckGLError("After FrameBuffer::BindSubpass() Invalidate framebuffer");
        }
    }

    OpenGL::FrameBuffer* FrameBuffer::GetSubpassUnderlyingFramebuffer(unsigned subpassIndex)
    {
        assert(subpassIndex < _subpasses.size());
        return _subpasses[subpassIndex]._frameBuffer.get();
    }

    const OpenGL::FrameBuffer* FrameBuffer::GetSubpassUnderlyingFramebuffer(unsigned subpassIndex) const
    {
        assert(subpassIndex < _subpasses.size());
        return _subpasses[subpassIndex]._frameBuffer.get();
    }

	FrameBuffer::FrameBuffer() {}
    FrameBuffer::~FrameBuffer() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    static unsigned s_nextSubpass = 0;
    static std::vector<ClearValue> s_clearValues;

    void BeginRenderPass(
        DeviceContext& context,
        FrameBuffer& frameBuffer,
        const FrameBufferDesc& layout,
        const FrameBufferProperties& props,
        IteratorRange<const ClearValue*> clearValues)
    {
        s_nextSubpass = 0;
        s_clearValues.clear();
        s_clearValues.insert(s_clearValues.end(), clearValues.begin(), clearValues.end());
        BeginNextSubpass(context, frameBuffer);
    }

    void BeginNextSubpass(
        DeviceContext& context,
        FrameBuffer& frameBuffer)
    {
        // Queue up the next render targets
        auto subpassIndex = s_nextSubpass;
        if (subpassIndex < frameBuffer.GetSubpassCount())
            frameBuffer.BindSubpass(context, subpassIndex, MakeIteratorRange(s_clearValues));
        ++s_nextSubpass;
    }

    void EndSubpass(DeviceContext& context)
    {
    }

    void EndRenderPass(DeviceContext& context)
    {
        // For compatibility with Vulkan, it makes sense to unbind render targets here. This is important
        // if the render targets will be used as compute shader outputs in follow up steps. It also prevents
        // rendering outside of render passes. But sometimes it will produce redundant calls to OMSetRenderTargets().
    }
    
    unsigned GetCurrentSubpassIndex(DeviceContext& context)
    {
        return s_nextSubpass-1;
    }

}}

