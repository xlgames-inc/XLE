// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsInternal.h"
#include "AssetFuture.h"
#include "AssetTraits.h"
#include "IArtifact.h"
#include "../ConsoleRig/Log.h"
#include <memory>

namespace Assets
{
	namespace Internal 
	{
		std::shared_ptr<IArtifactCompileMarker> BeginCompileOperation(uint64_t typeCode, const StringSection<ResChar> initializers[], unsigned initializerCount);
	}

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


		template<typename AssetType, typename... Params>
			static auto HasConstructToFutureOverride_Helper(int) -> decltype(
				AssetType::ConstructToFuture(std::declval<::Assets::AssetFuture<AssetType>&>(), std::declval<Params>()...), 
				std::true_type{});

		template<typename...>
			static auto HasConstructToFutureOverride_Helper(...) -> std::false_type;

		template<typename AssetType, typename... Params>
			struct HasConstructToFutureOverride : decltype(HasConstructToFutureOverride_Helper<AssetType, Params...>(0)) {};
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
		TRY {
			auto asset = AutoConstructAsset<AssetType>(std::forward<Params>(initialisers)...);
			future.SetAsset(std::move(asset), {});
		} CATCH (const Exceptions::ConstructionError& e) {
			future.SetInvalidAsset(e.GetDependencyValidation(), e.GetActualizationLog());
		} CATCH (const Exceptions::InvalidAsset& e) {
			future.SetInvalidAsset(e.GetDependencyValidation(), e.GetActualizationLog());
			throw;	// Have to rethrow InvalidAsset, otherwise we loose our dependency validation. This can occur when the AutoConstructAsset function itself loads some other asset
		} CATCH (const std::exception& e) {
			Log(Warning) << "No dependency validation associated with asset after construction failure. Hot reloading will not function for this asset." << std::endl;
			future.SetInvalidAsset(std::make_shared<DependencyValidation>(), AsBlob(e));
		} CATCH_END
	}

	template<typename AssetType>
		static void DefaultCompilerConstruction(
			AssetFuture<AssetType>& future,
			const StringSection<ResChar> initializers[], unsigned initializerCount,
			uint64 compileTypeCode = AssetType::CompileProcessType)
	{
		// Begin a compilation operation via the registered compilers for this type.
		// Our deferred constructor will wait for the completion of that compilation operation,
		// and then construct the final asset from the result

		TRY { 
			auto marker = Internal::BeginCompileOperation(compileTypeCode, initializers, initializerCount);
			// std::basic_string<ResChar> init0 = initializers[0].AsString();

			// Attempt to load the existing asset immediately. In some cases we should fall back to a recompile (such as, if the
			// version number is bad). We could attempt to push this into a background thread, also

			auto existingArtifact = marker->GetExistingAsset();
			if (existingArtifact && existingArtifact->GetDependencyValidation() && existingArtifact->GetDependencyValidation()->GetValidationIndex()==0) {
				bool doRecompile = false;
				auto asset = AutoConstructAsset<AssetType>(existingArtifact->GetBlob(), existingArtifact->GetDependencyValidation(), existingArtifact->GetRequestParameters());
				future.SetAsset(std::move(asset), {});
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
						auto artifacts = pendingCompile->GetArtifacts();
						if (!artifacts.empty()) {
							// Try to find an artifact named "log". Otherwise, look for one called "exception". If neither exists, just drop back to the first one
							IArtifact* logArtifact = nullptr;
							for (const auto& e:artifacts)
								if (e.first == "log") {
									logArtifact = e.second.get();
									break;
								}
							if (!logArtifact) {
								for (const auto& e:artifacts)
									if (e.first == "exception") {
										logArtifact = e.second.get();
										break;
									}
								if (!logArtifact)
									logArtifact = artifacts[0].second.get();
							}
							thatFuture.SetInvalidAsset(artifacts[0].second->GetDependencyValidation(), logArtifact->GetBlob());
						} else {
							thatFuture.SetInvalidAsset(nullptr, nullptr);
						}
						return false;
					}

					assert(state == AssetState::Ready);
					auto& artifact = *pendingCompile->GetArtifacts()[0].second;
					AutoConstructToFutureDirect(thatFuture, artifact.GetBlob(), artifact.GetDependencyValidation(), artifact.GetRequestParameters());
					return false;
				});
		} CATCH(const Exceptions::ConstructionError& e) {
			future.SetInvalidAsset(e.GetDependencyValidation(), e.GetActualizationLog());
		} CATCH (const Exceptions::InvalidAsset& e) {
			future.SetInvalidAsset(e.GetDependencyValidation(), e.GetActualizationLog());
			throw;	// Have to rethrow InvalidAsset, otherwise we loose our dependency validation. This can occur when the AutoConstructAsset function itself loads some other asset
		} CATCH(const std::exception& e) {
			Log(Warning) << "No dependency validation associated with asset (" << (initializerCount ? initializers[0].AsString() : "<<empty initializers>>") << ") after construction failure. Hot reloading will not function for this asset." << std::endl;
			future.SetInvalidAsset(std::make_shared<DependencyValidation>(), AsBlob(e));
		} CATCH_END
	}

	template<
		typename AssetType, typename... Params, 
		typename std::enable_if<Internal::HasConstructToFutureOverride<AssetType, Params...>::value>::type* = nullptr>
		void AutoConstructToFuture(AssetFuture<AssetType>& future, Params... initialisers)
	{
		AssetType::ConstructToFuture(future, std::forward<Params>(initialisers)...);
	}

	template<
		typename AssetType, typename... Params, 
		typename std::enable_if<Internal::AssetTraits<AssetType>::HasCompileProcessType && !Internal::HasConstructToFutureOverride<AssetType, Params...>::value>::type* = nullptr>
		void AutoConstructToFuture(AssetFuture<AssetType>& future, Params... initialisers)
	{
		StringSection<ResChar> inits[] = { initialisers... };
		DefaultCompilerConstruction<AssetType>(future, inits, dimof(inits));
	}

	template<
		typename AssetType, typename... Params, 
		typename std::enable_if<!Internal::AssetTraits<AssetType>::HasCompileProcessType && !Internal::HasConstructToFutureOverride<AssetType, Params...>::value>::type* = nullptr>
		void AutoConstructToFuture(AssetFuture<AssetType>& future, Params... initialisers)
	{
		AutoConstructToFutureDirect(future, std::forward<Params>(initialisers)...);
	}
}

