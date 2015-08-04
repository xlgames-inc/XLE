// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Metal/Forward.h"       // for Metal::Blend
#include "../../Assets/Assets.h"
#include "../../Utility/ParameterBox.h"
#include "../../Utility/Streams/Serialization.h"
#include "../../Utility/Mixins.h"


namespace Assets { class DependencyValidation; class DirectorySearchRules; }
namespace Utility { class Data; }

namespace RenderCore { namespace Assets
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
        struct DeferredBlend
        {
            enum Enum { Opaque, Decal };
        };
        unsigned                _doubleSided : 1;
        unsigned                _wireframe : 1;
        unsigned                _writeMask : 4;
        DeferredBlend::Enum     _deferredBlend : 2;

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
                DeferredBlend = 1<<3, ForwardBlend = 1<<4, DepthBias = 1<<5 
            };
            typedef unsigned BitField;
        };
        Flag::BitField          _flag : 6;
        unsigned                _padding : 3;   // 8 + 15 + 32 + 5 = 60 bits... pad to 64 bits

        int                     _depthBias;     // do we need all of the bits for this?

        uint64 GetHash() const;
        RenderStateSet();
    };

    typedef uint64 MaterialGuid;

    /// <summary>Final material settings</summary>
    /// These are some material parameters that can be attached to a 
    /// ModelRunTime. This is the "resolved" format. Material settings
    /// start out as a hierarchical set of parameters. During preprocessing
    /// they should be resolved down to a final flattened form -- which is
    /// this form.
    ///
    /// This is used during runtime to construct the inputs for shaders.
    ///
    /// Material parameters and settings are purposefully kept fairly
    /// low-level. These parameters are settings that can be used during
    /// the main render step (rather than some higher level, CPU-side
    /// operation).
    ///
    /// Typically, material parameters effect these shader inputs:
    ///     Resource bindings (ie, textures assigned to shader resource slots)
    ///     Shader constants
    ///     Some state settings (like blend modes, etc)
    ///
    /// Material parameters can also effect the shader selection through the 
    /// _matParams resource box.
    class ResolvedMaterial : noncopyable
    {
    public:
        ParameterBox _bindings;
        ParameterBox _matParams;
        RenderStateSet _stateSet;
        ParameterBox _constants;

        template<typename Serializer>
            void Serialize(Serializer& serializer) const;

        ResolvedMaterial();
        ResolvedMaterial(ResolvedMaterial&& moveFrom);
        ResolvedMaterial& operator=(ResolvedMaterial&& moveFrom);
    };

    #pragma pack(pop)

    /// <summary>Pre-resolved material settings</summary>
    /// Materials are a hierachical set of properties. Each RawMaterial
    /// object can inherit from sub RawMaterials -- and it can either
    /// inherit or override the properties in those sub RawMaterials.
    ///
    /// RawMaterials are intended to be used in tools (for preprocessing 
    /// and material authoring). ResolvedMaterial is the run-time representation.
    ///
    /// During preprocessing, RawMaterials should be resolved down to a 
    /// ResolvedMaterial object (using the Resolve() method). 
    class RawMaterial
    {
    public:
        ParameterBox _resourceBindings;
        ParameterBox _matParamBox;
        Assets::RenderStateSet _stateSet;
        ParameterBox _constants;

        using ResString = ::Assets::rstring;
        std::vector<ResString> _inherit;

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _depVal; }

        void Resolve(
            ResolvedMaterial& result,
            const ::Assets::DirectorySearchRules& searchRules,
            std::vector<::Assets::DependentFileState>* deps = nullptr) const;

        std::vector<ResString> ResolveInherited(
            const ::Assets::DirectorySearchRules& searchRules) const;

        void Serialize(OutputStreamFormatter& formatter) const;
        
        RawMaterial();
        RawMaterial(
            InputStreamFormatter<utf8>& formatter, 
            const ::Assets::DirectorySearchRules&);
        ~RawMaterial();

        // static std::unique_ptr<RawMaterial> CreateNew(const ::Assets::ResChar initialiser[]);

        class RawMatSplitName
        {
        public:
            ResString _settingName;
            ResString _concreteFilename;
            ResString _initializerName;

            RawMatSplitName();
            RawMatSplitName(const ::Assets::ResChar initialiser[]);
        };

    private:
        std::shared_ptr<::Assets::DependencyValidation> _depVal;

        void MergeInto(ResolvedMaterial& dest) const;

        RawMatSplitName _splitName;
    };

    void MakeConcreteRawMaterialFilename(
        ::Assets::ResChar dest[], unsigned dstCount, const ::Assets::ResChar inputName[]);
    void ResolveMaterialFilename(
        ::Assets::ResChar resolvedFile[], unsigned resolvedFileCount,
        const ::Assets::DirectorySearchRules& searchRules, const char baseMatName[]);
    uint64 MakeMaterialGuid(const utf8* nameStart, const utf8* nameEnd);


    template<typename Serializer>
        void ResolvedMaterial::Serialize(Serializer& serializer) const
    {
        ::Serialize(serializer, _bindings);
        ::Serialize(serializer, _matParams);
        ::Serialize(serializer, _stateSet.GetHash());
        ::Serialize(serializer, _constants);
    }

}}

