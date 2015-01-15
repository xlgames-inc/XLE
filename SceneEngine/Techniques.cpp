// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Techniques.h"
#include "LightDesc.h"      // for MaxShadowTexturesPerLight
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Assets/AssetUtils.h"    // for Assets::LocalTransform_Elements
#include "../Math/Vector.h"
#include "../Math/Matrix.h"
#include "../Utility/Streams/Data.h"
#include "../Utility/Streams/FileUtils.h"
#include <algorithm>

namespace SceneEngine
{
        ///////////////////////   T E C H N I Q U E   I N T E R F A C E   ///////////////////////////

    class TechniqueInterface::Pimpl
    {
    public:
        std::vector<RenderCore::Metal::InputElementDesc>    _vertexInputLayout;
        std::vector<std::pair<uint64, unsigned>>            _constantBuffers;
        std::vector<std::pair<uint64, unsigned>>            _shaderResources;

        uint64 _hashValue;
        Pimpl() : _hashValue(0) {}

        void        UpdateHashValue();
    };

    void        TechniqueInterface::Pimpl::UpdateHashValue()
    {
        _hashValue = 
              Hash64(AsPointer(_constantBuffers.cbegin()), AsPointer(_constantBuffers.cend()))
            ^ Hash64(AsPointer(_shaderResources.cbegin()), AsPointer(_shaderResources.cend()))
            ;

        unsigned index = 0;
        for (auto i=_vertexInputLayout.cbegin(); i!=_vertexInputLayout.cend(); ++i, ++index) {
            struct PartialDesc
            {
                unsigned                    _semanticIndex;
                RenderCore::Metal::NativeFormat::Enum          _nativeFormat;
                unsigned                    _inputSlot;
                unsigned                    _alignedByteOffset;
                RenderCore::Metal::InputClassification::Enum   _inputSlotClass;
                unsigned                    _instanceDataStepRate;
            } partialDesc;

            partialDesc._semanticIndex          = i->_semanticIndex;
            partialDesc._nativeFormat           = i->_nativeFormat;
            partialDesc._inputSlot              = i->_inputSlot;
            partialDesc._alignedByteOffset      = i->_alignedByteOffset;        // could get a better hash if we convert this to a true offset, here (if it's the default offset marker)
            partialDesc._inputSlotClass         = i->_inputSlotClass;
            partialDesc._instanceDataStepRate   = i->_instanceDataStepRate;

            auto elementHash = 
                Hash64(&partialDesc, PtrAdd(&partialDesc, sizeof(partialDesc)))
                ^ Hash64(i->_semanticName);
            _hashValue ^= (elementHash<<(2*index));     // index is a slight problem here. two different interfaces with the same elements, but in reversed order would come to the same hash. We can shift it slightly to true to avoid this
        }
    }

    void    TechniqueInterface::BindConstantBuffer(uint64 hashName, unsigned slot, unsigned stream)
    {
        auto i = std::lower_bound(_pimpl->_constantBuffers.begin(), _pimpl->_constantBuffers.end(),
            slot, CompareFirst<uint64, unsigned>());
        if (i == _pimpl->_constantBuffers.end() || i->first != hashName) {
            _pimpl->_constantBuffers.insert(i, std::make_pair(hashName, slot | (stream<<16)));
            _pimpl->UpdateHashValue();
            return;
        }
        assert(0);  // attempting to bind the same constant buffer twice
    }

    void    TechniqueInterface::BindShaderResource(uint64 hashName, unsigned slot, unsigned stream)
    {
        auto i = std::lower_bound(_pimpl->_shaderResources.begin(), _pimpl->_shaderResources.end(),
            slot, CompareFirst<uint64, unsigned>());
        if (i == _pimpl->_shaderResources.end() || i->first != hashName) {
            _pimpl->_shaderResources.insert(i, std::make_pair(hashName, slot | (stream<<16)));
            _pimpl->UpdateHashValue();
            return;
        }
        assert(0);  // attempting to bind the same constant buffer twice
    }

    uint64  TechniqueInterface::GetHashValue() const
    {
        return _pimpl->_hashValue;
    }

    TechniqueInterface::TechniqueInterface() 
    {
        _pimpl = std::make_unique<TechniqueInterface::Pimpl>();
    }

    TechniqueInterface::TechniqueInterface(const RenderCore::Metal::InputLayout& vertexInputLayout)
    {
        _pimpl = std::make_unique<TechniqueInterface::Pimpl>();
        _pimpl->_vertexInputLayout.insert(
            _pimpl->_vertexInputLayout.begin(),
            vertexInputLayout.first, &vertexInputLayout.first[vertexInputLayout.second]);
        _pimpl->UpdateHashValue();
    }

    TechniqueInterface::~TechniqueInterface()
    {}

    TechniqueInterface::TechniqueInterface(TechniqueInterface&& moveFrom)
    {
        _pimpl = std::move(moveFrom._pimpl);
    }

    TechniqueInterface&TechniqueInterface::operator=(TechniqueInterface&& moveFrom)
    {
        _pimpl = std::move(moveFrom._pimpl);
        return *this;
    }

        ///////////////////////   T E C H N I Q U E   I N T E R F A C E   ///////////////////////////

    #if defined(CHECK_TECHNIQUE_HASH_CONFLICTS)
        Technique::HashConflictTest::HashConflictTest(const ParameterBox* globalState[ShaderParameters::Source::Max], uint64 rawHash, uint64 filteredHash, uint64 interfaceHash)
        {
            _rawHash = rawHash; _filteredHash = filteredHash; _interfaceHash = interfaceHash;
            for (unsigned c=0; c<ShaderParameters::Source::Max; ++c) {
                _globalState[c] = *globalState[c];
            }
        }

        Technique::HashConflictTest::HashConflictTest() {}

        void Technique::TestHashConflict(const ParameterBox* globalState[ShaderParameters::Source::Max], const HashConflictTest& comparison)
        {
                // check to make sure the parameter names in both of these boxes is the same
                // note -- this isn't exactly correctly. we need to filter out parameters that are not relevant to this technique
            for (unsigned c=0; c<ShaderParameters::Source::Max; ++c) {
                assert(globalState[c]->ParameterNamesAreEqual(comparison._globalState[c]));
            }
        }
    #endif

    ResolvedShader      Technique::FindVariation(   const ParameterBox* globalState[ShaderParameters::Source::Max],
                                                    const TechniqueInterface& techniqueInterface)
    {
            //
            //      todo --     It would be cool if the caller passed in some kind of binding desc
            //                  object... That would affect the binding part of the resolved shader
            //                  (vertex layout, shader constants and resources), but not the shader
            //                  itself. This could allow us to have different bindings without worrying
            //                  about invoking redundant shader compiles.
            //
        uint64 inputHash = 0;
        for (unsigned c=0; c<ShaderParameters::Source::Max; ++c) {
            inputHash ^= globalState[c]->GetParameterNamesHash();
            inputHash ^= globalState[c]->GetHash() << (c*6);    // we have to be careful of cases where the values in one box is very similar to the values in another
        }
        
        uint64 globalHashWithInterface = inputHash ^ techniqueInterface.GetHashValue();
        auto i = std::lower_bound(_globalToResolved.begin(), _globalToResolved.end(), globalHashWithInterface, CompareFirst<uint64, ResolvedShader>());
        if (i!=_globalToResolved.cend() && i->first == globalHashWithInterface) {
            if (i->second._shaderProgram && (i->second._shaderProgram->GetDependancyValidation().GetValidationIndex()!=0)) {
                ResolveAndBind(i->second, globalState, techniqueInterface);
            }

            #if defined(CHECK_TECHNIQUE_HASH_CONFLICTS)
                auto ti = std::lower_bound(_globalToResolvedTest.begin(), _globalToResolvedTest.end(), globalHashWithInterface, CompareFirst<uint64, HashConflictTest>());
                assert(ti!=_globalToResolvedTest.cend() && ti->first == globalHashWithInterface);
                TestHashConflict(globalState, ti->second);
            #endif
            return i->second;
        }

        uint64 filteredHashValue = _baseParameters.CalculateFilteredHash(inputHash, globalState);
        uint64 filteredHashWithInterface = filteredHashValue ^ techniqueInterface.GetHashValue();
        auto i2 = std::lower_bound(_filteredToResolved.begin(), _filteredToResolved.end(), filteredHashWithInterface, CompareFirst<uint64, ResolvedShader>());
        if (i2!=_filteredToResolved.cend() && i2->first == filteredHashWithInterface) {
            _globalToResolved.insert(i, std::make_pair(globalHashWithInterface, i2->second));
            if (i2->second._shaderProgram && (i2->second._shaderProgram->GetDependancyValidation().GetValidationIndex()!=0)) {
                ResolveAndBind(i2->second, globalState, techniqueInterface);
            }

            #if defined(CHECK_TECHNIQUE_HASH_CONFLICTS)
                auto gti = std::lower_bound(_globalToResolvedTest.begin(), _globalToResolvedTest.end(), globalHashWithInterface, CompareFirst<uint64, HashConflictTest>());
                _globalToResolvedTest.insert(gti, std::make_pair(globalHashWithInterface, HashConflictTest(globalState, inputHash, filteredHashValue, techniqueInterface.GetHashValue())));

                auto lti = std::lower_bound(_localToResolvedTest.begin(), _localToResolvedTest.end(), filteredHashWithInterface, CompareFirst<uint64, HashConflictTest>());
                assert(lti!=_localToResolvedTest.cend() && lti->first == filteredHashWithInterface);
                TestHashConflict(globalState, lti->second);
            #endif
            return i2->second;
        }

        ResolvedShader newResolvedShader;
        newResolvedShader._variationHash = filteredHashValue;
        ResolveAndBind(newResolvedShader, globalState, techniqueInterface);
        _filteredToResolved.insert(i2, std::make_pair(filteredHashWithInterface, newResolvedShader));
        _globalToResolved.insert(i, std::make_pair(globalHashWithInterface, newResolvedShader));

        #if defined(CHECK_TECHNIQUE_HASH_CONFLICTS)
            auto gti = std::lower_bound(_globalToResolvedTest.begin(), _globalToResolvedTest.end(), globalHashWithInterface, CompareFirst<uint64, HashConflictTest>());
            _globalToResolvedTest.insert(gti, std::make_pair(globalHashWithInterface, HashConflictTest(globalState, inputHash, filteredHashValue, techniqueInterface.GetHashValue())));

            auto lti = std::lower_bound(_localToResolvedTest.begin(), _localToResolvedTest.end(), filteredHashWithInterface, CompareFirst<uint64, HashConflictTest>());
            _localToResolvedTest.insert(lti, std::make_pair(filteredHashWithInterface, HashConflictTest(globalState, inputHash, filteredHashValue, techniqueInterface.GetHashValue())));
        #endif
        return newResolvedShader;
    }
    
    void        Technique::ResolveAndBind(  ResolvedShader& resolvedShader, 
                                            const ParameterBox* globalState[ShaderParameters::Source::Max],
                                            const TechniqueInterface& techniqueInterface)
    {
        std::vector<std::pair<std::string, std::string>> defines;
        _baseParameters.BuildStringTable(defines);
        for (unsigned c=0; c<ShaderParameters::Source::Max; ++c) {
            globalState[c]->OverrideStringTable(defines);
        }

        std::string vsShaderModel, psShaderModel, gsShaderModel;
        auto vsi = std::lower_bound(defines.cbegin(), defines.cend(), "vs_", CompareFirst<std::string, std::string>());
        if (vsi != defines.cend() && vsi->first == "vs_") {
            char buffer[32];
            int integerValue = Utility::XlAtoI32(vsi->second.c_str());
            sprintf_s(buffer, dimof(buffer), ":vs_%i_%i", integerValue/10, integerValue%10);
            vsShaderModel = buffer;
        } else {
            vsShaderModel = ":" VS_DefShaderModel;
        }
        auto psi = std::lower_bound(defines.cbegin(), defines.cend(), "ps_", CompareFirst<std::string, std::string>());
        if (psi != defines.cend() && psi->first == "ps_") {
            char buffer[32];
            int integerValue = Utility::XlAtoI32(psi->second.c_str());
            sprintf_s(buffer, dimof(buffer), ":ps_%i_%i", integerValue/10, integerValue%10);
            psShaderModel = buffer;
        } else {
            psShaderModel = ":" PS_DefShaderModel;
        }
        auto gsi = std::lower_bound(defines.cbegin(), defines.cend(), "gs_", CompareFirst<std::string, std::string>());
        if (gsi != defines.cend() && gsi->first == "gs_") {
            char buffer[32];
            int integerValue = Utility::XlAtoI32(psi->second.c_str());
            sprintf_s(buffer, dimof(buffer), ":gs_%i_%i", integerValue/10, integerValue%10);
            gsShaderModel = buffer;
        } else {
            gsShaderModel = ":" GS_DefShaderModel;
        }

        std::string combinedStrings;
        size_t size = 0;
        std::for_each(defines.cbegin(), defines.cend(), 
            [&size](const std::pair<std::string, std::string>& object) { size += 2 + object.first.size() + object.second.size(); });
        combinedStrings.reserve(size);
        std::for_each(defines.cbegin(), defines.cend(), 
            [&combinedStrings](const std::pair<std::string, std::string>& object) 
            {
                combinedStrings.insert(combinedStrings.end(), object.first.cbegin(), object.first.cend()); 
                combinedStrings.push_back('=');
                combinedStrings.insert(combinedStrings.end(), object.second.cbegin(), object.second.cend()); 
                combinedStrings.push_back(';');
            });

        using namespace RenderCore;
        using namespace RenderCore::Metal;
    
        std::unique_ptr<ShaderProgram> shaderProgram;
        std::unique_ptr<BoundUniforms> boundUniforms;
        std::unique_ptr<BoundInputLayout> boundInputLayout;
        std::unique_ptr<ConstantBufferLayout> boundMaterialConstants;

        if (_geometryShaderName.empty()) {
            shaderProgram = std::make_unique<ShaderProgram>(
                (_vertexShaderName + vsShaderModel).c_str(), 
                (_pixelShaderName + psShaderModel).c_str(), 
                combinedStrings.c_str());
        } else {
            shaderProgram = std::make_unique<ShaderProgram>(
                (_vertexShaderName + vsShaderModel).c_str(), 
                (_geometryShaderName + gsShaderModel).c_str(), 
                (_pixelShaderName + psShaderModel).c_str(), 
                combinedStrings.c_str());
        }

        boundUniforms = std::make_unique<BoundUniforms>(std::ref(*shaderProgram));
        for (auto i = techniqueInterface._pimpl->_constantBuffers.cbegin();
            i != techniqueInterface._pimpl->_constantBuffers.cend(); ++i) {
            boundUniforms->BindConstantBuffer(i->first, i->second & 0xff, i->second >> 16, nullptr, 0);
        }

        for (auto i = techniqueInterface._pimpl->_shaderResources.cbegin();
            i != techniqueInterface._pimpl->_shaderResources.cend(); ++i) {
            boundUniforms->BindShaderResource(i->first, i->second & 0xff, i->second >> 16);
        }

        boundInputLayout = std::make_unique<BoundInputLayout>(
            std::make_pair(AsPointer(techniqueInterface._pimpl->_vertexInputLayout.cbegin()), techniqueInterface._pimpl->_vertexInputLayout.size()),
            std::ref(*shaderProgram));

            // resolving the "materialConstants" buffer is useful for CryRenderXLE right now
        boundMaterialConstants = std::unique_ptr<ConstantBufferLayout>(
            new ConstantBufferLayout(boundUniforms->GetConstantBufferLayout("MaterialConstants")));

        resolvedShader._shaderProgram = shaderProgram.get();
        resolvedShader._boundUniforms = boundUniforms.get();
        resolvedShader._boundLayout = boundInputLayout.get();
        resolvedShader._materialConstantsLayout = boundMaterialConstants.get();
        _resolvedShaderPrograms.push_back(std::move(shaderProgram));
        _resolvedBoundUniforms.push_back(std::move(boundUniforms));
        _resolvedBoundInputLayouts.push_back(std::move(boundInputLayout));
        _resolvedMaterialConstantsLayouts.push_back(std::move(boundMaterialConstants));
    }

    Technique::Technique(Data& source)
    {
            //
            //      There are some parameters that will we always have an effect on the
            //      binding. We need to make sure these are initialized with sensible
            //      values.
            //
        auto& globalParam = _baseParameters._parameters[ShaderParameters::Source::GlobalEnvironment];
        globalParam.SetParameter("vs_", 50);
        globalParam.SetParameter("ps_", 50);

        _name = source.StrValue();
        auto* p = source.ChildWithValue("Parameters");
        if (p) {
            const char* parameterBoxNames[] = { "Geometry", "GlobalEnvironment", "Runtime", "Material" };
            for (unsigned q=0; q<dimof(parameterBoxNames); ++q) {
                auto* d = p->ChildWithValue(parameterBoxNames[q]);
                auto& destinationParameters = _baseParameters._parameters[q];
                if (d) {
                    for (int c=0; c<d->Size(); ++c) {
                        auto child = d->ChildAt(c);
                        if (child) {
                            destinationParameters.SetParameter(
                                child->StrValue(),
                                child->ChildAt(0)?child->ChildAt(0)->IntValue():0);
                        }
                    }
                }
            }
        }

        _vertexShaderName = source.StrAttribute("VertexShader");
        _pixelShaderName = source.StrAttribute("PixelShader");
        _geometryShaderName = source.StrAttribute("GeometryShader");
    }

    Technique::Technique(Technique&& moveFrom)
    :       _name(moveFrom._name)
    ,       _baseParameters(std::move(moveFrom._baseParameters))
    ,       _filteredToResolved(std::move(moveFrom._filteredToResolved))
    ,       _globalToResolved(std::move(moveFrom._globalToResolved))
    ,       _vertexShaderName(moveFrom._vertexShaderName)
    ,       _pixelShaderName(moveFrom._pixelShaderName)
    ,       _geometryShaderName(moveFrom._geometryShaderName)
    {}

    Technique& Technique::operator=(Technique&& moveFrom)
    {
        _name = moveFrom._name;
        _baseParameters = std::move(moveFrom._baseParameters);
        _filteredToResolved = std::move(moveFrom._filteredToResolved);
        _globalToResolved = std::move(moveFrom._globalToResolved);
        _vertexShaderName = moveFrom._vertexShaderName;
        _pixelShaderName = moveFrom._pixelShaderName;
        _geometryShaderName = moveFrom._geometryShaderName;
        return *this;
    }




    ///////////////////////   S H A D E R   T Y P E   ///////////////////////////

    ResolvedShader      ShaderType::FindVariation(  int techniqueIndex, 
                                                    const ParameterBox* globalState[ShaderParameters::Source::Max],
                                                    const TechniqueInterface& techniqueInterface)
    {
        if (techniqueIndex >= int(_technique.size()) || !_technique[techniqueIndex].IsValid()) {
            return ResolvedShader();
        }
        return _technique[techniqueIndex].FindVariation(globalState, techniqueInterface);
    }

    static std::string LoadSourceFile(const std::string& sourceFileName)
    {
        TRY {
            Utility::BasicFile file(sourceFileName.c_str(), "rb");

            file.Seek(0, SEEK_END);
            size_t size = file.TellP();
            file.Seek(0, SEEK_SET);

            std::string result;
            result.resize(size, '\0');
            file.Read(&result.at(0), 1, size);
            return result;
        } CATCH(const std::exception& ) {
            return std::string();
        } CATCH_END
    }

    ShaderType::ShaderType(const char resourceName[])
    {
        Data data;
        auto sourceFile = LoadSourceFile(resourceName);
        if (!sourceFile.empty()) {
            data.Load(&(*sourceFile.begin()), (int)sourceFile.size());
            for (int c=0; c<data.Size(); ++c) {
                auto child = data.ChildAt(c);
                if (child) {
                    _technique.push_back(Technique(*child));
                }
            }
        }

        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        ::Assets::RegisterFileDependency(_validationCallback, resourceName);
    }

    ShaderType::~ShaderType()
    {}

        //////////////////////-------//////////////////////

    ParameterBox::ParameterNameHash ParameterBox::MakeParameterNameHash(const std::string& name)
    {
        return Hash32(AsPointer(name.cbegin()), AsPointer(name.cend()));
    }

    void        ParameterBox::SetParameter(const std::string& name, uint32 value)
    {
        auto hash = MakeParameterNameHash(name);
        auto i = std::lower_bound(_parameterHashValues.cbegin(), _parameterHashValues.cend(), hash);
        if (i==_parameterHashValues.cend()) {
            _parameterHashValues.push_back(hash);
            auto offset = _values.size();
            _parameterOffsets.push_back((ParameterNameHash)offset);
            _parameterNames.push_back(name);
            _values.resize(offset+sizeof(uint32), 0);
            *(uint32*)&_values[offset] = value;
            _cachedHash = 0;
            _cachedParameterNameHash = 0;
            return;
        }

        size_t index = std::distance(_parameterHashValues.cbegin(), i);
        if (*i!=hash) {
            _parameterHashValues.insert(i, hash);
            _parameterNames.insert(_parameterNames.begin()+index, name);
            size_t offset = _parameterOffsets[index];
            _parameterOffsets.insert(_parameterOffsets.begin()+index, uint32(offset));
            for (auto i2=_parameterOffsets.begin()+index+1; i2<_parameterOffsets.end(); ++i2) {
                (*i2) += sizeof(uint32);
            }
            _values.insert(_values.cbegin()+offset, (byte*)&value, (byte*)PtrAdd(&value, sizeof(uint32)));
            *(uint32*)&_values[offset] = value;
            _cachedHash = 0;
            _cachedParameterNameHash = 0;
            return;
        }

        assert(_parameterNames[index] == name);
        auto offset = _parameterOffsets[index];
        *(uint32*)&_values[offset] = value;
        _cachedHash = 0;
    }

    uint32      ParameterBox::GetParameter(const std::string& name) const
    {
        auto hash = MakeParameterNameHash(name);
        auto i = std::lower_bound(_parameterHashValues.cbegin(), _parameterHashValues.cend(), hash);
        if (i!=_parameterHashValues.cend() && *i == hash) {
            size_t index = std::distance(_parameterHashValues.cbegin(), i);
            auto offset = _parameterOffsets[index];
            return *(uint32*)&_values[offset];
        }
        return 0;
    }

    uint32      ParameterBox::GetParameter(ParameterNameHash name) const
    {
        auto i = std::lower_bound(_parameterHashValues.cbegin(), _parameterHashValues.cend(), name);
        if (i!=_parameterHashValues.cend() && *i == name) {
            size_t index = std::distance(_parameterHashValues.cbegin(), i);
            auto offset = _parameterOffsets[index];
            return *(uint32*)&_values[offset];
        }
        return 0;
    }

    uint64      ParameterBox::CalculateParameterNamesHash() const
    {
            //  Note that the parameter names are always in the same order (unless 
            //  two names resolve to the same 32 bit hash value). So, even though
            //  though the xor operation here doesn't depend on order, it should be
            //  ok -- because if the same parameter names appear in two different
            //  parameter boxes, they should have the same order.
        uint64 result = 0x7EF5E3B02A75ED13ui64;
        for (auto i=_parameterNames.cbegin(); i!=_parameterNames.cend(); ++i) {
            result ^= Hash64(AsPointer(i->cbegin()), AsPointer(i->cend()));
        }
        return result;
    }

    uint32      ParameterBox::GetValue(size_t index)
    {
        if (index < _parameterOffsets.size()) {
            return 0;
        }
        auto offset = _parameterOffsets[index];
        return *(uint32*)&_values[offset];
    }

    uint64      ParameterBox::CalculateHash() const
    {
        return Hash64(AsPointer(_values.cbegin()), AsPointer(_values.cend()));
    }

    uint64      ParameterBox::GetHash() const
    {
        if (!_cachedHash) {
            _cachedHash = CalculateHash();
        }
        return _cachedHash;
    }

    uint64      ParameterBox::GetParameterNamesHash() const
    {
        if (!_cachedParameterNameHash) {
            _cachedParameterNameHash = CalculateParameterNamesHash();
        }
        return _cachedParameterNameHash;
    }

    uint64      ParameterBox::TranslateHash(const ParameterBox& source) const
    {
        if (_values.size() > 1024) {
            assert(0);
            return 0;
        }

        uint8 temporaryValues[1024];
        std::copy(_values.cbegin(), _values.cend(), temporaryValues);
        auto i  = _parameterHashValues.cbegin();
        auto i2 = source._parameterHashValues.cbegin();
        while (i < _parameterHashValues.cend() && i2 < source._parameterHashValues.cend()) {
            if (*i < *i2) {
                ++i;
            } else if (*i > *i2) {
                ++i2;
            } else if (*i == *i2) {
                size_t offsetDest   = _parameterOffsets[std::distance(_parameterHashValues.cbegin(), i)];
                size_t offsetSrc    = source._parameterOffsets[std::distance(source._parameterHashValues.cbegin(), i2)];
                *(uint32*)PtrAdd(temporaryValues, offsetDest) = *(uint32*)PtrAdd(AsPointer(source._values.cbegin()), offsetSrc);
                ++i;
                ++i2;
            }
        }

        return Hash64(temporaryValues, PtrAdd(temporaryValues, _values.size()));
    }

    static std::string AsString(uint32 value)
    {
        char buffer[32];
        Utility::XlI32toA_s(value, buffer, dimof(buffer), 10);
        return buffer;
    }

    void        ParameterBox::BuildStringTable(std::vector<std::pair<std::string, std::string>>& defines) const
    {
        for (auto i=_parameterNames.cbegin(); i!=_parameterNames.cend(); ++i) {
            auto insertPosition     = std::lower_bound(defines.begin(), defines.end(), *i, CompareFirst<std::string, std::string>());
            auto offset             = _parameterOffsets[std::distance(_parameterNames.cbegin(), i)];
            auto value              = *(uint32*)&_values[offset];
            if (insertPosition!=defines.cend() && insertPosition->first == *i) {
                insertPosition->second = AsString(value);
            } else {
                defines.insert(insertPosition, std::make_pair(*i, AsString(value)));
            }
        }
    }

    void        ParameterBox::OverrideStringTable(std::vector<std::pair<std::string, std::string>>& defines) const
    {
        for (auto i=_parameterNames.cbegin(); i!=_parameterNames.cend(); ++i) {
            auto insertPosition     = std::lower_bound(defines.begin(), defines.end(), *i, CompareFirst<std::string, std::string>());
            auto offset             = _parameterOffsets[std::distance(_parameterNames.cbegin(), i)];
            auto value              = *(uint32*)&_values[offset];
            if (insertPosition!=defines.cend() && insertPosition->first == *i) {
                insertPosition->second = AsString(value);
            }
        }
    }

    bool        ParameterBox::ParameterNamesAreEqual(const ParameterBox& other) const
    {
            // return true iff both boxes have exactly the same parameter names, in the same order
        if (_parameterNames.size() != other._parameterNames.size()) {
            return false;
        }
        for (unsigned c=0; c<_parameterNames.size(); ++c) {
            if (_parameterNames[c] != other._parameterNames[c]) {
                return false;
            }
        }
        return true;
    }

    ParameterBox::ParameterBox()
    {
        _cachedHash = _cachedParameterNameHash = 0;
    }

    ParameterBox::~ParameterBox()
    {
    }

        //////////////////////-------//////////////////////

    uint64      ShaderParameters::CalculateFilteredState(const ParameterBox* globalState[Source::Max])
    {
        uint64 filteredState = 0;
        for (unsigned c=0; c<Source::Max; ++c) {
              filteredState ^= _parameters[c].TranslateHash(*globalState[c]) << (c*6);     // we have to be careful of cases where 2 boxes have their filtered tables sort of swapped... Those cases should produce distinctive hashes
        }
        return filteredState;
    }

    uint64      ShaderParameters::CalculateFilteredHash(uint64 inputHash, const ParameterBox* globalState[Source::Max])
    {
            //      Find a local state to match
        auto i = std::lower_bound(_globalToFilteredTable.cbegin(), _globalToFilteredTable.cend(), inputHash, CompareFirst<uint64, uint64>());
        if (i!=_globalToFilteredTable.cend() && i->first == inputHash) {
            return i->second;
        }

        uint64 filteredState = CalculateFilteredState(globalState);
        _globalToFilteredTable.insert(i, std::make_pair(inputHash, filteredState));
        return filteredState;
    }

    void        ShaderParameters::BuildStringTable(std::vector<std::pair<std::string, std::string>>& defines) const
    {
        for (unsigned c=0; c<dimof(_parameters); ++c) {
            _parameters[c].BuildStringTable(defines);
        }
    }

    ResolvedShader::ResolvedShader()
    {
        _variationHash = 0;
        _shaderProgram = nullptr;
        _boundUniforms = nullptr;
        _boundLayout = nullptr;
    }

        //////////////////////-------//////////////////////

    
    void     MergeParameterBoxes(ParameterBox& lhs, ParameterBox& rhs)
    {
        size_t i=0;

        for (;;) {
            if (i>=lhs._parameterHashValues.size()) {
                if (i>=rhs._parameterHashValues.size()) {
                    break;
                }
                lhs.SetParameter(rhs._parameterNames[i], rhs.GetValue(i));
            } else if (i>=rhs._parameterHashValues.size()) {
                rhs.SetParameter(lhs._parameterNames[i], lhs.GetValue(i));
            } else if (lhs._parameterHashValues[i] < rhs._parameterHashValues[i]) {
                rhs.SetParameter(lhs._parameterNames[i], lhs.GetValue(i));
            } else if (rhs._parameterHashValues[i] < lhs._parameterHashValues[i]) {
                lhs.SetParameter(rhs._parameterNames[i], rhs.GetValue(i));
            } else {
                ++i;
            }
        }
    }

    void     TechniqueContext::BindGlobalUniforms(TechniqueInterface& binding)
    {
            //  We need to specify the order of resources as they appear in 
            //  _globalUniformsStream
        static auto HashGlobalTransform     = Hash64("GlobalTransform");
        static auto HashGlobalState         = Hash64("GlobalState");
        static auto HashFogSettings         = Hash64("FogSettings");
        static auto HashShadowProjection    = Hash64("ShadowProjection");
        binding.BindConstantBuffer(HashGlobalTransform, 0, 0);
        binding.BindConstantBuffer(HashGlobalState, 1, 0);
        binding.BindConstantBuffer(HashFogSettings, 2, 0);
        binding.BindConstantBuffer(HashShadowProjection, 3, 0);
    }

    void     TechniqueContext::BindGlobalUniforms(RenderCore::Metal::BoundUniforms& binding)
    {
            //  We need to specify the order of resources as they appear in 
            //  _globalUniformsStream
        static auto HashGlobalTransform     = Hash64("GlobalTransform");
        static auto HashGlobalState         = Hash64("GlobalState");
        static auto HashFogSettings         = Hash64("FogSettings");
        static auto HashShadowProjection    = Hash64("ShadowProjection");
        binding.BindConstantBuffer(HashGlobalTransform, 0, 0);
        binding.BindConstantBuffer(HashGlobalState, 1, 0);
        binding.BindConstantBuffer(HashFogSettings, 2, 0);
        binding.BindConstantBuffer(HashShadowProjection, 3, 0);
    }

}

