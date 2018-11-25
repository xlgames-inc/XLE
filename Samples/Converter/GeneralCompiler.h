// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Assets/AssetUtils.h"
#include "../../ConsoleRig/GlobalServices.h"	// (for LibVersionDesc)
#include "../../Utility/MemoryUtils.h"
#include "../../Core/Types.h"
#include <memory>
#include <functional>
#include <regex>

namespace Assets { class ICompileOperation; }

namespace Converter 
{

    class GeneralCompiler : public ::Assets::IAssetCompiler, public std::enable_shared_from_this<GeneralCompiler>
    {
    public:
        std::shared_ptr<::Assets::IArtifactPrepareMarker> Prepare(
            uint64 typeCode, 
            const StringSection<::Assets::ResChar> initializers[], unsigned initializerCount);
        void StallOnPendingOperations(bool cancelAll);
		
		using CompileOperationDelegate = std::function<std::shared_ptr<::Assets::ICompileOperation>(StringSection<>)>;

		struct ExtensionAndDelegate
		{
			std::regex _extensionFilter;
			std::string _name;
			ConsoleRig::LibVersionDesc _srcVersion;
			CompileOperationDelegate _delegate;
		};
		
		enum class ArtifactType
		{
			ArchivedFile,
			Blob
		};

		GeneralCompiler(
			IteratorRange<const ExtensionAndDelegate*> delegates,
			ArtifactType artifactType);
        ~GeneralCompiler();
    protected:
        class Pimpl;
        std::shared_ptr<Pimpl> _pimpl;

        class Marker;
    };

	::Assets::DirectorySearchRules DefaultLibrarySearchDirectories();

	std::vector<GeneralCompiler::ExtensionAndDelegate> DiscoverCompileOperations(
		const ::Assets::DirectorySearchRules& searchRules = DefaultLibrarySearchDirectories());
}

