// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "InputLayout.h"
#include "TextureView.h"
#include "../../IDevice_Forward.h"
#include "../../ResourceList.h"
#include "../../Format.h"
#include "../../BufferView.h"
#include "../../../Utility/Threading/ThreadingUtils.h"
#include <assert.h>

@class MTLRenderPassDescriptor;
@class MTLRenderPipelineReflection;
@protocol MTLCommandBuffer;
@protocol MTLDevice;

namespace RenderCore { namespace Metal_AppleMetal
{
    class ShaderResourceView;
    class SamplerState;
    class ConstantBuffer;
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

    class ReflectionInformation
    {
    public:
        /* This contains the vertex and fragment mappings for the hashed name of a shader function argument
         * and its type and index.s
         * For example, the vertex function might have "metalUniforms" with hashName 1234567, type Buffer, index 3,
         * and the fragment function might have "colorMap" with hashName 7654321, type Texture, index 1.
         * The same argument might be in both the vertex and fragment functions, but could have different
         * indices in the argument table.
         */
        enum MappingType {
            Buffer,
            Texture,
            Sampler,
            Unknown
        };
        struct Mapping {
            uint64_t hashName = ~0ull;
            MappingType type = Unknown;
            unsigned index = ~0u;
        };

        std::vector<Mapping> _vfMappings;
        std::vector<Mapping> _ffMappings;

        TBC::OCPtr<MTLRenderPipelineReflection> _debugReflection;
    };

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
        void Bind(const ViewportDesc& viewport);

        void Bind(MTLVertexDescriptor* descriptor);

        enum ShaderTarget { Vertex, Fragment };
        void Bind(id<MTLBuffer> buffer, unsigned offset, unsigned bufferIndex, ShaderTarget target);
        void Bind(const void* bytes, unsigned length, unsigned bufferIndex, ShaderTarget target);
        void Bind(id<MTLTexture> texture, unsigned textureIndex, ShaderTarget target);

        const ReflectionInformation& GetReflectionInformation(TBC::OCPtr<id> vf, TBC::OCPtr<id> ff);

        void FinalizePipeline();

        void Draw(unsigned vertexCount, unsigned startVertexLocation=0);
        void DrawIndexed(unsigned indexCount, unsigned startIndexLocation=0, unsigned baseVertexLocation=0);
        void DrawInstances(unsigned vertexCount, unsigned instanceCount, unsigned startVertexLocation=0);
        void DrawIndexedInstances(unsigned indexCount, unsigned instanceCount, unsigned startIndexLocation=0, unsigned baseVertexLocation=0);

        void            HoldDevice(id<MTLDevice>);
        void            HoldCommandBuffer(id<MTLCommandBuffer>);
        void            ReleaseCommandBuffer();
        void            CreateRenderCommandEncoder(MTLRenderPassDescriptor* renderPassDescriptor);
        void            EndEncoding();
        void            DestroyRenderCommandEncoder();

        void            PushDebugGroup(const char annotationName[]);
        void            PopDebugGroup();

        GraphicsPipeline();
        GraphicsPipeline(const GraphicsPipeline&) = delete;
        GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;
        virtual ~GraphicsPipeline();

    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
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

        DeviceContext();
        DeviceContext(const DeviceContext&) = delete;
        DeviceContext& operator=(const DeviceContext&) = delete;
        virtual ~DeviceContext();
    };


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
