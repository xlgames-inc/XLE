// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "InputLayout.h"
#include "Shader.h"
#include "ShaderIntrospection.h"
#include "Format.h"
#include "TextureView.h"
#include "PipelineLayout.h"
#include "State.h"
#include "Resource.h"
#include "DeviceContext.h"
#include "../../Types.h"
#include "../../Format.h"
#include "../../BufferView.h"
#include "../../../ConsoleRig/Log.h"
#include "../../../Core/SelectConfiguration.h"
#include "../../../Utility/StringUtils.h"
#include "../../../Utility/StringFormat.h"
#include "../../../Utility/PtrUtils.h"
#include "../../../Utility/ArithmeticUtils.h"
#include "IncludeGLES.h"

// Some platforms won't optimize unused varyigs out, which means some attributes might not be actually
// used by the program. For now, we'll just disable the checks and warnings.
#if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS || PLATFORMOS_TARGET == PLATFORMOS_OSX
    #define DISABLE_ATTRIBUTE_BINDING_CHECK 1
#endif

namespace RenderCore { namespace Metal_OpenGLES
{
    BoundInputLayout::BoundInputLayout(IteratorRange<const InputElementDesc*> layout, const ShaderProgram& program)
    {
            //
            //      For each entry in "layout", we need to compare its name
            //      with the names of the attributes in "shader".
            //
            //      When we find a match, write the details into a binding object.
            //      The binding object will be used to call glVertexAttribPointer.
            //
        const InputElementDesc* elements = layout.begin();
        size_t elementsCount = layout.size();
        _bindings.reserve(elementsCount);
        _attributeState = 0;
        _allAttributesBound = false;

        unsigned vbMax = 0;
        for (const auto&l:layout)
            vbMax = std::max(l._inputSlot, vbMax);

        auto programIndex = program.GetUnderlying();
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, (GLint*)&_maxVertexAttributes);

        for (unsigned vbIndex = 0; vbIndex <= vbMax; ++vbIndex) {
            auto bindingStart = _bindings.size();
            size_t vertexStride = 0;
            {
                unsigned lastElementEnd = 0;
                for (size_t c=0; c<elementsCount; ++c) {
                    if (elements[c]._inputSlot != vbIndex) continue;

                    const unsigned elementSize  = BitsPerPixel(elements[c]._nativeFormat) / 8;
                    const unsigned elementStart =
                        (elements[c]._alignedByteOffset != ~unsigned(0x0))?elements[c]._alignedByteOffset:lastElementEnd;
                    vertexStride = std::max(vertexStride, size_t(elementStart + elementSize));
                    lastElementEnd = elementStart + elementSize;
                }
            }

            unsigned lastElementEnd = 0;
            for (size_t c=0; c<elementsCount; ++c) {
                if (elements[c]._inputSlot != vbIndex) continue;

                char buffer[64];
                XlCopyString(buffer, elements[c]._semanticName.c_str());
                XlCatString(buffer, dimof(buffer), char('0' + elements[c]._semanticIndex));
                GLint attribute = glGetAttribLocation(programIndex->AsRawGLHandle(), buffer);

                const unsigned elementSize      = BitsPerPixel(elements[c]._nativeFormat) / 8;
                const unsigned elementStart     = (elements[c]._alignedByteOffset != ~unsigned(0x0)) ? elements[c]._alignedByteOffset : lastElementEnd;

                    //
                    //  The index can be left off the string for the first
                    //  one
                    //
                if (attribute == -1 && elements[c]._semanticIndex == 0) {
                    attribute = glGetAttribLocation(programIndex->AsRawGLHandle(), elements[c]._semanticName.c_str());
                }

                if (attribute < 0) {
                    #ifndef DISABLE_ATTRIBUTE_BINDING_CHECK
                        //  Binding failure! Write a warning, but ignore it. The binding is
                        //  still valid even if one or more attributes fail
                        Log(Warning) << "Failure during vertex attribute binding. Attribute (" << buffer << ") cannot be found in the program. Ignoring" << std::endl;
                    #endif
                } else {
                    const auto componentType = GetComponentType(elements[c]._nativeFormat);
                    _bindings.push_back({
                            unsigned(attribute),
                            GetComponentCount(GetComponents(elements[c]._nativeFormat)), AsGLVertexComponentType(elements[c]._nativeFormat),
                            (componentType == FormatComponentType::UNorm) || (componentType == FormatComponentType::SNorm) || (componentType == FormatComponentType::UNorm_SRGB),
                            unsigned(vertexStride),
                            elementStart,
                            elements[c]._inputSlotClass == InputDataRate::PerVertex ? 0 : elements[c]._instanceDataStepRate
                        });

                    assert(!(_attributeState & 1<<unsigned(attribute)));
                    _attributeState |= 1<<unsigned(attribute);
                }

                lastElementEnd = elementStart + elementSize;
            }

            if (bindingStart != (unsigned)_bindings.size())
                _bindingsByVertexBuffer.push_back(unsigned(_bindings.size() - bindingStart));
        }

        _allAttributesBound = CalculateAllAttributesBound(program);

        CheckGLError("Construct BoundInputLayout");
    }

    BoundInputLayout::BoundInputLayout(IteratorRange<const SlotBinding*> layouts, const ShaderProgram& program)
    {
        _attributeState = 0;
        _allAttributesBound = false;

        auto programHandle = program.GetUnderlying()->AsRawGLHandle();

        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, (GLint*)&_maxVertexAttributes);
        
        int activeAttributeCount = 0, activeAttributeMaxLength = 0;
        glGetProgramiv(programHandle, GL_ACTIVE_ATTRIBUTES, &activeAttributeCount);
        glGetProgramiv(programHandle, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &activeAttributeMaxLength);

        GLchar buffer[activeAttributeMaxLength];

        if (!layouts.empty()) {
            std::vector<Binding> workingBindings[layouts.size()];
            unsigned vertexStrides[layouts.size()];
            for (unsigned c=0; c<layouts.size(); ++c)
                vertexStrides[c] = CalculateVertexStride(layouts[c]._elements, false);

            for (int attrIndex=0; attrIndex<activeAttributeCount; ++attrIndex) {
                GLint size; GLenum type;
                GLsizei nameLen;
                glGetActiveAttrib(programHandle, attrIndex, activeAttributeMaxLength, &nameLen, &size, &type, buffer);
                if (!nameLen) continue;
                
                // ignore "gl" system attributes
                if (!strncmp(buffer, "gl_", 3)) continue;

                GLint attrLoc = glGetAttribLocation(programHandle, buffer);

                auto semanticIdx = nameLen;
                while (std::isdigit(buffer[semanticIdx-1])) --semanticIdx;
                uint64_t hash = Hash64(buffer, &buffer[semanticIdx]);
                hash += std::atoi(&buffer[semanticIdx]);

                bool foundBinding = false;
                for (auto l=layouts.begin(); l!=layouts.end(); ++l) {
                    auto i = std::find_if(l->_elements.begin(), l->_elements.end(), [hash](const MiniInputElementDesc&e) { return e._semanticHash == hash; });
                    if (i == l->_elements.end())
                        continue;

                    const auto elementStart = CalculateVertexStride(MakeIteratorRange(l->_elements.begin(), i), false);
                    unsigned slotIdx = (unsigned)std::distance(layouts.begin(), l);

                    const auto componentType = GetComponentType(i->_nativeFormat);
                    workingBindings[slotIdx].push_back({
                        unsigned(attrLoc),
                        GetComponentCount(GetComponents(i->_nativeFormat)), AsGLVertexComponentType(i->_nativeFormat),
                        (componentType == FormatComponentType::UNorm) || (componentType == FormatComponentType::SNorm) || (componentType == FormatComponentType::UNorm_SRGB),
                        vertexStrides[slotIdx],
                        elementStart,
                        l->_instanceStepDataRate
                    });

                    foundBinding = true;
                }

                if (!foundBinding) {
                    #ifndef DISABLE_ATTRIBUTE_BINDING_CHECK
                        Log(Warning) << "Failure during vertex attribute binding. Attribute (" << (const char*)buffer << ") cannot be found in the input binding. Ignoring" << std::endl;
                    #endif
                    continue;
                }

                assert(!(_attributeState & 1<<unsigned(attrLoc)));
                _attributeState |= 1<<unsigned(attrLoc);
            }

            for (unsigned c=0; c<layouts.size(); ++c) {
                _bindings.insert(_bindings.end(), workingBindings[c].begin(), workingBindings[c].end());
                _bindingsByVertexBuffer.push_back(unsigned(workingBindings[c].size()));
            }
            assert(_bindings.size() <= _maxVertexAttributes);
        } else {
            #ifndef DISABLE_ATTRIBUTE_BINDING_CHECK
                // note -- if layouts is empty, we must spit errors for all attributes, because they are
                // all unbound

                for (int attrIndex=0; attrIndex<activeAttributeCount; ++attrIndex) {
                    GLint size; GLenum type;
                    GLsizei nameLen;
                    glGetActiveAttrib(programHandle, attrIndex, activeAttributeMaxLength, &nameLen, &size, &type, buffer);
                    if (!nameLen) continue;
                    Log(Warning) << "Failure during vertex attribute binding. Attribute (" << (const char*)buffer << ") cannot be found in the input binding. Ignoring" << std::endl;
                }
            #endif
        }

        _allAttributesBound = CalculateAllAttributesBound(program);

        CheckGLError("Construct BoundInputLayout");
    }

    bool BoundInputLayout::CalculateAllAttributesBound(const ShaderProgram& program)
    {
    #ifndef DISABLE_ATTRIBUTE_BINDING_CHECK
        auto programHandle = program.GetUnderlying()->AsRawGLHandle();

        int activeAttributeCount = 0, activeAttributeMaxLength = 0;
        glGetProgramiv(programHandle, GL_ACTIVE_ATTRIBUTES, &activeAttributeCount);
        glGetProgramiv(programHandle, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &activeAttributeMaxLength);
        GLchar buffer[activeAttributeMaxLength];

        for (unsigned attrIndex = 0; attrIndex < activeAttributeCount; ++attrIndex) {
            GLint size; GLenum type;
            GLsizei nameLen;
            glGetActiveAttrib(programHandle, attrIndex, activeAttributeMaxLength, &nameLen, &size, &type, buffer);

            // ignore "gl" system attributes
            if (!strncmp(buffer, "gl_", 3)) continue;

            auto location = glGetAttribLocation(programHandle, buffer);

            bool hasBoundAttribute = false;
            for (const auto&b:_bindings) {
                if (b._attributeLocation == location) {
                    hasBoundAttribute = true;
                    break;
                }
            }

            if (!hasBoundAttribute) {
                Log(Warning) << "Failure during vertex attribute binding. Attribute (" << (const char*)buffer << ") cannot be found in the input binding." << std::endl;
                return false;
            }
        }
    #endif

        return true;
    }

    void BoundInputLayout::UnderlyingApply(DeviceContext& devContext, IteratorRange<const VertexBufferView*> vertexBuffers) const never_throws
    {
        #if !PGDROID
            auto featureSet = devContext.GetFeatureSet();
        #endif

        uint32_t instanceFlags = 0u;
        uint32_t instanceDataRate[32];

        unsigned attributeIterator = 0;
        for (unsigned b=0; b<unsigned(_bindingsByVertexBuffer.size()); ++b) {
            auto bindingCount = _bindingsByVertexBuffer[b];
            assert(b < vertexBuffers.size());
            if (!bindingCount) continue;
            const auto& vb = vertexBuffers[b];
            glBindBuffer(GL_ARRAY_BUFFER, GetBufferRawGLHandle(*vb._resource));
            for(auto ai=attributeIterator; ai<(attributeIterator+bindingCount); ++ai) {
                const auto& i = _bindings[ai];
                glVertexAttribPointer(
                    i._attributeLocation, i._size, i._type, i._isNormalized,
                    i._stride,
                    (const void*)(size_t)(vb._offset + i._offset));

                instanceFlags |= ((i._instanceDataRate!=0)<<i._attributeLocation);
                instanceDataRate[i._attributeLocation] = i._instanceDataRate;
            }
            attributeIterator += bindingCount;
        }

        auto* capture = devContext.GetCapturedStates();

        if (capture) {
            #if !PGDROID
            if (featureSet & FeatureSet::GLES300) {
                auto differences = (capture->_instancedVertexAttrib & _attributeState) | instanceFlags;
                if (differences) {
                    int firstActive = xl_ctz4(differences);
                    int lastActive = 32u - xl_clz4(differences);
                    for (int c=firstActive; c<lastActive; ++c)
                        if (_attributeState & (1<<c))
                            glVertexAttribDivisor(c, instanceDataRate[c]);
                    capture->_instancedVertexAttrib = (capture->_instancedVertexAttrib & ~_attributeState) | instanceFlags;
                }
            }
            #endif

            // set enable/disable flags --
            // Note that this method cannot support more than 32 vertex attributes

            auto differences = capture->_activeVertexAttrib ^ _attributeState;
            if (differences) {
                int firstActive = xl_ctz4(differences);
                int lastActive = 32u - xl_clz4(differences);

                for (int c=firstActive; c<lastActive; ++c)
                    if (_attributeState & (1<<c)) {
                        glEnableVertexAttribArray(c);
                    } else {
                        glDisableVertexAttribArray(c);
                    }

                capture->_activeVertexAttrib = _attributeState;
            }
        } else {
            #if !PGDROID
            if (featureSet & FeatureSet::GLES300) {
                for (int c=0; c<std::min(32u, _maxVertexAttributes); ++c)
                    if (_attributeState & (1<<c))
                        glVertexAttribDivisor(c, instanceDataRate[c]);
            }
            #endif
            for (int c=0; c<std::min(32u, _maxVertexAttributes); ++c)
                if (_attributeState & (1<<c)) {
                    glEnableVertexAttribArray(c);
                } else {
                    glDisableVertexAttribArray(c);
                }
        }

        CheckGLError("Apply BoundInputLayout");
    }

    static void BindVAO(DeviceContext& devContext, RawGLHandle vao)
    {
        auto* capture = devContext.GetCapturedStates();
        if (capture) {
            capture->VerifyIntegrity();
            if (capture->_boundVAO == vao) return;
            capture->_boundVAO = vao;
        }
        
        auto featureSet = devContext.GetFeatureSet();
        if (featureSet & FeatureSet::GLES300) {
            glBindVertexArray(vao);
        } else {
            #if GL_APPLE_vertex_array_object
                glBindVertexArrayAPPLE(vao);
            #else
                glBindVertexArrayOES(vao);
            #endif
        }
    }

    static uint64_t Hash(IteratorRange<const VertexBufferView*> vertexBuffers)
    {
        auto hash = DefaultSeed64;
        for (auto& vbv:vertexBuffers) {
            hash = HashCombine(hash, vbv._resource->GetGUID());
            if (vbv._offset)
                hash = HashCombine(hash, vbv._offset);
        }
        return hash;
    }

    void BoundInputLayout::Apply(DeviceContext& devContext, IteratorRange<const VertexBufferView*> vertexBuffers) const never_throws
    {
        if (_vao) {
            // The "vao" binds this input layout to a specific set of vertex buffers (passed to CreateVAO())
            // If you hit this assert, it means that the vertex buffers passed to CreateVAO don't match what's
            // passed to this function. That won't work; you need to either clone the BoundInputLayout for
            // each set of vertex buffers you want to use, or just don't call CreateVAO at all.
            assert(_vaoBindingHash == Hash(vertexBuffers));
            BindVAO(devContext, _vao->AsRawGLHandle());
        } else {
            BindVAO(devContext, 0);
            UnderlyingApply(devContext, vertexBuffers);
        }
    }

    void BoundInputLayout::CreateVAO(DeviceContext& devContext, IteratorRange<const VertexBufferView*> vertexBuffers)
    {
        _vao = nullptr;

        _vao = GetObjectFactory(devContext).CreateVAO();
        if (!_vao) return;

        auto* originalCapture = devContext.GetCapturedStates();
        if (originalCapture)
            devContext.EndStateCapture();

        BindVAO(devContext, _vao->AsRawGLHandle());
        UnderlyingApply(devContext, vertexBuffers);
        _vaoBindingHash = Hash(vertexBuffers);

        // Reset cached state in devContext
        // When a vao other than 0 is bound, it's unclear to me how calls to glEnableVertexAttribArray, etc,
        // affect VAO 0. Let's just play safe, and reset everything to default, and clear out the cached values
        // in DeviceContext
        BindVAO(devContext, 0);
        if (originalCapture)
            devContext.BeginStateCapture(*originalCapture);
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    void BoundUniforms::Apply(
        DeviceContext& context,
        unsigned streamIdx,
        const UniformsStream& stream) const
    {
        // this could be improved by ordering _cbs, _srvs by stream idx -- that way we wouldn't
        // have to spend so much time searching through for the right bindings
        for (const auto&cb:_cbs) {
            if (cb._stream != streamIdx) continue;
            assert(cb._slot < stream._constantBuffers.size());
            assert(!cb._commandGroup._commands.empty());

            const auto& cbv = stream._constantBuffers[cb._slot];
            const auto& pkt = cbv._packet;
            if (pkt.size() != 0) {
                Bind(context, cb._commandGroup, MakeIteratorRange(pkt.begin(), pkt.end()));
            } else {
                const auto pkt2 = ((Resource*)cbv._prebuiltBuffer)->GetConstantBuffer();
                if (pkt2.size() != 0)
                    Bind(context, cb._commandGroup, pkt2);
            }
        }

        auto* capture = context.GetCapturedStates();

        for (const auto&srv:_srvs) {
            if (srv._stream != streamIdx) continue;
            assert(srv._slot < stream._resources.size());
            const auto& res = *(ShaderResourceView*)stream._resources[srv._slot];
            const auto& sampler = *(SamplerState*)stream._samplers[srv._slot];

            if (res.GetResource()) {
                glActiveTexture(GL_TEXTURE0 + srv._textureUnit);
                glBindTexture(srv._dimensionality, res.GetUnderlying()->AsRawGLHandle());
                if (capture) {
                    sampler.Apply(*capture, srv._textureUnit, srv._dimensionality, res.GetResource().get(), res.HasMipMaps());
                } else {
                    sampler.Apply(srv._textureUnit, srv._dimensionality, res.HasMipMaps());
                }
            } else {
                #if 0 // defined(_DEBUG)
                    Log(Warning) << "Null resource while binding SRV to texture uniform (" << srv._name << ")" << std::endl;
                #endif
            }
        }

        // Commit changes to texture uniforms
        // This must be done separately to the texture binding, because when using array uniforms,
        // a single uniform set operation can be used for multiple texture bindings
        if (streamIdx == 0 && !_textureAssignmentCommands._commands.empty())
            Bind(context, _textureAssignmentCommands, MakeIteratorRange(_textureAssignmentByteData));

        CheckGLError("Apply BoundUniforms");
    }

    static GLenum DimensionalityForUniformType(GLenum uniformType)
    {
        switch (uniformType) {
        case GL_SAMPLER_2D:
        case GL_SAMPLER_2D_SHADOW:
        case GL_INT_SAMPLER_2D:
        case GL_UNSIGNED_INT_SAMPLER_2D:
            return GL_TEXTURE_2D;

        case GL_SAMPLER_CUBE:
            return GL_TEXTURE_CUBE_MAP;

        default:
            return GL_NONE;
        }
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

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

    static std::string AdaptNameForIndex(const std::string& name, unsigned elementIdx, unsigned uniformElementCount)
    {
        // If the input uniform name already has [], then modify the string to insert the particular
        // element idx we're actually interested in... 
        // Otherwise we will just append the indexor to the end of the string name
        if (name.size() >= 2 && name[name.size()-1] == ']') {
            auto i = name.begin() + (name.size()-2);
            while (i > name.begin() && *i >= '0' && *i <= '9') --i;
            if (*i == '[')
                return std::string(name.begin(), i+1) + std::to_string(elementIdx) + "]";
        }

        if (elementIdx != 0 || uniformElementCount > 1)
            return name + "[" + std::to_string(elementIdx) + "]";
        return name;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    BoundUniforms::BoundUniforms(
        const ShaderProgram& shader,
        const PipelineLayoutConfig& pipelineLayout,
        const UniformsStreamInterface& interface0,
        const UniformsStreamInterface& interface1,
        const UniformsStreamInterface& interface2,
        const UniformsStreamInterface& interface3)
    {
        for (auto&v:_boundUniformBufferSlots) v = 0u;
        for (auto&v:_boundResourceSlots) v = 0u;

        auto introspection = ShaderIntrospection(shader);

        unsigned textureUnitAccumulator = 0;

        struct UniformSet { int _location; unsigned _index, _value; GLenum _type; int _elementCount; };
        std::vector<UniformSet> uniformSets;

        const UniformsStreamInterface* inputInterface[] = { &interface0, &interface1, &interface2, &interface3 };
        auto streamCount = (unsigned)dimof(inputInterface);
        for (unsigned s=0; s<streamCount; ++s) {
            const auto& interf = *inputInterface[s];
            for (unsigned slot=0; slot<interf._cbBindings.size(); ++slot) {
                const auto& binding = interf._cbBindings[slot];
                if (binding._elements.empty()) continue;

                // ensure it's not shadowed by a future binding
                if (HasCBBinding(MakeIteratorRange(&inputInterface[s+1], &inputInterface[dimof(inputInterface)]), binding._hashName)) continue;

                auto cmdGroup = introspection.MakeBinding(binding._hashName, MakeIteratorRange(binding._elements));
                if (!cmdGroup._commands.empty()) {
                    _cbs.emplace_back(CB{s, slot, std::move(cmdGroup) 
                        #if defined(_DEBUG)
                            , cmdGroup._name
                        #endif
                        });
                    _boundUniformBufferSlots[s] |= (1ull << uint64_t(slot));
                }
            }

            for (unsigned slot=0; slot<interf._srvBindings.size(); ++slot) {
                auto binding = interf._srvBindings[slot];
                if (!binding) continue;

                // ensure it's not shadowed by a future binding
                if (HasSRVBinding(MakeIteratorRange(&inputInterface[s+1], &inputInterface[dimof(inputInterface)]), binding)) continue;

                auto uniform = introspection.FindUniform(binding);
                if (uniform._elementCount != 0) {
                    #if defined(_DEBUG)
                        introspection.MarkBound(binding);
                    #endif
                    // assign a texture unit for this binding
                    auto textureUnit = pipelineLayout.GetFixedTextureUnit(binding);
                    if (textureUnit == ~0u) {
                        textureUnit = pipelineLayout.GetFlexibleTextureUnit(textureUnitAccumulator);
                        textureUnitAccumulator++;
                    }
                    auto dim = DimensionalityForUniformType(uniform._type);
                    auto elementIndex = unsigned(binding - uniform._bindingName);
                    assert(dim != GL_NONE);
                    _srvs.emplace_back(SRV{s, slot, textureUnit, dim 
                        #if defined(_DEBUG)
                            , AdaptNameForIndex(uniform._name, elementIndex, uniform._elementCount)
                        #endif
                        });

                    // Record the command to set the uniform. Note that this
                    // is made a little more complicated due to array uniforms.
                    assert((binding - uniform._bindingName) < uniform._elementCount);
                    uniformSets.push_back({uniform._location, elementIndex, textureUnit, uniform._type, uniform._elementCount});

                    _boundResourceSlots[s] |= (1ull << uint64_t(slot));
                }
            }
        }
        #if defined(_DEBUG)
            _unboundUniforms = introspection.UnboundUniforms();
        #endif

        // sort the uniform sets to collect up sequential sets on the same uniforms
        std::sort(
            uniformSets.begin(), uniformSets.end(),
            [](const UniformSet& lhs, const UniformSet& rhs) {
                if (lhs._location < rhs._location) return true;
                if (lhs._location > rhs._location) return false;
                return lhs._index < rhs._index;
            });

        if (!uniformSets.empty()) {
            for (auto i=uniformSets.begin(); (i+1)!=uniformSets.end(); ++i) {
                assert(i->_location != (i+1)->_location || i->_index != (i+1)->_index);
            }
        }

        // Now generate the set commands that will assign the uniforms as required
        for (auto i = uniformSets.begin(); i!=uniformSets.end();) {
            auto i2 = i+1;
            while (i2 != uniformSets.end() && i2->_location == i->_location) { ++i2; };

            // unsigned maxIndex = (i2-1)->_index;
            unsigned elementCount = i->_elementCount;
            auto dataOffsetStart = _textureAssignmentByteData.size();
            _textureAssignmentByteData.resize(dataOffsetStart+sizeof(GLuint)*(elementCount+1), 0u);
            GLuint* dst = (GLuint*)&_textureAssignmentByteData[dataOffsetStart];
            for (auto q=i; q<i2; ++q) {
                assert(q->_index < elementCount);
                dst[q->_index] = q->_value;
            }
            _textureAssignmentCommands._commands.push_back(
                SetUniformCommandGroup::SetCommand{i->_location, i->_type, elementCount, dataOffsetStart});

            i = i2;
        }

        CheckGLError("Construct BoundUniforms");
    }

    BoundUniforms::~BoundUniforms() {}

    BoundUniforms::BoundUniforms()
    {
        for (unsigned c=0; c<dimof(_boundUniformBufferSlots); ++c) {
            _boundUniformBufferSlots[c] = 0ull;
            _boundResourceSlots[c] = 0ull;
        }
    }

    BoundUniforms::BoundUniforms(BoundUniforms&& moveFrom) never_throws
    : _cbs(std::move(moveFrom._cbs))
    , _srvs(std::move(moveFrom._srvs))
    , _textureAssignmentCommands(std::move(moveFrom._textureAssignmentCommands))
    , _textureAssignmentByteData(std::move(moveFrom._textureAssignmentByteData))
    #if defined(_DEBUG)
    , _unboundUniforms(std::move(moveFrom._unboundUniforms))
    #endif
    {
        for (unsigned c=0; c<dimof(_boundUniformBufferSlots); ++c) {
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
        _textureAssignmentCommands = std::move(moveFrom._textureAssignmentCommands);
        _textureAssignmentByteData = std::move(moveFrom._textureAssignmentByteData);
        for (unsigned c=0; c<dimof(_boundUniformBufferSlots); ++c) {
            _boundUniformBufferSlots[c] = moveFrom._boundUniformBufferSlots[c];
            _boundResourceSlots[c] = moveFrom._boundResourceSlots[c];
            moveFrom._boundUniformBufferSlots[c] = 0ull;
            moveFrom._boundResourceSlots[c] = 0ull;
        }
        #if defined(_DEBUG)
            _unboundUniforms = std::move(moveFrom._unboundUniforms);
        #endif
        return *this;
    }

}}

