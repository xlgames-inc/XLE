// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Assets/IntermediateAssets.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Core/Types.h"
#include <memory>

namespace Converter 
{

    class GeneralCompiler : public ::Assets::IntermediateAssets::IAssetCompiler, public std::enable_shared_from_this<GeneralCompiler>
    {
    public:
        std::shared_ptr<::Assets::ICompileMarker> PrepareAsset(
            uint64 typeCode, 
            const StringSection<::Assets::ResChar> initializers[], unsigned initializerCount,
            const ::Assets::IntermediateAssets::Store& destinationStore);

        void StallOnPendingOperations(bool cancelAll);
		void AddLibrarySearchDirectories(const ::Assets::DirectorySearchRules&);

		enum class ArtifactType
		{
			ArchivedFile,
			Blob
		};

		GeneralCompiler(ArtifactType artifactType);
        ~GeneralCompiler();
    protected:
        class Pimpl;
        std::shared_ptr<Pimpl> _pimpl;

        class Marker;
    };

}

