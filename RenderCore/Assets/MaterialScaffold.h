// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderCore/Types.h"
#include "../../Assets/AssetsCore.h"
#include "../../Utility/ParameterBox.h"
#include "../../Utility/Streams/Serialization.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/IteratorUtils.h"
#include <memory>

namespace Assets 
{ 
    class DependencyValidation;
	class AssetChunkResult;
	class AssetChunkRequest;
}
namespace Utility { class Data; }

namespace RenderCore { namespace Assets
{
    using MaterialGuid = uint64_t;

    class MaterialImmutableData;

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
        Blend		_forwardBlendSrc : 5;
        Blend		_forwardBlendDst : 5;
        BlendOp		_forwardBlendOp  : 5;

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

	/// <summary>Common material settings</summary>
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
	class MaterialScaffoldMaterial
	{
	public:
		ParameterBox		_bindings;				// shader resource bindings
		ParameterBox		_matParams;				// material parameters used for selecting the appropriate shader variation
		RenderStateSet		_stateSet;				// used by the RenderStateResolver for selecting render state settings (like depth read/write settings, blend modes)
		ParameterBox		_constants;				// values typically passed to shader constants
		::Assets::ResChar	_techniqueConfig[32];	// root technique config file

		template<typename Serializer>
			void Serialize(Serializer& serializer) const;

		MaterialScaffoldMaterial();
		MaterialScaffoldMaterial(MaterialScaffoldMaterial&& moveFrom) never_throws;
		MaterialScaffoldMaterial& operator=(MaterialScaffoldMaterial&& moveFrom) never_throws;
		MaterialScaffoldMaterial(const MaterialScaffoldMaterial&);
		MaterialScaffoldMaterial& operator=(const MaterialScaffoldMaterial&);
	};

	#pragma pack(pop)

    /// <summary>An asset containing compiled material settings</summary>
    /// This is the equivalent of other scaffold objects (like ModelScaffold
    /// and AnimationSetScaffold). It contains the processed and ready-to-use
    /// material information.
    class MaterialScaffold
    {
    public:
		using Material = MaterialScaffoldMaterial;

        const MaterialImmutableData&    ImmutableData() const;
        const Material*					GetMaterial(MaterialGuid guid) const;
        StringSection<>					GetMaterialName(MaterialGuid guid) const;

		const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _depVal; }

        static const auto CompileProcessType = ConstHash64<'ResM', 'at'>::Value;
		static const ::Assets::AssetChunkRequest ChunkRequests[1];

        MaterialScaffold(IteratorRange<::Assets::AssetChunkResult*> chunks, const ::Assets::DepValPtr& depVal);
        MaterialScaffold(MaterialScaffold&& moveFrom) never_throws;
        MaterialScaffold& operator=(MaterialScaffold&& moveFrom) never_throws;
        ~MaterialScaffold();

    protected:
        std::unique_ptr<uint8[], PODAlignedDeletor>	_rawMemoryBlock;
		::Assets::DepValPtr _depVal;
    };

	static constexpr uint64 ChunkType_ResolvedMat = ConstHash64<'ResM', 'at'>::Value;
	static constexpr unsigned ResolvedMat_ExpectedVersion = 1;

///////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename Serializer>
        void MaterialScaffold::Material::Serialize(Serializer& serializer) const
    {
        ::Serialize(serializer, _bindings);
        ::Serialize(serializer, _matParams);
        ::Serialize(serializer, _stateSet.GetHash());
        ::Serialize(serializer, _constants);
        serializer.SerializeRaw(_techniqueConfig);
    }
		
    inline RenderStateSet::RenderStateSet()
    {
        _doubleSided = false;
        _wireframe = false;
        _writeMask = 0xf;
        _blendType = BlendType::Basic;
        _depthBias = 0;
        _flag = 0;
        
        _forwardBlendSrc = Blend(0); // Metal::Blend::One;
        _forwardBlendDst = Blend(0); // Metal::Blend::Zero;
        _forwardBlendOp = BlendOp(0); // Metal::BlendOp::NoBlending;
    }
    
    inline uint64 RenderStateSet::GetHash() const
    {
        static_assert(sizeof(*this) == sizeof(uint64), "expecting StateSet to be 64 bits long");
        return *(const uint64*)this;
    }

}}

