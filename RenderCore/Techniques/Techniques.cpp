// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Techniques.h"
#include "ParsingContext.h"
#include "RenderStateResolver.h"
#include "../Metal/Shader.h"
#include "../Metal/InputLayout.h"
#include "../Metal/DeviceContext.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/InvalidAssetManager.h"
#include "../../Assets/ConfigFileContainer.h"
#include "../../Math/Vector.h"
#include "../../Math/Matrix.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/IteratorUtils.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/Conversion.h"
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

        Technique::HashConflictTest::HashConflictTest(const ParameterBox globalState[ShaderParameters::Source::Max], uint64 rawHash, uint64 filteredHash, uint64 interfaceHash)
        {
            _rawHash = rawHash; _filteredHash = filteredHash; _interfaceHash = interfaceHash;
            for (unsigned c=0; c<ShaderParameters::Source::Max; ++c) {
                _globalState[c] = globalState[c];
            }
        }

        Technique::HashConflictTest::HashConflictTest() {}

        void Technique::TestHashConflict(const ParameterBox* globalState[ShaderParameters::Source::Max], const HashConflictTest& comparison) const
        {
                // check to make sure the parameter names in both of these boxes is the same
                // note -- this isn't exactly correctly. we need to filter out parameters that are not relevant to this technique
            // for (unsigned c=0; c<ShaderParameters::Source::Max; ++c) {
            //     assert(globalState[c]->AreParameterNamesEqual(comparison._globalState[c]));
            // }
        }

        static std::string BuildParamsAsString(
            const ShaderParameters& baseParameters,
            const ParameterBox globalState[ShaderParameters::Source::Max])
        {
            std::vector<std::pair<const char*, std::string>> defines;
            baseParameters.BuildStringTable(defines);
            for (unsigned c=0; c<ShaderParameters::Source::Max; ++c) {
                globalState[c].OverrideStringTable(defines);
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
            return combinedStrings;
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
		const bool simpleHash = false;
		if (constant_expression<simpleHash>::result()) {
			for (unsigned c = 0; c < ShaderParameters::Source::Max; ++c) {
				inputHash ^= globalState[c]->GetParameterNamesHash();
				inputHash ^= globalState[c]->GetHash() << (c * 6);    // we have to be careful of cases where the values in one box is very similar to the values in another
			}
		} else {
			inputHash = HashCombine(globalState[0]->GetHash(), globalState[0]->GetParameterNamesHash());
			for (unsigned c = 1; c < ShaderParameters::Source::Max; ++c) {
				inputHash = HashCombine(globalState[c]->GetParameterNamesHash(), inputHash);
				inputHash = HashCombine(globalState[c]->GetHash(), inputHash);
			}
		}
        
        uint64 globalHashWithInterface = inputHash ^ techniqueInterface.GetHashValue();
        auto i = std::lower_bound(_globalToResolved.begin(), _globalToResolved.end(), globalHashWithInterface, CompareFirst<uint64, ResolvedShader>());
        if (i!=_globalToResolved.cend() && i->first == globalHashWithInterface) {
            if (i->second._shaderProgram && (i->second._shaderProgram->GetDependencyValidation()->GetValidationIndex()!=0)) {
                ResolveAndBind(i->second, globalState, techniqueInterface);
            }

            #if defined(CHECK_TECHNIQUE_HASH_CONFLICTS)
                auto ti = std::lower_bound(_globalToResolvedTest.begin(), _globalToResolvedTest.end(), globalHashWithInterface, CompareFirst<uint64, HashConflictTest>());
                assert(ti!=_globalToResolvedTest.cend() && ti->first == globalHashWithInterface);
                TestHashConflict(globalState, ti->second);

                OutputDebugString((BuildParamsAsString(_baseParameters, ti->second._globalState) + "\r\n").c_str());
            #endif
            return i->second;
        }

        uint64 filteredHashValue = _baseParameters.CalculateFilteredHash(inputHash, globalState);
        uint64 filteredHashWithInterface = filteredHashValue ^ techniqueInterface.GetHashValue();
        auto i2 = std::lower_bound(_filteredToResolved.begin(), _filteredToResolved.end(), filteredHashWithInterface, CompareFirst<uint64, ResolvedShader>());
        if (i2!=_filteredToResolved.cend() && i2->first == filteredHashWithInterface) {
            _globalToResolved.insert(i, std::make_pair(globalHashWithInterface, i2->second));
            if (i2->second._shaderProgram && (i2->second._shaderProgram->GetDependencyValidation()->GetValidationIndex()!=0)) {
                ResolveAndBind(i2->second, globalState, techniqueInterface);
            }

            #if defined(CHECK_TECHNIQUE_HASH_CONFLICTS)
                auto lti = std::lower_bound(_localToResolvedTest.begin(), _localToResolvedTest.end(), filteredHashWithInterface, CompareFirst<uint64, HashConflictTest>());
                assert(lti!=_localToResolvedTest.cend() && lti->first == filteredHashWithInterface);
				TestHashConflict(globalState, lti->second);

                auto gti = std::lower_bound(_globalToResolvedTest.begin(), _globalToResolvedTest.end(), globalHashWithInterface, CompareFirst<uint64, HashConflictTest>());
                _globalToResolvedTest.insert(gti, std::make_pair(globalHashWithInterface, HashConflictTest(lti->second._globalState, inputHash, filteredHashValue, techniqueInterface.GetHashValue())));

                OutputDebugString((BuildParamsAsString(_baseParameters, lti->second._globalState) + "\r\n").c_str());
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
        std::vector<std::pair<const utf8*, std::string>> defines;
        _baseParameters.BuildStringTable(defines);
        for (unsigned c=0; c<ShaderParameters::Source::Max; ++c) {
            OverrideStringTable(defines, *globalState[c]);
        }

        auto combinedStrings = FlattenStringTable(defines);

        std::string vsShaderModel, psShaderModel, gsShaderModel;
        auto vsi = std::lower_bound(defines.cbegin(), defines.cend(), (const utf8*)"vs_", CompareFirst<const utf8*, std::string>());
        if (vsi != defines.cend() && !XlCompareString(vsi->first, (const utf8*)"vs_")) {
            char buffer[32];
            int integerValue = Utility::XlAtoI32(vsi->second.c_str());
            sprintf_s(buffer, dimof(buffer), ":vs_%i_%i", integerValue/10, integerValue%10);
            vsShaderModel = buffer;
        } else {
            vsShaderModel = ":" VS_DefShaderModel;
        }
        auto psi = std::lower_bound(defines.cbegin(), defines.cend(), (const utf8*)"ps_", CompareFirst<const utf8*, std::string>());
        if (psi != defines.cend() && !XlCompareString(psi->first, (const utf8*)"ps_")) {
            char buffer[32];
            int integerValue = Utility::XlAtoI32(psi->second.c_str());
            sprintf_s(buffer, dimof(buffer), ":ps_%i_%i", integerValue/10, integerValue%10);
            psShaderModel = buffer;
        } else {
            psShaderModel = ":" PS_DefShaderModel;
        }
        auto gsi = std::lower_bound(defines.cbegin(), defines.cend(), (const utf8*)"gs_", CompareFirst<const utf8*, std::string>());
        if (gsi != defines.cend() && !XlCompareString(gsi->first, (const utf8*)"gs_")) {
            char buffer[32];
            int integerValue = Utility::XlAtoI32(psi->second.c_str());
            sprintf_s(buffer, dimof(buffer), ":gs_%i_%i", integerValue/10, integerValue%10);
            gsShaderModel = buffer;
        } else {
            gsShaderModel = ":" GS_DefShaderModel;
        }

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
            Setting(
                InputStreamFormatter<utf8>& source,
                const ::Assets::DirectorySearchRules& searchRules,
                std::vector<std::shared_ptr<::Assets::DependencyValidation>>& inherited);
        };
        std::vector<std::pair<uint64,Setting>> _settings;

        ParameterBoxTable(const ::Assets::ResChar filename[]);
        ParameterBoxTable(ParameterBoxTable&& moveFrom);
        ~ParameterBoxTable();
        ParameterBoxTable& operator=(ParameterBoxTable&& moveFrom);

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _depVal; }
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

    using Formatter = InputStreamFormatter<utf8>;

    static bool Is(const char name[], Utility::InputStreamFormatter<utf8>::InteriorSection section)
    {
        return !XlComparePrefixI(section._start, (const utf8*)name, section._end - section._start);
    }

    static std::basic_string<Formatter::value_type> AsString(Formatter::InteriorSection& input)
    {
        return std::basic_string<Formatter::value_type>(input._start, input._end);
    }

    static void LoadInheritedParameterBoxes(
        Formatter& source, ParameterBox dst[4],
        const ::Assets::DirectorySearchRules* searchRules,
        std::vector<std::shared_ptr<::Assets::DependencyValidation>>* inherited)
    {
            //  We will serialize in a list of 
            //  shareable settings that we can inherit from
            //  Inherit lists should take the form "FileName:Setting"
            //  The "setting" should be a top-level item in the file

        for (;;) {

            auto next = source.PeekNext();
            if (next == Formatter::Blob::EndElement) return;
            if (next != Formatter::Blob::AttributeName)
                Throw(FormatException("Unexpected blob when serializing inheritted list", source.GetLocation()));
            
            Formatter::InteriorSection name, value;
            if (!source.TryAttribute(name, value))
                Throw(FormatException("Bad attribute in inheritted list", source.GetLocation()));
        
            auto colon = std::find(name._start, name._end, ':');
            if (colon == name._end) 
                Throw(FormatException("Inheritted object missing a colon", source.GetLocation()));

            ::Assets::ResChar resolvedFile[MaxPath];
            XlCopyNString(resolvedFile, (const ::Assets::ResChar*)name._start, colon-name._start);
            if (searchRules) {
                searchRules->ResolveFile(
                    resolvedFile, dimof(resolvedFile), resolvedFile);
            }

            auto& settingsTable = ::Assets::GetAssetDep<ParameterBoxTable>(resolvedFile);
            auto settingHash = Hash64(colon+1, name._end);
                    
            bool foundAtLeastOne = false;
            auto s = LowerBound(settingsTable._settings, settingHash);
            if (s != settingsTable._settings.end() && s->first == settingHash) {
                for (unsigned c=0; c<dimof(s_parameterBoxNames); ++c) {
                    dst[c].MergeIn(s->second._boxes[c]);
                    foundAtLeastOne = true;
                }
            }

            if (!foundAtLeastOne)
                Throw(FormatException("Inheritted object not found", source.GetLocation()));

            if (inherited && std::find(inherited->begin(), inherited->end(), settingsTable.GetDependencyValidation()) == inherited->end()) {
                inherited->push_back(settingsTable.GetDependencyValidation());
            }
        }
    }

    static void LoadParameterBoxes(Formatter& source, ParameterBox dst[4])
    {
        for (;;) {
            auto next = source.PeekNext();
            if (next == Formatter::Blob::EndElement) return;
            if (next != Formatter::Blob::BeginElement)
                Throw(FormatException("Unexpected blob when serializing parameter box list", source.GetLocation()));

            Formatter::InteriorSection eleName;
            if (!source.TryBeginElement(eleName))
                Throw(FormatException("Bad begin element in parameter box list", source.GetLocation()));

            bool matched = false;
            for (unsigned q=0; q<dimof(s_parameterBoxNames); ++q)
                if (Is(s_parameterBoxNames[q], eleName)) {
                        // When a value is not specified, we should default to "0u"
                        // This works well with the C preprocessor; it just means
                        // things will generally default to off
                    unsigned zero = 0u;
                    dst[q].MergeIn(ParameterBox(source, &zero, ImpliedTyping::TypeOf<unsigned>()));
                    matched = true;
                }

            if (!matched)
                Throw(FormatException("Unknown parameter box name", source.GetLocation()));

            if (!source.TryEndElement())
                Throw(FormatException("Bad end element in parameter box list", source.GetLocation()));
        }
    }

    ParameterBoxTable::Setting::Setting(
        Formatter& formatter,
        const ::Assets::DirectorySearchRules& searchRules,
        std::vector<std::shared_ptr<::Assets::DependencyValidation>>& inherited)
    {
        for (;;) {
            auto next = formatter.PeekNext();
            if (next == Formatter::Blob::EndElement) break;
            if (next != Formatter::Blob::BeginElement)
                Throw(FormatException("Unexpected blob in parameter box table setting", formatter.GetLocation()));

            Formatter::InteriorSection eleName;
            if (!formatter.TryBeginElement(eleName)) 
                Throw(FormatException("Bad begin element", formatter.GetLocation()));

            if (Is("Inherit", eleName)) {
                LoadInheritedParameterBoxes(formatter, _boxes, &searchRules, &inherited);
            } else if (Is("Parameters", eleName)) {
                LoadParameterBoxes(formatter, _boxes);
            } else break;

            if (!formatter.TryEndElement()) 
                Throw(FormatException("Bad end element", formatter.GetLocation()));
        }
    }

    ParameterBoxTable::ParameterBoxTable(const ::Assets::ResChar filename[])
    {
        size_t sourceFileSize = 0;
        auto sourceFile = LoadFileAsMemoryBlock(filename, &sourceFileSize);

        _depVal = std::make_shared<::Assets::DependencyValidation>();
        RegisterFileDependency(_depVal, filename);

        if (sourceFile) {
            auto searchRules = ::Assets::DefaultDirectorySearchRules(filename);
            std::vector<std::shared_ptr<::Assets::DependencyValidation>> inherited;

            TRY
            {
                Formatter formatter(MemoryMappedInputStream(sourceFile.get(), PtrAdd(sourceFile.get(), sourceFileSize)));
            
                    //  each top-level entry is a "Setting", which can contain parameter
                    //  boxes (and possibly inherit statements and shaders)

                for (;;) {
                    bool cleanQuit = false;
                    switch (formatter.PeekNext()) {
                    case Formatter::Blob::BeginElement:
                        {
                            Formatter::InteriorSection settingName;
                            if (!formatter.TryBeginElement(settingName)) break;

                            auto hash = Hash64(settingName._start, settingName._end);
                            auto i = LowerBound(_settings, hash);
                            _settings.insert(i, std::make_pair(hash, Setting(formatter, searchRules, inherited)));

                            if (!formatter.TryEndElement()) break;
                        }
                        continue;

                    case Formatter::Blob::None:
                        cleanQuit = true;
                        break;

                    default:
                        break;
                    }

                    if (!cleanQuit)
                        Throw(FormatException("Unexpected blob while reading stream", formatter.GetLocation()));
                    break;
                }

                if (::Assets::Services::GetInvalidAssetMan())
                    ::Assets::Services::GetInvalidAssetMan()->MarkValid(filename);
            }
            CATCH (const FormatException& e)
            {
                if (::Assets::Services::GetInvalidAssetMan())
                    ::Assets::Services::GetInvalidAssetMan()->MarkInvalid(filename, e.what());
                Throw(::Assets::Exceptions::InvalidAsset(filename, e.what()));
            }
            CATCH_END

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

    ParameterBoxTable::~ParameterBoxTable() {}

    Technique::Technique(
        Formatter& formatter, 
        const std::string& name,
        const ::Assets::DirectorySearchRules* searchRules,
        std::vector<std::shared_ptr<::Assets::DependencyValidation>>* inherited)
    {
            //
            //      There are some parameters that will we always have an effect on the
            //      binding. We need to make sure these are initialized with sensible
            //      values.
            //
        auto& globalParam = _baseParameters._parameters[ShaderParameters::Source::GlobalEnvironment];
        globalParam.SetParameter((const utf8*)"vs_", 50);
        globalParam.SetParameter((const utf8*)"ps_", 50);

        using ParsingString = std::basic_string<Formatter::value_type>;

        for (;;) {
            bool cleanQuit = false;
            switch (formatter.PeekNext())
            {
            case Formatter::Blob::BeginElement:
                {
                    Formatter::InteriorSection eleName;
                    if (!formatter.TryBeginElement(eleName)) break;

                    if (Is("Inherit", eleName)) {
                        LoadInheritedParameterBoxes(formatter, _baseParameters._parameters, searchRules, inherited);
                    } else if (Is("Parameters", eleName)) {
                        LoadParameterBoxes(formatter, _baseParameters._parameters);
                    } else break;

                    if (!formatter.TryEndElement()) break;
                    continue;
                }

            case Formatter::Blob::AttributeName:
                {
                    Formatter::InteriorSection name, value;
                    if (!formatter.TryAttribute(name, value)) break;
                    if (Is("VertexShader", name)) {
                        _vertexShaderName = Conversion::Convert<decltype(_vertexShaderName)>(AsString(value));
                    } else if (Is("PixelShader", name)) {
                        _pixelShaderName = Conversion::Convert<decltype(_pixelShaderName)>(AsString(value));
                    } else if (Is("GeometryShader", name)) {
                        _geometryShaderName = Conversion::Convert<decltype(_geometryShaderName)>(AsString(value));
                    }
                    continue;
                }

            case Formatter::Blob::EndElement:
                cleanQuit = true;
                break;

            default: break;
            }

            if (!cleanQuit)
                Throw(FormatException("Unexpected blob while reading technique", formatter.GetLocation()));
            break;
        }

        _name = name;
    }

    void Technique::MergeIn(const Technique& source)
    {
        if (!source._name.empty()) _name = source._name;
        if (!source._vertexShaderName.empty()) _vertexShaderName = source._vertexShaderName;
        if (!source._pixelShaderName.empty()) _pixelShaderName = source._pixelShaderName;
        if (!source._geometryShaderName.empty()) _geometryShaderName = source._geometryShaderName;

        for (unsigned c=0; c<ShaderParameters::Source::Max; ++c) {
            const auto& s = source._baseParameters._parameters[c];
            auto& d = _baseParameters._parameters[c];
            for (auto i = s.Begin(); !i.IsEnd(); ++i)
                d.SetParameter(i.Name(), i.RawValue(), i.Type());
        }
    }

	template<typename Char>
		static void ReplaceInString(
			std::basic_string<Char>& str, 
			StringSection<Char> oldText, 
			StringSection<Char> newText)
		{
			size_t i = 0;
			for (;;) {
				i = str.find(oldText.begin(), i, oldText.Length());
				if (i == std::basic_string<Char>::npos) return;
				str.replace(i, oldText.Length(), newText.begin());
				i += newText.Length(); // prevent infinite loops if newText contains oldText
			}
		}

	void Technique::ReplaceSelfReference(StringSection<::Assets::ResChar> filename)
	{
		auto selfRef = MakeStringSection("<.>");
		ReplaceInString(_vertexShaderName, selfRef, filename);
		ReplaceInString(_pixelShaderName, selfRef, filename);
		ReplaceInString(_geometryShaderName, selfRef, filename);
	}

    Technique::Technique(Technique&& moveFrom)
    :   _name(moveFrom._name)
    ,   _baseParameters(std::move(moveFrom._baseParameters))
    ,   _filteredToResolved(std::move(moveFrom._filteredToResolved))
    ,   _globalToResolved(std::move(moveFrom._globalToResolved))
    ,   _vertexShaderName(moveFrom._vertexShaderName)
    ,   _pixelShaderName(moveFrom._pixelShaderName)
    ,   _geometryShaderName(moveFrom._geometryShaderName)
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

    Technique::Technique() {}
    Technique::~Technique() {}



    ///////////////////////   S H A D E R   T Y P E   ///////////////////////////

    ResolvedShader ShaderType::FindVariation(  
		int techniqueIndex, 
        const ParameterBox* globalState[ShaderParameters::Source::Max],
        const TechniqueInterface& techniqueInterface) const
    {
        if (techniqueIndex >= dimof(_technique) || !_technique[techniqueIndex].IsValid())
            return ResolvedShader();
        return _technique[techniqueIndex].FindVariation(globalState, techniqueInterface);
    }

    T1(Pair) class CompareFirstString
    {
    public:
        bool operator()(const Pair& lhs, const Pair& rhs) const { return XlCompareString(lhs.first, rhs.first) < 0; }
        using Section = StringSection<
            typename std::remove_const<typename std::remove_reference<decltype(*std::declval<typename Pair::first_type>())>::type>::type
            >;
        bool operator()(const Pair& lhs, const Section& rhs) const { return XlCompareString(lhs.first, rhs) < 0; }
        bool operator()(const Section& lhs, const Pair& rhs) const { return XlCompareString(lhs, rhs.first) < 0; }
    };

    static unsigned AsTechniqueIndex(StringSection<utf8> name)
    {
        using Pair = std::pair<const utf8*, unsigned>;
        const Pair bindingNames[] = 
        {
                // note -- lexographically sorted!
            { u("Deferred"),                       unsigned(TechniqueIndex::Deferred) },
            { u("DepthOnly"),                      unsigned(TechniqueIndex::DepthOnly) },
            { u("Illum"),                          unsigned(TechniqueIndex::Forward) },
            { u("OrderIndependentTransparency"),   unsigned(TechniqueIndex::OrderIndependentTransparency) },
            { u("PrepareVegetationSpawn"),         unsigned(TechniqueIndex::PrepareVegetationSpawn) },
            { u("RayTest"),                        unsigned(TechniqueIndex::RayTest) },
            { u("ShadowGen"),                      unsigned(TechniqueIndex::ShadowGen) },
            { u("StochasticTransparency"),         unsigned(TechniqueIndex::StochasticTransparency) },
            { u("VisNormals"),                     unsigned(TechniqueIndex::VisNormals) },
            { u("VisWireframe"),                   unsigned(TechniqueIndex::VisWireframe) },
            { u("WriteTriangleIndex"),             unsigned(TechniqueIndex::WriteTriangleIndex) }
        };

        auto i = std::lower_bound(bindingNames, ArrayEnd(bindingNames), name, CompareFirstString<Pair>());
        if (XlEqString(name, i->first))
            return i->second;
        return ~0u;
    }

    ShaderType::ShaderType(const ::Assets::ResChar resourceName[])
    {
        size_t sourceFileSize = 0;
        auto sourceFile = LoadFileAsMemoryBlock(resourceName, &sourceFileSize);

        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterFileDependency(_validationCallback, resourceName);
        
        if (sourceFile && sourceFileSize) {
            auto searchRules = ::Assets::DefaultDirectorySearchRules(resourceName);
            std::vector<std::shared_ptr<::Assets::DependencyValidation>> inheritedAssets;

            StringSection<char> configSection((const char*)sourceFile.get(), (const char*)PtrAdd(sourceFile.get(), sourceFileSize));

            auto compoundDoc = ::Assets::ReadCompoundTextDocument(configSection);
            if (!compoundDoc.empty()) {
                auto i = std::find_if(
                    compoundDoc.cbegin(), compoundDoc.cend(),
                    [](const ::Assets::TextChunk<char>& chunk)
                    { return XlEqString(chunk._type, "TechniqueConfig"); });

                if (i != compoundDoc.cend())
                    configSection = i->_content;
            }

            TRY
            {
                ParseConfigFile(
                    StringSection<utf8>((const utf8*)configSection.begin(), (const utf8*)configSection.end()), 
                    searchRules, inheritedAssets);
                if (::Assets::Services::GetInvalidAssetMan())
                    ::Assets::Services::GetInvalidAssetMan()->MarkValid(resourceName);
            }
            CATCH (const FormatException& e)
            {
                if (::Assets::Services::GetInvalidAssetMan())
                    ::Assets::Services::GetInvalidAssetMan()->MarkInvalid(resourceName, e.what());
                Throw(::Assets::Exceptions::InvalidAsset(resourceName, e.what()));
            }
			CATCH_END

				// Do some patch-up after parsing...
				// we want to replace <.> with the name of the asset
				// This allows the asset to reference itself (without complications
				// for related to directories, etc)
			for (unsigned c=0; c<dimof(_technique); ++c)
				_technique[c].ReplaceSelfReference(resourceName);

            for (auto i=inheritedAssets.begin(); i!=inheritedAssets.end(); ++i)
                ::Assets::RegisterAssetDependency(_validationCallback, *i);
        }
    }

    ShaderType::~ShaderType()
    {}

    void ShaderType::ParseConfigFile(
        StringSection<utf8> input, 
        const ::Assets::DirectorySearchRules& searchRules,
        std::vector<std::shared_ptr<::Assets::DependencyValidation>>& inheritedAssets)
    {
        Formatter formatter(MemoryMappedInputStream(input.begin(), input.end()));
        for (;;) {
            bool cleanQuit = false;
            switch (formatter.PeekNext()) {
            case Formatter::Blob::BeginElement:
                {
                    Formatter::InteriorSection eleName;
                    if (!formatter.TryBeginElement(eleName)) break;

                    if (XlEqString(eleName, u("Inherit"))) {
                        // we should find a list of other technique configuation files to inherit from
                        for (;;) {
                            auto next = formatter.PeekNext();
                            if (next == Formatter::Blob::EndElement) break;
                            if (next != Formatter::Blob::AttributeName)
                                Throw(FormatException("Unexpected blob when serializing inheritted list", formatter.GetLocation()));
            
                            Formatter::InteriorSection name, value;
                            if (!formatter.TryAttribute(name, value))
                                Throw(FormatException("Bad attribute in inheritted list", formatter.GetLocation()));

                            ::Assets::ResChar resolvedFile[MaxPath];
                            XlCopyNString(resolvedFile, (const ::Assets::ResChar*)name._start, name._end-name._start);
                            searchRules.ResolveFile(resolvedFile, resolvedFile);

                            // exceptions thrown by from the inheritted asset will not be suppressed
                            const auto& inheritFrom = ::Assets::GetAssetDep<ShaderType>(resolvedFile);
                            inheritedAssets.push_back(inheritFrom.GetDependencyValidation());

                            // we should merge in the content from all the inheritted's assets
                            for (unsigned c=0; c<dimof(_technique); ++c)
                                _technique[c].MergeIn(inheritFrom._technique[c]);
                        }
                    } else {
                        auto index = AsTechniqueIndex(eleName);
                        if (index < dimof(_technique)) {
                            Technique newTech(
                                formatter, 
                                Conversion::Convert<std::string>(AsString(eleName)),
                                &searchRules, &inheritedAssets);
                        
                            // Merge this new technique definition into our existing
                            // technique. The new technique values will override any
                            // values already there.
                            if (_technique[index].IsValid()) {
                                _technique[index].MergeIn(newTech);
                            } else {
                                _technique[index] = std::move(newTech);
                            }
                        } else {
                            LogWarning 
                                << "Ignoring technique label (" << eleName.AsString().c_str() 
                                << ") because it is unrecognised";
                            formatter.SkipElement();
                        }
                    }

                    if (!formatter.TryEndElement()) break;
                }
                continue;

            case Formatter::Blob::None:
                cleanQuit = true;
                break;

            default:
                break;
            }

            if (!cleanQuit)
                Throw(FormatException("Unexpected blob while reading stream", formatter.GetLocation()));
            break;
        }
    }

        //////////////////////-------//////////////////////

    uint64      ShaderParameters::CalculateFilteredHash(const ParameterBox* globalState[Source::Max]) const
    {
		uint64 filteredState = _parameters[0].CalculateFilteredHashValue(*globalState[0]);
        for (unsigned c=1; c<Source::Max; ++c) {
              // filteredState ^= _parameters[c].TranslateHash(*globalState[c]) << (c*6);     // we have to be careful of cases where 2 boxes have their filtered tables sort of swapped... Those cases should produce distinctive hashes

			filteredState = HashCombine(_parameters[c].CalculateFilteredHashValue(*globalState[c]), filteredState);
        }
        return filteredState;
    }

    uint64      ShaderParameters::CalculateFilteredHash(uint64 inputHash, const ParameterBox* globalState[Source::Max]) const
    {
            //      Find a local state to match
        auto i = LowerBound(_globalToFilteredTable, inputHash);
        if (i!=_globalToFilteredTable.cend() && i->first == inputHash) {
            return i->second;
        }

            //  The call to "CalculateFilteredHash" here is quite expensive... Ideally we should only get here during
            //  initialisation steps (or perhaps on the first few frames. We ideally don't want to be going through
            //  all of this during normal frames.
        uint64 filteredState = CalculateFilteredHash(globalState);
        _globalToFilteredTable.insert(i, std::make_pair(inputHash, filteredState));
        return filteredState;
    }

    void        ShaderParameters::BuildStringTable(std::vector<std::pair<const utf8*, std::string>>& defines) const
    {
        for (unsigned c=0; c<dimof(_parameters); ++c) {
            Utility::BuildStringTable(defines, _parameters[c]);
        }
    }

    void ResolvedShader::Apply(
        Metal::DeviceContext& devContext,
        ParsingContext& parserContext,
        const std::initializer_list<Metal::ConstantBufferPacket>& pkts) const
    {
        _boundUniforms->Apply(
            devContext, 
            parserContext.GetGlobalUniformsStream(),
            Metal::UniformsStream(pkts.begin(), nullptr, pkts.size()));
        devContext.Bind(*_boundLayout);
        devContext.Bind(*_shaderProgram);
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
        static auto HashGlobalTransform             = Hash64("GlobalTransform");
        static auto HashGlobalState                 = Hash64("GlobalState");
        static auto HashFogSettings                 = Hash64("FogSettings");
        static auto HashShadowProjection            = Hash64("ArbitraryShadowProjection");
        static auto HashOrthoShadowProjection       = Hash64("OrthogonalShadowProjection");
        static auto HashBasicLightingEnvironment    = Hash64("BasicLightingEnvironment");
        binding.BindConstantBuffer(HashGlobalTransform, CB_GlobalTransform, 0);
        binding.BindConstantBuffer(HashGlobalState, CB_GlobalState, 0);
        binding.BindConstantBuffer(HashFogSettings, CB_FogSettings, 0);
        binding.BindConstantBuffer(HashShadowProjection, CB_ShadowProjection, 0);
        binding.BindConstantBuffer(HashOrthoShadowProjection, CB_OrthoShadowProjection, 0);
        binding.BindConstantBuffer(HashBasicLightingEnvironment, CB_BasicLightingEnvironment, 0);
    }

    void     TechniqueContext::BindGlobalUniforms(Metal::BoundUniforms& binding)
    {
            //  We need to specify the order of resources as they appear in 
            //  _globalUniformsStream
        static auto HashGlobalTransform             = Hash64("GlobalTransform");
        static auto HashGlobalState                 = Hash64("GlobalState");
        static auto HashFogSettings                 = Hash64("FogSettings");
        static auto HashShadowProjection            = Hash64("ShadowProjection");
        static auto HashOrthoShadowProjection       = Hash64("OrthogonalShadowProjection");
        static auto HashBasicLightingEnvironment    = Hash64("BasicLightingEnvironment");
        binding.BindConstantBuffer(HashGlobalTransform, CB_GlobalTransform, 0);
        binding.BindConstantBuffer(HashGlobalState, CB_GlobalState, 0);
        binding.BindConstantBuffer(HashFogSettings, CB_FogSettings, 0);
        binding.BindConstantBuffer(HashShadowProjection, CB_ShadowProjection, 0);
        binding.BindConstantBuffer(HashOrthoShadowProjection, CB_OrthoShadowProjection, 0);
        binding.BindConstantBuffer(HashBasicLightingEnvironment, CB_BasicLightingEnvironment, 0);
    }

    TechniqueContext::TechniqueContext()
    {
        _defaultStateSetResolver = CreateStateSetResolver_Default();
    }

}}

