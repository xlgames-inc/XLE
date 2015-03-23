// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Techniques.h"
#include "../Metal/Shader.h"
#include "../Metal/InputLayout.h"
#include "../../Assets/AssetUtils.h"
#include "../../Math/Vector.h"
#include "../../Math/Matrix.h"
#include "../../Utility/Streams/Data.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/IteratorUtils.h"
#include <algorithm>

namespace RenderCore { namespace Techniques
{
        ///////////////////////   T E C H N I Q U E   I N T E R F A C E   ///////////////////////////

    class TechniqueInterface::Pimpl
    {
    public:
        std::vector<Metal::InputElementDesc>        _vertexInputLayout;
        std::vector<std::pair<uint64, unsigned>>    _constantBuffers;
        std::vector<std::pair<uint64, unsigned>>    _shaderResources;

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
                Metal::NativeFormat::Enum          _nativeFormat;
                unsigned                    _inputSlot;
                unsigned                    _alignedByteOffset;
                Metal::InputClassification::Enum   _inputSlotClass;
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

    TechniqueInterface::TechniqueInterface(const Metal::InputLayout& vertexInputLayout)
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
                                                    const TechniqueInterface& techniqueInterface) const
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
            if (i->second._shaderProgram && (i->second._shaderProgram->GetDependencyValidation().GetValidationIndex()!=0)) {
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
            if (i2->second._shaderProgram && (i2->second._shaderProgram->GetDependencyValidation().GetValidationIndex()!=0)) {
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
                                            const TechniqueInterface& techniqueInterface) const
    {
        std::vector<std::pair<const char*, std::string>> defines;
        _baseParameters.BuildStringTable(defines);
        for (unsigned c=0; c<ShaderParameters::Source::Max; ++c) {
            globalState[c]->OverrideStringTable(defines);
        }

        std::string vsShaderModel, psShaderModel, gsShaderModel;
        auto vsi = std::lower_bound(defines.cbegin(), defines.cend(), "vs_", CompareFirst<std::string, std::string>());
        if (vsi != defines.cend() && !XlCompareString(vsi->first, "vs_")) {
            char buffer[32];
            int integerValue = Utility::XlAtoI32(vsi->second.c_str());
            sprintf_s(buffer, dimof(buffer), ":vs_%i_%i", integerValue/10, integerValue%10);
            vsShaderModel = buffer;
        } else {
            vsShaderModel = ":" VS_DefShaderModel;
        }
        auto psi = std::lower_bound(defines.cbegin(), defines.cend(), "ps_", CompareFirst<std::string, std::string>());
        if (psi != defines.cend() && !XlCompareString(psi->first, "ps_")) {
            char buffer[32];
            int integerValue = Utility::XlAtoI32(psi->second.c_str());
            sprintf_s(buffer, dimof(buffer), ":ps_%i_%i", integerValue/10, integerValue%10);
            psShaderModel = buffer;
        } else {
            psShaderModel = ":" PS_DefShaderModel;
        }
        auto gsi = std::lower_bound(defines.cbegin(), defines.cend(), "gs_", CompareFirst<std::string, std::string>());
        if (gsi != defines.cend() && !XlCompareString(gsi->first, "gs_")) {
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

        using namespace Metal;
    
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

    static const char* s_parameterBoxNames[] = 
        { "Geometry", "GlobalEnvironment", "Runtime", "Material" };

    class ParameterBoxTable
    {
    public:
        class Setting
        {
        public:
            ParameterBox _boxes[4];

            Setting();
            Setting(Setting&& moveFrom);
            Setting& operator=(Setting&& moveFrom);
        };
        std::vector<std::pair<uint64,Setting>> _settings;

        ParameterBoxTable(const ::Assets::ResChar filename[]);
        ParameterBoxTable(ParameterBoxTable&& moveFrom);
        ParameterBoxTable& operator=(ParameterBoxTable&& moveFrom);

        const ::Assets::DependencyValidation& GetDependencyValidation() const { return *_depVal; }
    protected:
        std::shared_ptr<::Assets::DependencyValidation> _depVal;
    };

    ParameterBoxTable::Setting::Setting() {}
    ParameterBoxTable::Setting::Setting(Setting&& moveFrom)
    {
        for (unsigned c=0; c<dimof(_boxes); ++c) {
            _boxes[c] = std::move(moveFrom._boxes[c]);
        }
    }
    auto ParameterBoxTable::Setting::operator=(Setting&& moveFrom) -> Setting&
    {
        for (unsigned c=0; c<dimof(_boxes); ++c) {
            _boxes[c] = std::move(moveFrom._boxes[c]);
        }
        return *this;
    }

    static void LoadInteritedParameterBoxes(
        Data& source, ParameterBox dst[4],
        ::Assets::DirectorySearchRules* searchRules,
        std::vector<const ::Assets::DependencyValidation*>* inherited)
    {
            //  Find the child called "Inherit". This will provide a list of 
            //  shareable settings that we can inherit from
            //  Inherit lists should take the form "FileName:Setting"
            //  FileName should have no extension -- we'll append .txt. 
            //  The "setting" should be a top-level item in the file
        auto* inheritList = source.ChildWithValue("Inherit");
        if (inheritList) {
            for (auto i=inheritList->child; i; i=i->next) {
                auto* colon = XlFindCharReverse(i->value, ':');
                if (colon) {
                    ::Assets::ResChar resolvedFile[MaxPath];
                    XlCopyNString(resolvedFile, i->value, colon-i->value);
                    XlCatString(resolvedFile, dimof(resolvedFile), ".txt");
                    if (searchRules) {
                        searchRules->ResolveFile(
                            resolvedFile, dimof(resolvedFile), resolvedFile);
                    }

                    auto& settingsTable = ::Assets::GetAssetDep<ParameterBoxTable>(resolvedFile);
                    auto settingHash = Hash64(colon+1);
                    
                    auto s = LowerBound(settingsTable._settings, settingHash);
                    if (s != settingsTable._settings.end() && s->first == settingHash) {
                        for (unsigned c=0; c<dimof(s_parameterBoxNames); ++c) {
                            dst[c].MergeIn(s->second._boxes[c]);
                        }
                    }

                    if (inherited) {
                        if (std::find(  inherited->begin(), inherited->end(),
                                        &settingsTable.GetDependencyValidation())
                            == inherited->end()) {

                            inherited->push_back(&settingsTable.GetDependencyValidation());
                        }
                    }
                }
            }
        }
    }

    static void LoadParameterBoxes(Data& source, ParameterBox dst[4])
    {
        auto* p = source.ChildWithValue("Parameters");
        if (p) {
            for (unsigned q=0; q<dimof(s_parameterBoxNames); ++q) {
                auto* d = p->ChildWithValue(s_parameterBoxNames[q]);
                auto& destinationParameters = dst[q];
                if (d) {
                    for (auto child = d->child; child; child = child->next) {
                        destinationParameters.SetParameter(
                            child->StrValue(),
                            child->ChildAt(0)?child->ChildAt(0)->IntValue():0);
                    }
                }
            }
        }
    }

    ParameterBoxTable::ParameterBoxTable(const ::Assets::ResChar filename[])
    {
        Data data;
        size_t sourceFileSize = 0;
        auto sourceFile = LoadFileAsMemoryBlock(filename, &sourceFileSize);

        _depVal = std::make_shared<::Assets::DependencyValidation>();
        RegisterFileDependency(_depVal, filename);

        if (sourceFile) {
            auto searchRules = ::Assets::DefaultDirectorySearchRules(filename);
            std::vector<const ::Assets::DependencyValidation*> inherited;

            data.Load((const char*)sourceFile.get(), (int)sourceFileSize);
            
                //  each top-level entry is a "Setting", which can contain parameter
                //  boxes (and possibly inherit statements and shaders)

            for (auto c=data.child; c; c=c->next) {
                Setting newSetting;
                LoadInteritedParameterBoxes(*c, newSetting._boxes, &searchRules, &inherited);
                LoadParameterBoxes(*c, newSetting._boxes);

                auto hash = Hash64(c->value);
                auto i = LowerBound(_settings, hash);
                _settings.insert(i, std::make_pair(hash, std::move(newSetting)));
            }

            for (auto i=inherited.begin(); i!=inherited.end(); ++i) {
                ::Assets::RegisterAssetDependency(_depVal, *i);
            }
        }
    }

    ParameterBoxTable::ParameterBoxTable(ParameterBoxTable&& moveFrom)
    : _settings(std::move(moveFrom._settings))
    , _depVal(std::move(moveFrom._depVal))
    {}

    ParameterBoxTable& ParameterBoxTable::operator=(ParameterBoxTable&& moveFrom)
    {
        _settings = std::move(moveFrom._settings);
        _depVal = std::move(moveFrom._depVal);
        return *this;
    }

    Technique::Technique(
        Data& source, 
        ::Assets::DirectorySearchRules* searchRules,
        std::vector<const ::Assets::DependencyValidation*>* inherited)
    {
            //
            //      There are some parameters that will we always have an effect on the
            //      binding. We need to make sure these are initialized with sensible
            //      values.
            //
        auto& globalParam = _baseParameters._parameters[ShaderParameters::Source::GlobalEnvironment];
        globalParam.SetParameter("vs_", 50);
        globalParam.SetParameter("ps_", 50);

        LoadInteritedParameterBoxes(source, _baseParameters._parameters, searchRules, inherited);
        LoadParameterBoxes(source, _baseParameters._parameters);

        _name = source.StrValue();
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

    ResolvedShader      ShaderType::FindVariation(  
		int techniqueIndex, 
        const ParameterBox* globalState[ShaderParameters::Source::Max],
        const TechniqueInterface& techniqueInterface) const
    {
        if (techniqueIndex >= int(_technique.size()) || !_technique[techniqueIndex].IsValid()) {
            return ResolvedShader();
        }
        return _technique[techniqueIndex].FindVariation(globalState, techniqueInterface);
    }

    ShaderType::ShaderType(const char resourceName[])
    {
        Data data;
        size_t sourceFileSize = 0;
        auto sourceFile = LoadFileAsMemoryBlock(resourceName, &sourceFileSize);

        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterFileDependency(_validationCallback, resourceName);
        
        if (sourceFile) {
            auto searchRules = ::Assets::DefaultDirectorySearchRules(resourceName);
            std::vector<const ::Assets::DependencyValidation*> inheritedAssets;

            data.Load((const char*)sourceFile.get(), (int)sourceFileSize);
            for (int c=0; c<data.Size(); ++c) {
                auto child = data.ChildAt(c);
                if (child) {
                    _technique.push_back(Technique(*child, &searchRules, &inheritedAssets));
                }
            }

            for (auto i=inheritedAssets.begin(); i!=inheritedAssets.end(); ++i) {
                ::Assets::RegisterAssetDependency(_validationCallback, *i);
            }
        }
    }

    ShaderType::~ShaderType()
    {}

        //////////////////////-------//////////////////////

    uint64      ShaderParameters::CalculateFilteredState(const ParameterBox* globalState[Source::Max]) const
    {
        uint64 filteredState = 0;
        for (unsigned c=0; c<Source::Max; ++c) {
              filteredState ^= _parameters[c].TranslateHash(*globalState[c]) << (c*6);     // we have to be careful of cases where 2 boxes have their filtered tables sort of swapped... Those cases should produce distinctive hashes
        }
        return filteredState;
    }

    uint64      ShaderParameters::CalculateFilteredHash(uint64 inputHash, const ParameterBox* globalState[Source::Max]) const
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

    void        ShaderParameters::BuildStringTable(std::vector<std::pair<const char*, std::string>>& defines) const
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


    void     TechniqueContext::BindGlobalUniforms(TechniqueInterface& binding)
    {
            //  We need to specify the order of resources as they appear in 
            //  _globalUniformsStream
        static auto HashGlobalTransform         = Hash64("GlobalTransform");
        static auto HashGlobalState             = Hash64("GlobalState");
        static auto HashFogSettings             = Hash64("FogSettings");
        static auto HashShadowProjection        = Hash64("ArbitraryShadowProjection");
        static auto HashOrthoShadowProjection   = Hash64("OrthogonalShadowProjection");
        binding.BindConstantBuffer(HashGlobalTransform, 0, 0);
        binding.BindConstantBuffer(HashGlobalState, 1, 0);
        binding.BindConstantBuffer(HashFogSettings, 2, 0);
        binding.BindConstantBuffer(HashShadowProjection, 3, 0);
        binding.BindConstantBuffer(HashOrthoShadowProjection, 4, 0);
    }

    void     TechniqueContext::BindGlobalUniforms(Metal::BoundUniforms& binding)
    {
            //  We need to specify the order of resources as they appear in 
            //  _globalUniformsStream
        static auto HashGlobalTransform         = Hash64("GlobalTransform");
        static auto HashGlobalState             = Hash64("GlobalState");
        static auto HashFogSettings             = Hash64("FogSettings");
        static auto HashShadowProjection        = Hash64("ShadowProjection");
        static auto HashOrthoShadowProjection   = Hash64("OrthogonalShadowProjection");
        binding.BindConstantBuffer(HashGlobalTransform, 0, 0);
        binding.BindConstantBuffer(HashGlobalState, 1, 0);
        binding.BindConstantBuffer(HashFogSettings, 2, 0);
        binding.BindConstantBuffer(HashShadowProjection, 3, 0);
        binding.BindConstantBuffer(HashOrthoShadowProjection, 4, 0);
    }

}}

