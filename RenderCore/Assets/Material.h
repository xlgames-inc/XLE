// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Techniques/RenderStateResolver.h"
#include "../../Assets/Assets.h"
#include "../../Assets/AssetUtils.h"
#include "../../Utility/ParameterBox.h"
#include "../../Utility/Streams/Serialization.h"
#include "../../Utility/Mixins.h"
#include <memory>

namespace Assets 
{ 
    class DependencyValidation; class DirectorySearchRules; 
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
    class ResolvedMaterial : noncopyable
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
		::Assets::AssetState			TryResolve() const;

        static const auto CompileProcessType = ConstHash64<'ResM', 'at'>::Value;

        MaterialScaffold(const ::Assets::ChunkFileContainer& chunkFile);
		MaterialScaffold(const std::shared_ptr<::Assets::DeferredConstruction>&);
        MaterialScaffold(MaterialScaffold&& moveFrom) never_throws;
        MaterialScaffold& operator=(MaterialScaffold&& moveFrom) never_throws;
        ~MaterialScaffold();

		static std::shared_ptr<::Assets::DeferredConstruction> BeginDeferredConstruction(
			const ::Assets::ResChar* initializers[], unsigned initializerCount);
    protected:
        std::unique_ptr<uint8[]> _rawMemoryBlock;

		std::shared_ptr<::Assets::DeferredConstruction> _deferredConstructor;
		::Assets::rstring			_filename;
		::Assets::DepValPtr			_depVal;
        
        const MaterialImmutableData*   TryImmutableData() const;
		void Resolve() const;
    };

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
        using AssetName = ::Assets::rstring;
        
        ParameterBox _resourceBindings;
        ParameterBox _matParamBox;
        Techniques::RenderStateSet _stateSet;
        ParameterBox _constants;
        AssetName _techniqueConfig;

        std::vector<AssetName> _inherit;

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _depVal; }

        ::Assets::AssetState TryResolve(
            ResolvedMaterial& result,
			const ::Assets::DirectorySearchRules& searchRules,
            std::vector<::Assets::DependentFileState>* deps = nullptr) const;

        std::vector<AssetName> ResolveInherited(
            const ::Assets::DirectorySearchRules& searchRules) const;

        void Serialize(OutputStreamFormatter& formatter) const;
        
        RawMaterial();
        RawMaterial(
            InputStreamFormatter<utf8>& formatter, 
            const ::Assets::DirectorySearchRules&,
			const ::Assets::DepValPtr& depVal);
        ~RawMaterial();

		static std::shared_ptr<::Assets::DeferredConstruction> BeginDeferredConstruction(
			const ::Assets::ResChar* initializers[], unsigned initializerCount);

        static const RawMaterial& GetAsset(const ::Assets::ResChar initializer[]);
        static const std::shared_ptr<::Assets::DivergentAsset<RawMaterial>>& GetDivergentAsset(const ::Assets::ResChar initializer[]);
		static std::unique_ptr<RawMaterial> CreateNew(const ::Assets::ResChar initialiser[]);

    private:
        std::shared_ptr<::Assets::DependencyValidation> _depVal;
		::Assets::DirectorySearchRules _searchRules;

        void MergeInto(ResolvedMaterial& dest) const;
    };

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
		serializer.SerializeRaw<const ::Assets::ResChar(&)[dimof(_techniqueConfig)]>(_techniqueConfig);
    }

}}

