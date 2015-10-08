// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RenderStateResolver.h"
#include "CommonResources.h"
#include "CompiledRenderStateSet.h"
#include "../Metal/State.h"
#include "../../Utility/MemoryUtils.h"

namespace RenderCore { namespace Techniques
{
    RenderStateSet::RenderStateSet()
    {
        _doubleSided = false;
        _wireframe = false;
        _writeMask = 0xf;
        _blendType = BlendType::Basic;
        _depthBias = 0;
        _flag = 0;
        
        _forwardBlendSrc = Metal::Blend::One;
        _forwardBlendDst = Metal::Blend::Zero;
        _forwardBlendOp = Metal::BlendOp::NoBlending;
    }

    uint64 RenderStateSet::GetHash() const
    {
        static_assert(sizeof(*this) == sizeof(uint64), "expecting StateSet to be 64 bits long");
        return *(const uint64*)this;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    IStateSetResolver::~IStateSetResolver() {}

    static Metal::RasterizerState BuildRastizerState(const RenderStateSet& states)
    {
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

        return Metal::RasterizerState(cullMode, true, fillMode, depthBias, 0.f, 0.f);
    }

    class StateSetResolver_Default : public IStateSetResolver
    {
    public:
        auto Resolve(
            const RenderStateSet& states, 
            const Utility::ParameterBox& globalStates,
            unsigned techniqueIndex) -> CompiledRenderStateSet
        {
            return CompiledRenderStateSet(Metal::BlendState(CommonResources()._blendOpaque), BuildRastizerState(states));
        }

        virtual uint64 GetHash()
        {
            return typeid(StateSetResolver_Default).hash_code();
        }
    };

    class StateSetResolver_Deferred : public IStateSetResolver
    {
    public:
        auto Resolve(
            const RenderStateSet& states, 
            const Utility::ParameterBox& globalStates,
            unsigned techniqueIndex) -> CompiledRenderStateSet
        {
            bool deferredDecal = 
                    (states._flag & RenderStateSet::Flag::BlendType)
                && (states._blendType == RenderStateSet::BlendType::DeferredDecal);

            auto& blendState = deferredDecal
                ? CommonResources()._blendStraightAlpha
                : CommonResources()._blendOpaque;

            return CompiledRenderStateSet(Metal::BlendState(blendState), BuildRastizerState(states));
        }

        virtual uint64 GetHash() { return typeid(StateSetResolver_Deferred).hash_code(); }
    };

    class StateSetResolver_Forward : public IStateSetResolver
    {
    public:
        auto Resolve(
            const RenderStateSet& states, 
            const Utility::ParameterBox& globalStates,
            unsigned techniqueIndex) -> CompiledRenderStateSet
        {
            CompiledRenderStateSet result;
            if (states._flag & RenderStateSet::Flag::ForwardBlend) {
                result._blendState = Metal::BlendState(
                    states._forwardBlendOp, states._forwardBlendSrc, states._forwardBlendDst);
            } else {
                result._blendState = Techniques::CommonResources()._blendOpaque;
            }

            result._rasterizerState = BuildRastizerState(states);
            return result;
        }

        virtual uint64 GetHash() { return typeid(StateSetResolver_Forward).hash_code(); }
    };

    class StateSetResolver_DepthOnly : public IStateSetResolver
    {
    public:
        auto Resolve(
            const RenderStateSet& states, 
            const Utility::ParameterBox& globalStates,
            unsigned techniqueIndex) -> CompiledRenderStateSet
        {
                // When rendering the shadows, most states are constant.
                // Blend state is always just opaque.
                // Rasterizer state only needs to change a few states, but
                // wants to inherit the depth bias and slope scaled depth bias
                // settings.
            CompiledRenderStateSet result;
            unsigned cullDisable = !!(states._flag & RenderStateSet::Flag::DoubleSided);
            unsigned wireframe = !!(states._flag & RenderStateSet::Flag::Wireframe);
            result._rasterizerState = _rs[wireframe<<1|cullDisable];
            return result;
        }

        virtual uint64 GetHash() { return _hashCode; }

        StateSetResolver_DepthOnly(
            int depthBias, float depthBiasClamp, 
            float slopeScaledBias, Metal::CullMode::Enum cullMode)
        {
            using namespace Metal;
            _rs[0x0] = RasterizerState(cullMode, true, FillMode::Solid, depthBias, depthBiasClamp, slopeScaledBias);
            _rs[0x1] = RasterizerState(CullMode::None, true, FillMode::Solid, depthBias, depthBiasClamp, slopeScaledBias);
            _rs[0x2] = RasterizerState(cullMode, true, FillMode::Wireframe, depthBias, depthBiasClamp, slopeScaledBias);
            _rs[0x3] = RasterizerState(CullMode::None, true, FillMode::Wireframe, depthBias, depthBiasClamp, slopeScaledBias);
            _hashCode = HashCombine(
                typeid(StateSetResolver_DepthOnly).hash_code(),
                HashCombine(HashCombine(HashCombine(depthBias, *(unsigned*)&depthBiasClamp), *(unsigned*)&slopeScaledBias), cullMode));
        }

    protected:
        Metal::RasterizerState _rs[4];
        uint64 _hashCode;
    };

    std::shared_ptr<IStateSetResolver> CreateStateSetResolver_Default()     { return std::make_shared<StateSetResolver_Default>(); }
    std::shared_ptr<IStateSetResolver> CreateStateSetResolver_Forward()     { return std::make_shared<StateSetResolver_Forward>(); }
    std::shared_ptr<IStateSetResolver> CreateStateSetResolver_Deferred()    { return std::make_shared<StateSetResolver_Deferred>(); }
    std::shared_ptr<IStateSetResolver> CreateStateSetResolver_DepthOnly(
        int depthBias, float depthBiasClamp, 
        float slopeScaledBias, Metal::CullMode::Enum cullMode)
    { 
        return std::make_shared<StateSetResolver_DepthOnly>(
            depthBias, depthBiasClamp, slopeScaledBias, cullMode); 
    }

}}

