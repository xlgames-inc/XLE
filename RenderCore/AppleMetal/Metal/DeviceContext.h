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

@class MTLRenderPassDescriptor;
@class MTLRenderPipelineReflection;
@protocol MTLCommandBuffer;
@protocol MTLDevice;
@protocol MTLRenderCommandEncoder;
@protocol MTLFunction;

namespace RenderCore { namespace Metal_AppleMetal
{
    class ShaderResourceView;
    class SamplerState;
    class ConstantBuffer;
    class BoundInputLayout;
    class ShaderProgram;
    class ViewportDesc;

    class RasterizationDesc;
    class DepthStencilDesc;
    class UnboundInterface;

////////////////////////////////////////////////////////////////////////////////////////////////////

    class CommandList : public RefCountedObject
    {
    public:
        CommandList() {}
        CommandList(const CommandList&) = delete;
        CommandList& operator=(const CommandList&) = delete;
    };

    using CommandListPtr = intrusive_ptr<CommandList>;

    class CapturedStates
    {
    public:
        unsigned        _captureGUID = ~0u;

        std::vector<std::pair<uint64_t, uint64_t>> _customBindings;
    };

    class GraphicsPipeline
    {
    public:
        TBC::OCPtr<NSObject<MTLRenderPipelineState>> _underlying;
        TBC::OCPtr<MTLRenderPipelineReflection> _reflection;
    };

    class GraphicsPipelineBuilder
    {
    public:
        void SetShaderFunctions(id<MTLFunction> vf, id<MTLFunction> ff);
        void Bind(const AttachmentBlendDesc& desc);

        void SetRasterSampleCount(unsigned sampleCount);
        void SetIBFormat(Format format);

        void Bind(MTLVertexDescriptor* descriptor);
        void UnbindInputLayout();

        void SetRenderPassStates(MTLRenderPassDescriptor* renderPassDescriptor);

        GraphicsPipeline MakePipeline(ObjectFactory&);
        bool IsDirty() const { return _dirty; }

        GraphicsPipelineBuilder();
        ~GraphicsPipelineBuilder();
    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
        bool _dirty;
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

        void    Bind(const ShaderProgram& shaderProgram);
        void    Bind(const RasterizationDesc& rasterizer);
        void    Bind(const DepthStencilDesc& depthStencil);
        DepthStencilDesc ActiveDepthStencilDesc();
        void    Bind(const ViewportDesc& viewport);
        ViewportDesc GetViewport();
        void    Bind(Topology topology);

        using GraphicsPipelineBuilder::Bind;

        void    Draw(unsigned vertexCount, unsigned startVertexLocation=0);
        void    DrawIndexed(unsigned indexCount, unsigned startIndexLocation=0, unsigned baseVertexLocation=0);
        void    DrawInstances(unsigned vertexCount, unsigned instanceCount, unsigned startVertexLocation=0);
        void    DrawIndexedInstances(unsigned indexCount, unsigned instanceCount, unsigned startIndexLocation=0, unsigned baseVertexLocation=0);

        void    PushDebugGroup(const char annotationName[]);
        void    PopDebugGroup();

        id<MTLRenderCommandEncoder> GetCommandEncoder();
        void    CreateRenderCommandEncoder(MTLRenderPassDescriptor* renderPassDescriptor);
        void    EndEncoding();
        void    DestroyRenderCommandEncoder();

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
        void            HoldDevice(id<MTLDevice>);
        void            HoldCommandBuffer(id<MTLCommandBuffer>);
        void            ReleaseCommandBuffer();
        id<MTLCommandBuffer>            RetrieveCommandBuffer();

        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        //      C M D L I S T
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        void            BeginCommandList();
        CommandListPtr  ResolveCommandList();
        void            CommitCommandList(CommandList& commandList);

        static void PrepareForDestruction(IDevice* device);

        static const std::shared_ptr<DeviceContext>& Get(IThreadContext& threadContext);

        DeviceContext();
        DeviceContext(const DeviceContext&) = delete;
        DeviceContext& operator=(const DeviceContext&) = delete;
        virtual ~DeviceContext();

    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        void FinalizePipeline();
    };

}}
