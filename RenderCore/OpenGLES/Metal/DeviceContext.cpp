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
    void DeviceContext::Bind(const IndexBuffer& ib, Format indexFormat, unsigned offset)
    {
            // (it seems that index formats are always 16 bit for OpenGLES?)
        assert(indexFormat == Format::R16_UINT);
        assert(offset == 0);    // (not supported currently... But we could safe it up for the draw call)
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib.GetUnderlying()->AsRawGLHandle());
    }

    void DeviceContext::Bind(const BoundInputLayout& inputLayout)
    {
        _savedInputLayout = inputLayout;
    }

    void DeviceContext::Bind(Topology topology)
    {
        _nativeTopology = unsigned(topology);
    }

    void DeviceContext::Bind(const ShaderProgram& shaderProgram)
    {
        glUseProgram(shaderProgram.GetUnderlying()->AsRawGLHandle());
    }

    void DeviceContext::Bind(const RasterizerState& rasterizer)
    {
        rasterizer.Apply();
    }

    void DeviceContext::Bind(const BlendState& blender)
    {
        blender.Apply();
    }

    void DeviceContext::Bind(const DepthStencilState& depthStencil)
    {
        depthStencil.Apply();
    }

    void DeviceContext::Draw(unsigned vertexCount, unsigned startVertexLocation)
    {
        _savedInputLayout.Apply(0, _savedVertexBufferStride);    // (must come after binding vertex buffers)
        glDrawArrays(GLenum(_nativeTopology), startVertexLocation, vertexCount);
    }

    void DeviceContext::DrawIndexed(unsigned indexCount, unsigned startIndexLocation, unsigned baseVertexLocation)
    {
        assert(baseVertexLocation==0);  // (doesn't seem to be supported. Maybe best to remove it from the interface)
        _savedInputLayout.Apply(0, _savedVertexBufferStride);
        glDrawArrays(GLenum(_nativeTopology), startIndexLocation, indexCount);
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

    DeviceContext::DeviceContext() {}

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

