// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Metal/Forward.h"       // for Metal::Blend
#include "../../Core/Types.h"
#include <memory>

namespace Utility { class ParameterBox; }

namespace RenderCore { namespace Techniques
{
    #pragma pack(push)
    #pragma pack(1)

    /// <summary>Render state settings</summary>
    /// These settings are used to select the low-level graphics API render 
    /// state while rendering using this material.
    ///
    /// There are only a few low-level states that are practical & meaningful
    /// to set this way. Often we get fighting between different parts of the
    /// engine wanting to control render states. For example, a graphics effect
    /// may want to select the back face culling mode -- but the material may
    /// have a setting for that as well. So who wins? The material or the 
    /// graphics effect? The answer changes from situation to situation.
    ///
    /// These are difficult problems! To try to avoid, we should make sure that
    /// the material only has settings for the minimal set of states it really 
    /// needs (and free everything else up for higher level stuff)
    ///
    /// RasterizerDesc:
    /// -----------------------------------------------------------------------------
    ///     double-sided culling enable/disable
    ///         Winding direction and CULL_FRONT/CULL_BACK don't really belong here.
    ///         Winding direction should be a property of the geometry and any
    ///         transforms applied to it. And we should only need to select CULL_FRONT
    ///         for special graphics techniques -- they can do it another way
    ///
    ///     depth bias
    ///         Sometimes it's handy to apply some bias at a material level. But it
    ///         should blend somehow with depth bias applied as part of the shadow 
    ///         rendering pass.
    ///
    ///     fill mode
    ///         it's rare to want to change the fill mode. But it feels like it should
    ///         be a material setting (though, I guess it could alternatively be
    ///         attached to the geometry).
    ///
    /// BlendDesc:
    /// -----------------------------------------------------------------------------
    ///     blend mode settings
    ///         This is mostly meaningful during forward rendering operations. But it
    ///         may be handy for deferred decals to select a blend mode at a material
    ///         based level. 
    ///
    ///         There may be some cases where we want to apply different blend mode 
    ///         settings in deferred and forward rendering. That suggests having 2
    ///         separate states -- one for deferred, one for forward.
    ///         We don't really want to use the low-level states in the deferred case,
    ///         because it may depend on the structure of the gbuffer (which is defined
    ///         elsewhere)
    ///
    ///         The blend mode might depend on the texture, as well. If the texture is
    ///         premultiplied alpha, it might end up with a different blend mode than
    ///         when using a non-premultiplied alpha texture.
    ///
    ///         The alpha channel blend settings (and IndependentBlendEnable setting)
    ///         are not exposed.
    ///
    ///     write mask
    ///         It's rare to want to change the write mask -- but it can be an interesting
    ///         trick. It doesn't hurt much to have some behaviour for it here.
    ///
    /// Other possibilities
    /// -----------------------------------------------------------------------------
    ///     stencil write states & stencil test states
    ///         there may be some cases where we want the material to define how we 
    ///         read and write the stencil buffer. Mostly some higher level state will
    ///         control this, but the material may want to have some effect..?
    ///
    /// Also note that alpha test is handled in a different way. We use shader behaviour
    /// (not a render state) to enable/disable 
    class RenderStateSet
    {
    public:
        enum class BlendType : unsigned
        {
            Basic, DeferredDecal, Ordered
        };
        unsigned    _doubleSided : 1;
        unsigned    _wireframe : 1;
        unsigned    _writeMask : 4;
        BlendType   _blendType : 2;

            //  These "blend" values may not be completely portable across all platforms
            //  (either because blend modes aren't supported, or because we need to
            //  change the meaning of the values)
        Metal::Blend::Enum      _forwardBlendSrc : 5;
        Metal::Blend::Enum      _forwardBlendDst : 5;
        Metal::BlendOp::Enum    _forwardBlendOp  : 5;

        struct Flag
        {
            enum Enum {
                DoubleSided = 1<<0, Wireframe = 1<<1, WriteMask = 1<<2, 
                BlendType = 1<<3, ForwardBlend = 1<<4, DepthBias = 1<<5 
            };
            typedef unsigned BitField;
        };
        Flag::BitField  _flag : 6;
        unsigned        _padding : 3;   // 8 + 15 + 32 + 5 = 60 bits... pad to 64 bits

        int             _depthBias;     // do we need all of the bits for this?

        uint64 GetHash() const;
        RenderStateSet();
    };

    #pragma pack(pop)

    class CompiledRenderStateSet;

    class IStateSetResolver
    {
    public:
        /// <summary>Given the current global state settings and a technique, build the low-level states for draw call<summary>
        /// There are only 3 influences on render states while rendering models:
        /// <list>
        ///     <item>Local states set on the draw call object
        ///     <item>The global state settings (eg, perhaps set by the lighting parser)
        ///     <item>The technique index/guid (ie, the type of rendering being performed)
        /// </list>
        /// These should be combined together to generate the low level state objects.
        virtual CompiledRenderStateSet Resolve(
            const RenderStateSet& states, 
            const Utility::ParameterBox& globalStates,
            unsigned techniqueIndex) = 0;
        virtual uint64 GetHash() = 0;
        virtual ~IStateSetResolver();
    };

    std::shared_ptr<IStateSetResolver> CreateStateSetResolver_Default();
    std::shared_ptr<IStateSetResolver> CreateStateSetResolver_Forward();
    std::shared_ptr<IStateSetResolver> CreateStateSetResolver_Deferred();

    struct RSDepthBias
    {
    public:
        int _depthBias; float _depthBiasClamp; float _slopeScaledBias;
        RSDepthBias(int depthBias=0, float depthBiasClamp=0, float slopeScaledBias=0.f)
            : _depthBias(depthBias), _depthBiasClamp(depthBiasClamp), _slopeScaledBias(slopeScaledBias) {}
    };
    std::shared_ptr<IStateSetResolver> CreateStateSetResolver_DepthOnly(
        const RSDepthBias& singleSidedBias = RSDepthBias(),
        const RSDepthBias& doubleSidedBias = RSDepthBias(),
        Metal::CullMode::Enum cullMode = Metal::CullMode::Enum(3));

///////////////////////////////////////////////////////////////////////////////////////////////////

    inline RenderStateSet::RenderStateSet()
    {
        _doubleSided = false;
        _wireframe = false;
        _writeMask = 0xf;
        _blendType = BlendType::Basic;
        _depthBias = 0;
        _flag = 0;
        
        _forwardBlendSrc = Metal::Blend::Enum(0); // Metal::Blend::One;
        _forwardBlendDst = Metal::Blend::Enum(0); // Metal::Blend::Zero;
        _forwardBlendOp = Metal::BlendOp::Enum(0); // Metal::BlendOp::NoBlending;
    }

}}

