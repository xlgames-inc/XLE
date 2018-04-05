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
#include "../../../ConsoleRig/Log.h"
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
            default:
                assert(0);
                return MTLWindingCounterClockwise;
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

    static ReflectionInformation::MappingType AsReflectionMappingType(MTLArgumentType type)
    {
        switch (type) {
            case MTLArgumentTypeBuffer:     return ReflectionInformation::MappingType::Buffer;
            case MTLArgumentTypeTexture:    return ReflectionInformation::MappingType::Texture;
            case MTLArgumentTypeSampler:    return ReflectionInformation::MappingType::Sampler;
            default: assert(0);             return ReflectionInformation::MappingType::Unknown;
            // ignoring MTLArgumentTypeThreadgroupMemory
        }
    }

    class GraphicsPipeline::Pimpl
    {
    public:
        /* For the current draw */
        MTLPrimitiveType _primitiveType;
        MTLIndexType _indexType;
        unsigned    _indexFormatBytes;
        TBC::OCPtr<id> _indexBuffer; // MTLBuffer
        MTLViewport _viewport;

        TBC::OCPtr<MTLRenderPipelineDescriptor> _pipelineDescriptor; // For the current draw

        TBC::OCPtr<id> _commandBuffer; // For the duration of the frame
        TBC::OCPtr<id> _commandEncoder; // For the current subpass
        TBC::OCPtr<id> _device;

        /* KenD -- we are currently caching reflection information obtained from temporary RenderPipelineStates.
         * We don't keep those temporary RenderPipelineStates at this point.  The reason is that we need reflection
         * information when applying bound uniforms - we have to determine the destination index in
         * the argument table - but when applying bound uniforms, it's possible that the pipelineDescriptor
         * is not fully set for the current drawable - for example, the blend state may not have been set
         * or some other state might not be set.
         *
         * Metal TODO -- management of RenderPipelineStates needs to be improved regarding caching
         */
        std::vector<std::pair<uint64_t, ReflectionInformation>> _reflectionInformation;

        /* Debug functions */
        void ClearTextureBindings()
        {
#if DEBUG
            /* KenD -- instead of clearing texture bindings, do not.  It seems drawable sequencer counts on bindings lingering. */
            return;

            /* KenD -- clear all texture bindings so that they don't spill over to or pollute subsequent draws.
             * I have found that even if a texture isn't set for a draw call, a is_null_texture check may fail in the fragment shader.
             */
            for (unsigned i=0; i<31; ++i) {
                [_commandEncoder setVertexTexture:nil atIndex:i];
                [_commandEncoder setFragmentTexture:nil atIndex:i];
            }
#endif
        }

        void CheckCommandBufferError()
        {
            id<MTLCommandBuffer> buffer = _commandBuffer.get();
            if (buffer.error) {
                NSLog(@"================> %@", buffer.error);
            }
        }

        uint32_t _boundVertexBuffers = 0u;
        uint32_t _boundVertexTextures = 0u;
        uint32_t _boundVertexSamplers = 0u;
        uint32_t _boundFragmentBuffers = 0u;
        uint32_t _boundFragmentTextures = 0u;
        uint32_t _boundFragmentSamplers = 0u;
    };

    void GraphicsPipeline::Bind(const IndexBufferView& IB)
    {
        auto resource = AsResource(IB._resource);
        id<MTLBuffer> buffer = resource.GetBuffer();
        if (!buffer)
            Throw(::Exceptions::BasicLabel("Attempting to bind index buffer view with invalid resource"));
        _pimpl->_indexBuffer = buffer;
        _pimpl->_indexType = AsMTLIndexType(IB._indexFormat);
        _pimpl->_indexFormatBytes = BitsPerPixel(IB._indexFormat) / 8;
    }

    void GraphicsPipeline::Bind(Topology topology)
    {
        _pimpl->_primitiveType = AsMTLenum(topology);
    }

    void GraphicsPipeline::Bind(const ShaderProgram& shaderProgram)
    {
        assert(_pimpl->_pipelineDescriptor);
        [_pimpl->_pipelineDescriptor setVertexFunction:shaderProgram._vf.get()];
        [_pimpl->_pipelineDescriptor setFragmentFunction:shaderProgram._ff.get()];
    }

    void GraphicsPipeline::Bind(const BlendState& blender)
    {
        assert(_pimpl->_pipelineDescriptor);
        /* KenD -- Metal TODO -- Configure MTLRenderPipelineColorAttachmentDescriptor
         * Set blend state on color attachments of MTLRenderPipelineDescriptor.
         * See "Configuring Blending in a Render Pipeline Attachment Descriptor"
         */
    }

    void GraphicsPipeline::Bind(const RasterizationDesc& desc)
    {
        assert(_pimpl->_commandEncoder);
        [_pimpl->_commandEncoder setFrontFacingWinding:AsMTLenum(desc._frontFaceWinding)];

        if (desc._cullMode != CullMode::None) {
            [_pimpl->_commandEncoder setCullMode:AsMTLenum(desc._cullMode)];
        }
    }

    void GraphicsPipeline::Bind(const DepthStencilDesc& desc)
    {
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

    void GraphicsPipeline::Bind(const ViewportDesc& viewport)
    {
        // MTLViewport's originX/Y are relative to upper-left
        MTLViewport vp;
        vp.originX = viewport.TopLeftX;
        vp.originY = viewport.TopLeftY;
        vp.width = viewport.Width;
        vp.height = viewport.Height;
        vp.znear = viewport.MinDepth;
        vp.zfar = viewport.MaxDepth;
        /* KenD -- because we may not have an encoder yet, delay setting the viewport until later */
        _pimpl->_viewport = vp;
    }

    void GraphicsPipeline::Bind(MTLVertexDescriptor* descriptor)
    {
        // KenD -- the vertex descriptor isn't necessary if the vertex function does not have an input argument declared [[stage_in]] */
        assert(_pimpl->_pipelineDescriptor);
        [_pimpl->_pipelineDescriptor setVertexDescriptor:descriptor];
    }

    void GraphicsPipeline::Bind(id<MTLBuffer> buffer, unsigned offset, unsigned bufferIndex, ShaderTarget target)
    {
        assert(target == Vertex || target == Fragment);
        assert(_pimpl->_commandEncoder);
        if (target == Vertex) {
            [_pimpl->_commandEncoder setVertexBuffer:buffer offset:offset atIndex:bufferIndex];
            _pimpl->_boundVertexBuffers |= (1 << bufferIndex);
        } else if (target == Fragment) {
            [_pimpl->_commandEncoder setFragmentBuffer:buffer offset:offset atIndex:bufferIndex];
            _pimpl->_boundFragmentBuffers |= (1 << bufferIndex);
        }
    }

    void GraphicsPipeline::Bind(const void* bytes, unsigned length, unsigned bufferIndex, ShaderTarget target)
    {
        assert(target == Vertex || target == Fragment);
        assert(_pimpl->_commandEncoder);
        if (target == Vertex) {
            [_pimpl->_commandEncoder setVertexBytes:bytes length:length atIndex:bufferIndex];
            _pimpl->_boundVertexBuffers |= (1 << bufferIndex);
        } else if (target == Fragment) {
            [_pimpl->_commandEncoder setFragmentBytes:bytes length:length atIndex:bufferIndex];
            _pimpl->_boundFragmentBuffers |= (1 << bufferIndex);
        }
    }

    void GraphicsPipeline::Bind(id<MTLTexture> texture, unsigned textureIndex, ShaderTarget target)
    {
        assert(target == Vertex || target == Fragment);
        assert(_pimpl->_commandEncoder);
        if (target == Vertex) {
            [_pimpl->_commandEncoder setVertexTexture:texture atIndex:textureIndex];
            _pimpl->_boundVertexTextures |= (1 << textureIndex);
        } else if (target == Fragment) {
            [_pimpl->_commandEncoder setFragmentTexture:texture atIndex:textureIndex];
            _pimpl->_boundFragmentTextures |= (1 << textureIndex);
        }
    }

    /* KenD -- cleanup TODO -- this was copied from LightWeightModel */
    static uint64 BuildSemanticHash(const char semantic[])
    {
        // strip off digits on the end of the string (these are optionally included and are used as
        // the semantic index)
        auto len = std::strlen(semantic);
        while (len > 0 && std::isdigit(semantic[len-1])) len--;
        uint64_t hash = Hash64(semantic, &semantic[len]);
        hash += std::atoi(&semantic[len]);
        return hash;
    }

    const ReflectionInformation& GraphicsPipeline::GetReflectionInformation(TBC::OCPtr<id> vf, TBC::OCPtr<id> ff)
    {
        /* KenD -- Metal TODO -- Management of RenderPipelineStates
         *
         * Creating the render pipeline state at this point just to get reflection data
         * (in order to determine proper indices in argument tables) might be premature.
         * For now, although a render pipeline state is required, we do not have to cache it.
         * We can cache the reflection information though.
         *
         * - Open question: when do we know that a pipeline descriptor is really complete?
         * - Reconsider threading with cache.
         * - Reconsider pointer hash.
         */

        uint64_t hash = HashCombine((uint64_t)vf.get(), (uint64_t)ff.get());
        auto i = LowerBound(_pimpl->_reflectionInformation, hash);
        if (i != _pimpl->_reflectionInformation.end() && i->first == hash) {
            return i->second;
        }

        /* We have to create a temporary RenderPipelineState to obtain reflection information.
         * We will just use the current pipeline descriptor for that; the current descriptor
         * must match the shader for which we need reflection data.
         */
        assert(_pimpl->_pipelineDescriptor.get().vertexDescriptor);
        assert(_pimpl->_pipelineDescriptor.get().vertexFunction && _pimpl->_pipelineDescriptor.get().vertexFunction == vf.get());
        assert(_pimpl->_pipelineDescriptor.get().fragmentFunction  && _pimpl->_pipelineDescriptor.get().fragmentFunction == ff.get());

        /* KenD -- Metal TODO -- temporary RenderPipelineState for reflection.
         * The render pipeline state also contains blending state, which currently isn't handled.
         *
         * For now, keeping it inefficient by caching the reflection information, but throwing away
         * the render pipeline state.  While this is inefficient, more considerations
         * need to be made before caching the render pipeline state.
         */
        MTLPipelineOption options = MTLPipelineOptionArgumentInfo;
        MTLAutoreleasedRenderPipelineReflection reflection;

        NSError* error = NULL;
        id<MTLRenderPipelineState> pipelineState = [_pimpl->_device newRenderPipelineStateWithDescriptor:_pimpl->_pipelineDescriptor
                                                                                                 options:options
                                                                                              reflection:&reflection
                                                                                                   error:&error];
        if (error) {
            Log(Error) << "Failed to create render pipeline state: " << [[error description] UTF8String] << std::endl;
        }
        assert(!error);
        // KenD -- Throw away the render pipeline state immediately :(
        [pipelineState release];

        // src arguments, dst mappings
        ReflectionInformation ri;
        ri._debugReflection = reflection;
        const std::pair<NSArray<MTLArgument*>*, std::vector<ReflectionInformation::Mapping>*> srcArgumentsDstMappings[] = { std::make_pair(reflection.vertexArguments, &ri._vfMappings), std::make_pair(reflection.fragmentArguments, &ri._ffMappings) };

        for (unsigned am=0; am < dimof(srcArgumentsDstMappings); ++am) {
            for (MTLArgument* arg in srcArgumentsDstMappings[am].first) {
                auto* riMap = srcArgumentsDstMappings[am].second;

                const char* argName = [arg.name cStringUsingEncoding:NSUTF8StringEncoding];
                auto argHash = BuildSemanticHash(argName);
                /* KenD -- Metal HACK -- Unlike vertex attributes, which are bound via semantic hash,
                 * textures in material use standard hash.  However, the exception
                 * is for textures that have "array indexor syntax."
                 * (see CC3Material's `setTexture:forBindingName:`
                 *  and `HashVariableName` in ShaderIntrospection)
                 *
                 * This hack is incomplete and should take into account all arrays of textures;
                 * currently, the only used array of textures is s_cc3Texture2Ds.
                 */
                if (arg.type == MTLArgumentTypeTexture) {
                    if ([arg.name rangeOfString:@"s_cc3Texture2Ds"].location == NSNotFound) {
                        argHash = Hash64(argName);
                    }
                }
                ReflectionInformation::MappingType argType = AsReflectionMappingType(arg.type);
                riMap->emplace_back(
                    ReflectionInformation::Mapping{argHash,
                    argType,
                    unsigned(arg.index)});
            }
        }
        i = _pimpl->_reflectionInformation.emplace(i, std::make_pair(hash, std::move(ri)));
        return i->second;
    }

    void GraphicsPipeline::FinalizePipeline()
    {
#if DEBUG
        {
            auto& reflectionInformation = GetReflectionInformation([_pimpl->_pipelineDescriptor.get() vertexFunction],
                                                                   [_pimpl->_pipelineDescriptor.get() fragmentFunction]);
            MTLRenderPipelineReflection* renderReflection = reflectionInformation._debugReflection.get();

            /* Expected to be bound */
            const NSArray<MTLArgument*>* argumentSets[] = { renderReflection.vertexArguments, renderReflection.fragmentArguments };
            for (unsigned as=0; as < dimof(argumentSets); ++as) {
                uint32_t activeBuffers = 0u;
                uint32_t activeTextures = 0u;
                uint32_t activeSamplers = 0u;
                const auto maxTextures = 31u;
                NSString* textureNames[maxTextures];
                for (MTLArgument* arg in argumentSets[as]) {
                    if (arg.active) {
                        uint32_t intendedIndex = (1 << arg.index);
                        if (arg.type == MTLArgumentTypeBuffer) {
                            activeBuffers |= intendedIndex;
                        } else if (arg.type == MTLArgumentTypeTexture) {
                            activeTextures |= intendedIndex;
                            textureNames[arg.index] = [arg.name copy];
                        } else if (arg.type == MTLArgumentTypeSampler) {
                            activeSamplers |= intendedIndex;
                        }
                    }
                }

                /* Check that the arguments expected by the shader are actually bound.
                 * It's okay if some things were bound that are not actually active.
                 */
                if (as == 0) {
                    for (int i=0; i < maxTextures; ++i) {
                        if ((activeTextures & (1 << i)) && !(_pimpl->_boundVertexTextures & (1 << i))) {
                            Log(Error) << "Expected vertex texture is not bound to index: " << textureNames[i].UTF8String << " (" << i << ")" << std::endl;
                        }
                    }
                    assert((activeBuffers & _pimpl->_boundVertexBuffers) == activeBuffers);
                    assert((activeTextures & _pimpl->_boundVertexTextures) == activeTextures);
                    assert((activeSamplers & _pimpl->_boundVertexSamplers) == activeSamplers);
                } else if (as == 1) {
                    for (int i=0; i < maxTextures; ++i) {
                        if ((activeTextures & (1 << i)) && !(_pimpl->_boundFragmentTextures & (1 << i))) {
                            Log(Error) << "Expected fragment texture is not bound to index: " << textureNames[i].UTF8String << " (" << i << ")" << std::endl;
                        }
                    }
                    assert((activeBuffers & _pimpl->_boundFragmentBuffers) == activeBuffers);
                    assert((activeTextures & _pimpl->_boundFragmentTextures) == activeTextures);
                    assert((activeSamplers & _pimpl->_boundFragmentSamplers) == activeSamplers);
                }
            }
        }
#endif

        assert(_pimpl->_commandEncoder);
        if (_pimpl->_viewport.width == 0) {
            Log(Error) << "Manually setting viewport because it was not already set!" << std::endl;
            _pimpl->_viewport.width = 2048;
            _pimpl->_viewport.height = 1536;
            _pimpl->_viewport.znear = -1;
            _pimpl->_viewport.zfar = 1;
        }
        [_pimpl->_commandEncoder setViewport:_pimpl->_viewport];

        // At this point, the MTLPipelineDescriptor should be fully set up for what will be encoded next.

        NSError* error = NULL;
        id<MTLRenderPipelineState> pipelineState = [_pimpl->_device newRenderPipelineStateWithDescriptor:_pimpl->_pipelineDescriptor
                                                                                                   error:&error];
        if (error) {
            Log(Error) << "Failed to create render pipeline state: " << [[error description] UTF8String] << std::endl;
        }
        assert(!error);
        [_pimpl->_commandEncoder setRenderPipelineState:pipelineState];
        [pipelineState release];
        /* KenD -- Metal TODO -- cache the RenderPipelineState so it can be reused */

        /* Metal TODO -- non-rasterized passes, multisampling */
    }

    void GraphicsPipeline::UnbindInputLayout()
    {
    }

    void GraphicsPipeline::Draw(unsigned vertexCount, unsigned startVertexLocation)
    {
        FinalizePipeline();
        assert(_pimpl->_commandEncoder);

        [_pimpl->_commandEncoder drawPrimitives:_pimpl->_primitiveType
                                    vertexStart:startVertexLocation
                                    vertexCount:vertexCount];

        _pimpl->ClearTextureBindings();
    }

    void GraphicsPipeline::DrawIndexed(unsigned indexCount, unsigned startIndexLocation, unsigned baseVertexLocation)
    {
        assert(baseVertexLocation==0);

        FinalizePipeline();
        assert(_pimpl->_commandEncoder);

        [_pimpl->_commandEncoder drawIndexedPrimitives:_pimpl->_primitiveType
                                            indexCount:indexCount
                                             indexType:_pimpl->_indexType
                                           indexBuffer:_pimpl->_indexBuffer
                                     indexBufferOffset:startIndexLocation * _pimpl->_indexFormatBytes];

        _pimpl->ClearTextureBindings();
    }

    void GraphicsPipeline::DrawInstances(unsigned vertexCount, unsigned instanceCount, unsigned startVertexLocation)
    {
        FinalizePipeline();
        assert(_pimpl->_commandEncoder);

        [_pimpl->_commandEncoder drawPrimitives:_pimpl->_primitiveType
                                    vertexStart:startVertexLocation
                                    vertexCount:vertexCount
                                  instanceCount:instanceCount];

        _pimpl->ClearTextureBindings();
    }

    void GraphicsPipeline::DrawIndexedInstances(unsigned indexCount, unsigned instanceCount, unsigned startIndexLocation, unsigned baseVertexLocation)
    {
        assert(baseVertexLocation==0);

        FinalizePipeline();
        assert(_pimpl->_commandEncoder);

        [_pimpl->_commandEncoder drawIndexedPrimitives:_pimpl->_primitiveType
                                            indexCount:indexCount
                                             indexType:_pimpl->_indexType
                                           indexBuffer:_pimpl->_indexBuffer
                                     indexBufferOffset:startIndexLocation * _pimpl->_indexFormatBytes
                                         instanceCount:instanceCount];

        _pimpl->ClearTextureBindings();
    }

    void            GraphicsPipeline::CreateRenderCommandEncoder(MTLRenderPassDescriptor* renderPassDescriptor)
    {
        {
            /* When the command encoder was destroyed, previously bound textures and buffers would no longer be bound. */
            _pimpl->_boundVertexBuffers = 0u;
            _pimpl->_boundVertexTextures = 0u;
            _pimpl->_boundVertexSamplers = 0u;
            _pimpl->_boundFragmentBuffers = 0u;
            _pimpl->_boundFragmentTextures = 0u;
            _pimpl->_boundFragmentSamplers = 0u;
        }

        _pimpl->CheckCommandBufferError();

        assert(!_pimpl->_commandEncoder);
        // renderCommandEncoderWithDescriptor: returns an autoreleased object, so don't use TBC::moveptr when constructing the OCPtr; the assignment operator is acceptable and will retain the object
        _pimpl->_commandEncoder = [_pimpl->_commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        assert(_pimpl->_commandEncoder);

        /* KenD -- Creating a MTLRenderPipelineDescriptor when creating the encoder; the descriptor is temporary
         * while the MTLRenderPipelineStates are more long-lived. */
        assert(!_pimpl->_pipelineDescriptor);
        _pimpl->_pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];

        /* The pixel formats of the attachments in the MTLRenderPipelineDescriptor must match
         * the pixel formats of the associated attachments' textures in the MTLRenderPassDescriptor. */
        auto* renderPipelineDescriptor = _pimpl->_pipelineDescriptor.get();
        /* There is a maximum of 8 color attachments, defined by MTLRenderPipelineColorAttachmentDescriptor */
        const unsigned maxColorAttachments = 8u;
        for (unsigned i=0; i<maxColorAttachments; ++i) {
            MTLRenderPassColorAttachmentDescriptor* renderPassColorAttachmentDesc = renderPassDescriptor.colorAttachments[i];
            if (renderPassColorAttachmentDesc.texture) {
                renderPipelineDescriptor.colorAttachments[i].pixelFormat = renderPassColorAttachmentDesc.texture.pixelFormat;
            }
        }
        if (renderPassDescriptor.depthAttachment.texture) {
            renderPipelineDescriptor.depthAttachmentPixelFormat = renderPassDescriptor.depthAttachment.texture.pixelFormat;
        }
        if (renderPassDescriptor.stencilAttachment.texture) {
            renderPipelineDescriptor.stencilAttachmentPixelFormat = renderPassDescriptor.stencilAttachment.texture.pixelFormat;
        }
    }

    void            GraphicsPipeline::EndEncoding()
    {
        assert(_pimpl->_commandEncoder);

        [_pimpl->_commandEncoder endEncoding];
    }

    void            GraphicsPipeline::DestroyRenderCommandEncoder()
    {
        assert(_pimpl->_commandEncoder);
        _pimpl->_commandEncoder = nullptr;

        /* KenD -- destroying the MTLRenderPipelineDescriptor when destroying the encoder */
        [_pimpl->_pipelineDescriptor release];
        _pimpl->_pipelineDescriptor = nil;
    }

    void            GraphicsPipeline::HoldDevice(id<MTLDevice> device)
    {
        _pimpl->_device = device;
        assert(_pimpl->_device);
    }

    void            GraphicsPipeline::HoldCommandBuffer(id<MTLCommandBuffer> commandBuffer)
    {
        /* Hold for the duration of the frame */
        assert(!_pimpl->_commandBuffer);
        _pimpl->_commandBuffer = commandBuffer;

        _pimpl->CheckCommandBufferError();
    }

    void            GraphicsPipeline::ReleaseCommandBuffer()
    {
        _pimpl->CheckCommandBufferError();

        /* The command encoder should have been released when the subpass was finished,
         * now we release the command buffer */
        assert(!_pimpl->_commandEncoder);
        assert(_pimpl->_commandBuffer);
        _pimpl->_commandBuffer = nullptr;
    }

    void            GraphicsPipeline::PushDebugGroup(const char annotationName[])
    {
        assert(_pimpl->_commandEncoder);
        [_pimpl->_commandEncoder pushDebugGroup:[NSString stringWithCString:annotationName encoding:NSUTF8StringEncoding]];
    }

    void            GraphicsPipeline::PopDebugGroup()
    {
        assert(_pimpl->_commandEncoder);
        [_pimpl->_commandEncoder popDebugGroup];
    }

    GraphicsPipeline::GraphicsPipeline()
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_indexType = MTLIndexTypeUInt16;
        _pimpl->_indexFormatBytes = 2; // two bytes for MTLIndexTypeUInt16
        _pimpl->_primitiveType = MTLPrimitiveTypeTriangle;
    }

    GraphicsPipeline::~GraphicsPipeline()
    {
    }

    DeviceContext::DeviceContext()
    {
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
