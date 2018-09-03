// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SharedStateSet.h"
#include "../Types.h"
#include "../Techniques/Techniques.h"
#include "../Techniques/CommonResources.h"
#include "../Techniques/ParsingContext.h"
#include "../Techniques/CommonBindings.h"
#include "../Techniques/RenderStateResolver.h"
#include "../Techniques/CompiledRenderStateSet.h"
#include "../Techniques/PredefinedCBLayout.h"
#include "../Techniques/ResolvedTechniqueShaders.h"
#include "../Metal/InputLayout.h"
#include "../Metal/DeviceContext.h"
#include "../Metal/Buffer.h"
#include "../Metal/State.h"
#include "../../ConsoleRig/ResourceBox.h"
#include "../../Assets/Assets.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/ParameterBox.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/StringFormat.h"
#include <vector>

namespace RenderCore { namespace Assets
{
    class SharedStateSet::Pimpl
    {
    public:
        using TechInterface = Techniques::TechniqueInterface;
        std::vector<uint64>                 _rawTechniqueConfigs;
        std::vector<::Assets::rstring>      _resolvedTechniqueConfigs;
        
        std::vector<TechInterface>  _techniqueInterfaces;
        std::vector<ParameterBox>   _parameterBoxes;
        std::vector<uint64>         _techniqueInterfaceHashes;

        std::vector<Techniques::RenderStateSet> _renderStateSets;
        std::vector<uint64>         _renderStateHashes;

        std::vector<std::pair<uint64, Techniques::CompiledRenderStateSet>>  _compiledStates;

        Metal::DeviceContext* _capturedContext;
        uint64 _currentGlobalRenderState;
        std::shared_ptr<Techniques::IStateSetResolver> _currentStateResolver;
        std::shared_ptr<ParameterBox> _environment;

        ::Assets::DirectorySearchRules _shaderSearchDirs;
    };

    static uint64 Hash(const InputElementDesc& desc)
    {
            //  hash the semantic name and the scalar parameters
            //  Note that sometimes there might be a chance of equivalent
            //  elements producing different hash values
            //      (for example, _alignedByteOffset can be ~unsigned(0x0) to
            //      choose the default offset -- so if another desc explicitly
            //      sets the offset, the values will be different)
        auto t = Hash64(desc._semanticName);
        t ^= Hash64(&desc._semanticIndex, PtrAdd(&desc._instanceDataStepRate, sizeof(unsigned)));
        return t;
    }

    SharedTechniqueInterface SharedStateSet::InsertTechniqueInterface(
        const RenderCore::InputElementDesc vertexElements[], unsigned count,
        const uint64 textureBindPoints[], unsigned textureBindPointsCount)
    {
        uint64 interfHash = 0;
        for (unsigned e=0; e<count; ++e) {
            interfHash ^= Hash(vertexElements[e]);
        }
        for (unsigned e=0; e<textureBindPointsCount; ++e) {
            interfHash ^= textureBindPoints[e];
        }

        unsigned techniqueInterfaceIndex = 0;
        auto& hashes = _pimpl->_techniqueInterfaceHashes;
        auto& interfaces = _pimpl->_techniqueInterfaces;

        auto existingInterface = std::find(hashes.cbegin(), hashes.cend(), interfHash);
        if (existingInterface == hashes.cend()) {
                //  No existing interface. We have to build a new one.
            Techniques::TechniqueInterface techniqueInterface(
                MakeIteratorRange(vertexElements, &vertexElements[count]));

			techniqueInterface.BindUniformsStream(0, Techniques::TechniqueContext::GetGlobalUniformsStreamInterface());

			{
				UniformsStreamInterface matInterf;
				matInterf.BindConstantBuffer(0, { Techniques::ObjectCB::BasicMaterialConstants });
				for (unsigned c=0; c<textureBindPointsCount; ++c)
					matInterf.BindShaderResource(c, textureBindPoints[c]);
				techniqueInterface.BindUniformsStream(1, matInterf);
			}

			{
				UniformsStreamInterface objInterf;
				objInterf.BindConstantBuffer(0, { Techniques::ObjectCB::LocalTransform });
				techniqueInterface.BindUniformsStream(2, objInterf);
			}
                
            interfaces.push_back(std::move(techniqueInterface));
            hashes.push_back(interfHash);
            techniqueInterfaceIndex = unsigned(interfaces.size()-1);
        } else {
            techniqueInterfaceIndex = unsigned(existingInterface - hashes.cbegin());
        }

        return techniqueInterfaceIndex;
    }

    SharedTechniqueConfig SharedStateSet::InsertTechniqueConfig(StringSection<::Assets::ResChar> shaderName)
    {
        auto hash = Hash64(shaderName.begin(), shaderName.end());
        auto& rawShaderNames = _pimpl->_rawTechniqueConfigs;
        auto n = std::find(rawShaderNames.begin(), rawShaderNames.end(), hash);
        if (n == rawShaderNames.end()) {
                // We must also resolve the full filename for this shader.
                // Shaders are referenced relative to a few fixed directories.
            ::Assets::ResChar resName[MaxPath];
            XlCopyString(resName, shaderName);
            XlCatString(resName, ".tech");
            _pimpl->_shaderSearchDirs.ResolveFile(resName, dimof(resName), resName);
            
            rawShaderNames.push_back(hash);
            _pimpl->_resolvedTechniqueConfigs.push_back(resName);
            return unsigned(rawShaderNames.size()-1);
        } else {
            return unsigned(std::distance(rawShaderNames.begin(), n));
        }
    }

    SharedParameterBox SharedStateSet::InsertParameterBox(const ParameterBox& box)
    {
        auto& paramBoxes = _pimpl->_parameterBoxes;
        auto boxHash = box.GetHash();
        auto namesHash = box.GetParameterNamesHash();
        auto p = std::find_if(
            paramBoxes.cbegin(), paramBoxes.cend(), 
            [=](const ParameterBox& box) 
            { 
                return box.GetHash() == boxHash && box.GetParameterNamesHash() == namesHash; 
            });
        if (p == paramBoxes.cend()) {
            paramBoxes.push_back(box);
            return unsigned(paramBoxes.size()-1);
        } else {
            return unsigned(std::distance(paramBoxes.cbegin(), p));
        }
    }

    unsigned SharedStateSet::InsertRenderStateSet(const Techniques::RenderStateSet& states)
    {
        //  The RenderStateSet has parameters that influence the BlendState 
        //  and RasteriserState low level graphics API objects.
        //  Here, "states" is usually a set of states associated with a draw
        //  call in a model.
        //
        //  They aren't the only influences, however. Sometimes higher level
        //  settings will need to affect the final states. So the state objects
        //  we choose should be a combination of the global state settings, and 
        //  these local states tied to model draw calls.
        //
        //  (so, for example, the depth bias might have a global value attached
        //  while rendering the shadows texture. But it might also have some 
        //  global depth bias setting. So the final low level states should be
        //  a combination of the global state settings and these local states)

        auto hash = states.GetHash();
        auto i = std::find(_pimpl->_renderStateHashes.begin(), _pimpl->_renderStateHashes.end(), hash);
        if (i != _pimpl->_renderStateHashes.end())
            return unsigned(std::distance(_pimpl->_renderStateHashes.begin(), i));

        _pimpl->_renderStateSets.push_back(states);
        _pimpl->_renderStateHashes.push_back(hash);
        return unsigned(_pimpl->_renderStateSets.size()-1);
    }

	auto SharedStateSet::BeginVariation(
        const ModelRendererContext& context,
        SharedTechniqueConfig shaderName, SharedTechniqueInterface techniqueInterface, 
        SharedParameterBox geoParamBox, SharedParameterBox materialParamBox) const
		-> BoundVariation
    {
        if (    shaderName == _currentShaderName && techniqueInterface == _currentTechniqueInterface 
            &&  geoParamBox == _currentGeoParamBox && materialParamBox == _currentMaterialParamBox) {
			return { _currentBoundUniforms, _currentBoundLayout };
        }

        auto& techniqueContext = context._parserContext->GetTechniqueContext();
        const auto& sn = _pimpl->_resolvedTechniqueConfigs[shaderName.Value()];
        auto& shaderType = ::Assets::GetAssetDep<Techniques::ResolvedTechniqueShaders>(MakeStringSection(sn));
        const ParameterBox* state[] = {
            &_pimpl->_parameterBoxes[geoParamBox.Value()],
            &techniqueContext._globalEnvironmentState,
            &techniqueContext._runtimeState,
            &_pimpl->_parameterBoxes[materialParamBox.Value()]
        };

        auto& techniqueInterfaceObj = _pimpl->_techniqueInterfaces[techniqueInterface.Value()];

            // (FindVariation can throw pending/invalid resource)
        auto variation = shaderType.FindVariation(context._techniqueIndex, state, techniqueInterfaceObj);
        if (!variation._shaderProgram || !variation._boundLayout)
            return {};

        context._context->Bind(*variation._shaderProgram);
        _currentShaderName = shaderName;
        _currentTechniqueInterface = techniqueInterface;
        _currentMaterialParamBox = materialParamBox;
        _currentGeoParamBox = geoParamBox;
        _currentBoundUniforms = variation._boundUniforms;
		_currentBoundLayout = variation._boundLayout;
		return { _currentBoundUniforms, _currentBoundLayout };
    }

    void SharedStateSet::BeginRenderState(
        const ModelRendererContext& context,
        SharedRenderStateSet renderStateSetIndex) const
    {
        assert(_pimpl->_capturedContext == context._context);

        if (_currentRenderState == renderStateSetIndex) { return; }

        const Techniques::CompiledRenderStateSet* compiled = nullptr;
        auto statesHash = _pimpl->_renderStateHashes[renderStateSetIndex.Value()];

        auto hash = HashCombine(statesHash, _pimpl->_currentGlobalRenderState);
        auto i = LowerBound(_pimpl->_compiledStates, hash);
        if (i != _pimpl->_compiledStates.end() && i->first == hash) {
            compiled = &i->second;
        } else {
            const auto& states = _pimpl->_renderStateSets[renderStateSetIndex.Value()];
            auto newlyCompiled = _pimpl->_currentStateResolver->Resolve(
                states, *_pimpl->_environment, context._techniqueIndex);
            compiled = &_pimpl->_compiledStates.insert(
                i, std::make_pair(hash, std::move(newlyCompiled)))->second;
        }
        
        assert(compiled);
        if (compiled->_blendState.GetUnderlying())
            context._context->Bind(compiled->_blendState);
        context._context->Bind(compiled->_rasterizerState);
        
        _currentRenderState = renderStateSetIndex;
    }

    const Techniques::PredefinedCBLayout* SharedStateSet::GetCBLayout(SharedTechniqueConfig shaderName)
    {
        // If the technique config has an embedded cblayout, we must return that.
        // Otherwise, we return the default
        const auto& sn = _pimpl->_resolvedTechniqueConfigs[shaderName.Value()];
        auto& shaderType = ::Assets::GetAssetDep<Techniques::ResolvedTechniqueShaders>(MakeStringSection(sn));
		return &shaderType.GetTechnique().TechniqueCBLayout();
    }

    auto SharedStateSet::CaptureState(
        IThreadContext& context,
        std::shared_ptr<Techniques::IStateSetResolver> stateResolver,
        std::shared_ptr<Utility::ParameterBox> environment) -> CaptureMarker
    {
        return CaptureState(*Metal::DeviceContext::Get(context), stateResolver, environment);
    }

    auto SharedStateSet::CaptureState(
        Metal::DeviceContext& metalContext,
        std::shared_ptr<Techniques::IStateSetResolver> stateResolver,
        std::shared_ptr<Utility::ParameterBox> environment) -> CaptureMarker
    {
        assert(!_pimpl->_capturedContext);
        _currentShaderName = SharedTechniqueConfig::Invalid;
        _currentTechniqueInterface = SharedTechniqueInterface::Invalid;
        _currentMaterialParamBox = SharedParameterBox::Invalid;
        _currentGeoParamBox = SharedParameterBox::Invalid;
        _currentRenderState = SharedRenderStateSet::Invalid;
        _currentBoundUniforms = nullptr;
		_currentBoundLayout = nullptr;

        _pimpl->_capturedContext = &metalContext;
        _pimpl->_currentGlobalRenderState = stateResolver->GetHash();
        if (environment) {
            _pimpl->_currentGlobalRenderState = HashCombine(
                _pimpl->_currentGlobalRenderState,
                HashCombine(environment->GetHash(), environment->GetParameterNamesHash()));
        }

        _pimpl->_currentStateResolver = std::move(stateResolver);
        _pimpl->_environment = std::move(environment);

        return CaptureMarker(metalContext, *this);
    }

    void SharedStateSet::ReleaseState(Metal::DeviceContext& context)
    {
        assert(_pimpl->_capturedContext==&context);
        _pimpl->_capturedContext = nullptr;
        _pimpl->_currentStateResolver.reset();
        _pimpl->_environment.reset();
    }


    SharedStateSet::SharedStateSet(const ::Assets::DirectorySearchRules& shaderSearchDir)
    {
        auto pimpl = std::make_unique<Pimpl>();

        _currentShaderName = SharedTechniqueConfig::Invalid;
        _currentTechniqueInterface = SharedTechniqueInterface::Invalid;
        _currentMaterialParamBox = SharedParameterBox::Invalid;
        _currentGeoParamBox = SharedParameterBox::Invalid;
        _currentRenderState = SharedRenderStateSet::Invalid;
        _currentBoundUniforms = nullptr;
		_currentBoundUniforms = nullptr;

        pimpl->_currentGlobalRenderState = SharedRenderStateSet::Invalid;
        pimpl->_capturedContext = nullptr;
        pimpl->_shaderSearchDirs = shaderSearchDir;
        _pimpl = std::move(pimpl);
    }

    SharedStateSet::~SharedStateSet()
    {
        assert(!_pimpl->_capturedContext);
    }

    ModelRendererContext::ModelRendererContext(
        IThreadContext& context, 
        Techniques::ParsingContext& parserContext,
        unsigned techniqueIndex)
    : ModelRendererContext(*Metal::DeviceContext::Get(context), parserContext, techniqueIndex)
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////
    
    SharedStateSet::CaptureMarker::CaptureMarker(Metal::DeviceContext& metalContext, SharedStateSet& state)
    : _state(&state), _metalContext(&metalContext)
    {
    }

    SharedStateSet::CaptureMarker::CaptureMarker() : _state(nullptr), _metalContext(nullptr) {}

    SharedStateSet::CaptureMarker::~CaptureMarker()
    {
        if (_state && _metalContext)
            _state->ReleaseState(*_metalContext);
    }

    SharedStateSet::CaptureMarker::CaptureMarker(CaptureMarker&& moveFrom) never_throws
    {
        _state = moveFrom._state; moveFrom._state = nullptr;
        _metalContext = moveFrom._metalContext; moveFrom._metalContext = nullptr;
    }
            
    auto SharedStateSet::CaptureMarker::operator=(CaptureMarker&& moveFrom) never_throws -> CaptureMarker&
    {
        if (_state && _metalContext)
            _state->ReleaseState(*_metalContext);

        _state = moveFrom._state; moveFrom._state = nullptr;
        _metalContext = moveFrom._metalContext; moveFrom._metalContext = nullptr;
        return *this;
    }

}}

