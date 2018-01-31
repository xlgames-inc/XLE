// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "InputLayout.h"
#include "IndexedGLType.h"
#include "ShaderResource.h"
#include "../IDeviceOpenGLES.h"
#include "../../IDevice_Forward.h"
#include "../../ResourceList.h"
#include "../../Format.h"
#include "../../BufferView.h"
#include "../../../Utility/Threading/ThreadingUtils.h"
#include <assert.h>
#include "IncludeGLES.h"

// typedef void*       EGLDisplay;
// typedef void*       EGLContext;

namespace RenderCore { namespace Metal_OpenGLES
{
    class ShaderResourceView;
    class SamplerState;
    class ConstantBuffer;
    class BoundInputLayout;
    class ShaderProgram;
    class BlendState;

    class RasterizationDesc;
    class DepthStencilDesc;

////////////////////////////////////////////////////////////////////////////////////////////////////

    class CommandList : public RefCountedObject
    {
    public:
        CommandList() {}
        CommandList(const CommandList&) = delete;
        CommandList& operator=(const CommandList&) = delete;
    };

    using CommandListPtr = intrusive_ptr<CommandList>;

    class GraphicsPipeline
    {
    public:
        template<int Count> void Bind(const ResourceList<VertexBufferView, Count>& VBs);
        void Bind(const IndexBufferView& IB);

        template<int Count> void BindPS(const ResourceList<ShaderResourceView, Count>& shaderResources);
        template<int Count> void BindPS(const ResourceList<SamplerState, Count>& samplerStates);
        template<int Count> void BindVS(const ResourceList<ConstantBuffer, Count>& constantBuffers);
        void Bind(const ShaderProgram& shaderProgram);

        void Bind(const BlendState& blender);
        void Bind(const RasterizationDesc& rasterizer);
        void Bind(const DepthStencilDesc& depthStencil);
        void Bind(Topology topology);

        void Draw(unsigned vertexCount, unsigned startVertexLocation=0);
        void DrawIndexed(unsigned indexCount, unsigned startIndexLocation=0, unsigned baseVertexLocation=0);
#if HACK_PLATFORM_IOS
        void DrawIndexedInstances(unsigned indexCount, unsigned instanceCount, unsigned startIndexLocation=0, unsigned baseVertexLocation=0);
#endif

    protected:
        unsigned    _nativeTopology;
        unsigned    _indicesFormat;
        unsigned    _indexFormatBytes;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////

    class DeviceContext : public GraphicsPipeline
    {
    public:
        void            BeginCommandList();
        CommandListPtr  ResolveCommandList();
        void            CommitCommandList(CommandList& commandList);

        // static intrusive_ptr<DeviceContext> GetImmediateContext(IDevice* device);
        // static intrusive_ptr<DeviceContext> CreateDeferredContext(IDevice* device);

        static void PrepareForDestruction(IDevice* device);

        unsigned FeatureLevel() const { return 300u; }

        // EGL::Context        GetUnderlying() { return _underlyingContext; }

        static const std::shared_ptr<DeviceContext>& Get(IThreadContext& threadContext);

        DeviceContext();
        DeviceContext(const DeviceContext&) = delete;
        DeviceContext& operator=(const DeviceContext&) = delete;
        ~DeviceContext();
    private:
        // EGL::Display        _display;
        // EGL::Context        _underlyingContext;
        // DeviceContext(EGLDisplay display, EGLContext underlyingContext);

        friend class Device;
        friend class DeviceOpenGLES;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    #pragma warning(push)
    #pragma warning(disable:4127)       // conditional expression is constant

    template<int Count> void GraphicsPipeline::Bind(const ResourceList<VertexBufferView, Count>& VBs)
    {
        static_assert(Count <= 1, "Cannot bind more than one vertex buffer in OpenGLES 2.0");
        assert(VBs._startingPoint == 0);
        if (Count == 1) {
            assert(VBs._buffers[0]->_offset == 0);
            glBindBuffer(GL_ARRAY_BUFFER, GetBufferRawGLHandle(*VBs._buffers[0]->_resource));
        }
    }

    #pragma warning(pop)

    template<int Count> void GraphicsPipeline::BindPS(const ResourceList<ShaderResourceView, Count>& shaderResources)
    {
        for (int c=0; c<Count; ++c) {
            glActiveTexture(GL_TEXTURE0 + c + shaderResources._startingPoint);
            glBindTexture(GL_TEXTURE_2D, shaderResources._buffers[c]->AsRawGLHandle());
        }
    }

    template<int Count> void GraphicsPipeline::BindPS(const ResourceList<SamplerState, Count>& samplerStates)
    {
        for (int c=0; c<Count; ++c) {
            samplerStates._buffers[c].Apply(c+samplerStates._startingPoint);
        }
    }

    template<int Count> void GraphicsPipeline::BindVS(const ResourceList<ConstantBuffer, Count>& constantBuffers)
    {
    }

}}
