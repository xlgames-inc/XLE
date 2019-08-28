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
#include "../../FrameBufferDesc.h"
#include "../../../ConsoleRig/Log.h"
#include "../../../ConsoleRig/LogUtil.h"
#include "../../../Externals/Misc/OCPtr.h"
#include "../../../Utility/MemoryUtils.h"
#include <assert.h>
#include <map>
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
        AttachmentBlendDesc _attachmentBlendDesc;
        MTLPrimitiveType _activePrimitiveType;
        DepthStencilDesc _activeDepthStencilDesc;
        TBC::OCPtr<MTLVertexDescriptor> _vertexDescriptor;

        uint32_t _shaderGuid = 0;
        uint64_t _rpHash = 0;
        uint64_t _inputLayoutGuid = 0;
        uint64_t _absHash = 0, _dssHash = 0;

        std::map<uint64_t, std::shared_ptr<GraphicsPipeline>> _prebuiltPipelines;

        #if defined(_DEBUG)
            std::string _shaderSourceIdentifiers;
        #endif
    };

    void GraphicsPipelineBuilder::Bind(const ShaderProgram& shaderProgram)
    {
        assert(_pimpl->_pipelineDescriptor);
        [_pimpl->_pipelineDescriptor setVertexFunction:shaderProgram._vf];
        [_pimpl->_pipelineDescriptor setFragmentFunction:shaderProgram._ff];
        _pimpl->_pipelineDescriptor.get().rasterizationEnabled = shaderProgram._ff != nullptr;
        _pimpl->_shaderGuid = shaderProgram.GetGUID();
        _dirty = true;

        #if defined(_DEBUG)
            _pimpl->_shaderSourceIdentifiers = shaderProgram.SourceIdentifiers();
        #endif
    }

    void GraphicsPipelineBuilder::Bind(const AttachmentBlendDesc& desc)
    {
        _pimpl->_attachmentBlendDesc = desc;
        _pimpl->_absHash = _pimpl->_attachmentBlendDesc.Hash();
        _dirty = true;
    }

    void GraphicsPipelineBuilder::SetRenderPassConfiguration(const FrameBufferProperties& fbProps, const FrameBufferDesc& fbDesc, unsigned subPass)
    {
        assert(subPass < fbDesc.GetSubpasses().size());
        assert(_pimpl->_pipelineDescriptor);

        auto& subpass = fbDesc.GetSubpasses()[subPass];

        // Derive the sample count directly from the framebuffer properties & the subpass.
        // TODO -- we should also enable specifying the sample count via a MSAA sampling state structure

        unsigned msaaAttachments = 0, singleSampleAttachments = 0;
        for (const auto&a:subpass._output) {
            if (a._window._flags & TextureViewDesc::Flags::ForceSingleSample) {
                ++singleSampleAttachments;
            } else {
                auto& attach = fbDesc.GetAttachments()[a._resourceName]._desc;
                if (attach._flags & AttachmentDesc::Flags::Multisampled) {
                    ++msaaAttachments;
                } else {
                    ++singleSampleAttachments;
                }
            }
        }

        unsigned sampleCount = 1;
        if (msaaAttachments == 0) {
            // no msaa attachments,
        } else if (fbProps._samples._sampleCount > 1) {
            if (singleSampleAttachments > 0) {
                Log(Warning) << "Subpass has a mixture of MSAA and non-MSAA attachments. MSAA can't be enabled, so falling back to single sample mode" << std::endl;
            } else {
                sampleCount = fbProps._samples._sampleCount;
            }
        }

        if ([_pimpl->_pipelineDescriptor.get() respondsToSelector:@selector(setRasterSampleCount:)]) {
            if (sampleCount != _pimpl->_pipelineDescriptor.get().rasterSampleCount) {
                _pimpl->_pipelineDescriptor.get().rasterSampleCount = sampleCount;
                _dirty = true;
            }
        } else {
            // Some drivers don't appear to have the "rasterSampleCount". It appears to be IOS 11+ only.
            // Falling back to the older name "sampleCount" -- documentation in the header suggests
            // they are the same thing
            if (sampleCount != _pimpl->_pipelineDescriptor.get().sampleCount) {
                _pimpl->_pipelineDescriptor.get().sampleCount = sampleCount;
                _dirty = true;
            }
        }

        uint64_t rpHash = sampleCount;

        // Figure out the pixel formats for each of the attachments (including depth/stencil)
        const unsigned maxColorAttachments = 8u;
        for (unsigned i=0; i<maxColorAttachments; ++i) {
            if (i < subpass._output.size()) {
                assert(subpass._output[i]._resourceName);
                const auto& window = subpass._output[i]._window;
                const auto& attachment = fbDesc.GetAttachments()[subpass._output[i]._resourceName]._desc;
                auto finalFormat = ResolveFormat(attachment._format, window._format, FormatUsage::RTV);
                auto mtlFormat = AsMTLPixelFormat(finalFormat);
                _pimpl->_pipelineDescriptor.get().colorAttachments[i].pixelFormat = mtlFormat;
                rpHash = HashCombine(mtlFormat, rpHash);
            } else {
                _pimpl->_pipelineDescriptor.get().colorAttachments[i].pixelFormat = MTLPixelFormatInvalid;
            }
        }

        _pimpl->_pipelineDescriptor.get().depthAttachmentPixelFormat = MTLPixelFormatInvalid;
        _pimpl->_pipelineDescriptor.get().stencilAttachmentPixelFormat = MTLPixelFormatInvalid;

        if (subpass._depthStencil._resourceName != SubpassDesc::Unused._resourceName) {
            const auto& window = subpass._depthStencil._window;
            const auto& attachment = fbDesc.GetAttachments()[subpass._depthStencil._resourceName]._desc;
            auto finalFormat = ResolveFormat(attachment._format, window._format, FormatUsage::DSV);

            auto components = GetComponents(finalFormat);
            auto mtlFormat = AsMTLPixelFormat(finalFormat);
            if (components == FormatComponents::Depth) {
                _pimpl->_pipelineDescriptor.get().depthAttachmentPixelFormat = mtlFormat;
            } else if (components == FormatComponents::Stencil) {
                _pimpl->_pipelineDescriptor.get().stencilAttachmentPixelFormat = mtlFormat;
            } else if (components == FormatComponents::DepthStencil) {
                _pimpl->_pipelineDescriptor.get().depthAttachmentPixelFormat = mtlFormat;
                _pimpl->_pipelineDescriptor.get().stencilAttachmentPixelFormat = mtlFormat;
            } else {
                assert(0);      // format doesn't appear to have either depth or stencil components
            }

            rpHash = HashCombine(mtlFormat, rpHash);
        }

        _dirty = true;
        _pimpl->_rpHash = rpHash;
    }

    uint64_t GraphicsPipelineBuilder::GetRenderPassConfigurationHash() const
    {
        return _pimpl->_rpHash;
    }

    void GraphicsPipelineBuilder::SetRenderPassConfiguration(MTLRenderPassDescriptor* renderPassDescriptor, unsigned sampleCount)
    {
        sampleCount = std::max(sampleCount, 1u);
        if ([_pimpl->_pipelineDescriptor.get() respondsToSelector:@selector(setRasterSampleCount:)]) {
            if (sampleCount != _pimpl->_pipelineDescriptor.get().rasterSampleCount) {
                _pimpl->_pipelineDescriptor.get().rasterSampleCount = sampleCount;
                _dirty = true;
            }
        } else {
            // Some drivers don't appear to have the "rasterSampleCount". It appears to be IOS 11+ only.
            // Falling back to the older name "sampleCount" -- documentation in the header suggests
            // they are the same thing
            if (sampleCount != _pimpl->_pipelineDescriptor.get().sampleCount) {
                _pimpl->_pipelineDescriptor.get().sampleCount = sampleCount;
                _dirty = true;
            }
        }

        uint64_t rpHash = sampleCount;

        const unsigned maxColorAttachments = 8u;
        for (unsigned i=0; i<maxColorAttachments; ++i) {
            MTLRenderPassColorAttachmentDescriptor* renderPassColorAttachmentDesc = renderPassDescriptor.colorAttachments[i];
            if (renderPassColorAttachmentDesc.texture) {
                _pimpl->_pipelineDescriptor.get().colorAttachments[i].pixelFormat = renderPassColorAttachmentDesc.texture.pixelFormat;
                rpHash = HashCombine(renderPassColorAttachmentDesc.texture.pixelFormat, rpHash);
            } else {
                _pimpl->_pipelineDescriptor.get().colorAttachments[i].pixelFormat = MTLPixelFormatInvalid;
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
            if (depthFormat == MTLPixelFormatDepth32Float_Stencil8 ||  depthFormat == MTLPixelFormatX32_Stencil8
                #if PLATFORMOS_TARGET == PLATFORMOS_OSX
                    || depthFormat == MTLPixelFormatDepth24Unorm_Stencil8 || depthFormat == MTLPixelFormatX24_Stencil8
                #endif
                ) {
                _pimpl->_pipelineDescriptor.get().stencilAttachmentPixelFormat = depthFormat;
            }
        }

        if (renderPassDescriptor.depthAttachment.texture) {
            rpHash = HashCombine(renderPassDescriptor.depthAttachment.texture.pixelFormat, rpHash);
        } else if (renderPassDescriptor.stencilAttachment.texture)
            rpHash = HashCombine(renderPassDescriptor.stencilAttachment.texture.pixelFormat, rpHash);

        _dirty = true;
        _pimpl->_rpHash = rpHash;
    }

    void GraphicsPipelineBuilder::SetInputLayout(const BoundInputLayout& inputLayout)
    {
        // KenD -- the vertex descriptor isn't necessary if the vertex function does not have an input argument declared [[stage_in]] */
        assert(_pimpl->_pipelineDescriptor);
        auto* descriptor = inputLayout.GetVertexDescriptor();
        if (descriptor != _pimpl->_vertexDescriptor.get()) {
            _pimpl->_vertexDescriptor = descriptor;
            _pimpl->_inputLayoutGuid = inputLayout.GetGUID();
            _dirty = true;
        }
    }

    void GraphicsPipelineBuilder::Bind(const DepthStencilDesc& desc)
    {
        _pimpl->_activeDepthStencilDesc = desc;
        _pimpl->_dssHash = _pimpl->_activeDepthStencilDesc.Hash();
    }

    TBC::OCPtr<NSObject<MTLDepthStencilState>> GraphicsPipelineBuilder::CreateDepthStencilState(ObjectFactory& factory)
    {
        /* KenD -- Metal TODO -- depth/stencil states are expensive to construct; cache them */
        auto& desc = _pimpl->_activeDepthStencilDesc;
        TBC::OCPtr<MTLDepthStencilDescriptor> mtlDesc = TBC::moveptr([[MTLDepthStencilDescriptor alloc] init]);
        mtlDesc.get().depthCompareFunction = AsMTLCompareFunction(desc._depthTest);
        mtlDesc.get().depthWriteEnabled = desc._depthWrite;

        TBC::OCPtr<MTLStencilDescriptor> frontStencilDesc = TBC::moveptr([[MTLStencilDescriptor alloc] init]);
        frontStencilDesc.get().stencilCompareFunction = AsMTLCompareFunction(desc._frontFaceStencil._comparisonOp);
        frontStencilDesc.get().stencilFailureOperation = AsMTLStencilOperation(desc._frontFaceStencil._failOp);
        frontStencilDesc.get().depthFailureOperation = AsMTLStencilOperation(desc._frontFaceStencil._depthFailOp);
        frontStencilDesc.get().depthStencilPassOperation = AsMTLStencilOperation(desc._frontFaceStencil._passOp);
        frontStencilDesc.get().readMask = desc._stencilReadMask;
        frontStencilDesc.get().writeMask = desc._stencilWriteMask;
        mtlDesc.get().frontFaceStencil = frontStencilDesc;

        TBC::OCPtr<MTLStencilDescriptor> backStencilDesc = TBC::moveptr([[MTLStencilDescriptor alloc] init]);
        backStencilDesc.get().stencilCompareFunction = AsMTLCompareFunction(desc._backFaceStencil._comparisonOp);
        backStencilDesc.get().stencilFailureOperation = AsMTLStencilOperation(desc._backFaceStencil._failOp);
        backStencilDesc.get().depthFailureOperation = AsMTLStencilOperation(desc._backFaceStencil._depthFailOp);
        backStencilDesc.get().depthStencilPassOperation = AsMTLStencilOperation(desc._backFaceStencil._passOp);
        backStencilDesc.get().readMask = desc._stencilReadMask;
        backStencilDesc.get().writeMask = desc._stencilWriteMask;
        mtlDesc.get().backFaceStencil = backStencilDesc;

        return factory.CreateDepthStencilState(mtlDesc.get());
    }

    DepthStencilDesc GraphicsPipelineBuilder::ActiveDepthStencilDesc()
    {
        return _pimpl->_activeDepthStencilDesc;
    }

    void GraphicsPipelineBuilder::Bind(Topology topology)
    {
        _pimpl->_activePrimitiveType = AsMTLenum(topology);
    }

    const std::shared_ptr<GraphicsPipeline>& GraphicsPipelineBuilder::CreatePipeline(ObjectFactory& factory)
    {
        auto hash = HashCombine(_pimpl->_shaderGuid, _pimpl->_rpHash);
        hash = HashCombine(_pimpl->_absHash, hash);
        hash = HashCombine(_pimpl->_dssHash, hash);
        hash = HashCombine(_pimpl->_activePrimitiveType, hash);
        hash = HashCombine(_pimpl->_inputLayoutGuid, hash);

        auto i = _pimpl->_prebuiltPipelines.find(hash);
        if (i!=_pimpl->_prebuiltPipelines.end())
            return i->second;

        MTLRenderPipelineColorAttachmentDescriptor* colAttachmentZero =
            _pimpl->_pipelineDescriptor.get().colorAttachments[0];
        if (colAttachmentZero.pixelFormat != MTLPixelFormatInvalid) {
            const auto& blendDesc = _pimpl->_attachmentBlendDesc;
            colAttachmentZero.blendingEnabled = blendDesc._blendEnable;

            if (blendDesc._colorBlendOp != BlendOp::NoBlending) {
                colAttachmentZero.rgbBlendOperation = AsMTLBlendOperation(blendDesc._colorBlendOp);
                colAttachmentZero.sourceRGBBlendFactor = AsMTLBlendFactor(blendDesc._srcColorBlendFactor);
                colAttachmentZero.destinationRGBBlendFactor = AsMTLBlendFactor(blendDesc._dstColorBlendFactor);
            } else {
                colAttachmentZero.rgbBlendOperation = MTLBlendOperationAdd;
                colAttachmentZero.sourceRGBBlendFactor = MTLBlendFactorOne;
                colAttachmentZero.destinationRGBBlendFactor = MTLBlendFactorZero;
            }

            if (blendDesc._colorBlendOp != BlendOp::NoBlending) {
                colAttachmentZero.alphaBlendOperation = AsMTLBlendOperation(blendDesc._alphaBlendOp);
                colAttachmentZero.sourceAlphaBlendFactor = AsMTLBlendFactor(blendDesc._srcAlphaBlendFactor);
                colAttachmentZero.destinationAlphaBlendFactor = AsMTLBlendFactor(blendDesc._dstAlphaBlendFactor);
            } else {
                colAttachmentZero.alphaBlendOperation = MTLBlendOperationAdd;
                colAttachmentZero.sourceAlphaBlendFactor = MTLBlendFactorOne;
                colAttachmentZero.destinationAlphaBlendFactor = MTLBlendFactorZero;
            }

            colAttachmentZero.writeMask =
                ((blendDesc._writeMask & ColorWriteMask::Red)    ? MTLColorWriteMaskRed   : 0) |
                ((blendDesc._writeMask & ColorWriteMask::Green)  ? MTLColorWriteMaskGreen : 0) |
                ((blendDesc._writeMask & ColorWriteMask::Blue)   ? MTLColorWriteMaskBlue  : 0) |
                ((blendDesc._writeMask & ColorWriteMask::Alpha)  ? MTLColorWriteMaskAlpha : 0);
        } else {
            colAttachmentZero.blendingEnabled = NO;
        }

        [_pimpl->_pipelineDescriptor setVertexDescriptor:_pimpl->_vertexDescriptor.get()];

        auto renderPipelineState = factory.CreateRenderPipelineState(_pimpl->_pipelineDescriptor.get(), true);
        if (renderPipelineState._error) {
            Log(Error) << "Failed to create render pipeline state: " << renderPipelineState._error << std::endl;
        }
        assert(!renderPipelineState._error);

        auto dss = CreateDepthStencilState(factory);

        // DavidJ -- note -- we keep the state _pimpl->_pipelineDescriptor from here.
        //      what happens if we continue to change it? It doesn't impact the compiled state we
        //      just made, right?

        _dirty = false;
        auto result  = std::make_shared<GraphicsPipeline>(GraphicsPipeline{
            std::move(renderPipelineState._renderPipelineState),
            std::move(renderPipelineState._reflection),
            std::move(dss),
            (unsigned)_pimpl->_activePrimitiveType,
            _pimpl->_activeDepthStencilDesc._stencilReference,
            hash

            #if defined(_DEBUG)
                , _pimpl->_shaderSourceIdentifiers
            #endif
        });

        i = _pimpl->_prebuiltPipelines.insert(std::make_pair(hash, result)).first;
        return i->second;
    }

    GraphicsPipelineBuilder::GraphicsPipelineBuilder()
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_pipelineDescriptor = TBC::moveptr([[MTLRenderPipelineDescriptor alloc] init]);
        _pimpl->_activePrimitiveType = MTLPrimitiveTypeTriangle;
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

        ViewportDesc _activeViewport;
        TBC::OCPtr<id> _activeIndexBuffer; // MTLBuffer

        MTLIndexType _indexType;
        unsigned _indexFormatBytes;
        unsigned _indexBufferOffsetBytes;

        TBC::OCPtr<MTLRenderPipelineReflection> _graphicsPipelineReflection;
        uint64_t _boundVSArgs = 0ull, _boundPSArgs = 0ull;

        const GraphicsPipeline* _boundGraphicsPipeline = nullptr;

        NSThread* _boundThread = nullptr;

        std::vector<std::function<void(void)>> _onEndEncodingFunctions;

        unsigned offsetToStartIndex(unsigned startIndex) {
            return startIndex * _indexFormatBytes + _indexBufferOffsetBytes;
        }
    };

    void DeviceContext::Bind(const IndexBufferView& IB)
    {
        assert(_pimpl->_boundThread == [NSThread currentThread]);
        auto resource = AsResource(IB._resource);
        id<MTLBuffer> buffer = resource.GetBuffer();
        if (!buffer)
            Throw(::Exceptions::BasicLabel("Attempting to bind index buffer view with invalid resource"));
        _pimpl->_activeIndexBuffer = buffer;
        _pimpl->_indexType = AsMTLIndexType(IB._indexFormat);
        _pimpl->_indexFormatBytes = BitsPerPixel(IB._indexFormat) / 8;
        _pimpl->_indexBufferOffsetBytes = IB._offset;
    }

    void DeviceContext::BindVS(id<MTLBuffer> buffer, unsigned offset, unsigned bufferIndex)
    {
        assert(_pimpl->_boundThread == [NSThread currentThread]);
        assert(_pimpl->_commandEncoder);
        [_pimpl->_commandEncoder setVertexBuffer:buffer offset:offset atIndex:bufferIndex];
    }

    void DeviceContext::Bind(const RasterizationDesc& desc)
    {
        assert(_pimpl->_boundThread == [NSThread currentThread]);
        assert(_pimpl->_commandEncoder);
        [_pimpl->_commandEncoder setFrontFacingWinding:AsMTLenum(desc._frontFaceWinding)];
        [_pimpl->_commandEncoder setCullMode:AsMTLenum(desc._cullMode)];
    }

    void DeviceContext::UnbindInputLayout()
    {
        assert(_pimpl->_boundThread == [NSThread currentThread]);
    }

    void DeviceContext::Bind(const ViewportDesc& viewport)
    {
        assert(_pimpl->_boundThread == [NSThread currentThread]);
        _pimpl->_activeViewport = viewport;

        /* KenD -- because we may not have an encoder yet, delay setting the viewport until later */
        if (_pimpl->_commandEncoder)
            [_pimpl->_commandEncoder setViewport:AsMTLViewport(_pimpl->_activeViewport)];
    }

    ViewportDesc DeviceContext::GetViewport()
    {
        return _pimpl->_activeViewport;
    }

    void DeviceContext::FinalizePipeline()
    {
        assert(_pimpl->_boundThread == [NSThread currentThread]);

        if (GraphicsPipelineBuilder::IsPipelineStale() || !_pimpl->_graphicsPipelineReflection) {
            auto& pipelineState = *GraphicsPipelineBuilder::CreatePipeline(GetObjectFactory());

            [_pimpl->_commandEncoder setRenderPipelineState:pipelineState._underlying];

            [_pimpl->_commandEncoder setDepthStencilState:pipelineState._depthStencilState];
            [_pimpl->_commandEncoder setStencilReferenceValue:pipelineState._stencilReferenceValue];

            _pimpl->_graphicsPipelineReflection = pipelineState._reflection;
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
        _pimpl->_boundGraphicsPipeline = nullptr;
        _pimpl->_queuedUniformSets.clear();

        // Bind standins for anything that have never been bound to anything correctly
        BoundUniforms::Apply_Standins(*this, _pimpl->_graphicsPipelineReflection.get(), ~_pimpl->_boundVSArgs, ~_pimpl->_boundPSArgs);
    }

    void DeviceContext::QueueUniformSet(
        const std::shared_ptr<UnboundInterface>& unboundInterf,
        unsigned streamIdx,
        const UniformsStream& stream)
    {
        assert(_pimpl->_boundThread == [NSThread currentThread]);

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
        assert(_pimpl->_boundThread == [NSThread currentThread]);
        assert(_pimpl->_commandEncoder);

        FinalizePipeline();
        [_pimpl->_commandEncoder drawPrimitives:GraphicsPipelineBuilder::_pimpl->_activePrimitiveType
                                    vertexStart:startVertexLocation
                                    vertexCount:vertexCount];
    }

    void DeviceContext::DrawIndexed(unsigned indexCount, unsigned startIndexLocation, unsigned baseVertexLocation)
    {
        assert(baseVertexLocation==0);
        assert(_pimpl->_boundThread == [NSThread currentThread]);
        assert(_pimpl->_commandEncoder);

        FinalizePipeline();
        [_pimpl->_commandEncoder drawIndexedPrimitives:GraphicsPipelineBuilder::_pimpl->_activePrimitiveType
                                            indexCount:indexCount
                                             indexType:_pimpl->_indexType
                                           indexBuffer:_pimpl->_activeIndexBuffer
                                     indexBufferOffset:_pimpl->offsetToStartIndex(startIndexLocation)];
    }

    void DeviceContext::DrawInstances(unsigned vertexCount, unsigned instanceCount, unsigned startVertexLocation)
    {
        assert(_pimpl->_boundThread == [NSThread currentThread]);
        assert(_pimpl->_commandEncoder);

        FinalizePipeline();
        [_pimpl->_commandEncoder drawPrimitives:GraphicsPipelineBuilder::_pimpl->_activePrimitiveType
                                    vertexStart:startVertexLocation
                                    vertexCount:vertexCount
                                  instanceCount:instanceCount];
    }

    void DeviceContext::DrawIndexedInstances(unsigned indexCount, unsigned instanceCount, unsigned startIndexLocation, unsigned baseVertexLocation)
    {
        assert(_pimpl->_boundThread == [NSThread currentThread]);
        assert(baseVertexLocation==0);

        FinalizePipeline();
        assert(_pimpl->_commandEncoder);

        [_pimpl->_commandEncoder drawIndexedPrimitives:GraphicsPipelineBuilder::_pimpl->_activePrimitiveType
                                            indexCount:indexCount
                                             indexType:_pimpl->_indexType
                                           indexBuffer:_pimpl->_activeIndexBuffer
                                     indexBufferOffset:_pimpl->offsetToStartIndex(startIndexLocation)
                                         instanceCount:instanceCount];
    }

    void    DeviceContext::Draw(
        const GraphicsPipeline& pipeline,
        unsigned vertexCount, unsigned startVertexLocation)
    {
        assert(_pimpl->_boundThread == [NSThread currentThread]);
        assert(_pimpl->_commandEncoder);
        if (_pimpl->_boundGraphicsPipeline != &pipeline) {
            [_pimpl->_commandEncoder setRenderPipelineState:pipeline._underlying];
            [_pimpl->_commandEncoder setDepthStencilState:pipeline._depthStencilState];
            [_pimpl->_commandEncoder setStencilReferenceValue:pipeline._stencilReferenceValue];

            _pimpl->_graphicsPipelineReflection = nullptr;
            _pimpl->_boundVSArgs = 0;
            _pimpl->_boundPSArgs = 0;
            _pimpl->_boundGraphicsPipeline = &pipeline;
            _pimpl->_queuedUniformSets.clear();
        }

        [_pimpl->_commandEncoder drawPrimitives:(MTLPrimitiveType)pipeline._primitiveType
                                    vertexStart:startVertexLocation
                                    vertexCount:vertexCount];
    }

    void    DeviceContext::DrawIndexed(
        const GraphicsPipeline& pipeline,
        unsigned indexCount, unsigned startIndexLocation)
    {
        assert(_pimpl->_boundThread == [NSThread currentThread]);
        assert(_pimpl->_commandEncoder);
        if (_pimpl->_boundGraphicsPipeline != &pipeline) {
            [_pimpl->_commandEncoder setRenderPipelineState:pipeline._underlying];
            [_pimpl->_commandEncoder setDepthStencilState:pipeline._depthStencilState];
            [_pimpl->_commandEncoder setStencilReferenceValue:pipeline._stencilReferenceValue];

            _pimpl->_graphicsPipelineReflection = nullptr;
            _pimpl->_boundVSArgs = 0;
            _pimpl->_boundPSArgs = 0;
            _pimpl->_boundGraphicsPipeline = &pipeline;
            _pimpl->_queuedUniformSets.clear();
        }

        [_pimpl->_commandEncoder drawIndexedPrimitives:(MTLPrimitiveType)pipeline._primitiveType
                                            indexCount:indexCount
                                             indexType:_pimpl->_indexType
                                           indexBuffer:_pimpl->_activeIndexBuffer
                                     indexBufferOffset:_pimpl->offsetToStartIndex(startIndexLocation)];
    }

    void    DeviceContext::DrawInstances(
        const GraphicsPipeline& pipeline,
        unsigned vertexCount, unsigned instanceCount, unsigned startVertexLocation)
    {
        assert(0);
    }

    void    DeviceContext::DrawIndexedInstances(
        const GraphicsPipeline& pipeline,
        unsigned indexCount, unsigned instanceCount, unsigned startIndexLocation)
    {
        assert(0);
    }

    void            DeviceContext::CreateRenderCommandEncoder(MTLRenderPassDescriptor* renderPassDescriptor)
    {
        assert(_pimpl->_boundThread == [NSThread currentThread]);
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
        assert(_pimpl->_boundThread == [NSThread currentThread]);
        assert(_pimpl->_commandEncoder);

        [_pimpl->_commandEncoder endEncoding];

        _pimpl->_graphicsPipelineReflection = nullptr;
        _pimpl->_boundVSArgs = 0;
        _pimpl->_boundPSArgs = 0;
        _pimpl->_boundGraphicsPipeline = nullptr;
        _pimpl->_queuedUniformSets.clear();

        for (const auto& fn: _pimpl->_onEndEncodingFunctions) {
            fn();
        }
    }

    void            DeviceContext::OnEndEncoding(std::function<void(void)> fn)
    {
        assert(_pimpl->_boundThread == [NSThread currentThread]);
        assert(_pimpl->_commandEncoder);
        _pimpl->_onEndEncodingFunctions.push_back(fn);
    }

    void            DeviceContext::DestroyRenderCommandEncoder()
    {
        assert(_pimpl->_boundThread == [NSThread currentThread]);
        assert(_pimpl->_commandEncoder);
        _pimpl->_commandEncoder = nullptr;
    }

    id<MTLRenderCommandEncoder> DeviceContext::GetCommandEncoder()
    {
        assert(_pimpl->_boundThread == [NSThread currentThread]);
        assert(_pimpl->_commandEncoder);
        return _pimpl->_commandEncoder;
    }

    void            DeviceContext::HoldDevice(id<MTLDevice> device)
    {
        assert(_pimpl->_boundThread == [NSThread currentThread]);
        _pimpl->_device = device;
        assert(_pimpl->_device);
    }

    void            DeviceContext::HoldCommandBuffer(id<MTLCommandBuffer> commandBuffer)
    {
        /* Hold for the duration of the frame */
        assert(_pimpl->_boundThread == [NSThread currentThread]);
        assert(!_pimpl->_commandBuffer);
        _pimpl->_commandBuffer = commandBuffer;

        CheckCommandBufferError(_pimpl->_commandBuffer);
    }

    void            DeviceContext::ReleaseCommandBuffer()
    {
        assert(_pimpl->_boundThread == [NSThread currentThread]);
        CheckCommandBufferError(_pimpl->_commandBuffer);

        /* The command encoder should have been released when the subpass was finished,
         * now we release the command buffer */
        assert(!_pimpl->_commandEncoder);
        assert(_pimpl->_commandBuffer);
        _pimpl->_commandBuffer = nullptr;
    }

    id<MTLCommandBuffer>            DeviceContext::RetrieveCommandBuffer()
    {
        assert(_pimpl->_boundThread == [NSThread currentThread]);
        return _pimpl->_commandBuffer;
    }

    void            DeviceContext::PushDebugGroup(const char annotationName[])
    {
        assert(_pimpl->_boundThread == [NSThread currentThread]);
        // assert(_pimpl->_commandEncoder);
        [_pimpl->_commandEncoder pushDebugGroup:[NSString stringWithCString:annotationName encoding:NSUTF8StringEncoding]];
    }

    void            DeviceContext::PopDebugGroup()
    {
        assert(_pimpl->_boundThread == [NSThread currentThread]);
        // assert(_pimpl->_commandEncoder);
        [_pimpl->_commandEncoder popDebugGroup];
    }

    CapturedStates* DeviceContext::GetCapturedStates() { return &_pimpl->_capturedStates; }
    void        DeviceContext::BeginStateCapture(CapturedStates& capturedStates) {}
    void        DeviceContext::EndStateCapture() {}

    DeviceContext::DeviceContext()
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_indexType = MTLIndexTypeUInt16;
        _pimpl->_indexFormatBytes = 2; // two bytes for MTLIndexTypeUInt16
        _pimpl->_indexBufferOffsetBytes = 0;
        _pimpl->_boundThread = [NSThread currentThread];
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
