// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Techniques/RenderStateResolver.h"
#include "../../Assets/Assets.h"
#include "../../Assets/ChunkFileAsset.h"
#include "../../Utility/ParameterBox.h"
#include "../../Utility/Streams/Serialization.h"
#include "../../Utility/Mixins.h"
#include <memory>

namespace Assets { 
    class DependencyValidation; class DirectorySearchRules; 
    class PendingCompileMarker; 
    template<typename T, typename F> class ConfigFileListContainer;
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

        template<typename Serializer>
            void Serialize(Serializer& serializer) const;

        ResolvedMaterial();
        ResolvedMaterial(ResolvedMaterial&& moveFrom);
        ResolvedMaterial& operator=(ResolvedMaterial&& moveFrom);
    };

    #pragma pack(pop)

    class MaterialImmutableData;

    /// <summary>An asset containing compiled material settings</summary>
    /// This is the equivalent of other scaffold objects (like ModelScaffold
    /// and AnimationSetScaffold). It contains the processed and ready-to-use
    /// material information.
    class MaterialScaffold : public ::Assets::ChunkFileAsset
    {
    public:
        const MaterialImmutableData&    ImmutableData() const;
        const ResolvedMaterial*         GetMaterial(MaterialGuid guid) const;
        const char*                     GetMaterialName(MaterialGuid guid) const;

        static const auto CompileProcessType = ConstHash64<'ResM', 'at'>::Value;

        MaterialScaffold(std::shared_ptr<::Assets::PendingCompileMarker>&& marker);
        MaterialScaffold(MaterialScaffold&& moveFrom) never_throws;
        MaterialScaffold& operator=(MaterialScaffold&& moveFrom) never_throws;
        ~MaterialScaffold();
    protected:
        std::unique_ptr<uint8[]> _rawMemoryBlock;

        static void Resolver(void*, IteratorRange<::Assets::AssetChunkResult*>);
        const MaterialImmutableData*   TryImmutableData() const;
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
        ParameterBox _resourceBindings;
        ParameterBox _matParamBox;
        Techniques::RenderStateSet _stateSet;
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

        using Container = ::Assets::ConfigFileListContainer<RawMaterial, InputStreamFormatter<utf8>>;
        static const Container& GetAsset(const ::Assets::ResChar initializer[]);
        static const std::shared_ptr<::Assets::DivergentAsset<Container>>& GetDivergentAsset(const ::Assets::ResChar initializer[]);

    private:
        std::shared_ptr<::Assets::DependencyValidation> _depVal;

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
    }

}}

