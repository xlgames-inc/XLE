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
namespace Utility { template<typename Char> class InputStreamFormatter; class OutputStreamFormatter; }

namespace RenderCore { namespace Assets
{
	class ShaderPatchCollection
	{
	public:
		IteratorRange<const std::pair<std::string, ShaderSourceParser::InstantiationRequest_ArchiveName>*> GetPatches() const { return MakeIteratorRange(_patches); }
		uint64_t GetHash() const { return _hash; }

		void MergeInto(ShaderPatchCollection& dest) const;

		void Serialize(OutputStreamFormatter& formatter) const;

		friend bool operator<(const ShaderPatchCollection& lhs, const ShaderPatchCollection& rhs);
		friend bool operator<(const ShaderPatchCollection& lhs, uint64_t rhs);
		friend bool operator<(uint64_t lhs, const ShaderPatchCollection& rhs);

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
	std::vector<ShaderPatchCollection> DeserializeShaderPatchCollectionSet(InputStreamFormatter<utf8>& formatter);
	void SerializeShaderPatchCollectionSet(OutputStreamFormatter& formatter, IteratorRange<const ShaderPatchCollection*> patchCollections);

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

		uint64_t _guid = 0;
		uint64_t GetGUID() const { return _guid; }

		CompiledShaderPatchCollection(const ShaderPatchCollection& src);
		CompiledShaderPatchCollection();
		~CompiledShaderPatchCollection();
	};

	class ShaderPatchCollectionRegistry
	{
	public:
		const CompiledShaderPatchCollection& GetCompiledShaderPatchCollection(uint64_t hash) const;
		void RegisterShaderPatchCollection(const ShaderPatchCollection& patchCollection) const;

		static ShaderPatchCollectionRegistry& GetInstance() { assert(s_instance); return *s_instance; }
		ShaderPatchCollectionRegistry();
		~ShaderPatchCollectionRegistry();
	private:
		static ShaderPatchCollectionRegistry* s_instance;
	};
}}

