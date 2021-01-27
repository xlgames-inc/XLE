// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetFuture.h"
#include "AssetUtils.h"

namespace Assets
{
	namespace Internal
	{
		template<size_t I = 0, typename... Tp>
			void CheckAssetState(
				AssetState& currentState, 
				Blob& actualizationBlob, 
				std::shared_ptr<DependencyValidation>& exceptionDepVal,
				std::tuple<std::shared_ptr<Tp>...>& actualized,
				const std::tuple<FuturePtr<Tp>...>& futures)
		{
			Blob queriedLog;
			DepValPtr queriedDepVal;
			auto state = std::get<I>(futures)->CheckStatusBkgrnd(std::get<I>(actualized), queriedDepVal, queriedLog);
			if (state != AssetState::Ready)
				currentState = state;

			if (state != AssetState::Invalid) {	// (on first invalid, stop looking any further)
				if constexpr(I+1 != sizeof...(Tp))
					CheckAssetState<I+1>(currentState, actualizationBlob, exceptionDepVal, actualized, futures);
			} else {
				std::stringstream str;
				str << "Failed to actualize subasset number (" << I << "): ";
				if (queriedLog) { str << ::Assets::AsString(queriedLog); } else { str << std::string("<<no log>>"); }
				actualizationBlob = AsBlob(str.str());
				exceptionDepVal = queriedDepVal;
			}
		}

		// Thanks to https://stackoverflow.com/questions/687490/how-do-i-expand-a-tuple-into-variadic-template-functions-arguments for this 
		// pattern. Using std::make_index_sequence to expand out a sequence of integers in a parameter pack, and then using this to
		// index the tuple
		template<typename Ty, typename Tuple, std::size_t ... I>
		auto ApplyMakeShared_impl(Tuple&& t, std::index_sequence<I...>) {
			return std::make_shared<Ty>(std::get<I>(std::forward<Tuple>(t))...);
		}
		template<typename Ty, typename Tuple>
		auto ApplyMakeShared(Tuple&& t) {
			using Indices = std::make_index_sequence<std::tuple_size<std::decay_t<Tuple>>::value>;
			return ApplyMakeShared_impl<Ty>(std::forward<Tuple>(t), Indices());
		}
	}

	template<typename... AssetTypes>
		class MultiAssetFuture
	{
	public:
		template<typename FinalAssetType>
			void ThenConstructToFuture(
				AssetFuture<FinalAssetType>& future,
				std::function<std::shared_ptr<FinalAssetType>(const std::shared_ptr<AssetTypes>&...)>&& continuationFunction)
		{
			future.SetPollingFunction(
				[subFutures{std::move(_subFutures)}, continuationFunction{std::move(continuationFunction)}](AssetFuture<FinalAssetType>& thatFuture) {

					AssetState currentState = AssetState::Ready;
					Blob actualizationBlob;
					std::shared_ptr<DependencyValidation> exceptionDepVal;
					std::tuple<std::shared_ptr<AssetTypes>...> actualized;
					Internal::CheckAssetState(currentState, actualizationBlob, exceptionDepVal, actualized, subFutures);

					if (currentState == AssetState::Invalid) {
						// Note that if one of the assets in invalid, we only consider the depVal for that specific asset
						thatFuture.SetInvalidAsset(exceptionDepVal, actualizationBlob);
						return false;
					} else if (currentState == AssetState::Ready) {
						Internal::FutureResolutionMoment<FinalAssetType> moment(thatFuture);
						TRY
						{
							auto finalConstruction = std::apply(continuationFunction, actualized);
							thatFuture.SetAsset(std::move(finalConstruction), {});
						} CATCH (const Exceptions::ConstructionError& e) {
							thatFuture.SetInvalidAsset(e.GetDependencyValidation(), e.GetActualizationLog());	
						} CATCH (const Exceptions::InvalidAsset& e) {
							thatFuture.SetInvalidAsset(e.GetDependencyValidation(), e.GetActualizationLog());
						} CATCH (const std::exception& e) {
							thatFuture.SetInvalidAsset(std::make_shared<DependencyValidation>(), AsBlob(e));
						} CATCH_END
						return false;
					}
					return true;
						
				});
		}

		template<typename FinalAssetType>
			void ThenConstructToFuture(
				AssetFuture<FinalAssetType>& future,
				std::function<void(AssetFuture<FinalAssetType>&, const std::shared_ptr<AssetTypes>&...)>&& continuationFunction)
		{
			future.SetPollingFunction(
				[subFutures{std::move(_subFutures)}, continuationFunction{std::move(continuationFunction)}](AssetFuture<FinalAssetType>& thatFuture) {

					AssetState currentState = AssetState::Ready;
					Blob actualizationBlob;
					std::shared_ptr<DependencyValidation> exceptionDepVal;
					std::tuple<std::shared_ptr<AssetTypes>...> actualized;
					Internal::CheckAssetState(currentState, actualizationBlob, exceptionDepVal, actualized, subFutures);
						
					if (currentState == AssetState::Invalid) {
						// Note that if one of the assets in invalid, we only consider the depVal for that specific asset
						thatFuture.SetInvalidAsset(exceptionDepVal, actualizationBlob);
						return false;
					} else if (currentState == AssetState::Ready) {
						Internal::FutureResolutionMoment<FinalAssetType> moment(thatFuture);
						TRY
						{
							// Note -- watch for a subtle edge condition here. Since we're passing the future to the callback function here,
							// it's quite possible that may set some other polling function on the same future. AssetFuture supports that,
							// but only because we already return false from this function.
							std::apply(continuationFunction, std::tuple_cat(std::make_tuple(std::ref(thatFuture)), actualized));
						} CATCH (const Exceptions::ConstructionError& e) {
							thatFuture.SetInvalidAsset(e.GetDependencyValidation(), e.GetActualizationLog());	
						} CATCH (const Exceptions::InvalidAsset& e) {
							thatFuture.SetInvalidAsset(e.GetDependencyValidation(), e.GetActualizationLog());
						} CATCH (const std::exception& e) {
							thatFuture.SetInvalidAsset(std::make_shared<DependencyValidation>(), AsBlob(e));
						} CATCH_END
						return false;
					}
					return true;
						
				});
		}

		template<typename FinalAssetType>
			void ThenConstructToFuture(AssetFuture<FinalAssetType>& future)
		{
			future.SetPollingFunction(
				[subFutures{std::move(_subFutures)}](AssetFuture<FinalAssetType>& thatFuture) {

					AssetState currentState = AssetState::Ready;
					Blob actualizationBlob;
					std::shared_ptr<DependencyValidation> exceptionDepVal;
					std::tuple<std::shared_ptr<AssetTypes>...> actualized;
					Internal::CheckAssetState(currentState, actualizationBlob, exceptionDepVal, actualized, subFutures);
						
					if (currentState == AssetState::Invalid) {
						// Note that if one of the assets in invalid, we only consider the depVal for that specific asset
						thatFuture.SetInvalidAsset(exceptionDepVal, actualizationBlob);
						return false;
					} else if (currentState == AssetState::Ready) {
						Internal::FutureResolutionMoment<FinalAssetType> moment(thatFuture);
						TRY
						{
							auto finalConstruction = Internal::ApplyMakeShared<FinalAssetType>(actualized);
							thatFuture.SetAsset(std::move(finalConstruction), {});
						} CATCH (const Exceptions::ConstructionError& e) {
							thatFuture.SetInvalidAsset(e.GetDependencyValidation(), e.GetActualizationLog());	
						} CATCH (const Exceptions::InvalidAsset& e) {
							thatFuture.SetInvalidAsset(e.GetDependencyValidation(), e.GetActualizationLog());
						} CATCH (const std::exception& e) {
							thatFuture.SetInvalidAsset(std::make_shared<DependencyValidation>(), AsBlob(e));
						} CATCH_END
						return false;
					}
					return true;
						
				});
		}

		std::tuple<FuturePtr<AssetTypes>...> _subFutures;
	};

	template<typename... AssetTypes>
		MultiAssetFuture<AssetTypes...> WhenAll(const FuturePtr<AssetTypes>&... subFutures)
	{
		return {
			std::tuple<FuturePtr<AssetTypes>...>{ subFutures... }
		};
	}

}
