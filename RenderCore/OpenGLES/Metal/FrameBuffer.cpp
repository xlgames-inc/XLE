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
        
        assert(subpasses.size() <= s_maxSubpasses);
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
				sp._rtvLoad[r] = attachmentView._loadFromPreviousPhase;
				sp._rtvClearValue[r] = clearValueIterator++;
			}

			if (spDesc._depthStencil._resourceName != ~0u) {
				auto resource = namedResources.GetResource(spDesc._depthStencil._resourceName);
				if (!resource)
					Throw(::Exceptions::BasicLabel("Could not find attachment resource for DSV in FrameBuffer::FrameBuffer"));
				sp._dsv = *dsvPool.GetView(resource, spDesc._depthStencil._window);
				sp._dsvLoad = spDesc._depthStencil._loadFromPreviousPhase;
				sp._dsvClearValue = clearValueIterator++;
			}

            GLenum drawBuffers[dimof(Subpass::_rtvs)] = { GL_NONE, GL_NONE, GL_NONE, GL_NONE };
            unsigned colorAttachmentIterator = 0;

            #if defined(_DEBUG)
                int maxDrawBuffers = 0;
                glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
                assert(sp._rtvCount <= maxDrawBuffers);
            #endif

            sp._frameBuffer = factory.CreateFrameBuffer();
            glBindFramebuffer(GL_FRAMEBUFFER, sp._frameBuffer->AsRawGLHandle());
            for (unsigned rtv=0; rtv<sp._rtvCount; ++rtv) {
                assert(sp._rtvs[rtv].IsGood());

                auto& res = *sp._rtvs[rtv].GetResource();
                if (res.IsBackBuffer()) {
                    drawBuffers[rtv] = GL_BACK;
                } else {
                    BindToFramebuffer(GL_COLOR_ATTACHMENT0 + colorAttachmentIterator, res, sp._rtvs[rtv]._window);
                    drawBuffers[rtv] = GL_COLOR_ATTACHMENT0 + colorAttachmentIterator;
                    ++colorAttachmentIterator;
                }
            }
            if (sp._dsv.IsGood()) {
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

            glDrawBuffers(sp._rtvCount, drawBuffers);

            // auto validationFlag = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            // assert(validationFlag == GL_FRAMEBUFFER_COMPLETE);
        }
        _subpassCount = (unsigned)subpasses.size();
    }

    void FrameBuffer::BindSubpass(DeviceContext& context, unsigned subpassIndex, IteratorRange<const ClearValue*> clearValues) const
    {
        if (subpassIndex >= _subpassCount)
            Throw(::Exceptions::BasicLabel("Attempting to set invalid subpass"));

        const auto& s = _subpasses[subpassIndex];
        glBindFramebuffer(GL_FRAMEBUFFER, s._frameBuffer->AsRawGLHandle());

        for (unsigned rtv=0; rtv<s._rtvCount; ++rtv) {
            auto attachmentIdx = s._rtvs[rtv];
            auto load = s._rtvLoad[rtv];
            if (load == LoadStore::Clear) {
                glClearBufferfv(GL_COLOR, rtv, clearValues[s._rtvClearValue[rtv]]._float);
            }
        }

        if (s._dsvLoad == LoadStore::Clear_ClearStencil) {
            glClearBufferfi(GL_DEPTH_STENCIL, 0, clearValues[s._dsvClearValue]._depthStencil._depth, clearValues[s._dsvClearValue]._depthStencil._stencil);
        } else if (s._dsvLoad == LoadStore::Clear || s._dsvLoad == LoadStore::Clear_RetainStencil) {
            glClearBufferfi(GL_DEPTH, 0, clearValues[s._dsvClearValue]._depthStencil._depth, clearValues[s._dsvClearValue]._depthStencil._stencil);
        } else if (s._dsvLoad == LoadStore::DontCare_ClearStencil || s._dsvLoad == LoadStore::Retain_ClearStencil) {
            glClearBufferfi(GL_STENCIL, 0, clearValues[s._dsvClearValue]._depthStencil._depth, clearValues[s._dsvClearValue]._depthStencil._stencil);
        }
    }

    OpenGL::FrameBuffer* FrameBuffer::GetSubpassUnderlyingFramebuffer(unsigned subpassIndex)
    {
        assert(subpassIndex < _subpassCount);
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
        frameBuffer.BindSubpass(context, subpassIndex, MakeIteratorRange(s_clearValues));
        ++s_nextSubpass;
    }

    void EndRenderPass(DeviceContext& context)
    {
        // For compatibility with Vulkan, it makes sense to unbind render targets here. This is important
        // if the render targets will be used as compute shader outputs in follow up steps. It also prevents
        // rendering outside of render passes. But sometimes it will produce redundant calls to OMSetRenderTargets().
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class FrameBufferPool::Pimpl 
    {
    public:
    };

    std::shared_ptr<FrameBuffer> FrameBufferPool::BuildFrameBuffer(
		ObjectFactory& factory,
		const FrameBufferDesc& desc,
        const FrameBufferProperties& props,
        const INamedAttachments& namedResources,
        uint64 hashName)
    {
        return std::make_shared<FrameBuffer>(factory, desc, namedResources);
    }

    FrameBufferPool::FrameBufferPool()
    {
        _pimpl = std::make_unique<Pimpl>();
    }

    FrameBufferPool::~FrameBufferPool()
    {}



}}

