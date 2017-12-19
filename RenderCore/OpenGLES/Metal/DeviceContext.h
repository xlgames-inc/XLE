// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "InputLayout.h"
#include "../../ResourceList.h"
#include "IndexedGLType.h"
#include "../IDeviceOpenGLES.h"
#include "../../../Utility/Threading/ThreadingUtils.h"
#include "ShaderResource.h"
#include "../../IDevice_Forward.h"
#include "IncludeGLES.h"

// typedef void*       EGLDisplay;
// typedef void*       EGLContext;

namespace RenderCore { namespace Metal_OpenGLES
{
    class VertexBuffer;
    class IndexBuffer;
    class ShaderResourceView;
    class SamplerState;
    class ConstantBuffer;
    class BoundInputLayout;
    class ShaderProgram;
    class BlendState;

    class RasterizationDesc;
    class DepthStencilDesc;

    class CommandList : public RefCountedObject, noncopyable
    {
    public:
    };

    using CommandListPtr = intrusive_ptr<CommandList>;

    class DeviceContext
    {
    public:
        template<int Count> void Bind(const ResourceList<VertexBuffer, Count>& VBs, unsigned stride, unsigned offset);
        template<int Count> void BindPS(const ResourceList<ShaderResourceView, Count>& shaderResources);
        template<int Count> void BindPS(const ResourceList<SamplerState, Count>& samplerStates);
        template<int Count> void BindVS(const ResourceList<ConstantBuffer, Count>& constantBuffers);
        
        void Bind(const IndexBuffer& ib, Format indexFormat, unsigned offset=0);
        void Bind(const BoundInputLayout& inputLayout);
        void Bind(Topology topology);
        void Bind(const ShaderProgram& shaderProgram);
        void Bind(const BlendState& blender);

        void Bind(const RasterizationDesc& rasterizer);
        void Bind(const DepthStencilDesc& depthStencil);

        void Draw(unsigned vertexCount, unsigned startVertexLocation=0);
        void DrawIndexed(unsigned indexCount, unsigned startIndexLocation=0, unsigned baseVertexLocation=0);

        void            BeginCommandList();
        CommandListPtr  ResolveCommandList();
        void            CommitCommandList(CommandList& commandList);

        // static intrusive_ptr<DeviceContext> GetImmediateContext(IDevice* device);
        // static intrusive_ptr<DeviceContext> CreateDeferredContext(IDevice* device);

        static void PrepareForDestruction(IDevice* device);

        unsigned FeatureLevel() const { return 300u; }

        // EGL::Context        GetUnderlying() { return _underlyingContext; }

        static std::shared_ptr<DeviceContext> Get(IThreadContext& threadContext);

        DeviceContext();
        DeviceContext(const DeviceContext&) = delete;
        DeviceContext& operator=(const DeviceContext&) = delete;
        ~DeviceContext();
    private:
        unsigned            _nativeTopology;
        // EGL::Display        _display;
        // EGL::Context        _underlyingContext;
        BoundInputLayout    _savedInputLayout;
        unsigned            _savedVertexBufferStride;

        // DeviceContext(EGLDisplay display, EGLContext underlyingContext);

        friend class Device;
        friend class DeviceOpenGLES;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    #pragma warning(push)
    #pragma warning(disable:4127)       // conditional expression is constant
    
    template<int Count> void DeviceContext::Bind(const ResourceList<VertexBuffer, Count>& VBs, unsigned stride, unsigned offset)
    {
        static_assert(Count <= 1, "Cannot bind more than one vertex buffer in OpenGLES 2.0");

        if (Count == 1) {
            glBindBuffer(GL_ARRAY_BUFFER, (GLuint)VBs._buffers[0]);
            _savedVertexBufferStride = stride;
        }
    }

    #pragma warning(pop)

    template<int Count> void DeviceContext::BindPS(const ResourceList<ShaderResourceView, Count>& shaderResources)
    {
        for (int c=0; c<Count; ++c) {
            glActiveTexture(GL_TEXTURE0 + c + shaderResources._startingPoint);
            glBindTexture(GL_TEXTURE_2D, (GLuint)shaderResources._buffers[c]);
        }
    }

    template<int Count> void DeviceContext::BindPS(const ResourceList<SamplerState, Count>& samplerStates)
    {
        for (int c=0; c<Count; ++c) {
            samplerStates._buffers[c].Apply(c+samplerStates._startingPoint);
        }
    }

    template<int Count> void DeviceContext::BindVS(const ResourceList<ConstantBuffer, Count>& constantBuffers)
    {
    }

}}
