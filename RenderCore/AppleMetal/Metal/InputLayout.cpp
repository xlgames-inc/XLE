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
            context.Bind(buffer, vbv._offset, i, ShaderStage::Vertex);
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

#if DEBUG
    NSString* DeHashVertexAttribute(uint64 semanticHash)
    {
        static NSMutableDictionary* knownAttrs = nil;
        if (!knownAttrs) {
            knownAttrs = [[NSMutableDictionary alloc] init];
            knownAttrs[@(Hash64("in_position"))] = @"in_position";
            knownAttrs[@(Hash64("in_speed"))] = @"in_speed";
            knownAttrs[@(Hash64("in_uv_life"))] = @"in_uv_life";
            knownAttrs[@(Hash64("in_color") + 1)] = @"in_color1";
            knownAttrs[@(Hash64("in_color") + 2)] = @"in_color2";
            knownAttrs[@(Hash64("in_color") + 3)] = @"in_color3";
            knownAttrs[@(Hash64("in_scale"))] = @"in_scale";
            knownAttrs[@(Hash64("in_moreScale"))] = @"in_moreScale";
            knownAttrs[@(Hash64("in_offsetPosition"))] = @"in_offsetPosition";
            knownAttrs[@(Hash64("in_rotation"))] = @"in_rotation";
            knownAttrs[@(Hash64("in_textureRandomStart"))] = @"in_textureRandomStart";
            knownAttrs[@(Hash64("in_positionCoord"))] = @"in_positionCoord";
            knownAttrs[@(Hash64("in_modelMatrix_4e") + 0)] = @"in_modelMatrix_4e0";
            knownAttrs[@(Hash64("in_modelMatrix_4e") + 1)] = @"in_modelMatrix_4e1";
            knownAttrs[@(Hash64("in_modelMatrix_4e") + 2)] = @"in_modelMatrix_4e2";

            knownAttrs[@(Hash64("a_cc3Bitangent"))] = @"a_cc3Bitangent";
            knownAttrs[@(Hash64("a_cc3BoneIndices"))] = @"a_cc3BoneIndices";
            knownAttrs[@(Hash64("a_cc3BoneWeights"))] = @"a_cc3BoneWeights";
            knownAttrs[@(Hash64("a_cc3Color"))] = @"a_cc3Color";
            knownAttrs[@(Hash64("a_cc3Normal"))] = @"a_cc3Normal";
            knownAttrs[@(Hash64("a_cc3Position"))] = @"a_cc3Position";
            knownAttrs[@(Hash64("a_cc3Tangent"))] = @"a_cc3Tangent";
            knownAttrs[@(Hash64("a_cc3TexCoord"))] = @"a_cc3TexCoord";
        }

        id res = knownAttrs[@(semanticHash)];
        if (res)
            return (NSString*)res;
        return [NSString stringWithFormat:@"Unknown semantic hash: 0x%llx", semanticHash];
    }

    void PrintMissingTextureBinding(uint64 missingSemanticHash)
    {
        static NSMutableDictionary* knownAttrs = nil;
        if (!knownAttrs) {
            knownAttrs = [[NSMutableDictionary alloc] init];

            knownAttrs[@(Hash64("u_irradianceMap"))] = @"u_irradianceMap";
            knownAttrs[@(Hash64("u_reflectionMap"))] = @"u_reflectionMap";
            knownAttrs[@(Hash64("SpecularIBL"))] = @"SpecularIBL";
            knownAttrs[@(Hash64("u_shadowMap"))] = @"u_shadowMap";
            knownAttrs[@(Hash64("u_shadowRotateTable"))] = @"u_shadowRotateTable";

            knownAttrs[@(Hash64("s_cc3Texture2Ds")+0)] = @"s_cc3Texture2Ds0";
            knownAttrs[@(Hash64("s_cc3Texture2Ds")+1)] = @"s_cc3Texture2Ds1";
            knownAttrs[@(Hash64("s_cc3Texture2Ds")+2)] = @"s_cc3Texture2Ds2";
        }

        NSLog(@"==================> Missing %@ (%llu)", knownAttrs[@(missingSemanticHash)], missingSemanticHash);
    }
#endif

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
                        Log(Warning) << "  [" << l << ", " << e << "] " << DeHashVertexAttribute(layouts[l]._elements[e]._semanticHash) << std::endl;
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

    /* KenD -- cleanup TODO -- this was copied from OpenGLES implementation */

    static bool HasCBBinding(IteratorRange<const UniformsStreamInterface**> interfaces, uint64_t hash)
    {
        for (auto interf:interfaces) {
            auto i = std::find_if(
                                  interf->_cbBindings.begin(),
                                  interf->_cbBindings.end(),
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

////////////////////////////////////////////////////////////////////////////////////////////////////

    void BoundUniforms::Apply(
            DeviceContext& context,
            unsigned streamIdx,
            const UniformsStream& stream) const
    {
        /* Overview: Use reflection to determine the proper indices in the argument table
         * for buffers and textures.  Then, bind the resources from the stream. */

        const auto& reflectionInformation = context.GetReflectionInformation(_vf, _ff);
        const std::pair<const std::vector<ReflectionInformation::Mapping>*,  ShaderStage> shaderMappings[] = {
            std::make_pair(&reflectionInformation._vfMappings, ShaderStage::Vertex),
            std::make_pair(&reflectionInformation._ffMappings, ShaderStage::Pixel)
        };
#if DEBUG
        /* Validation/sanity check of reflection information */
        {
            for (unsigned m=0; m < dimof(shaderMappings); ++m) {
                const auto& mappingSet = shaderMappings[m];
                if (m == 0) {
                    assert(mappingSet.second == ShaderStage::Vertex);
                    const auto* vfmap = mappingSet.first;
                    assert(vfmap->size() == reflectionInformation._vfMappings.size());
                }
                if (m == 1) {
                    assert(mappingSet.second == ShaderStage::Pixel);
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
                                if (mappingSet.second == ShaderStage::Vertex) {
                                    arguments = renderReflection.vertexArguments;
                                } else if (mappingSet.second == ShaderStage::Pixel) {
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
                if (map.type != ReflectionInformation::MappingType::Texture && map.type != ReflectionInformation::MappingType::Sampler)
                    continue;

                // When binding a shader resource view, we might be setting a texture or a sampler.
                // While the resource and sampler are at the same slot in the UniformsStream,
                // the index that we actually bind to in the shader may differ between texture and sampler.
                // We cannot assume that the index in the shader is the same for texture and sampler.
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

                                if (map.type == ReflectionInformation::MappingType::Texture) {
                                    context.Bind(mtlTexture, map.index, mappingSet.second);
                                    gotBinding = true;
                                } else if (map.type == ReflectionInformation::MappingType::Sampler) {
                                    const auto& samplerState = *(SamplerState*)stream._samplers[srv.slot];

                                    samplerState.Apply(context, mtlTexture.mipmapLevelCount > 1, map.index, mappingSet.second);
                                    gotBinding = true;
                                }
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
                    if (map.type == ReflectionInformation::MappingType::Texture) {
                        if (map.textureType == MTLTextureTypeCube) {
                            context.Bind(GetObjectFactory().StandInCubeTexture(), map.index, mappingSet.second);
                        } else {
                            context.Bind(GetObjectFactory().StandIn2DTexture(), map.index, mappingSet.second);
                        }
                    } else if (map.type == ReflectionInformation::MappingType::Sampler) {
                        context.Bind(GetObjectFactory().StandInSamplerState(), map.index, mappingSet.second);
                    }
                }
            }
        }
    }

    BoundUniforms::BoundUniforms(
        const ShaderProgram& shader,
        const PipelineLayoutConfig& pipelineLayout,
        const UniformsStreamInterface& interface0,
        const UniformsStreamInterface& interface1,
        const UniformsStreamInterface& interface2,
        const UniformsStreamInterface& interface3)
    {
        _vf = shader._vf.get();
        _ff = shader._ff.get();

        for (auto& s : _boundUniformBufferSlots) s = 0ull;
        for (auto& s : _boundResourceSlots) s = 0ull;

        /* Store stream and slot indices for resource hashes so the matching resources (matching based on hash)
         * can be bound to the proper destination indices when applying a UniformsStream. */

        const UniformsStreamInterface* interfaces[] = { &interface0, &interface1, &interface2, &interface3 };
        auto streamCount = dimof(interfaces);
        for (unsigned s=0; s < streamCount; ++s) {
            const auto& interface = *interfaces[s];

            for (unsigned slot = 0; slot < interface._cbBindings.size(); ++slot) {
                const auto& cbBinding = interface._cbBindings[slot];

                // Skip if future binding should take precedence
                if (HasCBBinding(MakeIteratorRange(&interfaces[s+1], &interfaces[streamCount]), cbBinding._hashName)) {
                    continue;
                }

                _cbs.emplace_back(CB{s, slot, cbBinding._hashName});
                _boundUniformBufferSlots[s] |= (1ull << uint64_t(slot));
            }

            for (unsigned slot=0; slot < interface._srvBindings.size(); ++slot) {
                auto& srvBinding = interface._srvBindings[slot];

                // Skip if future binding should take precedence
                if (HasCBBinding(MakeIteratorRange(&interfaces[s+1], &interfaces[streamCount]), srvBinding)) {
                    continue;
                }

                _srvs.emplace_back(SRV{s, slot, interface._srvBindings[slot]});
                _boundResourceSlots[s] |= (1ull << uint64_t(slot));
            }
        }

        /* KenD -- Metal TODO -- validate the layout of the constant buffer view,
         * within the UniformsStreamInterface, against the reflected pipeline state.
         * The structure used in the shader must match the layout we use for the buffer.
         */
    }

    BoundUniforms::~BoundUniforms() {}

    BoundUniforms::BoundUniforms() {
        assert(0);
        for (auto& s : _boundUniformBufferSlots) s = 0ull;
        for (auto& s : _boundResourceSlots) s = 0ull;
    }

    BoundUniforms::BoundUniforms(BoundUniforms&& moveFrom) never_throws
    : _cbs(std::move(moveFrom._cbs))
    , _srvs(std::move(moveFrom._srvs))
    , _vf(std::move(moveFrom._vf))
    , _ff(std::move(moveFrom._ff))
    {
        for (unsigned c=0; c<dimof(moveFrom._boundUniformBufferSlots); ++c) {
            _boundUniformBufferSlots[c] = moveFrom._boundUniformBufferSlots[c];
            _boundResourceSlots[c] = moveFrom._boundResourceSlots[c];
            moveFrom._boundUniformBufferSlots[c] = 0ull;
            moveFrom._boundResourceSlots[c] = 0ull;
        }
    }

    BoundUniforms& BoundUniforms::operator=(BoundUniforms&& moveFrom) never_throws
    {
        _cbs = std::move(moveFrom._cbs);
        _srvs = std::move(moveFrom._srvs);
        _vf = std::move(moveFrom._vf);
        _ff = std::move(moveFrom._ff);
        for (unsigned c=0; c<dimof(moveFrom._boundUniformBufferSlots); ++c) {
            _boundUniformBufferSlots[c] = moveFrom._boundUniformBufferSlots[c];
            _boundResourceSlots[c] = moveFrom._boundResourceSlots[c];
            moveFrom._boundUniformBufferSlots[c] = 0ull;
            moveFrom._boundResourceSlots[c] = 0ull;
        }
        return *this;
    }
}}
