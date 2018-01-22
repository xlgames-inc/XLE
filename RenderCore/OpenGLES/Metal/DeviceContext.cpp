// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DeviceContext.h"
#include "State.h"
#include "Shader.h"
#include "InputLayout.h"
#include "Buffer.h"
#include "Format.h"
#include "../../IThreadContext.h"

#include "IncludeGLES.h"
#include <assert.h>

#define COMPOSITOR_EGL      1
#define COMPOSITOR_EAGL     2
#define COMPOSITOR COMPOSITOR_EAGL

#if COMPOSITOR == COMPOSITOR_EGL
    #include <EGL/egl.h>            // (brings in windows.h on Win32)
#endif

namespace RenderCore { namespace Metal_OpenGLES
{
    static GLenum AsGLIndexBufferType(Format idxFormat)
    {
        GLenum glFormat = GL_UNSIGNED_SHORT;
        switch (idxFormat) {
        case Format::R8_UINT: glFormat = GL_UNSIGNED_BYTE; break;
        case Format::R16_UINT: glFormat = GL_UNSIGNED_SHORT; break;
        case Format::R32_UINT: glFormat = GL_UNSIGNED_INT; break;
        default: assert(0);
        }
        return glFormat;
    }

    void GraphicsPipeline::Bind(const IndexBufferView& IB)
    {
        assert(IB._offset == 0);    // (not supported currently... But we could safe it up for the draw call)
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GetBufferRawGLHandle(*IB._resource));

        // Note that Format::R32_UINT is only supported on OGLES3.0+
        assert(IB._indexFormat == Format::R32_UINT || IB._indexFormat == Format::R16_UINT || IB._indexFormat == Format::R8_UINT);
        _indicesFormat = AsGLIndexBufferType(IB._indexFormat);
        _indexFormatBytes = BitsPerPixel(IB._indexFormat) / 8;
    }

    void GraphicsPipeline::Bind(Topology topology)
    {
        _nativeTopology = AsGLenum(topology);
    }

    void GraphicsPipeline::Bind(const ShaderProgram& shaderProgram)
    {
        glUseProgram(shaderProgram.GetUnderlying()->AsRawGLHandle());
    }

    void GraphicsPipeline::Bind(const BlendState& blender)
    {
        blender.Apply();
    }

    static void SetUnmanagedStates()
    {
        // The following render states are not managed by RenderCore, but we can use this function
        // to set them all to rational defaults. In many cases, this may be just the same as the
        // default; but it's still useful incase some other system is setting them, and we want to
        // reset to something that won't interfere with our rendering.

        // RasterizationDesc unmanaged states
        glLineWidth(1.f);
        glPolygonOffset(0.f, 0.f);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DITHER);       // (not supported in D3D11)
    }

    void GraphicsPipeline::Bind(const RasterizationDesc& desc)
    {
        if (desc._cullMode != CullMode::None) {
            glEnable(GL_CULL_FACE);
            glCullFace(AsGLenum(desc._cullMode));
        } else {
            glDisable(GL_CULL_FACE);
        }
        glFrontFace(AsGLenum(desc._frontFaceWinding));
    }

    void GraphicsPipeline::Bind(const DepthStencilDesc& desc)
    {
        glDepthFunc(AsGLenum(desc._depthTest));
        glDepthMask(desc._depthWrite ? GL_TRUE : GL_FALSE);

        if (desc._depthTest != CompareOp::Always) {
            glEnable(GL_DEPTH_TEST);
        } else {
            glDisable(GL_DEPTH_TEST);
        }

        if (desc._stencilEnable) {
            glStencilFuncSeparate(
                GL_FRONT,
                AsGLenum(desc._frontFaceStencil._comparisonOp),
                desc._stencilReference,
                desc._stencilWriteMask);
            glStencilOpSeparate(
                GL_FRONT,
                AsGLenum(desc._frontFaceStencil._failOp),
                AsGLenum(desc._frontFaceStencil._depthFailOp),
                AsGLenum(desc._frontFaceStencil._passOp));
            glStencilFuncSeparate(
                GL_BACK,
                AsGLenum(desc._backFaceStencil._comparisonOp),
                desc._stencilReference,
                desc._stencilWriteMask);
            glStencilOpSeparate(
                GL_BACK,
                AsGLenum(desc._backFaceStencil._failOp),
                AsGLenum(desc._backFaceStencil._depthFailOp),
                AsGLenum(desc._backFaceStencil._passOp));
            glStencilMaskSeparate(GL_FRONT_AND_BACK, desc._stencilReadMask);
            glEnable(GL_STENCIL_TEST);
        } else {
            glDisable(GL_STENCIL_TEST);
        }
    }

    void GraphicsPipeline::Draw(unsigned vertexCount, unsigned startVertexLocation)
    {
        glDrawArrays(GLenum(_nativeTopology), startVertexLocation, vertexCount);
    }

    void GraphicsPipeline::DrawIndexed(unsigned indexCount, unsigned startIndexLocation, unsigned baseVertexLocation)
    {
        assert(baseVertexLocation==0);  // (doesn't seem to be supported. Maybe best to remove it from the interface)
        glDrawElements(
            GLenum(_nativeTopology), GLsizei(indexCount),
            GLenum(_indicesFormat),
            (const void*)(size_t)(_indexFormatBytes * startIndexLocation));
    }

    void GraphicsPipeline::DrawIndexedInstances(unsigned indexCount, unsigned instanceCount, unsigned startIndexLocation, unsigned baseVertexLocation)
    {
        assert(baseVertexLocation==0);  // (doesn't seem to be supported. Maybe best to remove it from the interface)
        glDrawElementsInstanced(
            GLenum(_nativeTopology), GLsizei(indexCount),
            GLenum(_indicesFormat),
            (const void*)(size_t)(_indexFormatBytes * startIndexLocation),
            instanceCount);
    }

#if 0
    DeviceContext::DeviceContext(EGLDisplay display, EGLContext underlyingContext)
    :       _display(display)
    ,       _underlyingContext(underlyingContext)
    {
            //  -- todo -- we can create another gl context and
            //      bind it to the current thread (if we don't want to
            //      use the immediate context)
        _nativeTopology = GL_TRIANGLES;
    }
#endif

    DeviceContext::DeviceContext()
    {
        _indicesFormat = AsGLIndexBufferType(Format::R16_UINT);
        _indexFormatBytes = 2;
        _nativeTopology = GL_TRIANGLES;
    }

    DeviceContext::~DeviceContext()
    {
        #if COMPOSITOR == COMPOSITOR_EGL
            eglDestroyContext(_display, _underlyingContext);
        #endif
    }

    void                            DeviceContext::BeginCommandList()
    {   
            //
            //      Bind this context to the current thread, so we can
            //      start using it.
            //
        #if COMPOSITOR == COMPOSITOR_EGL
            eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE, _underlyingContext);
        #endif
    }

    intrusive_ptr<CommandList>         DeviceContext::ResolveCommandList()
    {
        return intrusive_ptr<CommandList>();
    }

    void                            DeviceContext::CommitCommandList(CommandList& commandList)
    {

    }

    std::shared_ptr<DeviceContext> DeviceContext::Get(IThreadContext& threadContext)
    {
        auto tc = (IThreadContextOpenGLES*)threadContext.QueryInterface(typeid(IThreadContextOpenGLES).hash_code());
        if (tc) {
            return tc->GetUnderlying();
        }
        return nullptr;
    }

#if 0
    intrusive_ptr<DeviceContext>        DeviceContext::GetImmediateContext(IDevice* device)
    {
        if (device) {
            IDeviceOpenGLES* openGLESDevice = 
                (IDeviceOpenGLES*)device->QueryInterface(*(GUID*)nullptr);
            if (openGLESDevice) {
                return openGLESDevice->GetImmediateContext();
            }
        }
        return intrusive_ptr<DeviceContext>();
    }

    intrusive_ptr<DeviceContext>        DeviceContext::CreateDeferredContext(IDevice* device)
    {
        if (device) {
            IDeviceOpenGLES* openGLESDevice = 
                (IDeviceOpenGLES*)device->QueryInterface(*(GUID*)nullptr);
            if (openGLESDevice) {
                return openGLESDevice->CreateDeferredContext();
            }
        }
        return intrusive_ptr<DeviceContext>();
    }
#endif

}}

