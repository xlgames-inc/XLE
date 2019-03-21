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

        uint64_t _hashValue;
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

    uint64_t  TechniqueInterface::GetHashValue() const
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
		
	ResolvedTechniqueShaders::ResolvedShader ResolvedTechniqueShaders::Entry::AsResolvedShader(uint64_t hash, const BoundShader& shader)
	{
		return ResolvedShader {
			hash,
			shader._shaderProgram.get(),
			shader._boundUniforms.get(),
			shader._boundLayout.get() };
	}

    static ::Assets::FuturePtr<Metal::ShaderProgram> GetShaderVariation(
		StringSection<> vsName,
		StringSection<> gsName,
		StringSection<> psName,
		StringSection<> defines)
    {
        if (gsName.IsEmpty()) {
            return ::Assets::MakeAsset<Metal::ShaderProgram>(vsName, psName, defines);
        } else {
            return ::Assets::MakeAsset<Metal::ShaderProgram>(vsName, gsName, psName, defines);
        }
	}

	static uint64_t Hash64(const ParameterBox* shaderSelectors[ShaderSelectors::Source::Max])
	{
		uint64_t inputHash = 0;
		const bool simpleHash = false;
		if (constant_expression<simpleHash>::result()) {
			for (unsigned c = 0; c < ShaderSelectors::Source::Max; ++c) {
				inputHash ^= shaderSelectors[c]->GetParameterNamesHash();
				inputHash ^= shaderSelectors[c]->GetHash() << (c * 6);    // we have to be careful of cases where the values in one box is very similar to the values in another
			}
		} else {
			inputHash = HashCombine(shaderSelectors[0]->GetHash(), shaderSelectors[0]->GetParameterNamesHash());
			for (unsigned c = 1; c < ShaderSelectors::Source::Max; ++c) {
				inputHash = HashCombine(shaderSelectors[c]->GetParameterNamesHash(), inputHash);
				inputHash = HashCombine(shaderSelectors[c]->GetHash(), inputHash);
			}
		}
		return inputHash;
	}

	const ::Assets::FuturePtr<Metal::ShaderProgram>& ResolvedShaderVariationSet::FindVariation(
		const TechniqueEntry& techEntry,
		const ParameterBox* shaderSelectors[ShaderSelectors::Source::Max]) const
	{
		auto inputHash = Hash64(shaderSelectors);
        
		uint64_t filteredHashValue;
        auto i = LowerBound(_globalToFiltered, inputHash);
        if (i!=_globalToFiltered.cend() && i->first == inputHash) {
            filteredHashValue = i->second;
        } else {
			filteredHashValue = techEntry._baseSelectors.CalculateFilteredHash(inputHash, shaderSelectors);
			_globalToFiltered.insert(i, {inputHash, filteredHashValue});
		}

		auto i3 = LowerBound(_filteredToResolved, filteredHashValue);
        if (i3!=_filteredToResolved.cend() && i3->first == filteredHashValue) {
			if (i3->second->GetDependencyValidation() && i3->second->GetDependencyValidation()->GetValidationIndex()!=0)
				i3->second = MakeShaderVariation(techEntry, shaderSelectors);;
        } else {
			auto newVariation = MakeShaderVariation(techEntry, shaderSelectors);
			i3 = _filteredToResolved.insert(i3, {filteredHashValue, newVariation});
		}

		return i3->second;
	}

	auto ResolvedShaderVariationSet::MakeShaderVariation(
		const TechniqueEntry& techEntry,
		const ParameterBox* shaderSelectors[ShaderSelectors::Source::Max]) const -> ShaderFuture
	{
		std::vector<std::pair<const utf8*, std::string>> defines;
		techEntry._baseSelectors.BuildStringTable(defines);
		for (unsigned c=0; c<ShaderSelectors::Source::Max; ++c) {
			OverrideStringTable(defines, *shaderSelectors[c]);
		}

		auto combinedStrings = FlattenStringTable(defines);

		std::string vs, ps, gs;
		auto vsi = std::lower_bound(defines.cbegin(), defines.cend(), (const utf8*)"vs_", CompareFirst<const utf8*, std::string>());
		if (vsi != defines.cend() && !XlCompareString(vsi->first, (const utf8*)"vs_")) {
			char buffer[32];
			int integerValue = Utility::XlAtoI32(vsi->second.c_str());
			sprintf_s(buffer, dimof(buffer), ":vs_%i_%i", integerValue/10, integerValue%10);
			vs = techEntry._vertexShaderName + buffer;
		} else {
			vs = techEntry._vertexShaderName + ":" VS_DefShaderModel;
		}
		auto psi = std::lower_bound(defines.cbegin(), defines.cend(), (const utf8*)"ps_", CompareFirst<const utf8*, std::string>());
		if (psi != defines.cend() && !XlCompareString(psi->first, (const utf8*)"ps_")) {
			char buffer[32];
			int integerValue = Utility::XlAtoI32(psi->second.c_str());
			sprintf_s(buffer, dimof(buffer), ":ps_%i_%i", integerValue/10, integerValue%10);
			ps = techEntry._pixelShaderName + buffer;
		} else {
			ps = techEntry._pixelShaderName + ":" PS_DefShaderModel;
		}
		if (!techEntry._geometryShaderName.empty()) {
			auto gsi = std::lower_bound(defines.cbegin(), defines.cend(), (const utf8*)"gs_", CompareFirst<const utf8*, std::string>());
			if (gsi != defines.cend() && !XlCompareString(gsi->first, (const utf8*)"gs_")) {
				char buffer[32];
				int integerValue = Utility::XlAtoI32(psi->second.c_str());
				sprintf_s(buffer, dimof(buffer), ":gs_%i_%i", integerValue/10, integerValue%10);
				gs = techEntry._geometryShaderName + buffer;
			} else {
				gs = techEntry._geometryShaderName + ":" GS_DefShaderModel;
			}
		}

		return _creationFn(MakeStringSection(vs), MakeStringSection(gs), MakeStringSection(ps), MakeStringSection(combinedStrings));
	}

	ResolvedShaderVariationSet::ResolvedShaderVariationSet() 
	{
		_creationFn = GetShaderVariation;
	}
	ResolvedShaderVariationSet::~ResolvedShaderVariationSet() {}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    auto      ResolvedTechniqueShaders::Entry::FindResolvedShaderVariation(	
		const TechniqueEntry& techEntry,
		const ParameterBox* shaderSelectors[ShaderSelectors::Source::Max],
		const TechniqueInterface& techniqueInterface) const -> ResolvedShader
    {
        auto inputHash = Hash64(shaderSelectors);
        
		uint64_t filteredHashValue;
        auto i = LowerBound(_globalToFiltered, inputHash);
        if (i!=_globalToFiltered.cend() && i->first == inputHash) {
            filteredHashValue = i->second;
        } else {
			filteredHashValue = techEntry._baseSelectors.CalculateFilteredHash(inputHash, shaderSelectors);
			_globalToFiltered.insert(i, {inputHash, filteredHashValue});
		}

        uint64_t filteredHashWithInterface = filteredHashValue ^ techniqueInterface.GetHashValue();
        auto i2 = LowerBound(_filteredToBoundShader, filteredHashWithInterface);
        if (i2!=_filteredToBoundShader.cend() && i2->first == filteredHashWithInterface) {
			assert(i2->second._shaderProgram);
			// We can return now, only if it's not invalidated
            if (i2->second._shaderProgram->GetDependencyValidation()->GetValidationIndex()==0) {
                return Entry::AsResolvedShader(filteredHashValue, i2->second);
            }
			i2 = _filteredToBoundShader.erase(i2);
        }

		auto i3 = LowerBound(_filteredToResolved, filteredHashValue);
        if (i3!=_filteredToResolved.cend() && i3->first == filteredHashValue) {
			if (i3->second->GetDependencyValidation() && i3->second->GetDependencyValidation()->GetValidationIndex()!=0)
				i3->second = MakeShaderVariation(techEntry, shaderSelectors);
        } else {
			auto newVariation = MakeShaderVariation(techEntry, shaderSelectors);
			i3 = _filteredToResolved.insert(i3, {filteredHashValue, newVariation});
		}

		auto actual = i3->second->TryActualize();

		if (!actual)	// invalid or still pending
			return {};

        auto newBoundShader = MakeBoundShader(actual, techEntry, shaderSelectors, techniqueInterface);
		i2 = _filteredToBoundShader.insert(i2, {filteredHashWithInterface, std::move(newBoundShader)});
        return Entry::AsResolvedShader(filteredHashValue, i2->second);
    }

	auto ResolvedTechniqueShaders::Entry::MakeBoundShader(	
		const std::shared_ptr<Metal::ShaderProgram>& shader, 
		const TechniqueEntry& techEntry,
		const ParameterBox* globalState[ShaderSelectors::Source::Max],
		const TechniqueInterface& techniqueInterface) -> BoundShader
    {
		BoundShader result;
		result._shaderProgram = shader;
		// auto shaderProgramFuture = GetShaderVariation(techEntry, globalState);
		// shaderProgramFuture->StallWhilePending();
		// resolvedShader._shaderProgram = shaderProgramFuture->Actualize();

		UniformsStreamInterface streamInterfaces[4];
		for (unsigned c=0; c<std::min(dimof(streamInterfaces), techniqueInterface._pimpl->_uniformStreams.size()); ++c)
			streamInterfaces[c] = techniqueInterface._pimpl->_uniformStreams[c];

		result._boundUniforms = std::make_unique<Metal::BoundUniforms>(
			std::ref(*shader),
			Metal::PipelineLayoutConfig{},
			streamInterfaces[0],
			streamInterfaces[1],
			streamInterfaces[2],
			streamInterfaces[3]);

        result._boundLayout = std::make_unique<Metal::BoundInputLayout>(
			MakeIteratorRange(techniqueInterface._pimpl->_vertexInputLayout), std::ref(*shader));
		return result;
    }

	auto ResolvedTechniqueShaders::FindVariation(  
		int techniqueIndex, 
        const ParameterBox* shaderSelectors[ShaderSelectors::Source::Max],
        const TechniqueInterface& techniqueInterface) const -> ResolvedShader
    {
		const auto& techEntry = _technique->GetEntry(techniqueIndex);
        if (techniqueIndex >= dimof(_entries) || !techEntry.IsValid())
            return ResolvedShader();
        return _entries[techniqueIndex].FindResolvedShaderVariation(techEntry, shaderSelectors, techniqueInterface);
    }

	::Assets::FuturePtr<Metal::ShaderProgram> ResolvedTechniqueShaders::FindVariation(
		int techniqueIndex,
		const ParameterBox* shaderSelectors[ShaderSelectors::Source::Max]) const
	{
		const auto& techEntry = _technique->GetEntry(techniqueIndex);
        if (techniqueIndex >= dimof(_entries) || !techEntry.IsValid())
			return {};
        return _entries[techniqueIndex].FindVariation(techEntry, shaderSelectors);
	}

	void ResolvedTechniqueShaders::ResolvedShader::Apply(
        Metal::DeviceContext& devContext,
        ParsingContext& parserContext,
		const std::initializer_list<VertexBufferView>& vbs) const
    {
		_boundUniforms->Apply(devContext, 0, parserContext.GetGlobalUniformsStream());
		_boundLayout->Apply(devContext, MakeIteratorRange(vbs.begin(), vbs.end()));
        _shaderProgram->Apply(devContext);
    }

	void ResolvedTechniqueShaders::ResolvedShader::ApplyUniforms(
		Metal::DeviceContext& devContext,
		unsigned streamIdx,
		const UniformsStream& stream) const
	{
		_boundUniforms->Apply(devContext, streamIdx, stream);
	}

	ResolvedTechniqueShaders::ResolvedTechniqueShaders(const std::shared_ptr<Technique>& technique)
	: _technique(technique)
	{
	}
	ResolvedTechniqueShaders::~ResolvedTechniqueShaders(){}

	const ::Assets::DepValPtr& ResolvedTechniqueShaders::GetDependencyValidation()
	{
		return _technique->GetDependencyValidation();
	}

	void ResolvedTechniqueShaders::ConstructToFuture(
		::Assets::AssetFuture<ResolvedTechniqueShaders>& future,
		StringSection<::Assets::ResChar> modelScaffoldName)
	{
		auto scaffoldFuture = ::Assets::MakeAsset<Technique>(modelScaffoldName);

		future.SetPollingFunction(
			[scaffoldFuture](::Assets::AssetFuture<ResolvedTechniqueShaders>& thatFuture) -> bool {

			auto scaffoldActual = scaffoldFuture->TryActualize();

			if (!scaffoldActual) {
				auto state = scaffoldFuture->GetAssetState();
				if (state == ::Assets::AssetState::Invalid) {
					thatFuture.SetInvalidAsset(scaffoldFuture->GetDependencyValidation(), nullptr);
					return false;
				}
				return true;
			}

			auto newModel = std::make_shared<ResolvedTechniqueShaders>(scaffoldActual);
			thatFuture.SetAsset(std::move(newModel), {});
			return false;
		});
	}
}}