// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Techniques/RenderStateResolver.h"
#include "../../Assets/AssetsCore.h"
#include "../../Assets/AssetUtils.h"
#include "../../Utility/ParameterBox.h"
#include "../../Utility/MemoryUtils.h"
#include <memory>

namespace Assets 
{ 
    class DependencyValidation; class DirectorySearchRules; 
	class DependentFileState;
}
namespace Utility { class Data; }

namespace RenderCore { namespace Techniques { class Material;  } }

namespace RenderCore { namespace Assets
{
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
        
        ParameterBox	_resourceBindings;
        ParameterBox	_matParamBox;
        Techniques::RenderStateSet _stateSet;
        ParameterBox	_constants;
        AssetName		_techniqueConfig;

        std::vector<AssetName> _inherit;

		void					MergeInto(Techniques::Material& dest) const; 
		std::vector<AssetName>	ResolveInherited(const ::Assets::DirectorySearchRules& searchRules) const;

		const std::shared_ptr<::Assets::DependencyValidation>&	GetDependencyValidation() const { return _depVal; }
		const ::Assets::DirectorySearchRules&					GetDirectorySearchRules() const { return _searchRules; }

        void Serialize(OutputStreamFormatter& formatter) const;
        
        RawMaterial();
        RawMaterial(
            InputStreamFormatter<utf8>& formatter, 
            const ::Assets::DirectorySearchRules&,
			const ::Assets::DepValPtr& depVal);
        ~RawMaterial();

		static const auto CompileProcessType = ConstHash64<'RawM', 'at'>::Value;

    private:
        std::shared_ptr<::Assets::DependencyValidation> _depVal;
		::Assets::DirectorySearchRules _searchRules;
    };

    void ResolveMaterialFilename(
        ::Assets::ResChar resolvedFile[], unsigned resolvedFileCount,
        const ::Assets::DirectorySearchRules& searchRules, StringSection<char> baseMatName);


	void MergeIn_Stall(
		Techniques::Material& result,
		StringSection<> sourceMaterialName,
		const ::Assets::DirectorySearchRules& searchRules,
		std::vector<::Assets::DependentFileState>& deps);

	void MergeIn_Stall(
		RenderCore::Techniques::Material& result,
		const RenderCore::Assets::RawMaterial& src,
		const ::Assets::DirectorySearchRules& searchRules);

}}

