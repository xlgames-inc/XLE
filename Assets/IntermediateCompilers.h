// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Utility/MemoryUtils.h"
#include "../Utility/IteratorUtils.h"
#include <memory>
#include <functional>
#include <vector>
#include <string>

namespace ConsoleRig { class LibVersionDesc; }

namespace Assets
{
	class ICompileOperation;
	class IIntermediateCompileMarker;
	class InitializerPack;
	class IntermediatesStore;
	class DependencyValidation;

    class IntermediateCompilers : public std::enable_shared_from_this<IntermediateCompilers>
    {
    public:
        std::shared_ptr<IIntermediateCompileMarker> Prepare(
            uint64_t typeCode, InitializerPack&&);
        void StallOnPendingOperations(bool cancelAll);
		
		using CompileOperationDelegate = std::function<std::shared_ptr<ICompileOperation>(const InitializerPack&)>;

		using RegisteredCompilerId = uint64_t;
		struct  CompilerRegistration
		{
			RegisteredCompilerId _registrationId;
		};
		CompilerRegistration RegisterCompiler(
			const std::string& name,										///< string name for the compiler, usually something user-presentable
			ConsoleRig::LibVersionDesc srcVersion,							///< version information for the module (propagated onto any assets written to disk)
			const std::shared_ptr<DependencyValidation>& compilerDepVal,	///< dependency validation for the compiler shared library itself. Can trigger recompiles if the compiler changes
			CompileOperationDelegate&& delegate								///< delegate that can create the ICompileOperation for a given asset
			);

		void DeregisterCompiler(RegisteredCompilerId id);

		// AssociateRequest associates a pattern with a compiler (previously registered with RegisterCompiler)
		// When requests are made (via Prepare) that match the pattern, that compiler can be selected to
		// handle the request
		void AssociateRequest(
			RegisteredCompilerId compiler,
			IteratorRange<const uint64_t*> outputAssetTypes,	///< compiler can generate these output asset types (though this isn't strict, the ICompileOperation outputs can vary on a per-asset basis)
			const std::string& initializerRegexFilter = ".*"	///< compiler will be invoked for assets that match this regex filter
			);

		//

		// RegisterExtensions & GetExtensionsForType are both used for FileOpen dialogs in tools
		// It's so the tool knows what model formats are available to load (for example)
		void RegisterExtensions(RegisteredCompilerId associatedCompiler, const std::string& commaSeparatedExtensions);
		std::vector<std::pair<std::string, std::string>> GetExtensionsForType(uint64_t typeCode);

		//
		
		IntermediateCompilers(
			const std::shared_ptr<IntermediatesStore>& store);
        ~IntermediateCompilers();
    protected:
        class Pimpl;
        std::shared_ptr<Pimpl> _pimpl;

        class Marker;
    };

	class IArtifactCollection;
	class ArtifactCollectionFuture;

	/// <summary>Returned from a IAssetCompiler on response to a compile request</summary>
	/// After receiving a compile marker, the caller can choose to either retrieve an existing
	/// artifact from a previous compile, or begin a new asynchronous compile operation.
	class IIntermediateCompileMarker
	{
	public:
		virtual std::shared_ptr<IArtifactCollection> GetExistingAsset() const = 0;
		virtual std::shared_ptr<ArtifactCollectionFuture> InvokeCompile() = 0;
		virtual ~IIntermediateCompileMarker();
	};
}

