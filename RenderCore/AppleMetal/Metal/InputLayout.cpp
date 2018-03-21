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
#include "../../../Utility/StringUtils.h"
#include "../../../Utility/StringFormat.h"
#include "../../../Utility/PtrUtils.h"
#include "../../../Utility/ArithmeticUtils.h"
#include <map>

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
            context.Bind(buffer, vbv._offset, i, GraphicsPipeline::ShaderTarget::Vertex);
            ++i;
        }
    }

    BoundInputLayout::BoundInputLayout(IteratorRange<const InputElementDesc*> layout, const ShaderProgram& program)
    {
        // KenD -- Currently skipping InputElementDesc implementation and only supporting MiniInputElementDesc
        assert(0);
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
    void PrintMissingVertexAttribute(uint64 missingSemanticHash)
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

        NSLog(@"==================> Missing %@ (%llu)", knownAttrs[@(missingSemanticHash)], missingSemanticHash);
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

        // Map each vertex attribute's semantic hash to its attribute index
        id<MTLFunction> vf = program._vf.get();
        std::map<uint64_t, unsigned> hashToLocation;
        for (MTLVertexAttribute* vertexAttribute in vf.vertexAttributes) {
            hashToLocation[BuildSemanticHash(vertexAttribute.name.UTF8String)] = (unsigned)vertexAttribute.attributeIndex;
        }

        // Create a MTLVertexDescriptor to describe the input format for vertices
        _vertexDescriptor = TBC::OCPtr<MTLVertexDescriptor>(TBC::moveptr([[MTLVertexDescriptor alloc] init]));
        auto* desc = _vertexDescriptor.get();

        for (unsigned l=0; l < layouts.size(); ++l) {
            // Populate MTLVertexAttributeDescriptorArray
            for (const auto& e : layouts[l]._elements) {
                auto found = hashToLocation.find(e._semanticHash);
                if (found == hashToLocation.end()) {
                    /* There may be some data that is provided or specified in the layout that the shader will not use,
                     * such as bitangents or normals.
                     * That's okay - the shader is relatively simple compared to the vertex.
                     * However, it is a problem if the shader expects an attribute that is not provided by the input.
                     */
#if DEBUG
                    //PrintMissingVertexAttribute(e._semanticHash);
#endif
                    continue;
                }

                unsigned attributeLoc = found->second;
                desc.attributes[attributeLoc].bufferIndex = l;
                desc.attributes[attributeLoc].format = AsMTLVertexFormat(e._nativeFormat);
                desc.attributes[attributeLoc].offset = CalculateVertexStride(MakeIteratorRange(layouts[l]._elements.begin(), &e), false);
            }

            // Populate MTLVertexBufferLayoutDescriptorArray
            desc.layouts[l].stride = CalculateVertexStride(layouts[l]._elements, false);
            if (layouts[l]._instanceStepDataRate == 0) {
                desc.layouts[l].stepFunction = MTLVertexStepFunctionPerVertex;
            } else {
                desc.layouts[l].stepFunction = MTLVertexStepFunctionPerInstance;
                desc.layouts[l].stepRate = layouts[l]._instanceStepDataRate;
            }
        }
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
        const std::pair<const std::vector<ReflectionInformation::Mapping>*, GraphicsPipeline::ShaderTarget> shaderMappings[] = { std::make_pair(&reflectionInformation._vfMappings, GraphicsPipeline::ShaderTarget::Vertex), std::make_pair(&reflectionInformation._ffMappings, GraphicsPipeline::ShaderTarget::Fragment) };
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
                    uint32_t intendedIndex = (1 << arg.index);
                    if (arg.type == MTLArgumentTypeBuffer) {
                        if ((intendedIndex & bufferArgTable) != 0) {
                            NSLog(@"================> %@ is using buffer index %lu, which is already in use.  This could cause stomping of data and a GPU hang if accessed in a shader.", arg.name, (unsigned long)arg.index);
                        }
                        assert((intendedIndex & bufferArgTable) == 0);
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
                        /* KenD -- Metal TODO -- support binding buffers in addition to packets.
                         * When creating the constantBuffer, prefer packets for smaller sizes
                         * and buffers for larger sizes.
                         */
                        const auto& constantBuffer = stream._constantBuffers[cb.slot];
                        const auto& pkt = constantBuffer._packet;
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
                                if (arg.type == MTLArgumentTypeBuffer) {
                                    if (arg.index == map.index) {
                                        assert(length == arg.bufferDataSize);
                                        assert(BuildSemanticHash([arg.name cStringUsingEncoding:NSUTF8StringEncoding]) == cb.hashName);

                                        MTLPointerType* ptrType = arg.bufferPointerType;
                                        MTLStructType* structType = ptrType.elementStructType;
                                        if (structType) {
                                            /* Metal TODO -- examine elements, comparing pipeline layout with struct, ensuring the offset is reasonable */
                                        }
                                    }
                                }
                            }
                        }
#endif

                        context.Bind(constantBufferData, length, map.index, mappingSet.second);
                        break;
                    }
                }
            }
        }

        for (const auto& srv : _srvs) {
            if (srv.streamIdx != streamIdx) continue;

            // Bind to vertex and fragment functions
            for (unsigned m=0; m < dimof(shaderMappings); ++m) {
                const auto& mappingSet = shaderMappings[m];
                const auto& mappings = *mappingSet.first;
                for (unsigned r=0; r < mappings.size(); ++r) {
                    const auto& map = mappings[r];
                    if (srv.hashName == map.hashName) {
                        const auto& shaderResource = *(ShaderResourceView*)stream._resources[srv.slot];

                        if (!shaderResource.IsGood()) {
                            PrintMissingTextureBinding(srv.hashName);
                            NSLog(@"================> Error in texture when trying to bind");
                            continue;
                        }
                        const auto& texture = shaderResource.GetUnderlying();
                        id<MTLTexture> mtlTexture = texture.get();
                        context.Bind(mtlTexture, map.index, mappingSet.second);
                        break;
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

                // Skip binding if there are no elements (the interface may have non-contiguous bound slots; there's no need to keep track of them in the BoundUniforms)
                if (cbBinding._elements.size() == 0) {
                    continue;
                }

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
