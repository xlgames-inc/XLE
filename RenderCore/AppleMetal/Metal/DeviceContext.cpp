// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DeviceContext.h"
#include "State.h"
#include "Shader.h"
#include "InputLayout.h"
#include "Buffer.h"
#include "Format.h"
#include "../IDeviceAppleMetal.h"
#include "../../IThreadContext.h"
#include "../../Types.h"
#include "../../../ConsoleRig/Log.h"
#include "../../../ConsoleRig/LogUtil.h"
#include "../../../Externals/Misc/OCPtr.h"
#include "../../../Utility/MemoryUtils.h"
#include <assert.h>
#include "IncludeAppleMetal.h"

namespace RenderCore { namespace Metal_AppleMetal
{
    static const Resource& AsResource(const IResource* rp)
    {
        static Resource dummy;
        auto* res = (Resource*)const_cast<IResource*>(rp)->QueryInterface(typeid(Resource).hash_code());
        if (res)
            return *res;
        return dummy;
    }

    MTLPrimitiveType AsMTLenum(Topology topology)
    {
        switch (topology) {
            case Topology::PointList: return MTLPrimitiveTypePoint;
            case Topology::LineList: return MTLPrimitiveTypeLine;
            case Topology::LineStrip: return MTLPrimitiveTypeLineStrip;
            case Topology::TriangleList: return MTLPrimitiveTypeTriangle;
            case Topology::TriangleStrip: return MTLPrimitiveTypeTriangleStrip;
            default: assert(0); return MTLPrimitiveTypeTriangle;
        }
    }

    MTLCullMode AsMTLenum(CullMode cullMode)
    {
        switch (cullMode) {
            case CullMode::Front: return MTLCullModeFront;
            case CullMode::Back: return MTLCullModeBack;
            case CullMode::None: return MTLCullModeNone;
        }
    }

    MTLWinding AsMTLenum(FaceWinding faceWinding)
    {
        switch (faceWinding) {
            case FaceWinding::CCW: return MTLWindingCounterClockwise;
            case FaceWinding::CW: return MTLWindingClockwise;
            default: assert(0); return MTLWindingCounterClockwise;
        }
    }

    static MTLIndexType AsMTLIndexType(Format idxFormat)
    {
        switch (idxFormat) {
            case Format::R16_UINT: return MTLIndexTypeUInt16;
            case Format::R32_UINT: return MTLIndexTypeUInt32;
            default: assert(0); return MTLIndexTypeUInt16;
        }
    }

    static MTLCompareFunction AsMTLCompareFunction(CompareOp op)
    {
        switch (op) {
            case CompareOp::Never: return MTLCompareFunctionNever;
            case CompareOp::Less: return MTLCompareFunctionLess;
            case CompareOp::Equal: return MTLCompareFunctionEqual;
            case CompareOp::LessEqual: return MTLCompareFunctionLessEqual;
            case CompareOp::Greater: return MTLCompareFunctionGreater;
            case CompareOp::NotEqual: return MTLCompareFunctionNotEqual;
            case CompareOp::GreaterEqual: return MTLCompareFunctionGreaterEqual;
            case CompareOp::Always: return MTLCompareFunctionAlways;
            default: assert(0); return MTLCompareFunctionAlways;
        }
    }

    static MTLStencilOperation AsMTLStencilOperation(StencilOp op)
    {
        switch (op) {
            case StencilOp::Keep: return MTLStencilOperationKeep; // same as StencilOp::DontWrite
            case StencilOp::Zero: return MTLStencilOperationZero;
            case StencilOp::Replace: return MTLStencilOperationReplace;
            case StencilOp::IncreaseSat: return MTLStencilOperationIncrementClamp;
            case StencilOp::DecreaseSat: return MTLStencilOperationDecrementClamp;
            case StencilOp::Invert: return MTLStencilOperationInvert;
            case StencilOp::Increase: return MTLStencilOperationIncrementWrap;
            case StencilOp::Decrease: return MTLStencilOperationDecrementWrap;
            default: assert(0); return MTLStencilOperationKeep;
        }
    }

    static MTLBlendFactor AsMTLBlendFactor(Blend blend)
    {
        switch (blend) {
            case Blend::Zero: return MTLBlendFactorZero;
            case Blend::One: return MTLBlendFactorOne;
            case Blend::SrcColor: return MTLBlendFactorSourceColor;
            case Blend::InvSrcColor: return MTLBlendFactorOneMinusSourceColor;
            case Blend::DestColor: return MTLBlendFactorDestinationColor;
            case Blend::InvDestColor: return MTLBlendFactorOneMinusDestinationColor;
            case Blend::SrcAlpha: return MTLBlendFactorSourceAlpha;
            case Blend::InvSrcAlpha: return MTLBlendFactorOneMinusSourceAlpha;
            case Blend::DestAlpha: return MTLBlendFactorDestinationAlpha;
            case Blend::InvDestAlpha: return MTLBlendFactorOneMinusDestinationAlpha;
            default: assert(0); return MTLBlendFactorOne;
        }
    }

    static MTLBlendOperation AsMTLBlendOperation(BlendOp op)
    {
        switch (op) {
            case BlendOp::Add: return MTLBlendOperationAdd;
            case BlendOp::Subtract: return MTLBlendOperationSubtract;
            case BlendOp::RevSubtract: return MTLBlendOperationReverseSubtract;
            case BlendOp::Min: return MTLBlendOperationMin;
            case BlendOp::Max: return MTLBlendOperationMax;
            default: assert(0); return MTLBlendOperationAdd;
        }
    }

    static MTLViewport AsMTLViewport(const ViewportDesc& viewport)
    {
        MTLViewport vp;
        vp.originX = viewport.TopLeftX;
        vp.originY = viewport.TopLeftY;
        vp.width = viewport.Width;
        vp.height = viewport.Height;
        vp.znear = viewport.MinDepth;
        vp.zfar = viewport.MaxDepth;
        return vp;
    }

    static void CheckCommandBufferError(id<MTLCommandBuffer> buffer)
    {
        if (buffer.error) {
            Log(Warning) << "================> " << buffer.error << std::endl;
        }
    }

    class GraphicsPipelineBuilder::Pimpl
    {
    public:
        TBC::OCPtr<MTLRenderPipelineDescriptor> _pipelineDescriptor; // For the current draw
    };

    void GraphicsPipelineBuilder::SetShaderFunctions(id<MTLFunction> vf, id<MTLFunction> ff)
    {
        assert(_pimpl->_pipelineDescriptor);
        [_pimpl->_pipelineDescriptor setVertexFunction:vf];
        [_pimpl->_pipelineDescriptor setFragmentFunction:ff];
        _dirty = true;
    }

    void GraphicsPipelineBuilder::Bind(const AttachmentBlendDesc& desc)
    {
        assert(_pimpl->_pipelineDescriptor);
        /* Metal TODO -- may need to support more than the first color attachment */
        if (_pimpl->_pipelineDescriptor.get().colorAttachments[0].pixelFormat == MTLPixelFormatInvalid) {
            /* A color attachment may not be bound - for example, if only depth attachment is bound -
             * so there's no need to set up blending.
             */
            return;
        }

        _pimpl->_pipelineDescriptor.get().colorAttachments[0].blendingEnabled = desc._blendEnable;
        _pimpl->_pipelineDescriptor.get().colorAttachments[0].rgbBlendOperation = AsMTLBlendOperation(desc._colorBlendOp);
        _pimpl->_pipelineDescriptor.get().colorAttachments[0].alphaBlendOperation = AsMTLBlendOperation(desc._alphaBlendOp);

        _pimpl->_pipelineDescriptor.get().colorAttachments[0].sourceRGBBlendFactor = AsMTLBlendFactor(desc._srcColorBlendFactor);
        _pimpl->_pipelineDescriptor.get().colorAttachments[0].sourceAlphaBlendFactor = AsMTLBlendFactor(desc._srcAlphaBlendFactor);
        _pimpl->_pipelineDescriptor.get().colorAttachments[0].destinationRGBBlendFactor = AsMTLBlendFactor(desc._dstColorBlendFactor);
        _pimpl->_pipelineDescriptor.get().colorAttachments[0].destinationAlphaBlendFactor = AsMTLBlendFactor(desc._dstAlphaBlendFactor);

        _pimpl->_pipelineDescriptor.get().colorAttachments[0].writeMask =
            ((desc._writeMask & ColorWriteMask::Red)    ? MTLColorWriteMaskRed   : 0) |
            ((desc._writeMask & ColorWriteMask::Green)  ? MTLColorWriteMaskGreen : 0) |
            ((desc._writeMask & ColorWriteMask::Blue)   ? MTLColorWriteMaskBlue  : 0) |
            ((desc._writeMask & ColorWriteMask::Alpha)  ? MTLColorWriteMaskAlpha : 0);

        _dirty = true;
    }

    void GraphicsPipelineBuilder::SetRasterSampleCount(unsigned sampleCount)
    {
        assert(_pimpl->_pipelineDescriptor);
        if ([_pimpl->_pipelineDescriptor.get() respondsToSelector:@selector(setRasterSampleCount:)]) {
            _pimpl->_pipelineDescriptor.get().rasterSampleCount = std::max(1u, sampleCount);
        } else {
            // Some drivers don't appear to have the "rasterSampleCount". It appears to be IOS 11+ only.
            // Falling back to the older name "sampleCount" -- documentation in the header suggests
            // they are the same thing
            _pimpl->_pipelineDescriptor.get().sampleCount = std::max(1u, sampleCount);
        }
        _dirty = true;
    }

    void GraphicsPipelineBuilder::Bind(MTLVertexDescriptor* descriptor)
    {
        // KenD -- the vertex descriptor isn't necessary if the vertex function does not have an input argument declared [[stage_in]] */
        assert(_pimpl->_pipelineDescriptor);
        [_pimpl->_pipelineDescriptor setVertexDescriptor:descriptor];
        _dirty = true;
    }

    void GraphicsPipelineBuilder::UnbindInputLayout()
    {
    }

    GraphicsPipeline GraphicsPipelineBuilder::MakePipeline(ObjectFactory& factory)
    {
        auto renderPipelineState = factory.CreateRenderPipelineState(_pimpl->_pipelineDescriptor.get(), true);
        if (renderPipelineState._error) {
            Log(Error) << "Failed to create render pipeline state: " << renderPipelineState._error << std::endl;
        }
        assert(!renderPipelineState._error);

        // DavidJ -- note -- we keep the state _pimpl->_pipelineDescriptor from here.
        //      what happens if we continue to change it? It doesn't impact the compiled state we
        //      just made, right?

        _dirty = false;
        return { renderPipelineState._renderPipelineState, renderPipelineState._reflection };
    }

    void GraphicsPipelineBuilder::SetRenderPassStates(MTLRenderPassDescriptor* renderPassDescriptor)
    {
        /* The pixel formats of the attachments in the MTLRenderPipelineDescriptor must match
         * the pixel formats of the associated attachments' textures in the MTLRenderPassDescriptor. */
        /* There is a maximum of 8 color attachments, defined by MTLRenderPipelineColorAttachmentDescriptor */
        /* Metal TODO -- for now, when binding the attachmentBlendDesc, we only alter the first color attachment;
         * we may need to support more than the first color attachment */
        const unsigned maxColorAttachments = 8u;
        for (unsigned i=0; i<maxColorAttachments; ++i) {
            MTLRenderPassColorAttachmentDescriptor* renderPassColorAttachmentDesc = renderPassDescriptor.colorAttachments[i];
            if (renderPassColorAttachmentDesc.texture) {
                if (i > 0) {
                    assert(0); // If this assert hits, we need to support more color attachments (such as multiple render target methods)
                }
                _pimpl->_pipelineDescriptor.get().colorAttachments[i].pixelFormat = renderPassColorAttachmentDesc.texture.pixelFormat;
            }
        }

        _pimpl->_pipelineDescriptor.get().depthAttachmentPixelFormat = MTLPixelFormatInvalid;
        _pimpl->_pipelineDescriptor.get().stencilAttachmentPixelFormat = MTLPixelFormatInvalid;

        if (renderPassDescriptor.depthAttachment.texture) {
            _pimpl->_pipelineDescriptor.get().depthAttachmentPixelFormat = renderPassDescriptor.depthAttachment.texture.pixelFormat;
        }
        if (renderPassDescriptor.stencilAttachment.texture) {
            _pimpl->_pipelineDescriptor.get().stencilAttachmentPixelFormat = renderPassDescriptor.stencilAttachment.texture.pixelFormat;
        } else if (renderPassDescriptor.depthAttachment.texture) {
            // If the depth texture is a depth/stencil format, we must ensure that both the stencil and depth fields agree
            auto depthFormat = renderPassDescriptor.depthAttachment.texture.pixelFormat;
            if (    depthFormat == MTLPixelFormatDepth24Unorm_Stencil8 || depthFormat == MTLPixelFormatDepth32Float_Stencil8
                ||  depthFormat == MTLPixelFormatX32_Stencil8 || depthFormat == MTLPixelFormatX24_Stencil8) {
                _pimpl->_pipelineDescriptor.get().stencilAttachmentPixelFormat = depthFormat;
            }
        }

        _dirty = true;
    }

    GraphicsPipelineBuilder::GraphicsPipelineBuilder()
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_pipelineDescriptor = TBC::moveptr([[MTLRenderPipelineDescriptor alloc] init]);
        _dirty = true;
    }

    GraphicsPipelineBuilder::~GraphicsPipelineBuilder()
    {
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    class DeviceContext::Pimpl
    {
    public:
        TBC::OCPtr<id> _commandBuffer; // For the duration of the frame
        TBC::OCPtr<id> _commandEncoder; // For the current subpass
        TBC::OCPtr<id> _device;

        class QueuedUniformSet
        {
        public:
            std::shared_ptr<UnboundInterface> _unboundInterf;
            unsigned _streamIdx;

            std::vector<ConstantBufferView> _constantBuffers;
            std::vector<const ShaderResourceView*> _resources;
            std::vector<const SamplerState*> _samplers;
        };
        std::vector<QueuedUniformSet> _queuedUniformSets;

        CapturedStates _capturedStates;

        MTLPrimitiveType _activePrimitiveType;
        DepthStencilDesc _activeDepthStencilDesc;
        ViewportDesc _activeViewport;
        TBC::OCPtr<id> _activeIndexBuffer; // MTLBuffer

        MTLIndexType _indexType;
        unsigned _indexFormatBytes;
        unsigned _indexOffset;

        TBC::OCPtr<MTLRenderPipelineReflection> _graphicsPipelineReflection;
        uint64_t _boundVSArgs = 0ull, _boundPSArgs = 0ull;

        std::vector<std::function<void(void)>> _onEndEncodingFunctions;

        unsigned offsetToIndex(unsigned index) {
            return index * _indexFormatBytes + _indexOffset;
        }
    };

    void DeviceContext::Bind(const IndexBufferView& IB)
    {
        auto resource = AsResource(IB._resource);
        id<MTLBuffer> buffer = resource.GetBuffer();
        if (!buffer)
            Throw(::Exceptions::BasicLabel("Attempting to bind index buffer view with invalid resource"));
        _pimpl->_activeIndexBuffer = buffer;
        _pimpl->_indexType = AsMTLIndexType(IB._indexFormat);
        _pimpl->_indexFormatBytes = BitsPerPixel(IB._indexFormat) / 8;
        _pimpl->_indexOffset = IB._offset;
    }

    void DeviceContext::BindVS(id<MTLBuffer> buffer, unsigned offset, unsigned bufferIndex)
    {
        assert(_pimpl->_commandEncoder);
        [_pimpl->_commandEncoder setVertexBuffer:buffer offset:offset atIndex:bufferIndex];
    }

    void DeviceContext::Bind(const ShaderProgram& shaderProgram)
    {
        SetShaderFunctions(shaderProgram._vf, shaderProgram._ff);
    }

    void DeviceContext::Bind(const RasterizationDesc& desc)
    {
        assert(_pimpl->_commandEncoder);
        [_pimpl->_commandEncoder setFrontFacingWinding:AsMTLenum(desc._frontFaceWinding)];
        [_pimpl->_commandEncoder setCullMode:AsMTLenum(desc._cullMode)];
    }

    void DeviceContext::Bind(const DepthStencilDesc& desc)
    {
        _pimpl->_activeDepthStencilDesc = desc;

        /* KenD -- Metal TODO -- depth/stencil states are expensive to construct; cache them */
        assert(_pimpl->_device);
        assert(_pimpl->_commandEncoder);
        MTLDepthStencilDescriptor* mtlDesc = [[MTLDepthStencilDescriptor alloc] init];
        mtlDesc.depthCompareFunction = AsMTLCompareFunction(desc._depthTest);
        mtlDesc.depthWriteEnabled = desc._depthWrite;

        MTLStencilDescriptor* frontStencilDesc = [[MTLStencilDescriptor alloc] init];
        frontStencilDesc.stencilCompareFunction = AsMTLCompareFunction(desc._frontFaceStencil._comparisonOp);
        frontStencilDesc.stencilFailureOperation = AsMTLStencilOperation(desc._frontFaceStencil._failOp);
        frontStencilDesc.depthFailureOperation = AsMTLStencilOperation(desc._frontFaceStencil._depthFailOp);
        frontStencilDesc.depthStencilPassOperation = AsMTLStencilOperation(desc._frontFaceStencil._passOp);
        frontStencilDesc.readMask = desc._stencilReadMask;
        frontStencilDesc.writeMask = desc._stencilWriteMask;
        mtlDesc.frontFaceStencil = frontStencilDesc;

        MTLStencilDescriptor* backStencilDesc = [[MTLStencilDescriptor alloc] init];
        backStencilDesc.stencilCompareFunction = AsMTLCompareFunction(desc._backFaceStencil._comparisonOp);
        backStencilDesc.stencilFailureOperation = AsMTLStencilOperation(desc._backFaceStencil._failOp);
        backStencilDesc.depthFailureOperation = AsMTLStencilOperation(desc._backFaceStencil._depthFailOp);
        backStencilDesc.depthStencilPassOperation = AsMTLStencilOperation(desc._backFaceStencil._passOp);
        backStencilDesc.readMask = desc._stencilReadMask;
        backStencilDesc.writeMask = desc._stencilWriteMask;
        mtlDesc.backFaceStencil = backStencilDesc;

        id<MTLDepthStencilState> dss = [_pimpl->_device newDepthStencilStateWithDescriptor:mtlDesc];
        [_pimpl->_commandEncoder setDepthStencilState:dss];

        if (desc._stencilEnable) {
            [_pimpl->_commandEncoder setStencilReferenceValue:desc._stencilReference];
        }

        [dss release];
        [frontStencilDesc release];
        [backStencilDesc release];
        [mtlDesc release];
    }

    DepthStencilDesc DeviceContext::ActiveDepthStencilDesc()
    {
        return _pimpl->_activeDepthStencilDesc;
    }

    void DeviceContext::Bind(const ViewportDesc& viewport)
    {
        _pimpl->_activeViewport = viewport;

        /* KenD -- because we may not have an encoder yet, delay setting the viewport until later */
        if (_pimpl->_commandEncoder)
            [_pimpl->_commandEncoder setViewport:AsMTLViewport(_pimpl->_activeViewport)];
    }

    ViewportDesc DeviceContext::GetViewport()
    {
        return _pimpl->_activeViewport;
    }

    void DeviceContext::Bind(Topology topology)
    {
        _pimpl->_activePrimitiveType = AsMTLenum(topology);
    }

    void DeviceContext::FinalizePipeline()
    {
        if (GraphicsPipelineBuilder::IsDirty() || !_pimpl->_graphicsPipelineReflection) {
            auto pipelineState = GraphicsPipelineBuilder::MakePipeline(GetObjectFactory());
            [_pimpl->_commandEncoder setRenderPipelineState:pipelineState._underlying];
            _pimpl->_graphicsPipelineReflection = std::move(pipelineState._reflection);
            _pimpl->_boundVSArgs = 0;
            _pimpl->_boundPSArgs = 0;
        }

        uint64_t boundVSArgs = 0, boundPSArgs = 0;
        for (const auto&qus:_pimpl->_queuedUniformSets) {
            UniformsStream stream {
                MakeIteratorRange(qus._constantBuffers),
                MakeIteratorRange(qus._resources).Cast<const void*const*>(),
                MakeIteratorRange(qus._samplers).Cast<const void*const*>()
            };
            auto bound = BoundUniforms::Apply_UnboundInterfacePath(*this, _pimpl->_graphicsPipelineReflection.get(), *qus._unboundInterf, qus._streamIdx, stream);
            assert((boundVSArgs & bound._vsArguments) == 0);
            assert((boundPSArgs & bound._psArguments) == 0);
            boundVSArgs |= bound._vsArguments;
            boundPSArgs |= bound._psArguments;
        }
        _pimpl->_boundVSArgs |= boundVSArgs;
        _pimpl->_boundPSArgs |= boundPSArgs;
        _pimpl->_queuedUniformSets.clear();

        // Bind standins for anything that have never been bound to anything correctly
        BoundUniforms::Apply_Standins(*this, _pimpl->_graphicsPipelineReflection.get(), ~_pimpl->_boundVSArgs, ~_pimpl->_boundPSArgs);
    }

    void DeviceContext::QueueUniformSet(
        const std::shared_ptr<UnboundInterface>& unboundInterf,
        unsigned streamIdx,
        const UniformsStream& stream)
    {
        Pimpl::QueuedUniformSet qus;
        qus._unboundInterf = unboundInterf;
        qus._streamIdx = streamIdx;
        qus._constantBuffers = std::vector<ConstantBufferView>{stream._constantBuffers.begin(), stream._constantBuffers.end()};
        qus._resources = std::vector<const ShaderResourceView*>{(const ShaderResourceView*const*)stream._resources.begin(), (const ShaderResourceView*const*)stream._resources.end()};
        qus._samplers = std::vector<const SamplerState*>{(const SamplerState*const*)stream._samplers.begin(), (const SamplerState*const*)stream._samplers.end()};

        for (auto& q:_pimpl->_queuedUniformSets)
            if (q._streamIdx == streamIdx) {
                q = std::move(qus);
                return;
            }
        _pimpl->_queuedUniformSets.emplace_back(std::move(qus));
    }

    void DeviceContext::Draw(unsigned vertexCount, unsigned startVertexLocation)
    {
        FinalizePipeline();
        assert(_pimpl->_commandEncoder);

        [_pimpl->_commandEncoder drawPrimitives:_pimpl->_activePrimitiveType
                                    vertexStart:startVertexLocation
                                    vertexCount:vertexCount];
    }

    void DeviceContext::DrawIndexed(unsigned indexCount, unsigned startIndexLocation, unsigned baseVertexLocation)
    {
        assert(baseVertexLocation==0);

        FinalizePipeline();
        assert(_pimpl->_commandEncoder);

        [_pimpl->_commandEncoder drawIndexedPrimitives:_pimpl->_activePrimitiveType
                                            indexCount:indexCount
                                             indexType:_pimpl->_indexType
                                           indexBuffer:_pimpl->_activeIndexBuffer
                                     indexBufferOffset:_pimpl->offsetToIndex(startIndexLocation)];
    }

    void DeviceContext::DrawInstances(unsigned vertexCount, unsigned instanceCount, unsigned startVertexLocation)
    {
        FinalizePipeline();
        assert(_pimpl->_commandEncoder);

        [_pimpl->_commandEncoder drawPrimitives:_pimpl->_activePrimitiveType
                                    vertexStart:startVertexLocation
                                    vertexCount:vertexCount
                                  instanceCount:instanceCount];
    }

    void DeviceContext::DrawIndexedInstances(unsigned indexCount, unsigned instanceCount, unsigned startIndexLocation, unsigned baseVertexLocation)
    {
        assert(baseVertexLocation==0);

        FinalizePipeline();
        assert(_pimpl->_commandEncoder);

        [_pimpl->_commandEncoder drawIndexedPrimitives:_pimpl->_activePrimitiveType
                                            indexCount:indexCount
                                             indexType:_pimpl->_indexType
                                           indexBuffer:_pimpl->_activeIndexBuffer
                                     indexBufferOffset:_pimpl->offsetToIndex(startIndexLocation)
                                         instanceCount:instanceCount];
    }


    void            DeviceContext::CreateRenderCommandEncoder(MTLRenderPassDescriptor* renderPassDescriptor)
    {
        CheckCommandBufferError(_pimpl->_commandBuffer);

        assert(!_pimpl->_commandEncoder);
        _pimpl->_commandEncoder = [_pimpl->_commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        assert(_pimpl->_commandEncoder);

        [_pimpl->_commandEncoder setViewport:AsMTLViewport(_pimpl->_activeViewport)];

        _pimpl->_boundVSArgs = 0;
        _pimpl->_boundPSArgs = 0;
        _pimpl->_graphicsPipelineReflection = nullptr;
        _pimpl->_queuedUniformSets.clear();
    }

    void            DeviceContext::EndEncoding()
    {
        assert(_pimpl->_commandEncoder);

        [_pimpl->_commandEncoder endEncoding];

        for (auto fn: _pimpl->_onEndEncodingFunctions) {
            fn();
        }
    }

    void            DeviceContext::OnEndEncoding(std::function<void(void)> fn)
    {
        assert(_pimpl->_commandEncoder);
        _pimpl->_onEndEncodingFunctions.push_back(fn);
    }

    void            DeviceContext::DestroyRenderCommandEncoder()
    {
        assert(_pimpl->_commandEncoder);
        _pimpl->_commandEncoder = nullptr;
    }

    id<MTLRenderCommandEncoder> DeviceContext::GetCommandEncoder()
    {
        assert(_pimpl->_commandEncoder);
        return _pimpl->_commandEncoder;
    }

    void            DeviceContext::HoldDevice(id<MTLDevice> device)
    {
        _pimpl->_device = device;
        assert(_pimpl->_device);
    }

    void            DeviceContext::HoldCommandBuffer(id<MTLCommandBuffer> commandBuffer)
    {
        /* Hold for the duration of the frame */
        assert(!_pimpl->_commandBuffer);
        _pimpl->_commandBuffer = commandBuffer;

        CheckCommandBufferError(_pimpl->_commandBuffer);
    }

    void            DeviceContext::ReleaseCommandBuffer()
    {
        CheckCommandBufferError(_pimpl->_commandBuffer);

        /* The command encoder should have been released when the subpass was finished,
         * now we release the command buffer */
        assert(!_pimpl->_commandEncoder);
        assert(_pimpl->_commandBuffer);
        _pimpl->_commandBuffer = nullptr;
    }

    id<MTLCommandBuffer>            DeviceContext::RetrieveCommandBuffer()
    {
        return _pimpl->_commandBuffer;
    }

    void            DeviceContext::PushDebugGroup(const char annotationName[])
    {
        // assert(_pimpl->_commandEncoder);
        [_pimpl->_commandEncoder pushDebugGroup:[NSString stringWithCString:annotationName encoding:NSUTF8StringEncoding]];
    }

    void            DeviceContext::PopDebugGroup()
    {
        // assert(_pimpl->_commandEncoder);
        [_pimpl->_commandEncoder popDebugGroup];
    }

    CapturedStates* DeviceContext::GetCapturedStates() { return &_pimpl->_capturedStates; }
    void        DeviceContext::BeginStateCapture(CapturedStates& capturedStates) {}
    void        DeviceContext::EndStateCapture() {}

    DeviceContext::DeviceContext()
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_activePrimitiveType = MTLPrimitiveTypeTriangle;
        _pimpl->_indexType = MTLIndexTypeUInt16;
        _pimpl->_indexFormatBytes = 2; // two bytes for MTLIndexTypeUInt16
        _pimpl->_indexOffset = 0;
    }

    DeviceContext::~DeviceContext()
    {
    }

    void                            DeviceContext::BeginCommandList()
    {   
    }

    intrusive_ptr<CommandList>         DeviceContext::ResolveCommandList()
    {
        return intrusive_ptr<CommandList>();
    }

    void                            DeviceContext::CommitCommandList(CommandList& commandList)
    {
    }

    const std::shared_ptr<DeviceContext>& DeviceContext::Get(IThreadContext& threadContext)
    {
        static std::shared_ptr<DeviceContext> dummy;
        auto* tc = (IThreadContextAppleMetal*)threadContext.QueryInterface(typeid(IThreadContextAppleMetal).hash_code());
        if (tc) return tc->GetDeviceContext();
        return dummy;
    }
}}
