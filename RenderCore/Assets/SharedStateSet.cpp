// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SharedStateSet.h"
#include "Material.h"
#include "../Techniques/Techniques.h"
#include "../Techniques/CommonResources.h"
#include "../Techniques/ParsingContext.h"
#include "../Metal/InputLayout.h"
#include "../Metal/DeviceContext.h"
#include "../Metal/Buffer.h"
#include "../Metal/State.h"
#include "../Techniques/ResourceBox.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/ParameterBox.h"
#include <vector>

namespace RenderCore { namespace Assets
{
    class SharedStateSet::Pimpl
    {
    public:
        std::vector<std::string>                        _shaderNames;
        std::vector<Techniques::TechniqueInterface>     _techniqueInterfaces;
        std::vector<ParameterBox>                       _parameterBoxes;
        std::vector<uint64>                             _techniqueInterfaceHashes;
        std::vector<std::pair<uint64, RenderStateSet>>  _renderStateSets;

        Metal::DeviceContext* _capturedContext;
    };

    static uint64 Hash(const Metal::InputElementDesc& desc)
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

    unsigned SharedStateSet::InsertTechniqueInterface(
        const RenderCore::Metal::InputElementDesc vertexElements[], unsigned count,
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
                Metal::InputLayout(vertexElements, count));

            static const auto HashLocalTransform = Hash64("LocalTransform");
            static const auto HashBasicMaterial = Hash64("BasicMaterialConstants");
            techniqueInterface.BindConstantBuffer(HashLocalTransform, 0, 1);
            techniqueInterface.BindConstantBuffer(HashBasicMaterial, 1, 1);
            Techniques::TechniqueContext::BindGlobalUniforms(techniqueInterface);
            for (unsigned c=0; c<textureBindPointsCount; ++c) {
                techniqueInterface.BindShaderResource(textureBindPoints[c], c, 1);
            }
                
            interfaces.push_back(std::move(techniqueInterface));
            hashes.push_back(interfHash);
            techniqueInterfaceIndex = unsigned(interfaces.size()-1);
        } else {
            techniqueInterfaceIndex = unsigned(existingInterface - hashes.cbegin());
        }

        return techniqueInterfaceIndex;
    }

    unsigned SharedStateSet::InsertShaderName(const std::string& shaderName)
    {
        auto& shaderNames = _pimpl->_shaderNames;
        auto n = std::find(shaderNames.cbegin(), shaderNames.cend(), shaderName);
        if (n == shaderNames.cend()) {
            shaderNames.push_back(shaderName);
            return unsigned(shaderNames.size()-1);
        } else {
            return unsigned(std::distance(shaderNames.cbegin(), n));
        }
    }

    unsigned SharedStateSet::InsertParameterBox(const ParameterBox& box)
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

    unsigned SharedStateSet::InsertRenderStateSet(const RenderStateSet& states)
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
        for (auto i=_pimpl->_renderStateSets.begin(); i!=_pimpl->_renderStateSets.end(); ++i) {
            if (i->first == hash) {
                return unsigned(std::distance(_pimpl->_renderStateSets.begin(), i));
            }
        }

        _pimpl->_renderStateSets.push_back(std::make_pair(hash, states));
        return unsigned(_pimpl->_renderStateSets.size()-1);
    }

    RenderCore::Metal::BoundUniforms* SharedStateSet::BeginVariation(
            const ModelRendererContext& context,
            unsigned shaderName, unsigned techniqueInterface, 
            unsigned geoParamBox, unsigned materialParamBox) const
    {
        if (    shaderName == _currentShaderName && techniqueInterface == _currentTechniqueInterface 
            &&  geoParamBox == _currentGeoParamBox && materialParamBox == _currentMaterialParamBox) {
            return _currentBoundUniforms;
        }

            // we need to check both the "xleres" and "xleres_cry" folders for material files
        char buffer[MaxPath];
        XlCopyString(buffer, "game/xleres/");
        XlCatString(buffer, dimof(buffer), _pimpl->_shaderNames[shaderName].c_str());
        XlCatString(buffer, dimof(buffer), ".txt");

        if (!DoesFileExist(buffer)) {
            XlCopyString(buffer, "game/xleres_cry/");
            XlCatString(buffer, dimof(buffer), _pimpl->_shaderNames[shaderName].c_str());
            XlCatString(buffer, dimof(buffer), ".txt");
        }

        auto& techniqueContext = context._parserContext->GetTechniqueContext();
        auto& shaderType = ::Assets::GetAssetDep<Techniques::ShaderType>(buffer);
        const ParameterBox* state[] = {
            &_pimpl->_parameterBoxes[geoParamBox],
            &techniqueContext._globalEnvironmentState,
            &techniqueContext._runtimeState,
            &_pimpl->_parameterBoxes[materialParamBox]
        };

        auto& techniqueInterfaceObj = _pimpl->_techniqueInterfaces[techniqueInterface];

            // (FindVariation can throw pending/invalid resource)
        auto variation = shaderType.FindVariation(context._techniqueIndex, state, techniqueInterfaceObj);
        if (variation._shaderProgram && variation._boundLayout) {
            context._context->Bind(*variation._shaderProgram);
            context._context->Bind(*variation._boundLayout);
        }

        _currentShaderName = shaderName;
        _currentTechniqueInterface = techniqueInterface;
        _currentMaterialParamBox = materialParamBox;
        _currentGeoParamBox = geoParamBox;
        _currentBoundUniforms = variation._boundUniforms;
        return _currentBoundUniforms;
    }

    class CompiledRenderStateSet
    {
    public:
        Metal::BlendState _blendState;
        Metal::RasterizerState _rasterizerState;

            //  We need to initialise the members in the default
            //  constructor, otherwise they'll build the new
            //  underlying state objects
        CompiledRenderStateSet()
            : _blendState(Techniques::CommonResources()._blendOpaque)
            , _rasterizerState(Techniques::CommonResources()._defaultRasterizer)
        {}
    };

    IRenderStateSetResolver::~IRenderStateSetResolver() {}

    class DefaultRenderStateSetResolver
    {
    public:
        class Desc {};

        auto Compile(
            const RenderStateSet& states, 
            const Utility::ParameterBox& globalStates,
            unsigned techniqueIndex) -> const CompiledRenderStateSet*;

        DefaultRenderStateSetResolver(const Desc&) {}
    protected:
        std::vector<std::pair<uint64, CompiledRenderStateSet>> _forwardStates;
        std::vector<std::pair<uint64, CompiledRenderStateSet>> _deferredStates;
    };

    auto DefaultRenderStateSetResolver::Compile(
        const RenderStateSet& states, 
        const Utility::ParameterBox& globalStates,
        unsigned techniqueIndex) -> const CompiledRenderStateSet*
    {
        if (techniqueIndex == 0 || techniqueIndex == 3 || techniqueIndex == 4 || techniqueIndex == 2) {
            auto hash = states.GetHash();
            auto i = LowerBound(_forwardStates, hash);
            if (i != _forwardStates.end() && i->first == hash) {
                return &i->second;
            }

            CompiledRenderStateSet result;
            if (techniqueIndex == 2) {
                bool deferredDecal = 
                    (states._flag & RenderStateSet::Flag::DeferredBlend)
                    && (states._deferredBlend == RenderStateSet::DeferredBlend::Decal);

                if (deferredDecal) {
                    result._blendState = Metal::BlendState(
                        Metal::BlendOp::Add, Metal::Blend::SrcAlpha, Metal::Blend::InvSrcAlpha, true);
                } else {
                    result._blendState = Techniques::CommonResources()._blendOpaque;
                }
            } else if (techniqueIndex == 0 || techniqueIndex == 4) {
                if (states._flag & RenderStateSet::Flag::ForwardBlend) {
                    result._blendState = Metal::BlendState(
                        states._forwardBlendOp, states._forwardBlendSrc, states._forwardBlendDst);
                } else {
                    result._blendState = Techniques::CommonResources()._blendOpaque;
                }
            }

            Metal::CullMode::Enum cullMode = Metal::CullMode::Back;
            Metal::FillMode::Enum fillMode = Metal::FillMode::Solid;
            unsigned depthBias = 0;
            if (states._flag & RenderStateSet::Flag::DoubleSided) {
                cullMode = states._doubleSided ? Metal::CullMode::None : Metal::CullMode::Back;
            }
            if (states._flag & RenderStateSet::Flag::DepthBias) {
                depthBias = states._depthBias;
            }
            if (states._flag & RenderStateSet::Flag::Wireframe) {
                fillMode = states._wireframe ? Metal::FillMode::Wireframe : Metal::FillMode::Solid;
            }

            result._rasterizerState = Metal::RasterizerState(
                cullMode, true, fillMode, depthBias, 0.f, 0.f);

            i = _forwardStates.insert(i, std::make_pair(hash, result));
            return &i->second;
        }

        return nullptr;
    }

    void SharedStateSet::BeginRenderState(
        const ModelRendererContext& context,
        // IRenderStateSetResolver& resolver,
        const Utility::ParameterBox& globalStates,
        unsigned renderStateSetIndex) const
    {
        assert(_pimpl->_capturedContext == context._context);

        auto globalHash = globalStates.GetHash() ^ globalStates.GetParameterNamesHash();
        if (    _currentRenderState == renderStateSetIndex
            &&  _currentGlobalRenderState == globalHash) { return; }

        auto& resolver = Techniques::FindCachedBox<DefaultRenderStateSetResolver>(
            DefaultRenderStateSetResolver::Desc());

        auto compiled = resolver.Compile(
            _pimpl->_renderStateSets[renderStateSetIndex].second, globalStates, context._techniqueIndex);
        if (compiled) {
            context._context->Bind(compiled->_blendState);
            context._context->Bind(compiled->_rasterizerState);
        }

        _currentRenderState = renderStateSetIndex;
        _currentGlobalRenderState = globalHash;
    }

    void SharedStateSet::CaptureState(Metal::DeviceContext* context)
    {
        assert(context);
        assert(!_pimpl->_capturedContext);
        _currentShaderName = ~unsigned(0x0);
        _currentTechniqueInterface = ~unsigned(0x0);
        _currentMaterialParamBox = ~unsigned(0x0);
        _currentGeoParamBox = ~unsigned(0x0);
        _currentRenderState = ~unsigned(0x0);
        _currentGlobalRenderState = ~unsigned(0x0);
        _currentBoundUniforms = nullptr;
        _pimpl->_capturedContext = context;
    }

    void SharedStateSet::ReleaseState(Metal::DeviceContext* context)
    {
        assert(context);
        assert(_pimpl->_capturedContext==context);
        _pimpl->_capturedContext = nullptr;
    }


    SharedStateSet::SharedStateSet()
    {
        auto pimpl = std::make_unique<Pimpl>();

        _currentShaderName = ~unsigned(0x0);
        _currentTechniqueInterface = ~unsigned(0x0);
        _currentMaterialParamBox = ~unsigned(0x0);
        _currentGeoParamBox = ~unsigned(0x0);
        _currentRenderState = ~unsigned(0x0);
        _currentGlobalRenderState = ~unsigned(0x0);
        _currentBoundUniforms = nullptr;

        pimpl->_capturedContext = nullptr;
        _pimpl = std::move(pimpl);
    }

    SharedStateSet::~SharedStateSet()
    {
        assert(!_pimpl->_capturedContext);
    }

}}

