// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "InputLayout.h"
#include "ShaderResource.h"
#include "../../IDevice_Forward.h"
#include "../../ResourceList.h"
#include "../../Format.h"
#include "../../BufferView.h"
#include "../../../Utility/Threading/ThreadingUtils.h"
#include <assert.h>

namespace RenderCore { namespace Metal_AppleMetal
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

        static const std::shared_ptr<DeviceContext>& Get(IThreadContext& threadContext);

        DeviceContext();
        DeviceContext(const DeviceContext&) = delete;
        DeviceContext& operator=(const DeviceContext&) = delete;
        ~DeviceContext();
    };

////////////////////////////////////////////////////////////////////////////////////////////////////

    class ObjectFactory
    {
    public:
        ObjectFactory();
        ~ObjectFactory();

        ObjectFactory& operator=(const ObjectFactory&) = delete;
        ObjectFactory(const ObjectFactory&) = delete;
    };

    ObjectFactory& GetObjectFactory(IDevice& device);
    ObjectFactory& GetObjectFactory(DeviceContext&);
    ObjectFactory& GetObjectFactory();

////////////////////////////////////////////////////////////////////////////////////////////////////

    template<int Count> void GraphicsPipeline::Bind(const ResourceList<VertexBufferView, Count>& VBs)
    {
        assert(0);
    }

    template<int Count> void GraphicsPipeline::BindPS(const ResourceList<ShaderResourceView, Count>& shaderResources)
    {
        assert(0);
    }

    template<int Count> void GraphicsPipeline::BindPS(const ResourceList<SamplerState, Count>& samplerStates)
    {
        assert(0);
    }

    template<int Count> void GraphicsPipeline::BindVS(const ResourceList<ConstantBuffer, Count>& constantBuffers)
    {
    }

}}
