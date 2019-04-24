// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../ShaderParser/ShaderInstantiation.h"
#include "../../Assets/AssetsCore.h"
#include "../../Assets/AssetUtils.h"
#include "../../Utility/IteratorUtils.h"
#include <utility>

namespace ShaderSourceParser { class InstantiationRequest_ArchiveName; }
namespace Utility { template<typename Char> class InputStreamFormatter; }

namespace RenderCore { namespace Assets
{
	class ShaderPatchCollection
	{
	public:
		IteratorRange<const std::pair<std::string, ShaderSourceParser::InstantiationRequest_ArchiveName>*> GetPatches() const { return MakeIteratorRange(_patches); }
		uint64_t GetHash() const { return _hash; }

		void MergeInto(ShaderPatchCollection& dest) const; 

		ShaderPatchCollection();
		ShaderPatchCollection(IteratorRange<const std::pair<std::string, ShaderSourceParser::InstantiationRequest_ArchiveName>*> patches);
		ShaderPatchCollection(std::vector<std::pair<std::string, ShaderSourceParser::InstantiationRequest_ArchiveName>>&& patches);
		~ShaderPatchCollection();
	private:
		std::vector<std::pair<std::string, ShaderSourceParser::InstantiationRequest_ArchiveName>> _patches;
		uint64_t _hash;

		void SortAndCalculateHash();
	};

	ShaderPatchCollection DeserializeShaderPatchCollection(InputStreamFormatter<utf8>& formatter);

	class CompiledShaderPatchCollection
	{
	public:
		std::string _srcCode;

		struct Patch 
		{
			uint64_t		_implementsHash;
			std::string		_scaffoldInFunction;		// scaffold function to use for patching in this particular implementation.
		};
		std::vector<Patch> _patches;

		const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }
		::Assets::DepValPtr _depVal;
		std::vector<::Assets::DependentFileState> _dependencies;

		uint64_t GetGUID() const;

		CompiledShaderPatchCollection(const ShaderPatchCollection& src);
		~CompiledShaderPatchCollection();
	};
}}

