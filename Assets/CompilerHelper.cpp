// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CompilerHelper.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/IteratorUtils.h"

namespace Assets
{

    std::shared_ptr<::Assets::PendingCompileMarker> CompilerHelper::CheckExistingAsset(
        const ::Assets::IntermediateAssets::Store& destinationStore,
        const ::Assets::ResChar intermediateName[])
    {
            // check if there is an existing one we can use...
        if (DoesFileExist(intermediateName)) {
            auto depVal = destinationStore.MakeDependencyValidation(intermediateName);
            if (depVal && depVal->GetValidationIndex() == 0)
                return std::make_shared<::Assets::PendingCompileMarker>(
                    ::Assets::AssetState::Ready, intermediateName, 0, std::move(depVal));
        }
        return std::shared_ptr<::Assets::PendingCompileMarker>();
    }

    std::shared_ptr<::Assets::PendingCompileMarker> CompilerHelper::PrepareCompileMarker(
        const ::Assets::IntermediateAssets::Store& destinationStore,
        const ::Assets::ResChar intermediateName[],
        const CompileResult& compileResult)
    {
        auto depVal = destinationStore.WriteDependencies(
            intermediateName, compileResult._baseDir.c_str(),
            MakeIteratorRange(compileResult._dependencies));
        assert(depVal);

        return std::make_shared<::Assets::PendingCompileMarker>(
            ::Assets::AssetState::Ready, intermediateName, 0, std::move(depVal));
    }




}

