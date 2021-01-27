// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "IArtifact.h"
#include "AssetsCore.h"
#include "IArtifact.h"
#include "../ConsoleRig/GlobalServices.h"
#include "../Utility/Threading/CompletionThreadPool.h"
#include "../Core/Exceptions.h"

namespace Assets 
{
	void QueueCompileOperation(
		const std::shared_ptr<::Assets::ArtifactCollectionFuture>& future,
		std::function<void(::Assets::ArtifactCollectionFuture&)>&& operation)
	{
        if (!ConsoleRig::GlobalServices::GetInstance().GetLongTaskThreadPool().IsGood()) {
            operation(*future);
            return;
        }

		auto fn = std::move(operation);
		ConsoleRig::GlobalServices::GetInstance().GetLongTaskThreadPool().EnqueueBasic(
			[future, fn]() {
				TRY
				{
					fn(*future);
				}
				CATCH(const ::Assets::Exceptions::ConstructionError& e)
				{
					future->SetArtifactCollection(
						std::make_shared<::Assets::CompilerExceptionArtifact>(e.GetActualizationLog(), e.GetDependencyValidation()));
				}
				CATCH(const std::exception& e)
				{
					future->SetArtifactCollection(
						std::make_shared<::Assets::CompilerExceptionArtifact>(::Assets::AsBlob(e), nullptr));
				}
				CATCH(...)
				{
					future->SetState(::Assets::AssetState::Invalid);
				}
				CATCH_END
				assert(future->GetAssetState() != ::Assets::AssetState::Pending);	// if it is still marked "pending" at this stage, it will never change state
		});
	}

}

