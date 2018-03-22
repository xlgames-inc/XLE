// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "InputLayout.h"
#include "ObjectFactory.h"
#include "TextureView.h"
#include "Format.h"     // (for FeatureSet)
#include "../IDeviceOpenGLES.h"
#include "../../IDevice_Forward.h"
#include "../../ResourceList.h"
#include "../../Format.h"
#include "../../BufferView.h"
#include "../../../Utility/Threading/ThreadingUtils.h"
#include <assert.h>
#include "IncludeGLES.h"

namespace RenderCore { namespace Metal_OpenGLES
{
    class ShaderResourceView;
    class SamplerState;
    class BoundInputLayout;
    class ShaderProgram;
    class BlendState;
    class ViewportDesc;

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
        void Bind(const ShaderProgram& shaderProgram);

        void Bind(const BlendState& blender);
        void Bind(const RasterizationDesc& rasterizer);
        void Bind(const DepthStencilDesc& depthStencil);
        void Bind(Topology topology);
        void Bind(const ViewportDesc& viewport);

        void Draw(unsigned vertexCount, unsigned startVertexLocation=0);
        void DrawIndexed(unsigned indexCount, unsigned startIndexLocation=0, unsigned baseVertexLocation=0);
        void DrawInstances(unsigned vertexCount, unsigned instanceCount, unsigned startVertexLocation=0);
        void DrawIndexedInstances(unsigned indexCount, unsigned instanceCount, unsigned startIndexLocation=0, unsigned baseVertexLocation=0);

        FeatureSet::BitField GetFeatureSet() const { return _featureSet; }

        GraphicsPipeline(FeatureSet::BitField featureSet);
        GraphicsPipeline(const GraphicsPipeline&) = delete;
        GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;
        ~GraphicsPipeline();

    private:
        unsigned    _nativeTopology;
        unsigned    _indicesFormat;
        unsigned    _indexFormatBytes;
        FeatureSet::BitField _featureSet;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////

    class DeviceContext : public GraphicsPipeline
    {
    public:
        void            BeginCommandList();
        CommandListPtr  ResolveCommandList();
        void            CommitCommandList(CommandList& commandList);

        static void PrepareForDestruction(IDevice* device);

        static const std::shared_ptr<DeviceContext>& Get(IThreadContext& threadContext);

        DeviceContext(FeatureSet::BitField featureSet);
        DeviceContext(const DeviceContext&) = delete;
        DeviceContext& operator=(const DeviceContext&) = delete;
        ~DeviceContext();
    private:
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

}}
