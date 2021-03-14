// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "InputLayout.h"
#include "State.h"
#include "TextureView.h"
#include "../../IDevice_Forward.h"
#include "../../ResourceList.h"
#include "../../Format.h"
#include "../../BufferView.h"
#include "../../../Utility/Threading/ThreadingUtils.h"
#include <assert.h>
#include <Foundation/NSObject.h>

@class MTLRenderPassDescriptor;
@class MTLRenderPipelineReflection;
@protocol MTLBlitCommandEncoder;
@protocol MTLCommandBuffer;
@protocol MTLDevice;
@protocol MTLRenderCommandEncoder;
@protocol MTLFunction;
@protocol MTLDepthStencilState;
@protocol MTLRenderPipelineState;
@protocol MTLDepthStencilState;

namespace RenderCore { class FrameBufferDesc; class AttachmentBlendDesc; }

namespace RenderCore { namespace Metal_AppleMetal
{
    class ShaderResourceView;
    class SamplerState;
    class ConstantBuffer;
    class BoundInputLayout;
    class ShaderProgram;

    class UnboundInterface;

////////////////////////////////////////////////////////////////////////////////////////////////////

    class CommandList
    {
    public:
        CommandList() {}
        CommandList(const CommandList&) = delete;
        CommandList& operator=(const CommandList&) = delete;
    };

    class CapturedStates
    {
    public:
        unsigned        _captureGUID = ~0u;

        std::vector<std::pair<uint64_t, uint64_t>> _customBindings;
    };

    class GraphicsPipeline
    {
    public:
        OCPtr<NSObject<MTLRenderPipelineState>> _underlying;
        OCPtr<MTLRenderPipelineReflection> _reflection;
        OCPtr<NSObject<MTLDepthStencilState>> _depthStencilState;
        unsigned _primitiveType;              // MTLPrimitiveType
        unsigned _stencilReferenceValue;        // todo -- separate stencil reference value from DepthStencilDesc
        unsigned _cullMode;
        unsigned _faceWinding;
        uint64_t _hash;

        uint64_t GetGUID() const { return _hash; }

        #if defined(_DEBUG)
            std::string _shaderSourceIdentifiers;
        #endif
    };

    class GraphicsPipelineBuilder
    {
    public:
        void Bind(const ShaderProgram& shaderProgram);

        void Bind(const AttachmentBlendDesc& desc);
        void Bind(const DepthStencilDesc& depthStencil);
        void Bind(Topology topology);
        void Bind(const RasterizationDesc& rasterizer);

        // DepthStencilDesc ActiveDepthStencilDesc();

        void SetInputLayout(const BoundInputLayout& inputLayout);
        void SetRenderPassConfiguration(const FrameBufferDesc& fbDesc, unsigned subPass);
        void SetRenderPassConfiguration(MTLRenderPassDescriptor* desc, unsigned sampleCount);
        uint64_t GetRenderPassConfigurationHash() const;

        const std::shared_ptr<GraphicsPipeline>& CreatePipeline(ObjectFactory&);
        bool IsPipelineStale() const { return _dirty; }

        GraphicsPipelineBuilder();
        ~GraphicsPipelineBuilder();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
        bool _dirty;
        unsigned _cullMode;
        unsigned _faceWinding;

        OCPtr<NSObject<MTLDepthStencilState>> CreateDepthStencilState(ObjectFactory& factory);
    };

////////////////////////////////////////////////////////////////////////////////////////////////////

    class DeviceContext : public GraphicsPipelineBuilder
    {
    public:
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        //      E N C O D E R
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        void    BindVS(id<MTLBuffer> buffer, unsigned offset, unsigned bufferIndex);
        void    Bind(const IndexBufferView& IB);

        void UnbindInputLayout();

        void SetViewportAndScissorRects(IteratorRange<const Viewport*> viewports, IteratorRange<const ScissorRect*> scissorRects);

        using GraphicsPipelineBuilder::Bind;

        void    Draw(unsigned vertexCount, unsigned startVertexLocation=0);
        void    DrawIndexed(unsigned indexCount, unsigned startIndexLocation=0, unsigned baseVertexLocation=0);
        void    DrawInstances(unsigned vertexCount, unsigned instanceCount, unsigned startVertexLocation=0);
        void    DrawIndexedInstances(unsigned indexCount, unsigned instanceCount, unsigned startIndexLocation=0, unsigned baseVertexLocation=0);

        void    Draw(
            const GraphicsPipeline& pipeline,
            unsigned vertexCount, unsigned startVertexLocation=0);
        void    DrawIndexed(
            const GraphicsPipeline& pipeline,
            unsigned indexCount, unsigned startIndexLocation=0);
        void    DrawInstances(
            const GraphicsPipeline& pipeline,
            unsigned vertexCount, unsigned instanceCount, unsigned startVertexLocation=0);
        void    DrawIndexedInstances(
            const GraphicsPipeline& pipeline,
            unsigned indexCount, unsigned instanceCount, unsigned startIndexLocation=0);

        void    PushDebugGroup(const char annotationName[]);
        void    PopDebugGroup();

        void    BeginRenderPass();
        void    EndRenderPass();
        bool    InRenderPass();
        void    OnEndRenderPass(std::function<void(void)> fn);

        bool    HasEncoder();
        bool    HasRenderCommandEncoder();
        bool    HasBlitCommandEncoder();
        id<MTLRenderCommandEncoder> GetCommandEncoder();
        id<MTLRenderCommandEncoder> GetRenderCommandEncoder();
        id<MTLBlitCommandEncoder> GetBlitCommandEncoder();
        void    CreateRenderCommandEncoder(MTLRenderPassDescriptor* renderPassDescriptor);
        void    CreateBlitCommandEncoder();
        void    EndEncoding();
        void    OnEndEncoding(std::function<void(void)> fn);
        // METAL_TODO: This function shouldn't be needed; it's here only as a temporary substitute for OnEndRenderPass (which is a safe time when we know we will not have a current encoder).
        void    OnDestroyEncoder(std::function<void(void)> fn);
        void    DestroyRenderCommandEncoder();
        void    DestroyBlitCommandEncoder();

        void QueueUniformSet(
            const std::shared_ptr<UnboundInterface>& unboundInterf,
            unsigned streamIdx,
            const UniformsStream& stream);

        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        //      C A P T U R E D S T A T E S
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        CapturedStates* GetCapturedStates();
        void        BeginStateCapture(CapturedStates& capturedStates);
        void        EndStateCapture();

        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        //      U T I L I T Y
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        void            HoldCommandBuffer(id<MTLCommandBuffer>);
        void            ReleaseCommandBuffer();
        id<MTLCommandBuffer>            RetrieveCommandBuffer();

        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        //      C M D L I S T
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        void            BeginCommandList();
        std::shared_ptr<CommandList>  ResolveCommandList();
        void            CommitCommandList(CommandList& commandList);

        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        //      D E V I C E
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        std::shared_ptr<IDevice> GetDevice();

        static void PrepareForDestruction(IDevice* device);

        static const std::shared_ptr<DeviceContext>& Get(IThreadContext& threadContext);

        DeviceContext(std::shared_ptr<IDevice> device);
        DeviceContext(const DeviceContext&) = delete;
        DeviceContext& operator=(const DeviceContext&) = delete;
        virtual ~DeviceContext();

    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        void FinalizePipeline();
    };

}}
