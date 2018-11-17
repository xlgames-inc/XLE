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
///////////////////////////////////////////////////////////////////////////////////////////////////

    IRenderStateDelegate::~IRenderStateDelegate() {}

    Metal::RasterizerState BuildDefaultRastizerState(const RenderStateSet& states)
    {
        auto cullMode = CullMode::Back;
        auto fillMode = FillMode::Solid;
        unsigned depthBias = 0;
        if (states._flag & RenderStateSet::Flag::DoubleSided) {
            cullMode = states._doubleSided ? CullMode::None : CullMode::Back;
        }
        if (states._flag & RenderStateSet::Flag::DepthBias) {
            depthBias = states._depthBias;
        }
        if (states._flag & RenderStateSet::Flag::Wireframe) {
            fillMode = states._wireframe ? FillMode::Wireframe : FillMode::Solid;
        }

        return Metal::RasterizerState(cullMode, true, fillMode, depthBias, 0.f, 0.f);
    }

    class StateSetResolver_Default : public IRenderStateDelegate
    {
    public:
        auto Resolve(
            const RenderStateSet& states, 
            const Utility::ParameterBox& globalStates,
            unsigned techniqueIndex) -> CompiledRenderStateSet
        {
            return CompiledRenderStateSet(Metal::BlendState(CommonResources()._blendOpaque), BuildDefaultRastizerState(states));
        }

        virtual uint64 GetHash()
        {
            return typeid(StateSetResolver_Default).hash_code();
        }
    };

    class StateSetResolver_Deferred : public IRenderStateDelegate
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

            return CompiledRenderStateSet(Metal::BlendState(blendState), BuildDefaultRastizerState(states));
        }

        virtual uint64 GetHash() { return typeid(StateSetResolver_Deferred).hash_code(); }
    };

    class StateSetResolver_Forward : public IRenderStateDelegate
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

            result._rasterizerState = BuildDefaultRastizerState(states);
            return result;
        }

        virtual uint64 GetHash() { return typeid(StateSetResolver_Forward).hash_code(); }
    };

    class StateSetResolver_DepthOnly : public IRenderStateDelegate
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
            unsigned cullDisable    = !!(states._flag & RenderStateSet::Flag::DoubleSided);
            unsigned wireframe      = !!(states._flag & RenderStateSet::Flag::Wireframe);
            result._rasterizerState = _rs[wireframe<<1|cullDisable];
            return result;
        }

        virtual uint64 GetHash() { return _hashCode; }

        StateSetResolver_DepthOnly(
            const RSDepthBias& singleSidedBias,
            const RSDepthBias& doubleSidedBias,
            CullMode cullMode)
        {
            using namespace Metal;
            _rs[0x0] = RasterizerState(cullMode,        true, FillMode::Solid,      singleSidedBias._depthBias, singleSidedBias._depthBiasClamp, singleSidedBias._slopeScaledBias);
            _rs[0x1] = RasterizerState(CullMode::None,  true, FillMode::Solid,      doubleSidedBias._depthBias, doubleSidedBias._depthBiasClamp, doubleSidedBias._slopeScaledBias);
            _rs[0x2] = RasterizerState(cullMode,        true, FillMode::Wireframe,  singleSidedBias._depthBias, singleSidedBias._depthBiasClamp, singleSidedBias._slopeScaledBias);
            _rs[0x3] = RasterizerState(CullMode::None,  true, FillMode::Wireframe,  doubleSidedBias._depthBias, doubleSidedBias._depthBiasClamp, doubleSidedBias._slopeScaledBias);
            auto h0 = HashCombine(HashCombine(singleSidedBias._depthBias, *(unsigned*)&singleSidedBias._depthBiasClamp), *(unsigned*)&singleSidedBias._slopeScaledBias);
            auto h1 = HashCombine(HashCombine(doubleSidedBias._depthBias, *(unsigned*)&doubleSidedBias._depthBiasClamp), *(unsigned*)&doubleSidedBias._slopeScaledBias);
            _hashCode = HashCombine(
                typeid(StateSetResolver_DepthOnly).hash_code(),
                HashCombine(HashCombine(h0, h1), (uint64)cullMode));
        }

    protected:
        Metal::RasterizerState _rs[4];
        uint64 _hashCode;
    };

    std::shared_ptr<IRenderStateDelegate> CreateRenderStateDelegate_Default()     { return std::make_shared<StateSetResolver_Default>(); }
    std::shared_ptr<IRenderStateDelegate> CreateRenderStateDelegate_Forward()     { return std::make_shared<StateSetResolver_Forward>(); }
    std::shared_ptr<IRenderStateDelegate> CreateRenderStateDelegate_Deferred()    { return std::make_shared<StateSetResolver_Deferred>(); }
    std::shared_ptr<IRenderStateDelegate> CreateRenderStateDelegate_DepthOnly(
        const RSDepthBias& singleSidedBias, const RSDepthBias& doubleSidedBias, CullMode cullMode)
    { 
        return std::make_shared<StateSetResolver_DepthOnly>(
            singleSidedBias, doubleSidedBias, cullMode); 
    }

}}

