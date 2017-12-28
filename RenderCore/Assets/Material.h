// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Techniques/RenderStateResolver.h"
#include "../../Assets/AssetsCore.h"
#include "../../Utility/ParameterBox.h"
#include "../../Utility/Streams/Serialization.h"
#include "../../Utility/MemoryUtils.h"
#include <memory>

namespace Assets 
{ 
    class DependencyValidation;
	class ChunkFileContainer;
	class DeferredConstruction;
}
namespace Utility { class Data; }

namespace RenderCore { namespace Assets
{
    typedef uint64 MaterialGuid;

    #pragma pack(push)
    #pragma pack(1)

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
    class ResolvedMaterial
    {
    public:
        ParameterBox _bindings;
        ParameterBox _matParams;
        Techniques::RenderStateSet _stateSet;
        ParameterBox _constants;
        ::Assets::ResChar _techniqueConfig[32];

        template<typename Serializer>
            void Serialize(Serializer& serializer) const;

        ResolvedMaterial();
        ResolvedMaterial(ResolvedMaterial&& moveFrom) never_throws;
        ResolvedMaterial& operator=(ResolvedMaterial&& moveFrom) never_throws;
        ResolvedMaterial(const ResolvedMaterial&) = delete;
        ResolvedMaterial& operator=(const ResolvedMaterial&) = delete;
    };

    #pragma pack(pop)

    class MaterialImmutableData;

    /// <summary>An asset containing compiled material settings</summary>
    /// This is the equivalent of other scaffold objects (like ModelScaffold
    /// and AnimationSetScaffold). It contains the processed and ready-to-use
    /// material information.
    class MaterialScaffold
    {
    public:
        const MaterialImmutableData&    ImmutableData() const;
        const ResolvedMaterial*         GetMaterial(MaterialGuid guid) const;
        const char*                     GetMaterialName(MaterialGuid guid) const;

		const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _depVal; }

        static const auto CompileProcessType = ConstHash64<'ResM', 'at'>::Value;

        MaterialScaffold(const ::Assets::ChunkFileContainer& chunkFile);
        MaterialScaffold(MaterialScaffold&& moveFrom) never_throws;
        MaterialScaffold& operator=(MaterialScaffold&& moveFrom) never_throws;
        ~MaterialScaffold();

    protected:
        std::unique_ptr<uint8[], PODAlignedDeletor>	_rawMemoryBlock;
		::Assets::DepValPtr _depVal;
		::Assets::rstring _filename;
    };

    uint64 MakeMaterialGuid(StringSection<utf8> name);

	template<typename Serializer>
        void ResolvedMaterial::Serialize(Serializer& serializer) const
    {
        ::Serialize(serializer, _bindings);
        ::Serialize(serializer, _matParams);
        ::Serialize(serializer, _stateSet.GetHash());
        ::Serialize(serializer, _constants);
        serializer.SerializeRaw(_techniqueConfig);
    }
}}

