// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Techniques/RenderStateResolver.h"
#include "../../Assets/AssetsCore.h"
#include "../../Assets/AssetUtils.h"
#include "../../Utility/ParameterBox.h"
#include <memory>

#if defined(HAS_XLE_FULLASSETS)
    #include "../../Assets/DivergentAsset.h"
#endif

namespace Assets 
{ 
    class DependencyValidation; class DirectorySearchRules; 
	class DeferredConstruction;
	class DependentFileState;
}
namespace Utility { class Data; }

namespace RenderCore { namespace Assets
{
	class ResolvedMaterial;

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
			const StringSection<::Assets::ResChar> initializers[], unsigned initializerCount);

        static const RawMaterial& GetAsset(StringSection<::Assets::ResChar> initializer);
        #if defined(HAS_XLE_DIVERGENTASSET)
            static const std::shared_ptr<::Assets::DivergentAsset<RawMaterial>>& GetDivergentAsset(StringSection<::Assets::ResChar> initializer);
        #endif
		static std::unique_ptr<RawMaterial> CreateNew(StringSection<::Assets::ResChar> initialiser);

    private:
        std::shared_ptr<::Assets::DependencyValidation> _depVal;
		::Assets::DirectorySearchRules _searchRules;

        void MergeInto(ResolvedMaterial& dest) const;
    };

    void ResolveMaterialFilename(
        ::Assets::ResChar resolvedFile[], unsigned resolvedFileCount,
        const ::Assets::DirectorySearchRules& searchRules, StringSection<char> baseMatName);

}}

