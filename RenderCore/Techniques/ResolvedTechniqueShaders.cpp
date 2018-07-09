// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ResolvedTechniqueShaders.h"
#include "ParsingContext.h"
#include "../Types.h"
#include "../FrameBufferDesc.h"
#include "../UniformsStream.h"
#include "../BufferView.h"
#include "../Metal/InputLayout.h"
#include "../Metal/Shader.h"
#include "../Metal/DeviceContext.h"
#include "../../Assets/AssetsCore.h"
#include "../../Assets/Assets.h"

namespace RenderCore { namespace Techniques
{

        ///////////////////////   T E C H N I Q U E   I N T E R F A C E   ///////////////////////////

    class TechniqueInterface::Pimpl
    {
    public:
        std::vector<InputElementDesc>			_vertexInputLayout;
        std::vector<UniformsStreamInterface>    _uniformStreams;

        uint64 _hashValue;
        Pimpl() : _hashValue(0) {}

        void        UpdateHashValue();
    };

    void        TechniqueInterface::Pimpl::UpdateHashValue()
    {
		_hashValue = 0;
		for (const auto& i : _uniformStreams)
			_hashValue = HashCombine(i.GetHash(), _hashValue);

        unsigned index = 0;
        for (auto i=_vertexInputLayout.cbegin(); i!=_vertexInputLayout.cend(); ++i, ++index) {
            struct PartialDesc
            {
                unsigned        _semanticIndex;
                Format			_nativeFormat;
                unsigned        _inputSlot;
                unsigned        _alignedByteOffset;
                InputDataRate	_inputSlotClass;
                unsigned        _instanceDataStepRate;
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

    void    TechniqueInterface::BindUniformsStream(unsigned streamIndex, const UniformsStreamInterface& interf)
    {
		if (_pimpl->_uniformStreams.size() <= streamIndex)
			_pimpl->_uniformStreams.resize(streamIndex + 1);
		_pimpl->_uniformStreams[streamIndex] = interf;
		_pimpl->UpdateHashValue();
    }

	void     TechniqueInterface::BindGlobalUniforms()
    {
		BindUniformsStream(0, TechniqueContext::GetGlobalUniformsStreamInterface());
    }

    uint64  TechniqueInterface::GetHashValue() const
    {
        return _pimpl->_hashValue;
    }

    TechniqueInterface::TechniqueInterface() 
    {
        _pimpl = std::make_unique<TechniqueInterface::Pimpl>();
    }

    TechniqueInterface::TechniqueInterface(IteratorRange<const InputElementDesc*> vertexInputLayout)
    {
        _pimpl = std::make_unique<TechniqueInterface::Pimpl>();
        _pimpl->_vertexInputLayout.insert(
            _pimpl->_vertexInputLayout.begin(),
            vertexInputLayout.begin(), vertexInputLayout.end());
        _pimpl->UpdateHashValue();
    }

    TechniqueInterface::~TechniqueInterface()
    {}

    TechniqueInterface::TechniqueInterface(TechniqueInterface&& moveFrom) never_throws
    {
        _pimpl = std::move(moveFrom._pimpl);
    }

    TechniqueInterface&TechniqueInterface::operator=(TechniqueInterface&& moveFrom) never_throws
    {
        _pimpl = std::move(moveFrom._pimpl);
        return *this;
    }

        ///////////////////////   T E C H N I Q U E   I N T E R F A C E   ///////////////////////////

    #if defined(CHECK_TECHNIQUE_HASH_CONFLICTS)
        ResolvedTechniqueInterfaceShaders::Entry::HashConflictTest::HashConflictTest(const ParameterBox* globalState[ShaderSelectors::Source::Max], uint64 rawHash, uint64 filteredHash, uint64 interfaceHash)
        {
            _rawHash = rawHash; _filteredHash = filteredHash; _interfaceHash = interfaceHash;
            for (unsigned c=0; c<ShaderSelectors::Source::Max; ++c) {
                _globalState[c] = *globalState[c];
            }
        }

        ResolvedTechniqueInterfaceShaders::Entry::HashConflictTest::HashConflictTest(const ParameterBox globalState[ShaderSelectors::Source::Max], uint64 rawHash, uint64 filteredHash, uint64 interfaceHash)
        {
            _rawHash = rawHash; _filteredHash = filteredHash; _interfaceHash = interfaceHash;
            for (unsigned c=0; c<ShaderSelectors::Source::Max; ++c) {
                _globalState[c] = globalState[c];
            }
        }

        ResolvedTechniqueInterfaceShaders::Entry::HashConflictTest::HashConflictTest() {}

        void ResolvedTechniqueInterfaceShaders::Entry::TestHashConflict(const ParameterBox* globalState[ShaderSelectors::Source::Max], const HashConflictTest& comparison) const
        {
                // check to make sure the parameter names in both of these boxes is the same
                // note -- this isn't exactly correctly. we need to filter out parameters that are not relevant to this technique
            // for (unsigned c=0; c<ShaderSelectors::Source::Max; ++c) {
            //     assert(globalState[c]->AreParameterNamesEqual(comparison._globalState[c]));
            // }
        }

        static std::string BuildParamsAsString(
            const ShaderSelectors& baseParameters,
            const ParameterBox globalState[ShaderSelectors::Source::Max])
        {
            std::vector<std::pair<const utf8*, std::string>> defines;
            baseParameters.BuildStringTable(defines);
            for (unsigned c=0; c<ShaderSelectors::Source::Max; ++c) {
                OverrideStringTable(defines, globalState[c]);
            }

            std::string combinedStrings;
            size_t size = 0;
            std::for_each(defines.cbegin(), defines.cend(), 
                [&size](const std::pair<const utf8*, std::string>& object) { size += 2 + XlStringLen((const char*)object.first) + object.second.size(); });
            combinedStrings.reserve(size);
            std::for_each(defines.cbegin(), defines.cend(), 
                [&combinedStrings](const std::pair<const utf8*, std::string>& object) 
                {
                    combinedStrings.insert(combinedStrings.end(), (const char*)object.first, (const char*)XlStringEnd(object.first)); 
                    combinedStrings.push_back('=');
                    combinedStrings.insert(combinedStrings.end(), object.second.cbegin(), object.second.cend()); 
                    combinedStrings.push_back(';');
                });
            return combinedStrings;
        }
    #endif

    auto      ResolvedTechniqueInterfaceShaders::Entry::FindVariation(	
		const TechniqueEntry& techEntry,
		const ParameterBox* globalState[ShaderSelectors::Source::Max],
		const TechniqueInterface& techniqueInterface) const -> ResolvedShader
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
			for (unsigned c = 0; c < ShaderSelectors::Source::Max; ++c) {
				inputHash ^= globalState[c]->GetParameterNamesHash();
				inputHash ^= globalState[c]->GetHash() << (c * 6);    // we have to be careful of cases where the values in one box is very similar to the values in another
			}
		} else {
			inputHash = HashCombine(globalState[0]->GetHash(), globalState[0]->GetParameterNamesHash());
			for (unsigned c = 1; c < ShaderSelectors::Source::Max; ++c) {
				inputHash = HashCombine(globalState[c]->GetParameterNamesHash(), inputHash);
				inputHash = HashCombine(globalState[c]->GetHash(), inputHash);
			}
		}
        
        uint64 globalHashWithInterface = inputHash ^ techniqueInterface.GetHashValue();
        auto i = std::lower_bound(_globalToResolved.begin(), _globalToResolved.end(), globalHashWithInterface, CompareFirst<uint64, ResolvedShader>());
        if (i!=_globalToResolved.cend() && i->first == globalHashWithInterface) {
            if (i->second._shaderProgram && (i->second._shaderProgram->GetDependencyValidation()->GetValidationIndex()!=0)) {
                ResolveAndBind(i->second, techEntry, globalState, techniqueInterface);
            }

            #if defined(CHECK_TECHNIQUE_HASH_CONFLICTS)
                auto ti = std::lower_bound(_globalToResolvedTest.begin(), _globalToResolvedTest.end(), globalHashWithInterface, CompareFirst<uint64, HashConflictTest>());
                assert(ti!=_globalToResolvedTest.cend() && ti->first == globalHashWithInterface);
                TestHashConflict(globalState, ti->second);

                OutputDebugString((BuildParamsAsString(techEntry._baseSelectors, ti->second._globalState) + "\r\n").c_str());
            #endif
            return i->second;
        }

        uint64 filteredHashValue = techEntry._baseSelectors.CalculateFilteredHash(inputHash, globalState);
        uint64 filteredHashWithInterface = filteredHashValue ^ techniqueInterface.GetHashValue();
        auto i2 = std::lower_bound(_filteredToResolved.begin(), _filteredToResolved.end(), filteredHashWithInterface, CompareFirst<uint64, ResolvedShader>());
        if (i2!=_filteredToResolved.cend() && i2->first == filteredHashWithInterface) {
            _globalToResolved.insert(i, std::make_pair(globalHashWithInterface, i2->second));
            if (i2->second._shaderProgram && (i2->second._shaderProgram->GetDependencyValidation()->GetValidationIndex()!=0)) {
                ResolveAndBind(i2->second, techEntry, globalState, techniqueInterface);
            }

            #if defined(CHECK_TECHNIQUE_HASH_CONFLICTS)
                auto lti = std::lower_bound(_localToResolvedTest.begin(), _localToResolvedTest.end(), filteredHashWithInterface, CompareFirst<uint64, HashConflictTest>());
                assert(lti!=_localToResolvedTest.cend() && lti->first == filteredHashWithInterface);
				TestHashConflict(globalState, lti->second);

                auto gti = std::lower_bound(_globalToResolvedTest.begin(), _globalToResolvedTest.end(), globalHashWithInterface, CompareFirst<uint64, HashConflictTest>());
                _globalToResolvedTest.insert(gti, std::make_pair(globalHashWithInterface, HashConflictTest(lti->second._globalState, inputHash, filteredHashValue, techniqueInterface.GetHashValue())));

                OutputDebugString((BuildParamsAsString(techEntry._baseSelectors, lti->second._globalState) + "\r\n").c_str());
            #endif
            return i2->second;
        }

        ResolvedShader newResolvedShader;
        newResolvedShader._variationHash = filteredHashValue;
        ResolveAndBind(newResolvedShader, techEntry, globalState, techniqueInterface);
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

    void        ResolvedTechniqueInterfaceShaders::Entry::ResolveAndBind(	ResolvedShader& resolvedShader, 
																const TechniqueEntry& techEntry,
																const ParameterBox* globalState[ShaderSelectors::Source::Max],
																const TechniqueInterface& techniqueInterface) const
    {
        std::vector<std::pair<const utf8*, std::string>> defines;
        techEntry._baseSelectors.BuildStringTable(defines);
        for (unsigned c=0; c<ShaderSelectors::Source::Max; ++c) {
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

        ::Assets::FuturePtr<Metal::ShaderProgram> shaderProgramFuture;
        std::unique_ptr<Metal::BoundUniforms> boundUniforms;
        std::unique_ptr<Metal::BoundInputLayout> boundInputLayout;

        if (techEntry._geometryShaderName.empty()) {
            shaderProgramFuture = ::Assets::MakeAsset<Metal::ShaderProgram>(
                (techEntry._vertexShaderName + vsShaderModel).c_str(), 
                (techEntry._pixelShaderName + psShaderModel).c_str(), 
                combinedStrings.c_str());
        } else {
            shaderProgramFuture = ::Assets::MakeAsset<Metal::ShaderProgram>(
                (techEntry._vertexShaderName + vsShaderModel).c_str(), 
                (techEntry._geometryShaderName + gsShaderModel).c_str(), 
                (techEntry._pixelShaderName + psShaderModel).c_str(), 
                combinedStrings.c_str());
        }

		shaderProgramFuture->StallWhilePending();
		auto shaderProgram = shaderProgramFuture->Actualize();

		UniformsStreamInterface streamInterfaces[4];
		for (unsigned c=0; c<std::min(dimof(streamInterfaces), techniqueInterface._pimpl->_uniformStreams.size()); ++c)
			streamInterfaces[c] = techniqueInterface._pimpl->_uniformStreams[c];

		boundUniforms = std::make_unique<Metal::BoundUniforms>(
			std::ref(*shaderProgram),
			Metal::PipelineLayoutConfig{},
			streamInterfaces[0],
			streamInterfaces[1],
			streamInterfaces[2],
			streamInterfaces[3]);

        boundInputLayout = std::make_unique<Metal::BoundInputLayout>(
			MakeIteratorRange(techniqueInterface._pimpl->_vertexInputLayout), std::ref(*shaderProgram));

        resolvedShader._shaderProgram = shaderProgram.get();
        resolvedShader._boundUniforms = boundUniforms.get();
        resolvedShader._boundLayout = boundInputLayout.get();
        _resolvedShaderPrograms.push_back(std::move(shaderProgram));
        _resolvedBoundUniforms.push_back(std::move(boundUniforms));
        _resolvedBoundInputLayouts.push_back(std::move(boundInputLayout));
    }

	auto ResolvedTechniqueInterfaceShaders::FindVariation(  
		int techniqueIndex, 
        const ParameterBox* globalState[ShaderSelectors::Source::Max],
        const TechniqueInterface& techniqueInterface) const -> ResolvedShader
    {
		const auto& techEntry = _technique->GetEntry(techniqueIndex);
        if (techniqueIndex >= dimof(_entries) || !techEntry.IsValid())
            return ResolvedShader();
        return _entries[techniqueIndex].FindVariation(techEntry, globalState, techniqueInterface);
    }

	void ResolvedTechniqueInterfaceShaders::ResolvedShader::Apply(
        Metal::DeviceContext& devContext,
        ParsingContext& parserContext,
		const std::initializer_list<VertexBufferView>& vbs) const
    {
		_boundUniforms->Apply(devContext, 0, parserContext.GetGlobalUniformsStream());
		_boundLayout->Apply(devContext, MakeIteratorRange(vbs.begin(), vbs.end()));
        devContext.Bind(*_shaderProgram);
    }

	void ResolvedTechniqueInterfaceShaders::ResolvedShader::ApplyUniforms(
		Metal::DeviceContext& devContext,
		unsigned streamIdx,
		const UniformsStream& stream) const
	{
		_boundUniforms->Apply(devContext, streamIdx, stream);
	}

    ResolvedTechniqueInterfaceShaders::ResolvedShader::ResolvedShader()
    {
        _variationHash = 0;
        _shaderProgram = nullptr;
        _boundUniforms = nullptr;
        _boundLayout = nullptr;
    }

	ResolvedTechniqueInterfaceShaders::ResolvedTechniqueInterfaceShaders(const std::shared_ptr<Technique>& technique)
	: _technique(technique)
	{
	}
	ResolvedTechniqueInterfaceShaders::~ResolvedTechniqueInterfaceShaders(){}

	const ::Assets::DepValPtr& ResolvedTechniqueInterfaceShaders::GetDependencyValidation()
	{
		return _technique->GetDependencyValidation();
	}

	void ResolvedTechniqueInterfaceShaders::ConstructToFuture(
		::Assets::AssetFuture<ResolvedTechniqueInterfaceShaders>& future,
		StringSection<::Assets::ResChar> modelScaffoldName)
	{
		auto scaffoldFuture = ::Assets::MakeAsset<Technique>(modelScaffoldName);

		future.SetPollingFunction(
			[scaffoldFuture](::Assets::AssetFuture<ResolvedTechniqueInterfaceShaders>& thatFuture) -> bool {

			auto scaffoldActual = scaffoldFuture->TryActualize();

			if (!scaffoldActual) {
				auto state = scaffoldFuture->GetAssetState();
				if (state == ::Assets::AssetState::Invalid) {
					thatFuture.SetInvalidAsset(scaffoldFuture->GetDependencyValidation(), nullptr);
					return false;
				}
				return true;
			}

			auto newModel = std::make_shared<ResolvedTechniqueInterfaceShaders>(scaffoldActual);
			thatFuture.SetAsset(std::move(newModel), {});
			return false;
		});
	}
}}