// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "InputLayout.h"
#include "DeviceContext.h"
#include "Shader.h"
#include "Format.h"
#include "TextureView.h"
#include "PipelineLayout.h"
#include "State.h"
#include "Resource.h"
#include "../../Types.h"
#include "../../Format.h"
#include "../../BufferView.h"
#include "../../../ConsoleRig/Log.h"
#include "../../../ConsoleRig/LogUtil.h"
#include "../../../Utility/StringUtils.h"
#include "../../../Utility/StringFormat.h"
#include "../../../Utility/PtrUtils.h"
#include "../../../Utility/ArithmeticUtils.h"
#include <map>
#include <exception>

#include "IncludeAppleMetal.h"

namespace RenderCore { namespace Metal_AppleMetal
{
    MTLVertexFormat AsMTLVertexFormat(RenderCore::Format fmt);

    static const Resource& GetResource(const IResource* rp)
    {
        static Resource dummy;
        auto* res = (Resource*)const_cast<IResource*>(rp)->QueryInterface(typeid(Resource).hash_code());
        if (res)
            return *res;
        return dummy;
    }

    void BoundInputLayout::Apply(DeviceContext& context, IteratorRange<const VertexBufferView*> vertexBuffers) const never_throws
    {
        context.Bind(_vertexDescriptor.get());
        unsigned i = 0;
        for (const auto& vbv : vertexBuffers) {
            auto resource = GetResource(vbv._resource);
            id<MTLBuffer> buffer = resource.GetBuffer();
            if (!buffer)
                Throw(::Exceptions::BasicLabel("Attempting to apply vertex buffer view with invalid resource"));
            context.BindVS(buffer, vbv._offset, i);
            ++i;
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

    BoundInputLayout::BoundInputLayout(IteratorRange<const SlotBinding*> layouts, const ShaderProgram& program)
    {
        /* KenD -- Metal TODO -- validate the layout of vertex data.
         * The MTLVertexFormat for some attributes may be a float4 although the native format
         * (RenderCore format) is only a float3.
         * In the shader, the attribute should be a float3.
         * It will still be correctly accessed in the shader (because the offset is specified),
         * but frame capture will show those attributes as float4s with some overlap.
         *
         * For the bufferIndex, we are currently just using the index of the layout.
         * That works, provided that the input to the shader function does not use those same
         * buffers for other data.
         *
         * Validate the input layout by getting the function arguments (via reflection) and comparing
         * them with the input layout.  Ensure that the offsets of elements in the input layout
         * match the offset of the corresponding elements in the shader arguments.
         */

        // Create a MTLVertexDescriptor to describe the input format for vertices
        _vertexDescriptor = TBC::OCPtr<MTLVertexDescriptor>(TBC::moveptr([[MTLVertexDescriptor alloc] init]));
        auto* desc = _vertexDescriptor.get();

        // Map each vertex attribute's semantic hash to its attribute index
        id<MTLFunction> vf = program._vf.get();

        std::map<uint64_t, unsigned> hashToLocation;
        _allAttributesBound = true;
        for (unsigned a=0; a<vf.vertexAttributes.count; ++a) {
            if (!vf.vertexAttributes[a].active)
                continue;

            auto hash = BuildSemanticHash(vf.vertexAttributes[a].name.UTF8String);

            bool foundBinding = false;
            for (unsigned l=0; l < layouts.size() && !foundBinding; ++l) {
                for (const auto& e : layouts[l]._elements) {
                    if (e._semanticHash != hash)
                        continue;

                    auto attributeIdx = vf.vertexAttributes[a].attributeIndex;
                    desc.attributes[attributeIdx].bufferIndex = l;
                    desc.attributes[attributeIdx].format = AsMTLVertexFormat(e._nativeFormat);
                    desc.attributes[attributeIdx].offset = CalculateVertexStride(MakeIteratorRange(layouts[l]._elements.begin(), &e), false);
                    foundBinding = true;
                    break;
                }
            }

            if (!foundBinding) {
                _allAttributesBound = false;
            }
        }

        for (unsigned l=0; l < layouts.size(); ++l) {
            desc.layouts[l].stride = CalculateVertexStride(layouts[l]._elements, false);
            if (layouts[l]._instanceStepDataRate == 0) {
                desc.layouts[l].stepFunction = MTLVertexStepFunctionPerVertex;
            } else {
                desc.layouts[l].stepFunction = MTLVertexStepFunctionPerInstance;
                desc.layouts[l].stepRate = layouts[l]._instanceStepDataRate;
            }
        }

        #if defined(_DEBUG)
            if (!_allAttributesBound) {
                Log(Warning) << "Some attributes not bound for vertex shader:" << [program._vf.get() label] << std::endl;
                Log(Warning) << "Attributes on shader: " << std::endl;
                for (unsigned a=0; a<vf.vertexAttributes.count; ++a) {
                    Log(Warning) << "  [" << vf.vertexAttributes[a].attributeIndex << "] " << vf.vertexAttributes[a].name << std::endl;
                }
                Log(Warning) << "Attributes on provided from input layout: " << std::endl;
                for (unsigned l=0; l < layouts.size(); ++l) {
                    for (unsigned e=0; e < layouts[l]._elements.size(); ++e) {
                        Log(Warning) << "  [" << l << ", " << e << "] 0x" << std::hex << layouts[l]._elements[e]._semanticHash << std::dec << std::endl;
                    }
                }
            }
        #endif
    }

    BoundInputLayout::BoundInputLayout(IteratorRange<const InputElementDesc*> layout, const ShaderProgram& program)
    {
        id<MTLFunction> vf = program._vf.get();

        // Create a MTLVertexDescriptor to describe the input format for vertices
        _vertexDescriptor = TBC::OCPtr<MTLVertexDescriptor>(TBC::moveptr([[MTLVertexDescriptor alloc] init]));
        auto* desc = _vertexDescriptor.get();

        unsigned maxSlot = 0;
        for (const auto& e:layout)
            maxSlot = std::max(maxSlot, e._inputSlot);

        std::vector<unsigned> boundAttributes;
        boundAttributes.reserve(layout.size());

        std::vector<uint64_t> attributeHashes;
        attributeHashes.reserve(vf.vertexAttributes.count);
        for (MTLVertexAttribute* vertexAttribute in vf.vertexAttributes)
            attributeHashes.push_back(BuildSemanticHash(vertexAttribute.name.UTF8String));

        // Populate MTLVertexAttributeDescriptorArray
        for (unsigned slot=0; slot<maxSlot+1; ++slot) {
            unsigned workingStride = 0;

            unsigned inputDataRate = ~unsigned(0x0);
            unsigned inputStepFunction = ~unsigned(0x0);

            for (const auto& e:layout) {
                if (e._inputSlot != slot) continue;

                unsigned alignedOffset = e._alignedByteOffset;
                if (alignedOffset == ~unsigned(0x0)) {
                    alignedOffset = workingStride;
                }

                auto hash = BuildSemanticHash(e._semanticName.c_str()) + e._semanticIndex;

                auto i = std::find(attributeHashes.begin(), attributeHashes.end(), hash);
                if (i != attributeHashes.end()) {
                    auto* matchingAttribute = vf.vertexAttributes[std::distance(attributeHashes.begin(), i)];
                    if (matchingAttribute.active) {
                        auto attributeLoc = matchingAttribute.attributeIndex;
                        desc.attributes[attributeLoc].bufferIndex = e._inputSlot;
                        desc.attributes[attributeLoc].format = AsMTLVertexFormat(e._nativeFormat);
                        desc.attributes[attributeLoc].offset = alignedOffset;

                        // You will hit this assert if we attempt to bind the same attribute more
                        // than once
                        assert(std::find(boundAttributes.begin(), boundAttributes.end(), attributeLoc) == boundAttributes.end());
                        boundAttributes.push_back((unsigned)attributeLoc);
                    }
                }

                workingStride = alignedOffset + BitsPerPixel(e._nativeFormat) / 8;

                if (inputDataRate != ~unsigned(0x0) && e._instanceDataStepRate != inputDataRate)
                    Throw(std::runtime_error("Cannot create InputLayout because step rate not consistant across input slot"));
                if (inputStepFunction != ~unsigned(0x0) && unsigned(e._inputSlotClass) != inputStepFunction)
                    Throw(std::runtime_error("Cannot create InputLayout because step function not consistant across input slot"));
                inputDataRate = e._instanceDataStepRate;
                inputStepFunction = (unsigned)e._inputSlotClass;
            }

            if (inputDataRate == ~unsigned(0x0) && inputStepFunction == ~unsigned(0x0))
                continue;

            // Populate MTLVertexBufferLayoutDescriptorArray
            desc.layouts[slot].stride = CalculateVertexStrideForSlot(layout, slot);
            if (inputStepFunction == (unsigned)InputDataRate::PerVertex) {
                desc.layouts[slot].stepFunction = MTLVertexStepFunctionPerVertex;
            } else {
                desc.layouts[slot].stepFunction = MTLVertexStepFunctionPerInstance;
                desc.layouts[slot].stepRate = inputDataRate;
            }
        }

        _allAttributesBound = true;
        for (MTLVertexAttribute* vertexAttribute in vf.vertexAttributes) {
            if (!vertexAttribute.active)
                continue;

            if (std::find(boundAttributes.begin(), boundAttributes.end(), vertexAttribute.attributeIndex) == boundAttributes.end()) {
                _allAttributesBound = false;
                break;
            }
        }

        #if defined(_DEBUG)
            if (!_allAttributesBound) {
                Log(Warning) << "Some attributes not bound for vertex shader:" << [program._vf.get() label] << std::endl;
                Log(Warning) << "Attributes on shader: " << std::endl;
                for (unsigned a=0; a<vf.vertexAttributes.count; ++a) {
                    Log(Warning) << "  [" << vf.vertexAttributes[a].attributeIndex << "] " << vf.vertexAttributes[a].name << std::endl;
                }
                Log(Warning) << "Attributes on provided from input layout: " << std::endl;
                for (const auto&e:layout) {
                    Log(Warning) << "  [" << e._inputSlot << "] " << e._semanticName << " (" << e._semanticIndex << ")" << std::endl;
                }
            }
        #endif
    }

    BoundInputLayout::BoundInputLayout() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    static bool HasCBBinding(IteratorRange<const UniformsStreamInterface**> interfaces, uint64_t hash)
    {
        for (auto interf:interfaces) {
            auto i = std::find_if(
                interf->_cbBindings.begin(), interf->_cbBindings.end(),
                [hash](const UniformsStreamInterface::RetainedCBBinding& b) {
                    return b._hashName == hash;
                });
            if (i!=interf->_cbBindings.end())
                return true;
        }
        return false;
    }

    static bool HasSRVBinding(IteratorRange<const UniformsStreamInterface**> interfaces, uint64_t hash)
    {
        for (auto interf:interfaces) {
            auto i = std::find(interf->_srvBindings.begin(), interf->_srvBindings.end(), hash);
            if (i!=interf->_srvBindings.end())
                return true;
        }
        return false;
    }

    static bool HasCBBinding(IteratorRange<const UniformsStreamInterface*> interfaces, uint64_t hash)
    {
        for (const auto& interf:interfaces) {
            auto i = std::find_if(
                interf._cbBindings.begin(), interf._cbBindings.end(),
                [hash](const UniformsStreamInterface::RetainedCBBinding& b) {
                    return b._hashName == hash;
                });
            if (i!=interf._cbBindings.end())
                return true;
        }
        return false;
    }

    static bool HasSRVBinding(IteratorRange<const UniformsStreamInterface*> interfaces, uint64_t hash)
    {
        for (const auto& interf:interfaces) {
            auto i = std::find(interf._srvBindings.begin(), interf._srvBindings.end(), hash);
            if (i!=interf._srvBindings.end())
                return true;
        }
        return false;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    static StreamMapping MakeStreamMapping(
        MTLRenderPipelineReflection* reflection,
        unsigned streamIndex,
        const UniformsStreamInterface* interfaces,      // expecting exactly 4 entries
        ShaderStage stage)
    {
        assert(streamIndex < 4);
        assert(stage == ShaderStage::Vertex || stage == ShaderStage::Pixel);
        NSArray <MTLArgument *>* arguments = (stage == ShaderStage::Vertex) ? reflection.vertexArguments : reflection.fragmentArguments;
        StreamMapping result;

        auto argCount = arguments.count;
        for (unsigned argIdx=0; argIdx<argCount; ++argIdx) {
            MTLArgument* arg = arguments[argIdx];
            if (!arg.active)
                continue;

            const char* argName = [arg.name cStringUsingEncoding:NSUTF8StringEncoding];
            auto argHash = BuildSemanticHash(argName);

            // Look for matching input in our interface
            if (arg.type == MTLArgumentTypeTexture) {

                unsigned matchingSlot = ~0u;
                for (unsigned c=0; c<(unsigned)interfaces[streamIndex]._srvBindings.size(); ++c) {
                    if (interfaces[streamIndex]._srvBindings[c] == argHash) {
                        matchingSlot = c;
                        break;
                    }
                }

                if (matchingSlot != ~0u) {
                    // look for a binding in a later stream interface
                    if (!HasSRVBinding(MakeIteratorRange(&interfaces[streamIndex+1], &interfaces[4]), argHash)) {
                        result._srvs.push_back({
                            matchingSlot, (unsigned)arg.index
                            #if defined(_DEBUG)
                                , argName
                            #endif
                        });
                        assert(argIdx < 64);
                        result._boundArgs |= 1<<uint64_t(argIdx);
                    }
                }

            } else if (arg.type == MTLArgumentTypeBuffer) {

                unsigned matchingSlot = ~0u;
                for (unsigned c=0; c<(unsigned)interfaces[streamIndex]._cbBindings.size(); ++c) {
                    if (interfaces[streamIndex]._cbBindings[c]._hashName == argHash) {
                        matchingSlot = c;
                        break;
                    }
                }

                if (matchingSlot != ~0u) {
                    // look for a binding in a later stream interface
                    if (!HasCBBinding(MakeIteratorRange(&interfaces[streamIndex+1], &interfaces[4]), argHash)) {
                        result._cbs.push_back({
                            matchingSlot, (unsigned)arg.index
                            #if defined(_DEBUG)
                                , argName
                            #endif
                        });
                        assert(argIdx < 64);
                        result._boundArgs |= 1<<uint64_t(argIdx);
                    }
                }

            }

        }

        return result;
    }

    static void ApplyUniformStreamVS(
        GraphicsPipeline& context, const UniformsStream& stream,
        const StreamMapping& streamMapping)
    {
        id<MTLRenderCommandEncoder> encoder = context.GetCommandEncoder();

        for (const auto& b:streamMapping._cbs) {
            assert(b._uniformStreamSlot < stream._constantBuffers.size());
            const auto& constantBuffer = stream._constantBuffers[b._uniformStreamSlot];
            const auto& pkt = constantBuffer._packet;
            if (!pkt.size()) {
                assert(constantBuffer._prebuiltBuffer);
                [encoder setVertexBuffer:checked_cast<const Resource*>(constantBuffer._prebuiltBuffer)->GetBuffer().get()
                                  offset:0
                                 atIndex:b._shaderSlot];
            } else {
                [encoder setVertexBytes:pkt.get() length:(unsigned)pkt.size() atIndex:b._shaderSlot];
            }
        }

        for (const auto& b:streamMapping._srvs) {
            const auto& shaderResource = *(const ShaderResourceView*)stream._resources[b._uniformStreamSlot];
            if (!shaderResource.IsGood()) {
                Log(Verbose) << "==================> ShaderResource is bad/invalid while binding shader sampler (" << b._name << ")" << std::endl;
            } else {
                const auto& texture = shaderResource.GetUnderlying();
                [encoder setVertexTexture:texture.get() atIndex:b._shaderSlot];
            }
        }
    }

    static void ApplyUniformStreamPS(
        GraphicsPipeline& context, const UniformsStream& stream,
        const StreamMapping& streamMapping)
    {
        id<MTLRenderCommandEncoder> encoder = context.GetCommandEncoder();

        for (const auto& b:streamMapping._cbs) {
            assert(b._uniformStreamSlot < stream._constantBuffers.size());
            const auto& constantBuffer = stream._constantBuffers[b._uniformStreamSlot];
            const auto& pkt = constantBuffer._packet;
            if (!pkt.size()) {
                assert(constantBuffer._prebuiltBuffer);
                [encoder setFragmentBuffer:checked_cast<const Resource*>(constantBuffer._prebuiltBuffer)->GetBuffer().get()
                                  offset:0
                                 atIndex:b._shaderSlot];
            } else {
                [encoder setFragmentBytes:pkt.get() length:(unsigned)pkt.size() atIndex:b._shaderSlot];
            }
        }

        for (const auto& b:streamMapping._srvs) {
            const auto& shaderResource = *(const ShaderResourceView*)stream._resources[b._uniformStreamSlot];
            if (!shaderResource.IsGood()) {
                Log(Verbose) << "==================> ShaderResource is bad/invalid while binding shader sampler (" << b._name << ")" << std::endl;
            } else {
                const auto& texture = shaderResource.GetUnderlying();
                [encoder setFragmentTexture:texture.get() atIndex:b._shaderSlot];
            }
        }
    }

    void BoundUniforms::Apply(DeviceContext& context, unsigned streamIdx, const UniformsStream& stream) const
    {
        if (_unboundInterface) {
            context.QueueUniformSet(_unboundInterface, streamIdx, stream);
            return;
        }

        assert(streamIdx < dimof(_preboundInterfaceVS));
        ApplyUniformStreamVS(context, stream, _preboundInterfaceVS[streamIdx]);
        ApplyUniformStreamPS(context, stream, _preboundInterfacePS[streamIdx]);

        /*
        if (streamIdx == 0) {
            for (const auto& b:_unbound2DSRVs) {
                context.Bind(GetObjectFactory().StandInCubeTexture(), b.second, b.first);
            }
            for (const auto& b:_unboundCubeSRVs) {
                context.Bind(GetObjectFactory().StandIn2DTexture(), b.second, b.first);
            }
        }
        */
    }

    BoundUniforms::BoundArguments BoundUniforms::Apply_UnboundInterfacePath(
        GraphicsPipeline& context,
        MTLRenderPipelineReflection* pipelineReflection,
        const UnboundInterface& unboundInterface,
        unsigned streamIdx,
        const UniformsStream& stream)
    {
        auto bindingVS = MakeStreamMapping(pipelineReflection, streamIdx, unboundInterface._interface, ShaderStage::Vertex);
        ApplyUniformStreamVS(context, stream, bindingVS);
        auto bindingPS = MakeStreamMapping(pipelineReflection, streamIdx, unboundInterface._interface, ShaderStage::Pixel);
        ApplyUniformStreamPS(context, stream, bindingPS);

        return { bindingVS._boundArgs, bindingPS._boundArgs };
    }

    void BoundUniforms::Apply_Standins(
        GraphicsPipeline& context,
        MTLRenderPipelineReflection* pipelineReflection,
        uint64_t vsArguments, uint64_t psArguments)
    {
        id<MTLRenderCommandEncoder> encoder = context.GetCommandEncoder();
        auto* vsArgs = pipelineReflection.vertexArguments;

        unsigned vsArgCount = std::min(64u - xl_clz8(vsArguments), (unsigned)vsArgs.count);
        for (unsigned argIdx=0; argIdx<vsArgCount; ++argIdx) {
            MTLArgument* arg = vsArgs[argIdx];
            if (!arg.active || !(vsArguments & (1<<uint64_t(argIdx))))
                continue;

            if (arg.type == MTLArgumentTypeTexture) {
                if (arg.textureType == MTLTextureTypeCube) {
                    [encoder setVertexTexture:GetObjectFactory().StandInCubeTexture().get() atIndex:arg.index];
                } else {
                    [encoder setVertexTexture:GetObjectFactory().StandIn2DTexture().get() atIndex:arg.index];
                }
            }
        }

        auto* psArgs = pipelineReflection.fragmentArguments;

        unsigned psArgCount = std::min(64u - xl_clz8(psArguments), (unsigned)psArgs.count);
        for (unsigned argIdx=0; argIdx<psArgCount; ++argIdx) {
            MTLArgument* arg = psArgs[argIdx];
            if (!arg.active || !(psArguments & (1<<uint64_t(argIdx))))
                continue;

            if (arg.type == MTLArgumentTypeTexture) {
                if (arg.textureType == MTLTextureTypeCube) {
                    [encoder setFragmentTexture:GetObjectFactory().StandInCubeTexture().get() atIndex:arg.index];
                } else {
                    [encoder setFragmentTexture:GetObjectFactory().StandIn2DTexture().get() atIndex:arg.index];
                }
            }
        }
    }

#if 0
    void BoundUniforms::Apply(DeviceContext& context, unsigned streamIdx, const UniformsStream& stream) const
    {
        /* Overview: Use reflection to determine the proper indices in the argument table
         * for buffers and textures.  Then, bind the resources from the stream. */

        const auto& reflectionInformation = context.GetReflectionInformation(_vf, _ff);
        const std::pair<const std::vector<ReflectionInformation::Mapping>*, GraphicsPipeline::ShaderTarget> shaderMappings[] = {
            std::make_pair(&reflectionInformation._vfMappings, GraphicsPipeline::ShaderTarget::Vertex),
            std::make_pair(&reflectionInformation._ffMappings, GraphicsPipeline::ShaderTarget::Fragment)
        };
#if DEBUG
        /* Validation/sanity check of reflection information */
        {
            for (unsigned m=0; m < dimof(shaderMappings); ++m) {
                const auto& mappingSet = shaderMappings[m];
                if (m == 0) {
                    assert(mappingSet.second == GraphicsPipeline::ShaderTarget::Vertex);
                    const auto* vfmap = mappingSet.first;
                    assert(vfmap->size() == reflectionInformation._vfMappings.size());
                }
                if (m == 1) {
                    assert(mappingSet.second == GraphicsPipeline::ShaderTarget::Fragment);
                    const auto* ffmap = mappingSet.first;
                    assert(ffmap->size() == reflectionInformation._ffMappings.size());
                }
            }
        }

        // Ensure that constant buffer binding indices don't overlap with vertex buffer - this is one cause of a GPU hang.
        // Likewise, ensure that other bindings don't overlap.
        MTLRenderPipelineReflection* renderReflection = reflectionInformation._debugReflection.get();
        {
            /* Iterate over vertex and function arguments, ensuring that index in each argument table is not used more than once */
            const NSArray<MTLArgument*>* argumentSets[] = { renderReflection.vertexArguments, renderReflection.fragmentArguments };
            for (unsigned as=0; as < dimof(argumentSets); ++as) {
                uint32_t bufferArgTable = 0u;
                uint32_t textureArgTable = 0u;
                uint32_t samplerArgTable = 0u;
                for (MTLArgument* arg in argumentSets[as]) {
                    if (!arg.active) continue;

                    uint32_t intendedIndex = (1 << arg.index);
                    if (arg.type == MTLArgumentTypeBuffer) {
                        if ((intendedIndex & bufferArgTable) != 0) {
                            // NSLog(@"================> %@ is using buffer index %lu, which is already in use.  This could cause stomping of data and a GPU hang if accessed in a shader.", arg.name, (unsigned long)arg.index);
                        }
                        // assert((intendedIndex & bufferArgTable) == 0);
                        bufferArgTable |= intendedIndex;
                    } else if (arg.type == MTLArgumentTypeTexture) {
                        if ((intendedIndex & textureArgTable) != 0) {
                            NSLog(@"================> %@ is using texture index %lu, which is already in use.", arg.name, (unsigned long)arg.index);
                        }
                        assert((intendedIndex & textureArgTable) == 0);
                        textureArgTable |= intendedIndex;
                    } else if (arg.type == MTLArgumentTypeSampler) {
                        assert((intendedIndex & samplerArgTable) == 0);
                        samplerArgTable |= intendedIndex;
                    }
                }
            }
        }
#endif

        // KenD -- Metal optimization -- the binding lookup could probably be improved by reorganizing the iteration and ordering

        // If the constant buffer hash matches a function argument, bind the resource to the mapped location
        for (const auto& cb : _cbs) {
            if (cb.streamIdx != streamIdx) continue;

            // Bind to vertex and fragment functions
            for (unsigned m=0; m < dimof(shaderMappings); ++m) {
                const auto& mappingSet = shaderMappings[m];
                const auto& mappings = *mappingSet.first;
                for (unsigned r=0; r < mappings.size(); ++r) {
                    const auto& map = mappings[r];
                    if (cb.hashName == map.hashName) {
                        const auto& constantBuffer = stream._constantBuffers[cb.slot];
                        const auto& pkt = constantBuffer._packet;
                        //
                        // Input CBV might be a SharedPkt (ie, just some temporary data); or it
                        // could be an actual prebuilt hardware resource
                        //
                        if (!pkt.size()) {
                            assert(constantBuffer._prebuiltBuffer);
                            context.Bind(
                                checked_cast<const Resource*>(constantBuffer._prebuiltBuffer)->GetBuffer().get(),
                                0, map.index, mappingSet.second);
                        } else {
                            const void* constantBufferData = pkt.get();
                            unsigned length = (unsigned)pkt.size();

#if DEBUG
                            {
                                NSArray<MTLArgument*>* arguments = nil;
                                if (mappingSet.second == GraphicsPipeline::ShaderTarget::Vertex) {
                                    arguments = renderReflection.vertexArguments;
                                } else if (mappingSet.second == GraphicsPipeline::ShaderTarget::Fragment) {
                                    arguments = renderReflection.fragmentArguments;
                                } else {
                                    assert(0);
                                }
                                for (MTLArgument* arg in arguments) {
                                    if (!arg.active) continue;

                                    if (arg.type == MTLArgumentTypeBuffer) {
                                        if (arg.index == map.index) {
                                            // assert(length == arg.bufferDataSize);
                                            assert(BuildSemanticHash([arg.name cStringUsingEncoding:NSUTF8StringEncoding]) == cb.hashName);

/* Only in Metal shading language 2.0 -- newer versions of Xcode warned about this, but I ignored it */
#if 0
                                            MTLPointerType* ptrType = arg.bufferPointerType;
                                            MTLStructType* structType = ptrType.elementStructType;
                                            if (structType) {
                                                /* Metal TODO -- examine elements, comparing pipeline layout with struct, ensuring the offset is reasonable */
                                            }
#endif
                                        }
                                    }
                                }
                            }
#endif

                            context.Bind(constantBufferData, length, map.index, mappingSet.second);
                        }
                        break;
                    }
                }
            }
        }

        // Bind to vertex and fragment functions
        for (unsigned m=0; m < dimof(shaderMappings); ++m) {
            const auto& mappingSet = shaderMappings[m];
            const auto& mappings = *mappingSet.first;
            for (unsigned r=0; r < mappings.size(); ++r) {
                const auto& map = mappings[r];
                if (map.type != ReflectionInformation::MappingType::Texture)
                    continue;

                bool gotBinding = false;
                for (const auto& srv : _srvs) {
                    if (srv.hashName == map.hashName) {
                        if (srv.streamIdx == streamIdx) {
                            const auto& shaderResource = *(const ShaderResourceView*)stream._resources[srv.slot];
                            if (!shaderResource.IsGood()) {
                                #if DEBUG
                                    PrintMissingTextureBinding(srv.hashName);
                                    NSLog(@"================> Error in texture when trying to bind");
                                #endif
                            } else {
                                const auto& texture = shaderResource.GetUnderlying();
                                id<MTLTexture> mtlTexture = texture.get();
                                context.Bind(mtlTexture, map.index, mappingSet.second);
                                gotBinding = true;
                            }
                        } else {
                            gotBinding = true;      // if it's part of a different uniform stream, regard it as "bound"
                        }
                        break;
                    }
                }

                // For safety, we must bind a texture here, because some Metal drivers are unstable
                // when no texture is bound to a shader. We will either bind a cubemap or 2D texture.
                // There are other types of textures (1D, 3D, array, etc), but we will just use the
                // 2D texture in those cases
                if (!gotBinding) {
                    if (map.textureType == MTLTextureTypeCube) {
                        context.Bind(GetObjectFactory().StandInCubeTexture(), map.index, mappingSet.second);
                    } else {
                        context.Bind(GetObjectFactory().StandIn2DTexture(), map.index, mappingSet.second);
                    }
                }
            }
        }
    }
#endif

    /* KenD -- Metal TODO -- validate the layout of the constant buffer view,
         * within the UniformsStreamInterface, against the reflected pipeline state.
         * The structure used in the shader must match the layout we use for the buffer.
         */

    BoundUniforms::BoundUniforms(
        const ShaderProgram& shader,
        const PipelineLayoutConfig& pipelineLayout,
        const UniformsStreamInterface& interface0,
        const UniformsStreamInterface& interface1,
        const UniformsStreamInterface& interface2,
        const UniformsStreamInterface& interface3)
    {
        for (auto& s : _boundUniformBufferSlots) s = 0ull;
        for (auto& s : _boundResourceSlots) s = 0ull;

        const UniformsStreamInterface* interfaces[] = { &interface0, &interface1, &interface2, &interface3 };
        auto streamCount = dimof(interfaces);
        for (unsigned s=0; s < streamCount; ++s) {
            const auto& interface = *interfaces[s];

            for (unsigned slot = 0; slot < interface._cbBindings.size(); ++slot) {
                const auto& cbBinding = interface._cbBindings[slot];

                // Skip if future binding should take precedence
                if (HasCBBinding(MakeIteratorRange(&interfaces[s+1], &interfaces[streamCount]), cbBinding._hashName))
                    continue;
                _boundUniformBufferSlots[s] |= (1ull << uint64_t(slot));
            }

            for (unsigned slot=0; slot < interface._srvBindings.size(); ++slot) {
                auto& srvBinding = interface._srvBindings[slot];

                // Skip if future binding should take precedence
                if (HasSRVBinding(MakeIteratorRange(&interfaces[s+1], &interfaces[streamCount]), srvBinding))
                    continue;
                _boundResourceSlots[s] |= (1ull << uint64_t(slot));
            }
        }

        _unboundInterface = std::make_unique<UnboundInterface>();
        _unboundInterface->_interface[0] = interface0;
        _unboundInterface->_interface[1] = interface1;
        _unboundInterface->_interface[2] = interface2;
        _unboundInterface->_interface[3] = interface3;
    }

    BoundUniforms::~BoundUniforms() {}

    BoundUniforms::BoundUniforms()
    {
        for (auto& s : _boundUniformBufferSlots) s = 0ull;
        for (auto& s : _boundResourceSlots) s = 0ull;
    }

    BoundUniforms::BoundUniforms(BoundUniforms&& moveFrom) never_throws
    : _unboundInterface(std::move(moveFrom._unboundInterface))
    {
        for (unsigned c=0; c<dimof(moveFrom._boundUniformBufferSlots); ++c) {
            _boundUniformBufferSlots[c] = moveFrom._boundUniformBufferSlots[c];
            _boundResourceSlots[c] = moveFrom._boundResourceSlots[c];
            moveFrom._boundUniformBufferSlots[c] = 0ull;
            moveFrom._boundResourceSlots[c] = 0ull;
        }
        for (unsigned c=0; c<dimof(moveFrom._preboundInterfaceVS); ++c) {
            _preboundInterfaceVS[c] = std::move(_preboundInterfaceVS[c]);
        }
        for (unsigned c=0; c<dimof(moveFrom._preboundInterfacePS); ++c) {
            _preboundInterfacePS[c] = std::move(_preboundInterfacePS[c]);
        }
    }

    BoundUniforms& BoundUniforms::operator=(BoundUniforms&& moveFrom) never_throws
    {
        _unboundInterface = std::move(moveFrom._unboundInterface);
        for (unsigned c=0; c<dimof(moveFrom._boundUniformBufferSlots); ++c) {
            _boundUniformBufferSlots[c] = moveFrom._boundUniformBufferSlots[c];
            _boundResourceSlots[c] = moveFrom._boundResourceSlots[c];
            moveFrom._boundUniformBufferSlots[c] = 0ull;
            moveFrom._boundResourceSlots[c] = 0ull;
        }
        for (unsigned c=0; c<dimof(moveFrom._preboundInterfaceVS); ++c) {
            _preboundInterfaceVS[c] = std::move(_preboundInterfaceVS[c]);
        }
        for (unsigned c=0; c<dimof(moveFrom._preboundInterfacePS); ++c) {
            _preboundInterfacePS[c] = std::move(_preboundInterfacePS[c]);
        }
        return *this;
    }
}}
