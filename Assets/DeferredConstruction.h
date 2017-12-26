// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsInternal.h"
#include "AssetFuture.h"
#include "AssetTraits.h"
#include <memory>

namespace Assets
{
	inline std::string AsBlob(const std::exception& e) { return e.what(); }

	template<typename AssetType, typename... Params>
		void AutoConstructToFuture(const std::shared_ptr<AssetFuture<AssetType>>& future, Params... params)
	{
		TRY {
			auto asset = AutoConstructAsset<AssetType>(params...);
			future->SetAsset(std::move(asset), {}, asset ? AssetState::Ready : AssetState::Invalid);
		} CATCH(const std::exception& e) {
			future->SetAsset(nullptr, AsBlob(e), AssetState::Invalid);
		} CATCH_END
	}

	template<typename AssetType>
		static void DefaultCompilerConstruction(
			std::shared_ptr<AssetFuture<AssetType>> future,
			const StringSection<ResChar> initializers[], unsigned initializerCount,
			uint64 compileTypeCode = GetCompileProcessType<AssetType>())
	{
		// Begin a compilation operation via the registered compilers for this type.
		// Our deferred constructor will wait for the completion of that compilation operation,
		// and then construct the final asset from the result

		auto marker = Internal::BeginCompileOperation(compileTypeCode, initializers, initializerCount);
		std::basic_string<ResChar> init0 = initializers[0].AsString();

		// Attempt to load the existing asset immediately. In some cases we should fall back to a recompile (such as, if the
		// version number is bad). We could attempt to push this into a background thread, also

		auto existingArtifact = marker->GetExistingAsset();
		if (existingArtifact->GetDependencyValidation() && existingArtifact->GetDependencyValidation()->GetValidationIndex()==0) {
			bool doRecompile = false;
			AutoConstructToFuture(future, *existingArtifact, MakeStringSection(init0))
			if (!doRecompile) return;
		}

		auto pendingCompile = marker->InvokeCompile();
		std::weak_ptr<AssetFuture<AssetType>> weakPtrToFuture = future;
		// We must poll the compile operation every frame, and construct the asset when it is ready. Note that we're
		// still going to end up constructing the asset in the main thread.
		OnFrameBarrier(
			[pendingCompile, weakPtrToFuture, init0]() -> bool {
				auto state = pendingCompile->GetAssetState();
				if (state == AssetState::Pending) return true;

				auto thatFuture = weakPtrToFuture.lock();
				if (!thatFuture) return false;
					
				if (state == AssetState::Invalid) {
					thatFuture->SetAsset(nullptr, {}, AssetState::Invalid);
					return false;
				}

				assert(state == AssetState::Ready);
				AutoConstructToFuture(future, *pendingCompile->GetArtifacts()[0].second, MakeStringSection(init0));
				return false;
			}
		);

		return result;
	}
}

