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
        void Bind(const IndexBufferView& IB);
        void UnbindInputLayout();

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

        unsigned    _activeVertexAttrib;
        unsigned    _instancedVertexAttrib;

    private:
        unsigned    _nativeTopology;
        unsigned    _indicesFormat;
        unsigned    _indexFormatBytes;
        unsigned    _indexBufferOffsetBytes;
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
