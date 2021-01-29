// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsCore.h"
#include "ICompileOperation.h"
#include "../Utility/IteratorUtils.h"
#include "../Utility/StringUtils.h"

namespace Utility { class OutputStream; }
namespace OSServices { class BasicFile; }
namespace ConsoleRig { class LibVersionDesc; }

namespace Assets
{
	class DependentFileState;
	class DependencyValidation;
	class IArtifactCollection;

	/// <summary>Archive of compiled intermediate assets</summary>
	/// When compile operations succeed, the resulting artifacts are cached in an IntermediatesStore,
	/// which is typically in permanent memory (ie, on disk).
	///
	/// When working with multiple different versions of the engine codebase, it's necessary to have separate
	/// copies of the intermediate store (ie, because of changes to the data format, etc). This object provides
	/// the logic to select the correct store for the current codebase.
	///
	/// This make it easier to rapidly switch between different versions of the codebase, which can allow (for
	/// example) performance comparisons between different versions. Or, consider the case where we have 2
	/// executables (eg, a game executable and a GUI tool executable) which we want to use with the same 
	/// source assets, but they may have been compiled with different version of the engine code. This system
	/// allows both executables to maintain separate copies of the intermediate store.
	class IntermediatesStore
	{
	public:
		using DepVal = std::shared_ptr<DependencyValidation>;
		using CompileProductsGroupId = uint64_t;

		void StoreCompileProducts(
            StringSection<> archivableName,
			CompileProductsGroupId groupId,
			IteratorRange<const ICompileOperation::SerializedArtifact*> artifacts,
			::Assets::AssetState state,
			IteratorRange<const DependentFileState*> dependencies,
			const ConsoleRig::LibVersionDesc& compilerVersionInfo);

		std::shared_ptr<IArtifactCollection> RetrieveCompileProducts(
            StringSection<> archivableName,
			CompileProductsGroupId groupId);

		CompileProductsGroupId RegisterCompileProductsGroup(StringSection<> name);

		static auto GetDependentFileState(StringSection<> filename) -> DependentFileState;
		static void ShadowFile(StringSection<> filename);

		IntermediatesStore(
			const char baseDirectory[],
			const char versionString[],
			const char configString[],
			bool universal = false);
		~IntermediatesStore();
		IntermediatesStore(const IntermediatesStore&) = delete;
		IntermediatesStore& operator=(const IntermediatesStore&) = delete;

	protected:
		class Pimpl;
		std::unique_ptr<Pimpl> _pimpl;
	};
}



