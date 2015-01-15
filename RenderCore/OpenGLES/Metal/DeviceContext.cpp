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

#include <GLES2/gl2.h>
#include <EGL/egl.h>            // (brings in windows.h on Win32)

namespace RenderCore { namespace Metal_OpenGLES
{
    void DeviceContext::Bind(const IndexBuffer& ib, NativeFormat::Enum indexFormat, unsigned offset)
    {
            // (it seems that index formats are always 16 bit for OpenGLES?)
        assert(indexFormat == NativeFormat::R16_UINT);
        assert(offset == 0);    // (not supported currently... But we could safe it up for the draw call)
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)ib.GetUnderlying());
    }

    void DeviceContext::Bind(const BoundInputLayout& inputLayout)
    {
        _savedInputLayout = inputLayout;
    }

    void DeviceContext::Bind(Topology::Enum topology)
    {
        _nativeTopology = unsigned(topology);
    }

    void DeviceContext::Bind(const ShaderProgram& shaderProgram)
    {
        glUseProgram((GLuint)shaderProgram.GetUnderlying());
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

    DeviceContext::DeviceContext(EGLDisplay display, EGLContext underlyingContext)
    :       _display(display)
    ,       _underlyingContext(underlyingContext)
    {
            //  -- todo -- we can create another gl context and
            //      bind it to the current thread (if we don't want to
            //      use the immediate context)
        _nativeTopology = GL_TRIANGLES;
    }

    DeviceContext::~DeviceContext()
    {
        eglDestroyContext(_display, _underlyingContext);
    }

    void                            DeviceContext::BeginCommandList()
    {   
            //
            //      Bind this context to the current thread, so we can
            //      start using it.
            //
        eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE, _underlyingContext);
    }

    intrusive_ptr<CommandList>         DeviceContext::ResolveCommandList()
    {
        return intrusive_ptr<CommandList>();
    }

    void                            DeviceContext::CommitCommandList(CommandList& commandList)
    {

    }

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

    ObjectFactory::ObjectFactory() {}
    ObjectFactory::ObjectFactory(IDevice* device) {}
    ObjectFactory::ObjectFactory(const OpenGL::Resource& resource) {}
}}

