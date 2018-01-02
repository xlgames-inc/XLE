// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsInternal.h"
#include "AssetFuture.h"
#include "AssetTraits.h"
#include "IntermediateAssets.h"
#include <memory>

namespace Assets
{
	inline std::string AsBlob(const std::exception& e) { return e.what(); }

	namespace Internal 
	{
		std::shared_ptr<ICompileMarker> BeginCompileOperation(uint64_t typeCode, const StringSection<ResChar> initializers[], unsigned initializerCount);
	}

	#define ENABLE_IF(X) typename std::enable_if<X>::type* = nullptr

	namespace Internal
	{
		// Note -- here's a useful pattern that can turn any expression in a SFINAE condition
		// Taken from stack overflow -- https://stackoverflow.com/questions/257288/is-it-possible-to-write-a-template-to-check-for-a-functions-existence
		// If the expression in the first decltype() is invalid, we will trigger SFINAE and fall back to std::false_type
		template<typename T, typename... Params>
			static auto HasDirectAutoConstructAsset_Helper(int) -> decltype(AutoConstructAsset<T>(std::declval<Params>()...), std::true_type{});

		template<typename...>
			static auto HasDirectAutoConstructAsset_Helper(...) -> std::false_type;

		template<typename... Params>
			struct HasDirectAutoConstructAsset : decltype(HasDirectAutoConstructAsset_Helper<Params...>(0)) {};
	}
	
	// If we can construct an AssetType directly from the given parameters, then enable an implementation of
	// AutoConstructToFuture to do exactly that.
	// The compile operation version can work for any given initializer arguments, but the direct construction
	// version will only work when the arguments match one of the asset type's constructors. So, we need to avoid 
	// ambiguities between these implementations when they overlap.
	// To achieve this, we either need to use namespace tricks, or to use SFINAE to disable the implementation 
	// we don't need.
	template<
		typename AssetType, typename... Params, 
		typename std::enable_if<Internal::HasDirectAutoConstructAsset<AssetType, Params...>::value>::type* = nullptr>
		void AutoConstructToFutureDirect(AssetFuture<AssetType>& future, Params... initialisers)
	{
		TRY{
			auto asset = AutoConstructAsset<AssetType>(std::forward<Params>(initialisers)...);
			future.SetAsset(std::move(asset), {}, asset ? AssetState::Ready : AssetState::Invalid);
		} CATCH(const std::exception& e) {
			future.SetAsset(nullptr, AsBlob(e), AssetState::Invalid);
		} CATCH_END
	}

	template<typename AssetType>
		static void DefaultCompilerConstruction(
			AssetFuture<AssetType>& future,
			const StringSection<ResChar> initializers[], unsigned initializerCount,
			uint64 compileTypeCode = GetCompileProcessType<AssetType>())
	{
		// Begin a compilation operation via the registered compilers for this type.
		// Our deferred constructor will wait for the completion of that compilation operation,
		// and then construct the final asset from the result

		auto marker = Internal::BeginCompileOperation(compileTypeCode, initializers, initializerCount);
		// std::basic_string<ResChar> init0 = initializers[0].AsString();

		// Attempt to load the existing asset immediately. In some cases we should fall back to a recompile (such as, if the
		// version number is bad). We could attempt to push this into a background thread, also

		auto existingArtifact = marker->GetExistingAsset();
		if (existingArtifact->GetDependencyValidation() && existingArtifact->GetDependencyValidation()->GetValidationIndex()==0) {
			bool doRecompile = false;
			AutoConstructToFutureDirect(future, existingArtifact->GetBlob(), existingArtifact->GetDependencyValidation(), existingArtifact->GetRequestParameters());
			if (!doRecompile) return;
		}

		auto pendingCompile = marker->InvokeCompile();
		// We must poll the compile operation every frame, and construct the asset when it is ready. Note that we're
		// still going to end up constructing the asset in the main thread.
		future.SetPollingFunction(
			[pendingCompile](AssetFuture<AssetType>& thatFuture) -> bool {
				auto state = pendingCompile->GetAssetState();
				if (state == AssetState::Pending) return true;

				if (state == AssetState::Invalid) {
					thatFuture.SetAsset(nullptr, {}, AssetState::Invalid);
					return false;
				}

				assert(state == AssetState::Ready);
				auto& artifact = *pendingCompile->GetArtifacts()[0].second;
				AutoConstructToFutureDirect(thatFuture, artifact.GetBlob(), artifact.GetDependencyValidation(), artifact.GetRequestParameters());
				return false;
			});
	}

	template<
		typename AssetType, typename... Params, 
		ENABLE_IF(Internal::AssetTraits<AssetType>::HasCompileProcessType)>
		void AutoConstructToFuture(AssetFuture<AssetType>& future, Params... initialisers)
	{
		StringSection<ResChar> inits[] = { initialisers... };
		DefaultCompilerConstruction<AssetType>(future, inits, dimof(inits));
	}

	template<
		typename AssetType, typename... Params, 
		ENABLE_IF(!Internal::AssetTraits<AssetType>::HasCompileProcessType)>
		void AutoConstructToFuture(AssetFuture<AssetType>& future, Params... initialisers)
	{
		AutoConstructToFutureDirect(future, std::forward<Params>(initialisers)...);
	}

	#undef ENABLE_IF
}

